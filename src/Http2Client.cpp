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
      connection_(std::make_unique<Http2Connection>(host, port, secure))
{

    if (!connection_->waitToBeConnected())
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unable to connect '%s'", connection_->asString().c_str()), ERT_FILE_LOCATION));
    }
}

void Http2Client::enableMetrics(ert::metrics::Metrics *metrics,
                                const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries,
                                const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries, const std::string &source) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::labels_t familyLabels = {{"source", (source.empty() ? name_:source)}};

        observed_requests_sents_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_resquests_sents_counter"), std::string("Requests sents observed counter in ") + name_, familyLabels));
        observed_requests_unsents_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_resquests_unsent_counter"), std::string("Requests unsents observed counter in ") + name_, familyLabels));
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
    if (!mutex_.try_lock())
    {
        return;
    }
    connection_.reset();

    if (auto conn = std::make_unique<Http2Connection>(host_, port_, secure_);
            conn->waitToBeConnected())
    {
        connection_ = std::move(conn);
    }
    else
    {
        LOGWARNING(ert::tracing::Logger::warning(ert::tracing::Logger::asString("Unable to reconnect '%s'", conn->asString().c_str()), ERT_FILE_LOCATION));
        conn.reset();
    }
    mutex_.unlock();
}

Http2Client::response Http2Client::send(
    const std::string &method,
    const std::string &path,
    const std::string &body,
    const nghttp2::asio_http2::header_map &headers,
    const std::chrono::milliseconds& requestTimeout)
{
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

        return Http2Client::response{"", -1};
    }

// metrics
    if (metrics_) {
        auto& counter = observed_requests_sents_counter_family_ptr_->Add({{"method", method}});
        counter.Increment();

        std::size_t requestBodySize = body.size();
        auto& gauge = sent_messages_size_bytes_gauge_family_ptr_->Add({{"method", method}});
        gauge.Set(requestBodySize);
        auto& histogram = sent_messages_size_bytes_histogram_family_ptr_->Add({{"method", method}}, message_size_bytes_histogram_bucket_boundaries_);
        histogram.Observe(requestBodySize);
    }

    auto url = getUri(path);

    LOGINFORMATIONAL(
        ert::tracing::Logger::informational(ert::tracing::Logger::asString("Sending %s request to url: %s; body: %s; headers: %s; %s",
                                            method.c_str(), url.c_str(), body.c_str(), headersAsString(headers).c_str(), connection_->asString().c_str()), ERT_FILE_LOCATION);
    );

    auto submit = [&, url](const nghttp2::asio_http2::client::session & sess,
                           const nghttp2::asio_http2::header_map & headers, boost::system::error_code & ec)
    {

        return sess.submit(ec, method, url, body, headers);
    };

    const auto& session = connection_->getSession();
    auto task = std::make_shared<Http2Client::task>();

    session.io_service().post([&, task, method, headers, this]
    {
        boost::system::error_code ec;

        // // example to add headers:
        // nghttp2::asio_http2::header_value ctValue = {"application/json", 0};
        // nghttp2::asio_http2::header_value clValue = {std::to_string(body.length()), 0};
        // headers.emplace("content-type", ctValue);
        // headers.emplace("content-length", clValue);

        //perform submit
        task->sendingUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
        auto req = submit(session, headers, ec);
        if (!req) {
            ert::tracing::Logger::error("Request submit error, closing connection ...", ERT_FILE_LOCATION);
            connection_->close();
            // TODO OAM: client error, 468 (non-standard http status code)
            return;
        }

        req->on_response(
            [task, method, this](const nghttp2::asio_http2::client::response & res)
        {
            if (task->timed_out) {
                LOGINFORMATIONAL(
                    ert::tracing::Logger::informational("Answer received for discarded transaction due to timeout. Ignoring ...", ERT_FILE_LOCATION);
                );
                return;
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
                [task, &res, &method, this](const uint8_t* data, std::size_t len)
            {
                if (len > 0)
                {
                    std::string body (reinterpret_cast<const char*>(data), len);
                    task->data += body;
                }
                else
                {
                    //setting the value on 'response' (promise) will unlock 'done' (future)
                    task->response.set_value(Http2Client::response {task->data, res.status_code(), res.header(), task->sendingUs, task->receptionUs});
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
            [](uint32_t error_code)
        {
            //not implemented / not needed
        });
    });

// waits until 'done' (future) is available using the timeout
    if (task->done.wait_for(requestTimeout) == std::future_status::timeout)
    {
        LOGINFORMATIONAL(
            ert::tracing::Logger::informational("Request has timed out", ERT_FILE_LOCATION);
        );

        // metrics
        if (metrics_) {
            auto& counter = observed_responses_timedout_counter_family_ptr_->Add({{"method", method}});
            counter.Increment();
        }

        responseTimeout();
        task->timed_out = true;
        return Http2Client::response{"", -2};
    }
    else
    {
        return task->done.get();
    }

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

