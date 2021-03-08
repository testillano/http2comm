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

#ifndef ERT_HTTP2COMM_HTTP2SERVER_H_
#define ERT_HTTP2COMM_HTTP2SERVER_H_

#include <string>
#include <memory>

#include <nghttp2/asio_http2_server.h>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/ResponseHeader.hpp>

namespace ert
{
namespace http2comm
{

class Http2Server
{
    std::string name_{};
    std::string api_name_{};
    std::string api_version_{};
    nghttp2::asio_http2::server::request_cb handler();
    std::atomic<int> w_threads_{};
    int max_worker_threads_{};

    void commitError(std::shared_ptr<Stream> stream,
                     const nghttp2::asio_http2::server::request& req,
                     std::pair<int, const std::string> /* error code/cause */,
                     const std::string& location = "",
                     const std::vector<std::string>& allowedMethods = std::vector<std::string>());

protected:

    nghttp2::asio_http2::server::http2 server_;

    void processRequest(std::shared_ptr<Stream> st,
                        const nghttp2::asio_http2::server::request& req, const std::string&);


public:
    Http2Server(const std::string& name,
                size_t maxWorkerThreads = -1) : name_(name),
        max_worker_threads_(maxWorkerThreads) {;}

    void setApiName(const std::string& apiName)
    {
        api_name_ = apiName;
    }
    void setApiVersion(const std::string& apiVersion)
    {
        api_version_ = apiVersion;
    }

    std::string getApiPath() const
    {
        return (api_name_ + "/" + api_version_);
    }

    virtual bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods) = 0;

    virtual bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req) = 0;

    virtual bool checkHeaders(const nghttp2::asio_http2::server::request& req) = 0;

    void launchWorker(std::shared_ptr<Stream> stream,
                      const nghttp2::asio_http2::server::request& req, const std::string&);

    // Reception callback to be defined
    virtual void receive(const nghttp2::asio_http2::server::request&,
                         const std::string& requestBody,
                         unsigned int& statusCode, nghttp2::asio_http2::header_map& headers,
                         std::string& responseBody) = 0;

    // Default error handler
    //virtual nghttp2::asio_http2::server::request_cb errorHandler();

    int serve(const std::string& bind_address,
              const std::string& listen_port,
              const std::string& key,
              const std::string& cert,
              int numberThreads,
              bool asynchronous = false);

    int stop();
};

}
}


#endif
