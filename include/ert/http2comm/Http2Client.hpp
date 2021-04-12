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
#include <future>
#include <memory>

#include <nghttp2/asio_http2.h>

namespace nghttp2
{
namespace asio_http2
{
namespace client
{
class session;
class request;
}
}
}

namespace boost
{
namespace system
{
class error_code;
}
}

namespace ert
{
namespace http2comm
{

class Http2Connection;

typedef std::function <
const nghttp2::asio_http2::client::request* (const
        nghttp2::asio_http2::client::session& sess,
        const nghttp2::asio_http2::header_map& map,
        boost::system::error_code& ec) > submit_callback;
class Http2Client
{
    //inner classes
public:

    enum class Method
    {
        GET, PUT, POST, DELETE
    };

    struct response
    {
        std::string body;
        int status; //http result code
    };

private:
    struct task
    {
        task()
        {
            done = response.get_future();
        }

        std::promise<Http2Client::response> response;
        std::future<Http2Client::response> done;
        std::string data; //buffer to store a possible temporary data
    };

    //class members
public:
    Http2Client(std::shared_ptr<Http2Connection> connection,
                const std::chrono::milliseconds& request_timeout = std::chrono::milliseconds(
                            1000));
    Http2Client(const std::chrono::milliseconds& request_timeout =
                    std::chrono::milliseconds(
                        1000));
    virtual ~Http2Client() {};

    virtual void setHttp2Connection(std::shared_ptr<Http2Connection> connection);
    virtual std::shared_ptr<Http2Connection> getHttp2Connection();
    virtual Http2Client::response send(const Http2Client::Method&,
                                       const std::string& uri_path,
                                       const std::string& json);
private:
    std::shared_ptr<Http2Connection> connection_;
    const std::chrono::milliseconds request_timeout_;
    const std::string host_;
    const std::string scheme_;

    std::string getUri(const std::string& uri_path);
};

}
}

