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


namespace ert
{
namespace http2comm
{
Http2Client::Http2Client(const std::string& name, const std::string& host, const std::string& port, bool secure)
    : name_(name),
      host_(host),
      port_(port),
      secure_(secure),
      connection_(std::make_shared<Http2Connection>(host, port, secure))
{

    if (!connection_->waitToBeConnected())
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unable to connect '%s'", connection_->asString().c_str()), ERT_FILE_LOCATION));
    }
}

std::string Http2Client::response::asString() {
    std::string result{};

    result += "Response Status Code: ";
    switch (statusCode) {
    case -1:
        result += "-1 (initial connection error)";
        break;
    case -2:
        result += "-2 (request timeout)";
        break;
    case -3:
        result += "-3 (submit error)";
        break;
    case -4:
        result += "-4 (http2 stream closed)";
        break;
    default:
        result += std::to_string(statusCode);
    }

    result += " | Response Body: ";
    result += (body.empty() ? "<none>":body);
    result += " | Response Headers: ";
    std::string aux = headersAsString(headers);
    result += (aux.empty() ? "<none>":aux);

    return result;
}

void Http2Client::enableMetrics(ert::metrics::Metrics *metrics,
                                const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries,
                                const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries, const std::string &source) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::labels_t familyLabels = {{"source", (source.empty() ? name_:source)}};

        observed_requests_sents_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_requests_sents_counter"), std::string("Requests sents observed counter in ") + name_, familyLabels));
        observed_requests_unsents_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_requests_unsent_counter"), std::string("Requests unsents observed counter in ") + name_, familyLabels));
        observed_responses_received_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_responses_received_counter"), std::string("Responses received observed counter in ") + name_, familyLabels));
        observed_responses_timedout_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_responses_timedout_counter"), std::string("Responses timed-out observed counter in ") + name_, familyLabels));

        responses_delay_seconds_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_responses_delay_seconds_gauge"), std::string("Message responses delay gauge (seconds) in ") + name_, familyLabels));
        sent_messages_size_bytes_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_sent_messages_size_bytes_gauge"), std::string("Sent messages sizes gauge (bytes) in ") + name_, familyLabels));
        received_messages_size_bytes_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_received_messages_size_bytes_gauge"), std::string("Received messages sizes gauge (bytes) in ") + name_, familyLabels));

        responses_delay_seconds_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_responses_delay_seconds"), std::string("Message responses delay (seconds) in ") + name_, familyLabels));
        sent_messages_size_bytes_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_sent_messages_size_bytes"), std::string("Sent messages sizes (bytes) in ") + name_, familyLabels));
        received_messages_size_bytes_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_received_messages_size_bytes"), std::string("Received messages sizes (bytes) in ") + name_, familyLabels));

        response_delay_seconds_histogram_bucket_boundaries_ = responseDelaySecondsHistogramBucketBoundaries;
        message_size_bytes_histogram_bucket_boundaries_ = messageSizeBytesHistogramBucketBoundaries;
    }
}

void Http2Client::reconnect()
{
    std::unique_lock<std::shared_timed_mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return;
    }

    connection_->reconnect();
}

