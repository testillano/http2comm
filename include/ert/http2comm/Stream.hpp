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

#ifndef ERT_HTTP2COMM_STREAM_H_
#define ERT_HTTP2COMM_STREAM_H_

#include <mutex>

#include <nghttp2/asio_http2_server.h>

namespace nga = nghttp2::asio_http2;

namespace ert
{
namespace http2comm
{
/**
 * This class allows the response of an http2 transaction to be run in a
 * separate thread from the libnghttp2 loop thread.
 *
 * Example of use:
 * 1. Within the nghttp2 hander, before send:
 *    auto stream = std::make_shared<Stream>(req, res, res.io_service);
 * 2. Set the close callback:
 *    res.on_close([stream](uint32_t error_code) { stream->close(true); });
 * 3. Launch the working thread:
 *    auto thread = std::thread([stream]()
 *    {
 *      // do processing ...
 *      // ...
 *      // ...
 *      stream->commit(); // this completes the transaction (res.end())
 *    });
 *
 * @see https://gist.github.com/tatsuhiro-t/ba3f7d72d037027ae47b
 */
class Stream : public std::enable_shared_from_this<Stream>
{
public:
    Stream(const nga::server::request& req, const nga::server::response& res,
           boost::asio::io_service& io_service)
        : io_service_(io_service), req_(req), res_(res), closed_(false) {}
    Stream(const Stream&) = delete;
    ~Stream() = default;
    Stream& operator=(const Stream&) = delete;

    /**
    * Completes the nghttp2 transaction (res.end())
    */
    void commit(unsigned int statusCode,
                const nghttp2::asio_http2::header_map& headers,
                const std::string& responseBody);

    /**
    * Close indication
    */
    void close(bool c);

    // getters
    const nga::server::request& getRequest() const
    {
        return req_;
    }

private:
    std::mutex mutex_;
    boost::asio::io_service& io_service_;
    const nga::server::request& req_;
    const nga::server::response& res_;
    bool closed_;
};


}
}


#endif
