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
                                const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries) {

    metrics_ = metrics;

    if (metrics_) {
        observed_responses_counter_family_name_ = name_ + std::string("_observed_responses_total");

        ert::metrics::counter_family_ref_t cf = metrics->addCounterFamily(name_ + std::string("_observed_requests_total"), std::string("Http2 total requests observed in ") + name_);
        observed_requests_post_counter_ = &(cf.Add({{"method", "POST"}}));
        observed_requests_get_counter_ = &(cf.Add({{"method", "GET"}}));
        observed_requests_put_counter_ = &(cf.Add({{"method", "PUT"}}));
        observed_requests_delete_counter_ = &(cf.Add({{"method", "DELETE"}}));
        observed_requests_head_counter_ = &(cf.Add({{"method", "HEAD"}}));
        observed_requests_other_counter_ = &(cf.Add({{"method", "other"}}));
        observed_requests_error_post_counter_ = &(cf.Add({{"success", "false"}, {"method", "POST"}}));
        observed_requests_error_get_counter_ = &(cf.Add({{"success", "false"}, {"method", "GET"}}));
        observed_requests_error_put_counter_ = &(cf.Add({{"success", "false"}, {"method", "PUT"}}));
        observed_requests_error_delete_counter_ = &(cf.Add({{"success", "false"}, {"method", "DELETE"}}));
        observed_requests_error_head_counter_ = &(cf.Add({{"success", "false"}, {"method", "HEAD"}}));
        observed_requests_error_other_counter_ = &(cf.Add({{"success", "false"}, {"method", "other"}}));

        ert::metrics::gauge_family_ref_t gf1 = metrics->addGaugeFamily(name_ + std::string("_responses_delay_seconds_gauge"), std::string("Http2 message responses delay gauge (seconds) in ") + name_);
        responses_delay_seconds_gauge_ = &(gf1.Add({}));

        ert::metrics::gauge_family_ref_t gf2 = metrics->addGaugeFamily(name_ + std::string("_messages_size_bytes_gauge"), std::string("Http2 message sizes gauge (bytes) in ") + name_);
        messages_size_bytes_rx_gauge_ = &(gf2.Add({{"direction", "rx"}}));
        messages_size_bytes_tx_gauge_ = &(gf2.Add({{"direction", "tx"}}));

        ert::metrics::histogram_family_ref_t hf1 = metrics->addHistogramFamily(name_ + std::string("_responses_delay_seconds_histogram"), std::string("Http2 message responses delay (seconds) in ") + name_);
        responses_delay_seconds_histogram_ = &(hf1.Add({}, responseDelaySecondsHistogramBucketBoundaries));

        ert::metrics::histogram_family_ref_t hf2 = metrics->addHistogramFamily(name_ + std::string("_messages_size_bytes_histogram"), std::string("Http2 message sizes (bytes) in ") + name_);
        messages_size_bytes_rx_histogram_ = &(hf2.Add({{"direction", "rx"}}, messageSizeBytesHistogramBucketBoundaries));
        messages_size_bytes_tx_histogram_ = &(hf2.Add({{"direction", "tx"}}, messageSizeBytesHistogramBucketBoundaries));
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
            // counters
            if (method == "POST") {
                observed_requests_error_post_counter_->Increment();
            }
            else if (method == "GET") {
                observed_requests_error_get_counter_->Increment();
            }
            else if (method == "PUT") {
                observed_requests_error_put_counter_->Increment();
            }
            else if (method == "DELETE") {
                observed_requests_error_delete_counter_->Increment();
            }
            else if (method == "HEAD") {
                observed_requests_error_head_counter_->Increment();
            }
            else {
                observed_requests_error_other_counter_->Increment();
            }
        }

        return Http2Client::response{"", -1};
    }

    // metrics
    if (metrics_) {
        // counters
        if (method == "POST") {
            observed_requests_post_counter_->Increment();
        }
        else if (method == "GET") {
            observed_requests_get_counter_->Increment();
        }
        else if (method == "PUT") {
            observed_requests_put_counter_->Increment();
        }
        else if (method == "DELETE") {
            observed_requests_delete_counter_->Increment();
        }
        else if (method == "HEAD") {
            observed_requests_head_counter_->Increment();
        }
        else {
            observed_requests_other_counter_->Increment();
        }

        std::size_t requestBodySize = body.size();
        messages_size_bytes_tx_gauge_->Set(requestBodySize);
        messages_size_bytes_tx_histogram_->Observe(requestBodySize);
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

    session.io_service().post([&, task, headers, this]
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
            [task, this](const nghttp2::asio_http2::client::response & res)
        {
            if (task->timed_out) {
                LOGINFORMATIONAL(
                    ert::tracing::Logger::informational("Answer received for discarded transaction due to timeout. Ignoring ...", ERT_FILE_LOCATION);
                );
                return;
            }
            // metrics
            if (metrics_) {
                // Dynamic counters (status code):
                metrics_->increaseCounter(observed_responses_counter_family_name_, {{"statuscode", std::to_string(res.status_code())}});

                // histograms
                task->receptionUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
                double durationUs = (task->receptionUs - task->sendingUs).count();
                double durationSeconds = durationUs/1000000.0;
                LOGDEBUG(
                    std::string msg = ert::tracing::Logger::asString("Context duration: %d us", durationUs);
                    ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
                );
                responses_delay_seconds_gauge_->Set(durationSeconds);
                responses_delay_seconds_histogram_->Observe(durationSeconds);
            }

            res.on_data(
                [task, &res, this](const uint8_t* data, std::size_t len)
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
                        messages_size_bytes_rx_gauge_->Set(responseBodySize);
                        messages_size_bytes_rx_histogram_->Observe(responseBodySize);
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