void Http2Client::async_send(
    const std::string &method,
    const std::string &path,
    const std::string &body,
    const nghttp2::asio_http2::header_map &headers,
    std::function<void(Http2Client::response)> responseCallback,
    const std::chrono::milliseconds& requestTimeoutMs)
{
    std::shared_ptr<Http2Client> self = shared_from_this();
    auto cb = std::move(responseCallback);

    if (!connection_ || !connection_->isConnected())
    {
        LOGINFORMATIONAL(
            std::string msg = ert::tracing::Logger::asString("Connection must be OPEN to send request. Reconnection ongoing to %s:%s%s ...", host_.c_str(), port_.c_str(), (secure_ ? " (secured)":""));
            ert::tracing::Logger::informational(msg, ERT_FILE_LOCATION);
        );

        reconnect();

        // metrics
        if (metrics_) {
            auto& counter = observed_requests_unsents_counter_family_ptr_->Add({{"method", method}});
            counter.Increment();
        }

        // Invoke callback
        cb(Http2Client::response{"", -1});
        return;
    }

    // Ignore Body on GET, DELETE and HEAD:
    const bool noBodyMethod = (method == "GET" || method == "DELETE" || method == "HEAD");

    // metrics
    if (metrics_) {
        auto& counter = observed_requests_sents_counter_family_ptr_->Add({{"method", method}});
        counter.Increment();

        std::size_t requestBodySize = (noBodyMethod ? 0 : body.size());
        auto& gauge = sent_messages_size_bytes_gauge_family_ptr_->Add({{"method", method}});
        gauge.Set(requestBodySize);
        auto& histogram = sent_messages_size_bytes_histogram_family_ptr_->Add({{"method", method}}, message_size_bytes_histogram_bucket_boundaries_);
        histogram.Observe(requestBodySize);
    }

    auto url = getUri(path);

    LOGINFORMATIONAL(
        ert::tracing::Logger::informational(ert::tracing::Logger::asString("Sending %s request to url: %s; body: %s; headers: %s; %s",
                                            method.c_str(), url.c_str(), (noBodyMethod ? "<none>":body.c_str()), headersAsString(headers).c_str(), connection_->asString().c_str()), ERT_FILE_LOCATION);
    );

    auto task = std::make_shared<Http2Client::task>();
    const auto& session = connection_->getSession();
    auto& ioContext = session.io_service();

    ioContext.post([self, cb, noBodyMethod, requestTimeoutMs, task, url = std::move(url), method, headers, body, this]
    {
        boost::system::error_code ec;

        auto submit = [url, body, method, noBodyMethod](const nghttp2::asio_http2::client::session & sess,
                const nghttp2::asio_http2::header_map & headers, boost::system::error_code & ec)
        {
            return noBodyMethod ? sess.submit(ec, method, url, headers):sess.submit(ec, method, url, body, headers);
        };

        // // example to add headers:
        // nghttp2::asio_http2::header_value ctValue = {"application/json", 0};
        // nghttp2::asio_http2::header_value clValue = {std::to_string(body.length()), 0};
        // headers.emplace("content-type", ctValue);
        // headers.emplace("content-length", clValue);

        // Timer for expiration control (before submitting).
        // It must be cancelled in on_response() lambda.
        const auto& session = self->connection_->getSession();
        auto timer = std::make_shared<boost::asio::deadline_timer>(session.io_service());
        timer->expires_from_now(boost::posix_time::milliseconds(requestTimeoutMs.count()));
        timer->async_wait([&, task, cb, method, timer](const boost::system::error_code& ec) {
            if (ec != boost::asio::error::operation_aborted) {
                // Here expiration (before answer):
                if (!task->timed_out.load()) {
                    task->timed_out.store(true);

                    // logging
                    LOGINFORMATIONAL(
                        ert::tracing::Logger::informational("Request has timed out", ERT_FILE_LOCATION);
                    );

                    // metrics
                    if (metrics_) {
                        auto& counter = observed_responses_timedout_counter_family_ptr_->Add({{"method", method}});
                        counter.Increment();
                    }

                    // Optional: cancel HTTP/2 stream if possible
                    // req->cancel();

                    // virtual
                    responseTimeout();

                    // Invoke callback
                    if (!task->cb_invoked.load()) {
                        task->cb_invoked.store(true);
                        cb(Http2Client::response{"", -2});
                    }
                }
            }
        });

        //perform submit
        task->sendingUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        auto req = submit(session, headers, ec);
        if (!req) {
            ert::tracing::Logger::error("Request submit error, closing connection ...", ERT_FILE_LOCATION);
            self->connection_->close();
            // TODO OAM: client error, 468 (non-standard http status code)

            if (timer) {
                timer->cancel();
            }

            // Invoke callback
            if (!task->cb_invoked.load()) {
                task->cb_invoked.store(true);
                cb(Http2Client::response{"", -3});
            }

            return;
        }

        req->on_response(
            [task, cb, timer, method, this](const nghttp2::asio_http2::client::response & res)
        {
            // Timeout timer
            if (task->timed_out.load()) {
                LOGINFORMATIONAL(
                    ert::tracing::Logger::informational("Answer received for discarded transaction due to timeout. Ignoring ...", ERT_FILE_LOCATION);
                );
                return; // no need to cancel timer: it already expired
            }

            // metrics
            if (metrics_) {
                auto& counter = observed_responses_received_counter_family_ptr_->Add({{"method", method}, {"status_code", std::to_string(res.status_code())}});
                counter.Increment();

                task->receptionUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
                double durationUs = (task->receptionUs - task->sendingUs).count();
                double durationSeconds = durationUs/1000000.0;
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Context duration: %d us", durationUs);
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                auto& gauge = responses_delay_seconds_gauge_family_ptr_->Add({{"method", method}, {"status_code", std::to_string(res.status_code())}});
                gauge.Set(durationSeconds);
                auto& histogram = responses_delay_seconds_histogram_family_ptr_->Add({{"method", method}, {"status_code", std::to_string(res.status_code())}}, response_delay_seconds_histogram_bucket_boundaries_);
                histogram.Observe(durationSeconds);
            }

            res.on_data(
                [task, cb, timer, &res, &method, this](const uint8_t* data, std::size_t len)
            {
                if (len > 0)
                {
                    std::string body (reinterpret_cast<const char*>(data), len);
                    task->data += body;
                }
                else
                {
                    // End of transaction (len == 0): we cancel timeout timer before invoking callback
                    if (timer) {
                        timer->cancel();
                    }

                    // Invoke callback
                    if (!task->cb_invoked.load()) {
                        task->cb_invoked.store(true);
                        cb(Http2Client::response{task->data, res.status_code(), res.header(), task->sendingUs, task->receptionUs});
                    }

                    LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString(
                            "Request has been answered with status code: %d; data: %s; headers: %s", res.status_code(), task->data.c_str(), headersAsString(res.header()).c_str()), ERT_FILE_LOCATION));
                    // metrics
                    if (metrics_) {
                        std::size_t responseBodySize = task->data.size();
                        auto& gauge = received_messages_size_bytes_gauge_family_ptr_->Add({{"method", method}, {"status_code", std::to_string(res.status_code())}});
                        gauge.Set(responseBodySize);
                        auto& histogram = received_messages_size_bytes_histogram_family_ptr_->Add({{"method", method}, {"status_code", std::to_string(res.status_code())}}, message_size_bytes_histogram_bucket_boundaries_);
                        histogram.Observe(responseBodySize);
                    }
                }
            });
        });

        req->on_close(
            [task, cb, timer](uint32_t error_code)
        {
            if (!task->timed_out.load()) {
                // Stream was closed before reception
                if (timer) {
                    timer->cancel(); // avoid duplicated error by timer
                }

                // Invoke callback
                if (!task->cb_invoked.load()) {
                    task->cb_invoked.store(true);
                    cb(Http2Client::response{"", -4});
                }

                // logging & metrics ?
            }
            // return # implicit
        });
    });
}

