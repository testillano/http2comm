/*
 __________________________________________________________________________________
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

#include <mutex>
#include <sstream>
#include <memory>
#include <chrono>

#include <boost/asio.hpp>

#include <nghttp2/asio_http2_server.h>


namespace ert
{
namespace http2comm
{
class Http2Server;

/**
 * This class allows the response of an http2 transaction to be run in a
 * separate thread from the libnghttp2 loop thread.
 *
 * Just create a new stream within the nghttp2 hander, set the close callback,
 * and launch the working thread with the stream (or insert in a concurrent queue):
 *
 * <pre>
 *    auto stream = std::make_shared<Stream>(req, res, res.io_service, requestStream, server);
 *    res.on_close([stream](uint32_t error_code) { stream->close(true); });
 *    auto thread = std::thread([stream]()
 *    {
 *      stream->process();
 *    });
 * </pre>
 *
 * @see https://gist.github.com/tatsuhiro-t/ba3f7d72d037027ae47b
 */
class Stream : public std::enable_shared_from_this<Stream>
{
    std::mutex mutex_;
    const nghttp2::asio_http2::server::request& req_;
    const nghttp2::asio_http2::server::response& res_;
    std::shared_ptr<std::stringstream> request_body_;
    Http2Server *server_;
    bool closed_;

    // Response (members calculated at process()):
    unsigned int status_code_{};
    nghttp2::asio_http2::header_map response_headers_{};
    std::string response_body_{};
    boost::asio::deadline_timer *timer_{};

    // For metrics:
    std::chrono::microseconds reception_timestamp_us_{}; // timestamp in microsecods

public:

    Stream(const nghttp2::asio_http2::server::request& req,
           const nghttp2::asio_http2::server::response& res,
           std::shared_ptr<std::stringstream> requestBody,
           Http2Server *server) : req_(req), res_(res), request_body_(requestBody), server_(server), closed_(false), timer_(nullptr) {}

    Stream(const Stream&) = delete;
    ~Stream() = default;
    Stream& operator=(const Stream&) = delete;

    // Process reception
    void process();

    // Completes the nghttp2 transaction (res.end()) with the values calculated at process()
    void commit();

    // res.on_close()
    void close();
};


}
}

