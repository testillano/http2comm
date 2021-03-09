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

#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>
#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>


namespace ert
{
namespace http2comm
{

void Http2Server::launchWorker(std::shared_ptr<Stream> stream,
                               const nghttp2::asio_http2::server::request& req, const std::string& requestBody)
{
    if (max_worker_threads_ > 0)
    {
        while (w_threads_ >= max_worker_threads_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    w_threads_++;
    auto thread = std::thread([this, stream, &req, requestBody]()
    {
        processRequest(stream, req, requestBody);
        w_threads_--;
    });
    thread.detach();
}


void Http2Server::commitError(std::shared_ptr<Stream> stream,
                              const nghttp2::asio_http2::server::request& req,
                              std::pair<int, const std::string> error,
                              const std::string& location,
                              const std::vector<std::string>& allowedMethods)
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
                                    req.uri().path.c_str(), code, s_errorCause.c_str()), ERT_FILE_LOCATION);

    ResponseHeader responseHeader(api_version_, location, allowedMethods);

    // commit results
    stream->commit(code, responseHeader.getResponseHeader(j_errorCause.size(),
                   code), j_errorCause);
}


void Http2Server::processRequest(std::shared_ptr<Stream> stream,
                                 const nghttp2::asio_http2::server::request& req, const std::string& requestBody)
{
    std::vector<std::string> allowedMethods;

    if (!checkMethodIsAllowed(req, allowedMethods))
    {
        commitError(stream, req, ert::http2comm::METHOD_NOT_ALLOWED, "",
                    allowedMethods);
    }
    else if (!checkMethodIsImplemented(req))
    {
        commitError(stream, req, ert::http2comm::METHOD_NOT_IMPLEMENTED);
    }
    else
    {
        if (checkHeaders(req))
        {
            std::string apiPath = getApiPath();

            if (apiPath != "")
            {
                if (!ert::http2comm::URLFunctions::matchPrefix(req.uri().path, apiPath))
                {
                    commitError(stream, req, ert::http2comm::WRONG_API_NAME_OR_VERSION);
                    return;
                }
            }

            unsigned int statusCode;
            auto headers = nghttp2::asio_http2::header_map();
            std::string responseBody;

            receive(req, requestBody, statusCode, headers, responseBody);

            stream->commit(statusCode, headers, responseBody);
        }
        else
        {
            commitError(stream, req, ert::http2comm::UNSUPPORTED_MEDIA_TYPE);
        }
    }
}


nghttp2::asio_http2::server::request_cb Http2Server::handler()
{
    return [&](const nghttp2::asio_http2::server::request & req,
               const nghttp2::asio_http2::server::response & res)
    {
        auto request = std::make_shared<std::stringstream>();
        req.on_data([request, &req, &res, this](const uint8_t* data, std::size_t len)
        {
            if (len > 0)
            {
                std::copy(data, data + len, std::ostream_iterator<uint8_t>(*request));
            }
            else
            {
                auto stream = std::make_shared<Stream>(req, res, res.io_service());
                res.on_close([stream](uint32_t error_code)
                {
                    stream->close(true);
                });

                launchWorker(stream, req, request->str());
            }
        });
    };
}


int Http2Server::serve(const std::string& bind_address,
                       const std::string& listen_port,
                       const std::string& key,
                       const std::string& cert,
                       int numberThreads,
                       bool asynchronous)
{
    bool is_h2c = key.empty() or cert.empty();

    boost::system::error_code ec;

    server_.handle("/", handler());
    server_.num_threads(numberThreads);

    if (not is_h2c)
    {
        boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);
        tls.use_private_key_file(key, boost::asio::ssl::context::pem);
        tls.use_certificate_chain_file(cert);
        nghttp2::asio_http2::server::configure_tls_context_easy(ec, tls);
        server_.listen_and_serve(ec, tls, bind_address, listen_port, asynchronous);
    }
    else
    {
        server_.listen_and_serve(ec, bind_address, listen_port, asynchronous);
    }

    if (ec)
    {
        std::stringstream ss;
        ss << "Initialization error in " << name_ << " (" << bind_address << ":" <<
           listen_port << "): " << ec.message();
        ert::tracing::Logger::error(ss.str().c_str(), ERT_FILE_LOCATION);
        std::cerr << ss.str() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int Http2Server::stop()
{
    server_.stop();
    return EXIT_SUCCESS;
}

}
}
