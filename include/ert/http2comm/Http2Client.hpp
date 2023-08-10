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

#pragma once

#include <string>
#include <future>
#include <memory>
#include <shared_mutex>

#include <nghttp2/asio_http2.h>

#include <ert/http2comm/Http2Connection.hpp>

#include <ert/metrics/Metrics.hpp>

namespace nghttp2
{
namespace asio_http2
{
namespace client
{
class session;
class request;
}
}
}

namespace boost
{
namespace system
{
class error_code;
}
}

namespace ert
{
namespace http2comm
{

class Http2Connection;

typedef std::function <
const nghttp2::asio_http2::client::request* (const
        nghttp2::asio_http2::client::session& sess,
        const nghttp2::asio_http2::header_map& map,
        boost::system::error_code& ec) > submit_callback;
class Http2Client
{
    //inner classes
public:

    struct response
    {
        std::string body;
        int statusCode; // -1 = connection error
        nghttp2::asio_http2::header_map headers;
        std::chrono::microseconds sendingUs;
        std::chrono::microseconds receptionUs;
    };

private:
    struct task
    {
        task()
        {
            done = response.get_future();
        }

        std::promise<Http2Client::response> response;
        std::future<Http2Client::response> done;
        std::string data; //buffer to store a possible temporary data
        bool timed_out{};
        std::chrono::microseconds sendingUs;
        std::chrono::microseconds receptionUs;
    };

    // Metric names should be in lowercase and separated by underscores (_).
    // Metric names should start with a letter or an underscore (_).
    // Metric names should be descriptive and meaningful for their purpose.
    // Metric names should not be too long or too short.

    // metrics:
    ert::metrics::Metrics *metrics_{};

    // Create counters families for specific situations, and let dynamic labes for unpredictable (status codes) or small sets (method).
    // We could even make combinations with methods (POST, GET, PUT, DELETE, HEAD) but we would manage 5x4=20 families instead of 4:
    ert::metrics::counter_family_t *observed_requests_sents_counter_family_ptr_{};
    ert::metrics::counter_family_t *observed_requests_unsents_counter_family_ptr_{};
    ert::metrics::counter_family_t *observed_responses_received_counter_family_ptr_{};
    ert::metrics::counter_family_t *observed_responses_timedout_counter_family_ptr_{};

    // Idem for gauges:
    ert::metrics::gauge_family_t *responses_delay_seconds_gauge_family_ptr_{};
    ert::metrics::gauge_t *responses_delay_seconds_gauge_{};
    ert::metrics::gauge_family_t *sent_messages_size_bytes_gauge_family_ptr_{};
    ert::metrics::gauge_t *sent_messages_size_bytes_gauge_{};
    ert::metrics::gauge_family_t *received_messages_size_bytes_gauge_family_ptr_{};
    ert::metrics::gauge_t *received_messages_size_bytes_gauge_{};

    // Idem for histograms:
    ert::metrics::histogram_family_t *responses_delay_seconds_histogram_family_ptr_{};
    ert::metrics::histogram_t *responses_delay_seconds_histogram_{};
    ert::metrics::histogram_family_t *sent_messages_size_bytes_histogram_family_ptr_{};
    ert::metrics::histogram_t *sent_messages_size_bytes_histogram_{};
    ert::metrics::histogram_family_t *received_messages_size_bytes_histogram_family_ptr_{};
    ert::metrics::histogram_t *received_messages_size_bytes_histogram_{};

    std::atomic<std::uint64_t> reception_id_{};
    std::atomic<std::size_t> maximum_request_body_size_{};

    std::unique_ptr<Http2Connection> connection_;
    std::string host_;
    std::string port_;
    bool secure_;
    std::shared_timed_mutex mutex_;

    void reconnect();
    std::string getUri(const std::string &path, const std::string &scheme = "" /* http or https by default, but could be forced here */);

protected:
    std::string name_{};

public:
    /**
     * Class constructor given host, port and secure connection indicator
     *
     * @param name client name. It may be used to prefix the name of metrics families
     * (counters, gauges, histograms), so consider to provide a compatible name ([a-zA-Z0-9:_]).
     * A good name convention would include the application name and the endpoint identification,
     * for example:
     *   h2agent_traffic_client_myClient4
     *   udp_server_h2client[_traffic_client]
     *
     * @param host Endpoint host
     * @param port Endpoint port
     * @param secure Secure connection. False by default
     */
    Http2Client(const std::string &name, const std::string& host, const std::string& port, bool secure = false);
    virtual ~Http2Client() {};

    /**
    * Enable metrics
    *
    * The name of families created will be prefixed by the class name given in the constructor.
    *
    *  @param metrics Optional metrics object to compute counters and histograms
    *  @param responseDelaySecondsHistogramBucketBoundaries Optional bucket boundaries for response delay seconds histogram
    *  @param messageSizeBytesHistogramBucketBoundaries Optional bucket boundaries for message size bytes histogram
    */
    void enableMetrics(ert::metrics::Metrics *metrics,
                       const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries = {},
                       const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries = {});

    /**
     * Send request to the server
     *
     * @param method Request method (POST, GET, PUT, DELETE, HEAD)
     * @param path Request uri path including optional query parameters
     * @param body Request body
     * @param headers Request headers
     * @param requestTimeout Request timeout, 1 second by default
     *
     * @return Response structure. Status code -1 means connection error, and -2 means timeout.
     */
    Http2Client::response send(const std::string &method,
                               const std::string &path,
                               const std::string &body,
                               const nghttp2::asio_http2::header_map &headers,
                               const std::chrono::milliseconds& requestTimeout = std::chrono::milliseconds(1000));

    /*
     * Callback for request send expiration.
     * Default implementation does nothing.
     */
    virtual void responseTimeout() {;}

    /*
     * Check if connection is open
     *
     * @return Boolean with connection open status
     */
    bool isConnected() const;

    /*
     * Gets connection status string
     *
     * @return string with connection status (NotOpen, Open, Closed)
     */
    std::string getConnectionStatus() const;
};

}
}

