/*
 _________________________________________________________________________________
|             _          _     _   _        ___                                   |
|            | |        | |   | | | |      |__ \                                  |
|    ___ _ __| |_   __  | |__ | |_| |_ _ __   ) |   __ ___  _ __ ___  _ __ ___    |
|   / _ \ '__| __| |__| | '_ \| __| __| '_ \ / /  / __/ _ \| '_ ` _ \| '_ ` _ \   |
|  |  __/ |  | |_       | | | | |_| |_| |_) / /_ | (_| (_) | | | | | | | | | | |  |
|   \___|_|   \__|      |_| |_|\__|\__| .__/____| \___\___/|_| |_| |_|_| |_| |_|  |
|                                     | |                                         |
|                                     |_|                                         |
|_________________________________________________________________________________|

 HTTP/2 COMM LIBRARY C++ Based in @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2)
 Version 0.0.z
 https://github.com/testillano/http2comm

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <boost/asio/ip/tcp.hpp>

#include <ert/tracing/Logger.hpp>
#include <ert/http2comm/Http2Connection.hpp>


namespace {
std::map<ert::http2comm::Http2Connection::Status, std::string> status_to_str = {
    { ert::http2comm::Http2Connection::Status::NOT_OPEN, "NOT_OPEN" },
    { ert::http2comm::Http2Connection::Status::OPEN, "OPEN" },
    { ert::http2comm::Http2Connection::Status::CLOSED, "CLOSED" }
};
}


namespace ert
{
namespace http2comm
{
std::unique_ptr<nghttp2::asio_http2::client::session> Http2Connection::createSession(boost::asio::io_context &ioContext, const std::string &host, const std::string &port, bool secure) {
    if (secure)
    {
        boost::system::error_code ec;
        boost::asio::ssl::context tls_ctx(boost::asio::ssl::context::sslv23);
        tls_ctx.set_default_verify_paths();
        nghttp2::asio_http2::client::configure_tls_context(ec, tls_ctx);
        if (ec) {
            return nullptr;
        }
        return std::make_unique<nghttp2::asio_http2::client::session>(ioContext, tls_ctx, host, port);
    }

    return std::make_unique<nghttp2::asio_http2::client::session>(ioContext, host, port);
}

void Http2Connection::configureSession() {
    // The resolver iterator type exposed by Boost.Asio has changed across
    // versions: `boost::asio::ip::tcp::resolver::iterator` (nested typedef)
    // is not available in recent Boost releases, where only the underlying
    // template `boost::asio::ip::basic_resolver_iterator<tcp>` remains.
    // nghttp2-asio's `connect_cb` expects exactly that template instance,
    // so use a generic lambda to let the compiler deduce the correct type
    // regardless of the Boost version in use.
    session_->on_connect([this](auto endpoint_it)
    {
        status_ = Status::OPEN;
        ever_connected_ = true;
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Connected to '%s'", asString().c_str()), ERT_FILE_LOCATION));
        status_change_cond_var_.notify_one();
    });

    session_->on_error([this](const boost::system::error_code & ec)
    {
        notifyClose();
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Error on '%s'", asString().c_str()), ERT_FILE_LOCATION));
    });
}

Http2Connection::Http2Connection(const std::string& host,
                                 const std::string& port,
                                 bool secure) :
    work_(boost::asio::make_work_guard(io_context_)),
    status_(Status::NOT_OPEN),
    host_(host),
    port_(port),
    secure_(secure),
    session_(createSession(io_context_, host, port, secure))
{
    if (!session_) {
        throw std::runtime_error("Failed to create HTTP/2 session");
    }
    configureSession();
    thread_ = std::thread([&] { io_context_.run(); }); // 1 thread
}

Http2Connection::~Http2Connection()
{
    // A destructor must never let an exception escape (it would call
    // std::terminate). closeImpl() joins/stops the io_context thread and shuts
    // the session down, operations that may throw under teardown races.
    try
    {
        closeImpl();
    }
    catch (const std::exception& e)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Exception while closing connection '%s': %s", asString().c_str(), e.what()), ERT_FILE_LOCATION));
    }
    catch (...)
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unknown exception while closing connection '%s'", asString().c_str()), ERT_FILE_LOCATION));
    }
}

bool Http2Connection::reconnect() {
    notifyClose();
    session_.reset();
    // The new session has not connected yet: clear the flag so a teardown while
    // reconnecting does not call shutdown() on a not-yet-connected session.
    // on_connect() will set it true again once the handshake completes.
    ever_connected_ = false;
    auto new_session_ptr = createSession(io_context_, host_, port_, secure_);
    if (!new_session_ptr) {
        return false;
    }

    session_ = std::move(new_session_ptr);
    configureSession();
    status_ = Status::NOT_OPEN; // important to make waitToBeConnected() works:
    if (waitToBeConnected())
    {
        return true;
    }

    LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unable to reconnect '%s'", asString().c_str()), ERT_FILE_LOCATION));
    return false; // new_session_ptr will be destroyed
}

void Http2Connection::notifyClose()
{
    status_ = Status::CLOSED;
    status_change_cond_var_.notify_one();

    // Notify through the callback that the connection is closed
    if (connection_closed_callback_)
    {
        connection_closed_callback_(*this);
    }
}

void Http2Connection::closeImpl()
{
    notifyClose();

    // A connection owns its io_context and the single thread that runs it.
    // INVARIANT (caller responsibility): a connection must NEVER be destroyed
    // from its own io_context thread. Doing so makes a clean teardown
    // impossible -- joining the thread from itself deadlocks, and the
    // session/io_context are still in use by the running handler. Callers must
    // guarantee this by releasing the last reference OFF the io thread (e.g.
    // h2agent posts chain continuations to a worker pool so the endpoint's last
    // reference is dropped there, not inside the connection's io thread).
    //
    // KNOWN LIMITATION / TECH DEBT: if the invariant is violated we cannot make
    // teardown fully safe with the current design (io_context_ is a direct
    // member destroyed as ~Http2Connection unwinds, while the offending thread
    // is still inside io_context_.run()). We log loudly and detach to avoid an
    // immediate terminate(), but this leaves a use-after-free window (confirmed
    // by ASAN). A complete fix would require reworking ownership so the
    // io_context outlives its thread (e.g. shared_ptr self-ownership + posting
    // destruction elsewhere); intentionally deferred to avoid a large refactor.
    // The supported guarantee is: honour the invariant and this branch is never
    // taken.
    const bool onOwnThread = thread_.joinable() &&
                             (thread_.get_id() == std::this_thread::get_id());

    if (onOwnThread)
    {
        // Invariant violated: we are being destroyed from inside our own event
        // loop. There is NO fully-safe teardown here -- once ~Http2Connection
        // returns, the members (io_context_, session_) are destroyed while this
        // very thread is still executing inside io_context_.run(). We cannot
        // join (self-deadlock) and detaching does not prevent the member
        // destruction that follows. The only real fix is to never reach this
        // state; callers must release the last reference off the io thread
        // (h2agent posts chain continuations to a worker pool for exactly this
        // reason). We log loudly so the ownership bug is diagnosable, detach to
        // let std::thread's destructor not terminate(), and skip session
        // shutdown / io_context stop (touching them from the running handler
        // would only widen the race).
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString(
            "Connection '%s' destroyed from its own io_context thread: unsafe teardown (caller ownership bug). Release the connection off its io thread.",
            asString().c_str()), ERT_FILE_LOCATION));
        thread_.detach();
        return;
    }

    // Normal teardown from an external thread.
    // Shut the session down BEFORE stopping the io_context: nghttp2-asio's
    // shutdown() posts work to the event loop, which must still be alive to
    // process it. Only shut down a session that actually connected: shutting
    // down one that never established a connection (e.g. endpoint on a closed
    // port, stuck reconnecting) dereferences half-initialized internals and
    // segfaults inside nghttp2.
    if (session_ && ever_connected_)
    {
        session_->shutdown();
    }

    io_context_.stop();

    if (thread_.joinable())
    {
        thread_.join();  // safe: we are on a different thread and the loop is stopping
    }
}

void Http2Connection::close()
{
    notifyClose();
    // Same guard as closeImpl(): never shut down a session that never connected
    // (segfaults inside nghttp2). This method keeps the io_context running (it
    // is a soft close, not a teardown), so no thread handling is needed here.
    if (session_ && ever_connected_)
    {
        session_->shutdown();
    }
}

nghttp2::asio_http2::client::session& Http2Connection::getSession()
{
    if (!session_) {
        throw std::runtime_error("Session is not initialized/connected.");
    }
    return *session_;
}

const std::string& Http2Connection::getHost() const
{
    return host_;
}

const std::string& Http2Connection::getPort() const
{
    return port_;
}

bool Http2Connection::isSecure() const
{
    return secure_;
}

Http2Connection::Status
Http2Connection::getStatus()
const
{
    return status_.load();
}

bool Http2Connection::isConnected() const {
    return (status_ == Http2Connection::Status::OPEN);
}


bool Http2Connection::waitToBeConnected()
{
    static constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 2000;

    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("waitToBeConnected() to '%s'", asString().c_str()),  ERT_FILE_LOCATION));

    std::unique_lock<std::mutex> lock(mutex_);
    status_change_cond_var_.wait_for(lock, std::chrono::duration<int, std::milli>(DEFAULT_CONNECTION_TIMEOUT_MS), [&]
    {
        return (status_ != Http2Connection::Status::NOT_OPEN);
    });
    return isConnected();
}

bool Http2Connection::waitToBeDisconnected(const std::chrono::duration<int, std::milli>& time)
{
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("waitToBeDisconnected() from '%s'", asString().c_str()),  ERT_FILE_LOCATION));

    std::unique_lock<std::mutex> lock(mutex_);
    return status_change_cond_var_.wait_for(lock, time, [&]
    {
        return (!isConnected());
    });
}

void Http2Connection::onClose(connection_callback connection_closed_callback)
{
    connection_closed_callback_ = connection_closed_callback;
}

std::string Http2Connection::asString() const {
    std::string result{};

    result += (secure_ ? "secured":"regular");
    result += " connection | host: ";
    result += host_;
    result += " | port: ";
    result += port_;
    result += " | status: ";
    auto it = ::status_to_str.find(getStatus());
    result += (it != ::status_to_str.end()) ? it->second : "UNKNOWN";

    return result;
}

}
}

