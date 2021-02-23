/*
            _          _     _   _        ___
           | |        | |   | | | |      |__ \
   ___ _ __| |_   __  | |__ | |_| |_ _ __   ) |   __ ___  _ __ ___  _ __ ___
  / _ \ '__| __| |__| | '_ \| __| __| '_ \ / /  / __/ _ \| '_ ` _ \| '_ ` _ \
 |  __/ |  | |_       | | | | |_| |_| |_) / /_ | (_| (_) | | | | | | | | | | |
  \___|_|   \__|      |_| |_|\__|\__| .__/____| \___\___/|_| |_| |_|_| |_| |_|
                                    | |
                                    |_|

 HTTP/2 COMM LIBRARY C++ Based in @tatsuhiro-t nghttp2 library (https://github.com/nghttp2/nghttp2)
 Version 1.0.0
 https://github.com/testillano/http2comm

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2004-2021 Eduardo Ramos <http://www.teslayout.com>.

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

#include <tracing/Logger.hpp>
#include <http2comm/Http2Connection.hpp>

namespace nga = nghttp2::asio_http2;

namespace ert
{
namespace http2comm
{
Http2Connection::Http2Connection(const std::string& host,
                                 const std::string& port) :
    io_service_(new boost::asio::io_service()),
    work_(new boost::asio::io_service::work(*io_service_)),
    session_( nga::client::session(*io_service_, host, port)),
    status_(Status::NOT_OPEN),
    host_(host),
    port_(port)
{
    session_.on_connect([this](boost::asio::ip::tcp::resolver::iterator endpoint_it)
    {

        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString(
                             "Connected to %s:%s", host_.c_str(), port_.c_str()), ERT_FILE_LOCATION));
        status_ = Status::OPEN;
        status_change_cond_var_.notify_one();

    });

    session_.on_error([this](const boost::system::error_code & ec)
    {
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString(
                             "Error in the connection to %s:%s : %s", host_.c_str(), port_.c_str(), ec.message().c_str()), ERT_FILE_LOCATION));

        notifyClose();
    });

    execution_ = std::thread([&] {io_service_->run();});
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
    work_.reset();
    io_service_->stop();

    if (execution_.joinable())
    {
        execution_.join();
    }

    if (status_ == Status::OPEN)
    {
        session_.shutdown();
    }
}

void Http2Connection::close()
{
    closeImpl();
    notifyClose();
}

nga::client::session& Http2Connection::getSession()
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

const Http2Connection::Status&
Http2Connection::getStatus()
const
{
    return status_;
}

bool Http2Connection::waitToBeConnected()
{
    std::unique_lock<std::mutex> lock(mutex_);
    status_change_cond_var_.wait(lock, [&]
    {
        return (status_ != Http2Connection::Status::NOT_OPEN);
    });
    return (status_ == Http2Connection::Status::OPEN);
}

bool Http2Connection::waitToBeDisconnected(const
        std::chrono::duration<int, std::milli>& time)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return status_change_cond_var_.wait_for(lock, time, [&]
    {
        return (status_ != Http2Connection::Status::OPEN);
    });
}

void Http2Connection::onClose(connection_callback
                              connection_closed_callback)
{
    connection_closed_callback_ = connection_closed_callback;
}

}
}

