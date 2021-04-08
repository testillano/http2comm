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
#include <ert/http2comm/ResponseHeader.hpp>


namespace ert
{
namespace http2comm
{

Http2Server::Http2Server(const std::string& name, size_t workerThreads): name_(name) {

    queue_dispatcher_ = ((workerThreads != -1) ? new QueueDispatcher(name + "_queueDispatcher", workerThreads) : nullptr);
}

Http2Server::~Http2Server() {
    delete queue_dispatcher_;
}

void Http2Server::receiveError(const nghttp2::asio_http2::server::request& req,
                               const std::string& requestBody,
                               unsigned int& statusCode,
                               nghttp2::asio_http2::header_map& headers,
                               std::string& responseBody,
                               const std::pair<int, const std::string>& error,
                               const std::string &location,
                               const std::vector<std::string>& allowedMethods)
{
    statusCode = error.first;
    responseBody = "{}";

    std::string s_errorCause = "<none>";
    if (error.second != "")
    {
        s_errorCause = error.second;
        responseBody = "{\"cause\":\"" + (s_errorCause + "\"}");
    }

    ert::tracing::Logger::error(ert::tracing::Logger::asString(
                                    "UNSUCCESSFUL REQUEST: path %s, code %d, error cause %s",
                                    req.uri().path.c_str(), statusCode, s_errorCause.c_str()), ERT_FILE_LOCATION);

    ResponseHeader responseHeader(getApiVersion(), location, allowedMethods);
    headers = responseHeader.getResponseHeader(responseBody.size(), statusCode);
}


void Http2Server::launchWorker(std::shared_ptr<Stream> stream)
{
    auto thread = std::thread([this, stream]()
    {
        stream->process();
    });
    thread.detach();
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
                auto stream = std::make_shared<Stream>(req, res, res.io_service(), request, this);
                res.on_close([stream](uint32_t error_code)
                {
                    stream->close(true);
                });

                if (queue_dispatcher_) {
                    queue_dispatcher_->dispatch(stream); // pre initialized threads
                }
                else {
                    launchWorker(stream); // dynamic threads creation
                }
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
    bool secure = !key.empty() && !cert.empty();

    boost::system::error_code ec;

    server_.handle("/", handler());
    server_.num_threads(numberThreads);

    if (secure)
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
