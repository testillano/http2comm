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

    reception_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

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

    // Optional reponse delay
    boost::asio::deadline_timer *timer = nullptr;
    if (responseDelayMs != 0) { // optional delay:
        if (server_->getTimersIoService()) {
            auto responseDelayUs = 1000*responseDelayMs;
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Planned delay before response: %d us", responseDelayUs);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );

            // Final timestamp for delay correction: this correction could be noticeable with huge transformations, but this is not usual.
            auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto delayUsCorrection = (nowUs - reception_us_.count());
            responseDelayUs -= delayUsCorrection;
            LOGDEBUG(
            if (delayUsCorrection > 0) {
            std::string msg = ert::tracing::Logger::asString("Corrected delay: %d us (processing lapse was: %d us)", responseDelayUs, delayUsCorrection);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            }
            );

            timer = new boost::asio::deadline_timer(*(server_->getTimersIoService()), boost::posix_time::microseconds(responseDelayUs));
        }
        else {
            LOGWARNING(ert::tracing::Logger::warning("You must provide an 'io service for timers' in order to manage delays in http2 server", ERT_FILE_LOCATION));
        }
    }

    commit(statusCode, headers, responseBody, timer);
}

void Stream::commit(unsigned int statusCode,
                    const nghttp2::asio_http2::header_map& headers,
                    const std::string& responseBody,
                    boost::asio::deadline_timer *timer)
{
    auto self = shared_from_this();

    if (timer) {
        // timer is passed to the lambda to keep it alive (shared pointer not destroyed when it is out of scope):
        timer->async_wait([self, timer, statusCode, headers, responseBody] (const boost::system::error_code&) {
            self->commit(statusCode, headers, responseBody, nullptr);
            delete timer;
        });
    }
    else {
        if (server_->metrics_) {

            // histograms
            auto finalUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            double durationUs = finalUs - reception_us_.count();
            double durationSeconds = durationUs/1000000.0;
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Context duration: %d us", durationUs);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            server_->responses_delay_seconds_gauge_->Set(durationSeconds);
            server_->messages_size_bytes_rx_gauge_->Set(request_->str().size());
            server_->messages_size_bytes_tx_gauge_->Set(responseBody.size());

            server_->responses_delay_seconds_histogram_->Observe(durationSeconds);
            server_->messages_size_bytes_rx_histogram_->Observe(request_->str().size());
            server_->messages_size_bytes_tx_histogram_->Observe(responseBody.size());

            // counters
            if (req_.method() == "POST") {
                server_->observed_requests_post_counter_->Increment();
            }
            else if (req_.method() == "GET") {
                server_->observed_requests_get_counter_->Increment();
            }
            else if (req_.method() == "PUT") {
                server_->observed_requests_put_counter_->Increment();
            }
            else if (req_.method() == "DELETE") {
                server_->observed_requests_delete_counter_->Increment();
            }
            else if (req_.method() == "HEAD") {
                server_->observed_requests_head_counter_->Increment();
            }
            else {
                server_->observed_requests_other_counter_->Increment();
            }
        }

        // Send response
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
