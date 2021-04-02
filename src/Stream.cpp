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

#include <mutex>
#include <chrono> // temporary

#include <nghttp2/asio_http2_server.h>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>
#include <ert/http2comm/ResponseHeader.hpp>

namespace ert
{
namespace http2comm
{

void Stream::processError(std::pair<int, const std::string> error, const std::string &location, const std::vector<std::string>& allowedMethods)
{
    int code = error.first;
    std::string s_errorCause = "<none>";
    std::string j_errorCause = "{}";

    if (error.second != "")
    {
        s_errorCause = error.second;
        j_errorCause = "{\"cause\":\"" + (s_errorCause + "\"}");
    }

    ert::tracing::Logger::error(ert::tracing::Logger::asString(
                                    "UNSUCCESSFUL REQUEST: path %s, code %d, error cause %s",
                                    req_.uri().path.c_str(), code, s_errorCause.c_str()), ERT_FILE_LOCATION);

    ResponseHeader responseHeader(server_->getApiVersion(), location, allowedMethods);

    // commit results
    commit(code, responseHeader.getResponseHeader(j_errorCause.size(), code), j_errorCause);
}

void Stream::process()
{
    std::vector<std::string> allowedMethods;

    if (!server_->checkMethodIsAllowed(req_, allowedMethods))
    {
        processError(ert::http2comm::METHOD_NOT_ALLOWED, "", allowedMethods);
    }
    else if (!server_->checkMethodIsImplemented(req_))
    {
        processError(ert::http2comm::METHOD_NOT_IMPLEMENTED);
    }
    else
    {
        if (server_->checkHeaders(req_))
        {
            std::string apiPath = server_->getApiPath();

            if (apiPath != "")
            {
                if (!ert::http2comm::URLFunctions::matchPrefix(req_.uri().path, apiPath))
                {
                    processError(ert::http2comm::WRONG_API_NAME_OR_VERSION);
                    return;
                }
            }

            unsigned int statusCode;
            auto headers = nghttp2::asio_http2::header_map();
            std::string responseBody;

            server_->receive(req_, request_->str(), statusCode, headers, responseBody);

            commit(statusCode, headers, responseBody);
        }
        else
        {
            processError(ert::http2comm::UNSUPPORTED_MEDIA_TYPE);
        }
    }

}

void Stream::commit(unsigned int statusCode,
                    const nghttp2::asio_http2::header_map& headers,
                    const std::string& responseBody)
{
    auto self = shared_from_this();
    io_service_.post([self, statusCode, headers, responseBody]()
    {
        std::lock_guard<std::mutex> lg(self->mutex_);

        if (self->closed_)
        {
            return;
        }

        // WORKER THREAD PROCESSING CANNOT BE DONE HERE
        // (nghttp2 pool must be free), SO, IT WILL BE
        // DONE BEFORE commit()

        self->res_.write_head(statusCode, headers);
        self->res_.end(responseBody);
    });
}

void Stream::close(bool c)
{
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = c;
}

}
}
