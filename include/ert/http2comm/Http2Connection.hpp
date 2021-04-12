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

#pragma once

#include <string>
#include <thread>
#include <chrono>
#include <functional>
#include <condition_variable>

#include <nghttp2/asio_http2_client.h>

namespace nghttp2
{
namespace asio_http2
{
namespace client
{
class session;
}
}
}

namespace ert
{
namespace http2comm
{

class Http2Connection;
using connection_callback = std::function<void(Http2Connection&)>;

class Http2Connection
{
public:
    /**
     * Enumeration for all possible connection status
     */
    enum class Status
    {
        NOT_OPEN,
        OPEN,
        CLOSED
    };

public:
    /// Class constructors

    /**
     * Class constructor given host and port
     *
     * \param host Endpoint host
     * \param port Endpoint port
     */
    Http2Connection(const std::string& host, const std::string& port);

    /**
     * Copy constructor
     *
     * \param o Original object
     * \note Deleted as it makes no sense to create a copy of the connection
     */
    Http2Connection(const Http2Connection& o) = delete;

    /**
     * Move constructor
     *
     * \param o Original object
     * \note Deleted as it makes no sense to modify the connection
     */
    Http2Connection(Http2Connection&& o) = delete;

    /**
     * Class destructor
     */
    ~Http2Connection();

    /**
     * Copy assignation operator
     *
     * \param o Original object
     * \return A reference to itself
     * \note Deleted as it makes no sense to modify the connection
     */
    Http2Connection& operator=(const Http2Connection& o) = delete;

    /**
     * Move assignation operator
     *
     * \param o Original object
     * \return A reference to itself
     * \note Deleted as it makes no sense to modify the connection
     */
    Http2Connection& operator=(Http2Connection&& o) = delete;

    /**
     * Returns the connection session
     *
     * \return ASIO connection session
     */
    nghttp2::asio_http2::client::session& getSession();

    /**
     * Returns the endpoint host
     *
     * \return Endpoint host
     */
    const std::string& getHost() const;

    /**
     * Returns the endpoint port
     *
     * \return Endpoint port
     */
    const std::string& getPort() const;

    /**
     * Returns the connection status
     *
     * \return Http2Connection status
     */
    const Status& getStatus() const;

    /**
     * Waits while the connection is in progress
     *
     * \return true if connected and false if the connection could not be established
     */
    bool waitToBeConnected();

    /**
     * Wait for the client to be disconnected from the server
     *
     * \param max_time Max time to wait for the client to disconnect
     * \return true if the client disconnected within max_time, false otherwise
     */
    bool waitToBeDisconnected(const std::chrono::duration<int, std::milli>&
                              max_time);

    /**
     * Closes the connection
     */
    void close();

    /**
     * Sets callback called when connection is closed
     * \param connection_closed_callback
     */
    void onClose(connection_callback connection_closed_callback);

private:
    /// Internal methods

    /**
     * Performs actual close of the connection
     */
    void closeImpl();

    /**
     * Notifies that conection has been closed
     */
    void notifyClose();

private:

    /// ASIO attributes
    std::unique_ptr<boost::asio::io_service>
    io_service_; //non-copyable and non-movable
    std::unique_ptr<boost::asio::io_service::work> work_;
    nghttp2::asio_http2::client::session session_; //non-copyable

    /// Class attributes
    Status status_;
    const std::string host_;
    const std::string port_;
    connection_callback connection_closed_callback_;

    /// Concurrency attributes
    std::mutex mutex_;
    std::thread execution_;
    std::condition_variable status_change_cond_var_;


};

}
}

