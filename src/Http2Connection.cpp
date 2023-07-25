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
nghttp2::asio_http2::client::session Http2Connection::createSession(boost::asio::io_context &ioContext, const std::string &host, const std::string &port, bool secure) {
    if (secure)
    {
        boost::system::error_code ec;
        boost::asio::ssl::context tls_ctx(boost::asio::ssl::context::sslv23);
        tls_ctx.set_default_verify_paths();
        nghttp2::asio_http2::client::configure_tls_context(ec, tls_ctx);
        return nghttp2::asio_http2::client::session(ioContext, tls_ctx, host, port);
    }

    return nghttp2::asio_http2::client::session(ioContext, host, port);
}

void Http2Connection::configureSession() {
    session_.on_connect([this](boost::asio::ip::tcp::resolver::iterator endpoint_it)
    {
        status_ = Status::OPEN;
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Connected to '%s'", asString().c_str()), ERT_FILE_LOCATION));
        status_change_cond_var_.notify_one();
    });

    session_.on_error([this](const boost::system::error_code & ec)
    {
        notifyClose();
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Error on '%s'", asString().c_str()), ERT_FILE_LOCATION));
    });
}

Http2Connection::Http2Connection(const std::string& host,
                                 const std::string& port,
                                 bool secure) :
    work_(boost::asio::io_context::work(io_context_)),
    status_(Status::NOT_OPEN),
    host_(host),
    port_(port),
    secure_(secure),
    session_(nghttp2::asio_http2::client::session(createSession(io_context_, host, port, secure)))
{
    configureSession();
    thread_ = std::thread([&] { io_context_.run(); });
}

Http2Connection::~Http2Connection()
{
    closeImpl();
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
    io_context_.stop();

    if (thread_.joinable())
    {
        thread_.join();
    }

    if (isConnected())
    {
        session_.shutdown();
    }
}

void Http2Connection::close()
{
    session_.shutdown();
    notifyClose();
}

nghttp2::asio_http2::client::session& Http2Connection::getSession()
{
    return session_;
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

const Http2Connection::Status&
Http2Connection::getStatus()
const
{
    return status_;
}

bool Http2Connection::isConnected() const {
    return (status_ == Http2Connection::Status::OPEN);
}


bool Http2Connection::waitToBeConnected()
{
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("waitToBeConnected() to '%s'", asString().c_str()),  ERT_FILE_LOCATION));

    std::unique_lock<std::mutex> lock(mutex_);
    status_change_cond_var_.wait_for(lock, std::chrono::duration<int, std::milli>(2000), [&]
    {
        return (status_ != Http2Connection::Status::NOT_OPEN);
    });
    return isConnected();
}

bool Http2Connection::waitToBeDisconnected(const
        std::chrono::duration<int, std::milli>& time)
{
    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("waitToBeDisconnected() from '%s'", asString().c_str()),  ERT_FILE_LOCATION));

    std::unique_lock<std::mutex> lock(mutex_);
    return status_change_cond_var_.wait_for(lock, time, [&]
    {
        return (!isConnected());
    });
}

void Http2Connection::onClose(connection_callback
                              connection_closed_callback)
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
    result += ::status_to_str[getStatus()];

    return result;
}

}
}

