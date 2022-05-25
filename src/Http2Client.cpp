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

#include <boost/system/error_code.hpp>
#include <nghttp2/asio_http2_client.h>
#include <map>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/Http2Client.hpp>
#include <ert/http2comm/Http2Connection.hpp>

namespace
{
std::map<ert::http2comm::Http2Client::Method, std::string> method_to_str = {
    { ert::http2comm::Http2Client::Method::POST, "POST" },
    { ert::http2comm::Http2Client::Method::GET, "GET" },
    { ert::http2comm::Http2Client::Method::PUT, "PUT" },
    { ert::http2comm::Http2Client::Method::DELETE, "DELETE" },
    { ert::http2comm::Http2Client::Method::HEAD, "HEAD" }
};
}

namespace ert
{
namespace http2comm
{
Http2Client::Http2Client(std::shared_ptr<Http2Connection>
                         connection,
                         const std::chrono::milliseconds& request_timeout) :
    connection_(connection), request_timeout_(request_timeout)
{
}

Http2Client::Http2Client(const std::chrono::milliseconds&
                         request_timeout) :
    request_timeout_(request_timeout)
{
}

void Http2Client::setHttp2Connection(
    std::shared_ptr<Http2Connection>
    connection)
{
    connection_ = connection;
}

std::shared_ptr<Http2Connection>
Http2Client::getHttp2Connection()
{
    return connection_;
}

Http2Client::response Http2Client::send(
    const Http2Client::Method &method,
    const std::string &uri,
    const std::string &body,
    const nghttp2::asio_http2::header_map &headers)
{
    // Internal Server Error response if connection not initialized
    if (!connection_)
    {
        LOGINFORMATIONAL(ert::tracing::Logger::informational("There must be a connection instance !", ERT_FILE_LOCATION));
        return Http2Client::response{"", 500};
    }
    if (connection_->getStatus() != Http2Connection::Status::OPEN)
    {
        LOGINFORMATIONAL(ert::tracing::Logger::informational(ert::tracing::Logger::asString("Connection must be OPEN to send request (%s) !", connection_->asString().c_str()), ERT_FILE_LOCATION));

        connection_->reconnect();
        connection_->waitToBeConnected();
        if (connection_->getStatus() != Http2Connection::Status::OPEN) {
            LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unable to reconnect '%s'", connection_->asString().c_str()), ERT_FILE_LOCATION));
            return Http2Client::response{"", 503};
        }
    }

    auto url = getUri(uri);
    auto method_str = ::method_to_str[method];

    LOGINFORMATIONAL(
        ert::tracing::Logger::informational(ert::tracing::Logger::asString("Sending %s request to url: %s; body: %s; headers: %s; %s",
                                            method_str.c_str(), url.c_str(), body.c_str(), headersAsString(headers).c_str(), connection_->asString().c_str()), ERT_FILE_LOCATION);
    );

    auto submit = [&, url](const nghttp2::asio_http2::client::session & sess,
                           const nghttp2::asio_http2::header_map & headers, boost::system::error_code & ec)
    {
        return sess.submit(ec, method_str, url, body, headers);
    };

    auto& session = connection_->getSession();
    auto task = std::make_shared<Http2Client::task>();

    session.io_service().post([&, task, headers]
    {
        boost::system::error_code ec;

        // // example to add headers:
        // nghttp2::asio_http2::header_value ctValue = {"application/json", 0};
        // nghttp2::asio_http2::header_value clValue = {std::to_string(body.length()), 0};
        // headers.emplace("content-type", ctValue);
        // headers.emplace("content-length", clValue);

        //perform submit
        auto req = submit(session, headers, ec);
        req->on_response(
            [task](const nghttp2::asio_http2::client::response & res)
        {
            res.on_data(
                [task, &res](const uint8_t* data, std::size_t len)
            {
                if (len > 0)
                {
                    std::string body (reinterpret_cast<const char*>(data), len);
                    task->data += body;
                }
                else
                {
                    //setting the value on 'response' (promise) will unlock 'done' (future)
                    task->response.set_value(Http2Client::response {task->data, res.status_code(), res.header()});
                    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString(
                            "Request has been answered with status code: %d; data: %s; headers: %s", res.status_code(), task->data.c_str(), headersAsString(res.header()).c_str()), ERT_FILE_LOCATION));
                }
            });
        });

        req->on_close(
            [](uint32_t error_code)
        {
            //not implemented / not needed
        });
    });

    //waits until 'done' (future) is available using the timeout
    if (task->done.wait_for(request_timeout_) == std::future_status::timeout)
    {
        ert::tracing::Logger::error("Request has timed out", ERT_FILE_LOCATION);
        return Http2Client::response();
    }
    else
    {
        return task->done.get();
    }

}

std::string Http2Client::getUri(const std::string& uri, const std::string &scheme)
{
    std::string result{};

    if (scheme.empty()) {
        result = "http";
        if (connection_->isSecure()) {
            result += "s";
        }
    }
    else {
        result = scheme;
    }

    result += "://" + connection_->getHost() + ":" + connection_->getPort();
    if (uri.empty()) return result;

    if (uri[0] != '/') {
        result += "/";
    }

    result += uri;

    return result;
}

}
}

