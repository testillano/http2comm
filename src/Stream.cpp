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

#include <chrono> // temporary

#include <nghttp2/asio_http2_server.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>

namespace ert
{
namespace http2comm
{

void Stream::process()
{
    std::vector<std::string> allowedMethods;
    unsigned int statusCode;
    auto headers = nghttp2::asio_http2::header_map();
    std::string responseBody;
    unsigned int responseDelayMs{};

    // Initial timestamp for delay correction
    auto initTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (!server_->checkMethodIsAllowed(req_, allowedMethods))
    {
        server_->receiveError(req_, request_->str(), statusCode, headers, responseBody, ert::http2comm::METHOD_NOT_ALLOWED, "", allowedMethods);
    }
    else if (!server_->checkMethodIsImplemented(req_))
    {
        server_->receiveError(req_, request_->str(), statusCode, headers, responseBody, ert::http2comm::METHOD_NOT_IMPLEMENTED);
    }
    else
    {
        if (server_->checkHeaders(req_))
        {
            std::string apiPath = server_->getApiPath();

            if (apiPath != "" && !ert::http2comm::URLFunctions::matchPrefix(req_.uri().path, apiPath))
            {
                server_->receiveError(req_, request_->str(), statusCode, headers, responseBody, ert::http2comm::WRONG_API_NAME_OR_VERSION);
            }
            else
            {
                server_->receive(req_, request_->str(), statusCode, headers, responseBody, responseDelayMs);
            }
        }
        else
        {
            server_->receiveError(req_, request_->str(), statusCode, headers, responseBody, ert::http2comm::UNSUPPORTED_MEDIA_TYPE);
        }
    }

    // Optional delay:
    std::shared_ptr<boost::asio::deadline_timer> timer = nullptr;
    if (server_->getTimersIoService()) {
        if (responseDelayMs != 0) {
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Waiting for planned delay: %d ms", responseDelayMs);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );

            // Final timestamp for delay correction
            // This correction could be noticeable with huge transformations, but this is not usual. As normally, request processingshould be under 1 ms, this milliseconds-delta correction used to be insignificant
            auto finalTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto deltaCorrection = (finalTimestamp - initTimestamp)*1.2; // assume that the rest of processing (send reponse) will take the 20% of delta
            responseDelayMs -= deltaCorrection;
            LOGDEBUG(
            if (deltaCorrection > 0) {
            std::string msg = ert::tracing::Logger::asString("Corrected delay: %d ms (processing lapse was: %d ms)", responseDelayMs, deltaCorrection);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            }
            );

            if (responseDelayMs > 0) {
                timer = std::make_shared<boost::asio::deadline_timer>(*(server_->getTimersIoService()), boost::posix_time::milliseconds(responseDelayMs));
            }
        }
    }
    else {
        LOGWARNING(
            if(responseDelayMs != 0)
            ert::tracing::Logger::warning("You must provide an 'io service for timers' in order to manage delays in http2 server", ERT_FILE_LOCATION);
        );
    }

    // Commit transaction:
    commit(statusCode, headers, responseBody, timer);
}

void Stream::commit(unsigned int statusCode,
                    const nghttp2::asio_http2::header_map& headers,
                    const std::string& responseBody,
                    std::shared_ptr<boost::asio::deadline_timer> timer)
{
    auto self = shared_from_this();

    if (timer) {
        // timer is passed to the lambda to keep it alive (shared pointer not destroyed when it is out of scope):
        timer->async_wait([self, timer, statusCode, headers, responseBody] (const boost::system::error_code&) {
            self->commit(statusCode, headers, responseBody, nullptr);
        });
    }
    else {
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
}

void Stream::close(bool c)
{
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = c;
}

}
}