void Http2Client::asyncSend(
    const std::string &method,
    const std::string &path,
    const std::string &body,
    const nghttp2::asio_http2::header_map &headers,
    std::function<void(Http2Client::response)> responseCallback,
    const std::chrono::milliseconds& requestTimeoutMs,
    const std::chrono::milliseconds& sendDelayMs)
{
    if (sendDelayMs.count() <= 0) {
        return async_send(method, path, body, headers, responseCallback, requestTimeoutMs);
    }

    const auto& session = connection_->getSession();
    auto& ioContext = session.io_service();

    auto timer = std::make_shared<boost::asio::steady_timer>(ioContext);
    timer->expires_after(sendDelayMs);

    auto send_operation = [
                              this,
                              timer, // Captura el timer (shared_ptr) para mantenerlo vivo
                              method,
                              path,
                              body,
                              headers,
                              // Capturamos el callback y los demás parámetros para la llamada final
                              responseCallback,
                              requestTimeoutMs
                          ]() mutable
    {
        LOGDEBUG(ert::tracing::Logger::debug("Pre-send delay completed", ERT_FILE_LOCATION));
        async_send(method, path, body, headers, responseCallback, requestTimeoutMs);
    };

    // Program 'async_wait'
    timer->async_wait([send_operation](const boost::system::error_code& ec) mutable {
        if (ec) {
            // Timer cancelled (i.e. connection broken before delay)
            if (ec != boost::asio::error::operation_aborted) {
                // timer errors ?
            }
        } else {
            // Timer expired:
            send_operation();
        }
    });
}

//std::future<Http2Client::response> Http2Client::send(
Http2Client::response Http2Client::send(
    const std::string &method,
    const std::string &path,
    const std::string &body,
    const nghttp2::asio_http2::header_map &headers,
    const std::chrono::milliseconds& requestTimeoutMs,
    const std::chrono::milliseconds& sendDelayMs)
{
    auto promise = std::make_shared<std::promise<Http2Client::response>>(); // guarantee promise lifecycle until callback is executed
    std::future<Http2Client::response> future = promise->get_future();

    // To avoid multiple calls to 'promise->set_value' (due to multiple subyacent callback calls):
    auto satisfied_flag = std::make_shared<std::atomic_bool>(false);

    Http2Client::ResponseCallback callback_wrapper = [promise, satisfied_flag](Http2Client::response res) {
        if (satisfied_flag->exchange(true)) {
            // ignore call to avoid multiple set_value/set_exception
            return;
        }

        try {
            promise->set_value(std::move(res));
        } catch (...) {
            try {
                promise->set_exception(std::current_exception());
            } catch (const std::future_error& e) {
                ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
            }
        }
    };

    // Launch async operation with callback wrapper
    asyncSend(method, path, body, headers, std::move(callback_wrapper), requestTimeoutMs, sendDelayMs);

    // Return future
    return future.get();
}

std::string Http2Client::getUri(const std::string& path, const std::string &scheme)
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
    if (path.empty()) return result;

    if (path[0] != '/') {
        result += "/";
    }

    result += path;

    return result;
}


std::string Http2Client::getConnectionStatus() const {

    std::string result{};

    if (!connection_) return "NoConnectionCreated";

    if (connection_->getStatus() == ert::http2comm::Http2Connection::Status::NOT_OPEN) result = "NotOpen";
    else if (connection_->getStatus() == ert::http2comm::Http2Connection::Status::OPEN) result = "Open";
    else if (connection_->getStatus() == ert::http2comm::Http2Connection::Status::CLOSED) result = "Closed";

    return result;
}

bool Http2Client::isConnected() const {
    if (!connection_) return false;
    return (connection_->getStatus() == ert::http2comm::Http2Connection::Status::OPEN);
}

}
}

