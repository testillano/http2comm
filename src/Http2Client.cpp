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

#include <boost/system/error_code.hpp>
#include <nghttp2/asio_http2_client.h>
#include <map>

#include <tracing/Logger.hpp>

#include <http2comm/Http2Client.hpp>
#include <http2comm/Http2Connection.hpp>

namespace nga = nghttp2::asio_http2;

namespace
{
std::map<ert::http2comm::Http2Client::Method, std::string> method_to_str = { {
        ert::http2comm::Http2Client::Method::GET, "GET"
    }, {
        ert::http2comm::Http2Client::Method::PUT, "PUT"
    }, {
        ert::http2comm::Http2Client::Method::POST, "POST"
    }, {
        ert::http2comm::Http2Client::Method::DELETE, "DELETE"
    }
};
}

namespace ert
{
namespace http2comm
{
Http2Client::Http2Client(std::shared_ptr<Http2Connection>
                         connection,
                         const std::chrono::milliseconds& request_timeout) :
    connection_(connection), request_timeout_(request_timeout), scheme_(
        "http")
{
}

Http2Client::Http2Client(const std::chrono::milliseconds&
                         request_timeout) :
    request_timeout_(request_timeout), scheme_("http")
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
    const Http2Client::Method&
    method,
    const std::string& uri_path, const std::string& json)
{
    // Internal Server Error response if connection not initialized
    if (!connection_)
    {
        return Http2Client::response{"", 500};
    }

    auto url = getUri(uri_path);
    auto method_str = ::method_to_str[method];
    TRACE(ert::tracing::Logger::Informational,
          "Sending %s request to: %s; Data: %s; server connection status: %d",
          method_str.c_str(), url.c_str(), json.c_str(),
          static_cast<int>(connection_->getStatus()));

    auto submit = [&, url](const nghttp2::asio_http2::client::session & sess,
                           const nga::header_map & headers, boost::system::error_code & ec)
    {
        return sess.submit(ec, method_str, url, json, headers);
    };

    auto& session = connection_->getSession();
    auto task = std::make_shared<Http2Client::task>();

    session.io_service().post([&, task]
    {
        boost::system::error_code ec;

        //configure headers
        nga::header_value ctValue = {"application/json", 0};
        nga::header_value clValue = {std::to_string(json.length()), 0};
        nga::header_map headers = { {"content-type", ctValue}, {"content-length", clValue} };

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
                    std::string json (reinterpret_cast<const char*>(data), len);
                    task->data += json;
                }
                else
                {
                    //setting the value on 'response' (promise) will unlock 'done' (future)
                    task->response.set_value(Http2Client::response {task->data, res.status_code()});
                    TRACE(ert::tracing::Logger::Debug,
                          "Request has been answered with %d; Data: %s", res.status_code(),
                          task->data.c_str());
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
        TRACE(ert::tracing::Logger::Error, "Request has timed out");
        return Http2Client::response();
    }
    else
    {
        return task->done.get();
    }

}

std::string Http2Client::getUri(const std::string& uri_path)
{
    return scheme_ + "://" + connection_->getHost() + ":"
           + connection_->getPort() + "/" + uri_path;
}

}
}

