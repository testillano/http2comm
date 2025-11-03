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

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>

namespace ert
{
namespace http2comm
{
Stream::Stream(const nghttp2::asio_http2::server::request& req,
               const nghttp2::asio_http2::server::response& res,
               Http2Server *server) : req_(req), res_(res), server_(server), closed_(false), error_(false), timer_(nullptr), need_timer_(false) {

    if (server_->preReserveRequestBody()) request_body_.reserve(server_->maximum_request_body_size_.load());
}

void Stream::appendData(const uint8_t* data, std::size_t len) {
    // std::copy(data, data + len, std::ostream_iterator<std::uint8_t>(*requestBody));
    //   where we have std::shared_ptr request_body_ = std::make_shared<std::stringstream>();
    //
    // BUT: std::string append has better performance than stringstream one (https://gist.github.com/testillano/bc8944eec86fe4e857bf51d61d6c5e42):
    request_body_.append((const char *)data, len);
}

void Stream::process(bool busyConsumers, int queueSize) {
    reception(server_->getQueueDispatcherMaxSize() >= 0 /* congestion control enabled */ && busyConsumers && queueSize > server_->getQueueDispatcherMaxSize());
    commit();
}

void Stream::reception(bool congestion)
{
    reception_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

    std::vector<std::string> allowedMethods;
    unsigned int responseDelayMs{};

    if (congestion)
    {
        server_->receiveError(req_, request_body_, status_code_, response_headers_, response_body_, ert::http2comm::SERVICE_UNAVAILABLE);
    }
    else if (!server_->checkMethodIsAllowed(req_, allowedMethods))
    {
        server_->receiveError(req_, request_body_, status_code_, response_headers_, response_body_, ert::http2comm::METHOD_NOT_ALLOWED, "", allowedMethods);
    }
    else if (!server_->checkMethodIsImplemented(req_))
    {
        server_->receiveError(req_, request_body_, status_code_, response_headers_, response_body_, ert::http2comm::METHOD_NOT_IMPLEMENTED);
    }
    else
    {
        if (server_->checkHeaders(req_))
        {
            std::string apiPath = server_->getApiPath();

            if (apiPath != "" && !ert::http2comm::URLFunctions::matchPrefix(req_.uri().path, apiPath))
            {
                server_->receiveError(req_, request_body_, status_code_, response_headers_, response_body_, ert::http2comm::WRONG_API_NAME_OR_VERSION);
            }
            else
            {
                // VALID RECEPTION

                // metrics
                if (server_->metrics_) {
                    auto& counter = server_->observed_requests_accepted_counter_family_ptr_->Add({{"method", req_.method()}});
                    counter.Increment();
                }

                server_->receive(reception_id_, req_, request_body_, reception_timestamp_us_, status_code_, response_headers_, response_body_, responseDelayMs);
            }
        }
        else
        {
            server_->receiveError(req_, request_body_, status_code_, response_headers_, response_body_, ert::http2comm::UNSUPPORTED_MEDIA_TYPE);
        }
    }

    // Optional reponse delay
    bool ioContextWarning = false;
    if (responseDelayMs != 0) { // provision delay
        if (server_->getTimersIoContext()) {
            //if (!server_->getTimersIoContext()->stopped()) {
            timer_ = std::make_shared<boost::asio::steady_timer>(*(server_->getTimersIoContext()), std::chrono::milliseconds(responseDelayMs));
            need_timer_ = true;
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Server response delay scheduled (%d ms)", responseDelayMs);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            //}
        }
        else {
            ioContextWarning = true;
        }
    }
    else if (std::chrono::microseconds delayUs = server_->responseDelayTimer(reception_id_); delayUs > std::chrono::microseconds::zero()) { // reception-id specific delay
        if (server_->getTimersIoContext()) {
            //if (!server_->getTimersIoContext()->stopped()) {
            timer_ = std::make_shared<boost::asio::steady_timer>(*(server_->getTimersIoContext()), delayUs);
            need_timer_ = true;
            LOGDEBUG(
                std::string msg = ert::tracing::Logger::asString("Server responseDelayTimer() scheduled (%lld usecs) for reception identifier %llu", delayUs.count(), reception_id_);
                ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
            );
            //}
        }
        else {
            ioContextWarning = true;
        }
    }

    LOGWARNING(if (ioContextWarning) ert::tracing::Logger::warning("You must provide an 'io context for timers' in order to manage delays in http2 server", ERT_FILE_LOCATION));
}

void Stream::commit()
{
    if (need_timer_)
    {
        timer_->async_wait([this] (const boost::system::error_code& ec) {

            if (ec == boost::asio::error::operation_aborted) { // timer was cancelled, we musn't commit
                return;
            }

            // Re-scheduling of timers by mean virtual server responseDelayTimer():
            std::chrono::microseconds delayUs = server_->responseDelayTimer(reception_id_);
            need_timer_ = (delayUs > std::chrono::microseconds::zero());
            if (need_timer_) {
                timer_->expires_after(delayUs);
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Server responseDelayTimer() re-scheduled (%lld usecs) for reception identifier %llu", delayUs.count(), reception_id_);
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
            }

            commit();
        });
        return;
    }

    // Maybe transport is broken
    if (error_) {
        LOGWARNING(ert::tracing::Logger::warning("Discarding response over broken connection", ERT_FILE_LOCATION));
        return;
    }

    auto self = this;
    //auto self = shared_from_this();

    // Send response
    res_.io_service().post([self]()
    {
        std::lock_guard<std::mutex> guard(self->mutex_);
        if (self->closed_)
        {
            return;
        }

        // WORKER THREAD PROCESSING CANNOT BE DONE HERE
        // (nghttp2 pool must be free), SO, IT WILL BE
        // DONE BEFORE commit()
        if (self->status_code_ < 100) {
            // RST_STREAM and GOAWAY errors (https://datatracker.ietf.org/doc/html/rfc7540#section-7)
            // (Hex),Nombre,Descripción
            // 0x0    NO_ERROR              Normal graceful close GOAWAY
            // 0x1    PROTOCOL_ERROR        Protocol error at frame level
            // 0x2    INTERNAL_ERROR        Internal error
            // 0x3    FLOW_CONTROL_ERROR    Flow control error
            // 0x7    REFUSED_STREAM        Frame rejected before processing
            // 0x8    CANCEL                Stream cancelled by application
            // 0xB    STREAM_CLOSED         Reception of frame already closed
            // ---
            // Correspond to 'nghttp2_error_code' enum type within 'https://nghttp2.org/documentation/nghttp2.h.html'
            self->res_.cancel(self->status_code_); // this will be passed to on_close() as error_code
        }
        else {
            self->res_.write_head(self->status_code_, self->response_headers_);
            self->res_.end(self->response_body_);
        }
    });
}

void Stream::updateMetrics(const char *resultCodeLabel) {
    if (!server_->metrics_) return;

    // histograms
    auto finalUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    double durationUs = finalUs - reception_timestamp_us_.count();
    double durationSeconds = durationUs/1000000.0;
    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("Context duration: %d us", durationUs);
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );

