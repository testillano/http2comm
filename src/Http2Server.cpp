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

#include <string>
#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>
#include <boost/exception/diagnostic_information.hpp>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>
#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Headers.hpp>


namespace ert
{
namespace http2comm
{

Http2Server::Http2Server(const std::string& name, size_t workerThreads, boost::asio::io_service *timersIoService): name_(name), timers_io_service_(timersIoService) {

    queue_dispatcher_ = new QueueDispatcher(name + "_queueDispatcher", workerThreads);
}

Http2Server::~Http2Server() {
    delete queue_dispatcher_;
}

std::string Http2Server::getApiPath() const
{
    std::string result{};
    if (api_name_.empty()) return result;

    result += "/";
    result += api_name_;

    if (api_version_.empty()) return result;

    result += "/";
    result += api_version_;

    return result;
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

    // Headers:
    Http2Headers hdrs;
    hdrs.addVersion(getApiVersion());
    hdrs.addLocation(location);
    hdrs.addAllowedMethods(allowedMethods);
    hdrs.addContentLength(responseBody.size());
    hdrs.addContentType(((statusCode >= 200) && (statusCode < 300)) ? "application/json" : "application/problem+json");
    headers = hdrs.getHeaders();
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

                queue_dispatcher_->dispatch(stream);
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
        try {
            boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);

            if (!server_key_password_.empty()) {
                tls.set_password_callback ([this](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                    return server_key_password_;
                });
            }

            tls.use_private_key_file(key, boost::asio::ssl::context::pem);
            tls.use_certificate_chain_file(cert);
            nghttp2::asio_http2::server::configure_tls_context_easy(ec, tls);
            server_.listen_and_serve(ec, tls, bind_address, listen_port, asynchronous);
            if (asynchronous) server_.join();
        }
        catch (const boost::exception& e)
        {
            std::cerr << boost::diagnostic_information(e) << '\n';
            stop();
            return EXIT_FAILURE;
        }
        catch (...) {
            std::cerr << "Server initialization failure in " << name_ << '\n';
            stop();
            return EXIT_FAILURE;
        }

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