    auto& gauge = server_->responses_delay_seconds_gauge_family_ptr_->Add({{"method", req_.method()}, {resultCodeLabel, std::to_string(status_code_)}});
    gauge.Set(durationSeconds);

    std::size_t requestBodySize = request_body_.size();
    std::size_t responseBodySize = response_body_.size();

    auto& gauge2 = server_->received_messages_size_bytes_gauge_family_ptr_->Add({{"method", req_.method()}});
    gauge2.Set(requestBodySize);
    auto& gauge3 = server_->sent_messages_size_bytes_gauge_family_ptr_->Add({{"method", req_.method()}, {resultCodeLabel, std::to_string(status_code_)}});
    gauge3.Set(responseBodySize);

    auto& histogram = server_->responses_delay_seconds_histogram_family_ptr_->Add({{"method", req_.method()}, {resultCodeLabel, std::to_string(status_code_)}}, server_->response_delay_seconds_histogram_bucket_boundaries_);
    histogram.Observe(durationSeconds);
    auto& histogram2 = server_->received_messages_size_bytes_histogram_family_ptr_->Add({{"method", req_.method()}}, server_->message_size_bytes_histogram_bucket_boundaries_);
    histogram2.Observe(durationSeconds);
    auto& histogram3 = server_->sent_messages_size_bytes_histogram_family_ptr_->Add({{"method", req_.method()}, {resultCodeLabel, std::to_string(status_code_)}}, server_->message_size_bytes_histogram_bucket_boundaries_);
    histogram3.Observe(durationSeconds);

    // counters
    auto& counter = server_->observed_responses_counter_family_ptr_->Add({{"method", req_.method()}, {resultCodeLabel, std::to_string(status_code_)}});
    counter.Increment();
}

void Stream::error(uint32_t error_code) {
    std::lock_guard<std::mutex> guard(mutex_);
    error_ = true;

    status_code_ = error_code;
    updateMetrics("rst_stream_goaway_error_code");

    ert::tracing::Logger::error(ert::tracing::Logger::asString("Stream connection error (%d) for reception identifier %llu", error_code, reception_id_), ERT_FILE_LOCATION);
}

void Stream::close() {
    std::lock_guard<std::mutex> guard(mutex_);
    closed_ = true;

    updateMetrics("status_code");
}

void Stream::cancelTimer() {
    if (timer_) {
        timer_->cancel();
    }
}

}
}
