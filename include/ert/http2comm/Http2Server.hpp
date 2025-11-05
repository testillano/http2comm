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
#include <memory>
#include <chrono>
#include <atomic>

#include <boost/asio.hpp>

#include <nghttp2/asio_http2_server.h>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http2Headers.hpp>

#include <ert/queuedispatcher/QueueDispatcher.hpp>
#include <ert/metrics/Metrics.hpp>

namespace ert
{
namespace http2comm
{

class Http2Server
{
    std::string server_key_password_{};
    // Metric names should be in lowercase and separated by underscores (_).
    // Metric names should start with a letter or an underscore (_).
    // Metric names should be descriptive and meaningful for their purpose.
    // Metric names should not be too long or too short.
    std::string api_name_{};
    std::string api_version_{};
    boost::asio::io_context *timers_io_context_;
    ert::queuedispatcher::QueueDispatcher *queue_dispatcher_;
    int queue_dispatcher_max_size_{};

    nghttp2::asio_http2::server::request_cb handler();

    // metrics:
    ert::metrics::Metrics *metrics_{};

    // Create counters families for specific situations, and let dynamic labes for unpredictable (status codes) or small sets (method).
    // We could even make combinations with methods (POST, GET, PUT, DELETE, HEAD) but we would manage 5x3=15 families instead of 3:
    ert::metrics::counter_family_t *observed_requests_accepted_counter_family_ptr_{};
    ert::metrics::counter_family_t *observed_requests_errored_counter_family_ptr_{};
    ert::metrics::counter_family_t *observed_responses_counter_family_ptr_{};

    // Idem for gauges:
    ert::metrics::gauge_family_t *responses_delay_seconds_gauge_family_ptr_{};
    ert::metrics::gauge_family_t *received_messages_size_bytes_gauge_family_ptr_{};
    ert::metrics::gauge_family_t *sent_messages_size_bytes_gauge_family_ptr_{};

    // Idem for histograms:
    ert::metrics::histogram_family_t *responses_delay_seconds_histogram_family_ptr_{};
    ert::metrics::histogram_family_t *received_messages_size_bytes_histogram_family_ptr_{};
    ert::metrics::histogram_family_t *sent_messages_size_bytes_histogram_family_ptr_{};
    ert::metrics::bucket_boundaries_t response_delay_seconds_histogram_bucket_boundaries_;
    ert::metrics::bucket_boundaries_t message_size_bytes_histogram_bucket_boundaries_; // both received/sent (simplification)

    std::atomic<std::uint64_t> reception_id_{};
    std::atomic<std::size_t> maximum_request_body_size_{};

protected:

    nghttp2::asio_http2::server::http2 server_;
    std::string name_{};

public:

    /**
    * Class constructor
    *
    * @param name class name. It may be used to prefix the family name for every metric managed by
    * the class (counters, gauges, histograms), so consider to provide a compatible metrics name
    * ([a-zA-Z0-9:_]). You may also avoid dynamic names to build predictable grafana dashboards.
    * Dynamic information shall be passed to enableMetrics() to be part of 'source' label. For
    * example, the application generic name (project) and HTTP2 endpoint category would be
    * appropriate class names:
    *
    * - h2agent_traffic_server
    *
    * @param workerThreads number of worker threads.
    * @param maxWorkerThreads number of maximum worker threads which internal processing could grow to. Defaults to '0' which means that maximum equals to provided worker threads.
    * @param timerIoContext Optional io context to manage response delays
    * @param queueDispatcherMaxSize This library implements a simple congestion control algorithm which will indicate congestion status when queue dispatcher (when used) has no
    * idle consumer threads, and queue dispatcher size is over this value. Defaults to -1 which means 'no limit' to grow the queue (this probably implies response time degradation).
    * So, to enable the described congestion control algorithm, provide a non-negative value.
    */
    Http2Server(const std::string& name, size_t workerThreads, size_t maxWorkerThreads = 0, boost::asio::io_context *timerIoContext = nullptr, int queueDispatcherMaxSize = -1 /* no limit */);
    virtual ~Http2Server();

    // setters


    /**
    * Gets the queue dispatcher busy threads
    */
    int getQueueDispatcherBusyThreads() const;

    /**
    * Gets the queue dispatcher number of scheduled threads
    */
    int getQueueDispatcherThreads() const;

    /**
    * Gets the queue dispatcher size
    */
    int getQueueDispatcherSize() const;

    /**
    * Gets the queue dispatcher maximum size allowed on congestion control
    * Defaults to -1 (no limit to grow the queue).
    *
    * This enables a simple congestion control algorithm which consist in indicate congestion when
    * queue dispatcher has no idle consumer threads and also, queue size is over this specific
    * value.
    */
    int getQueueDispatcherMaxSize() const;

    /**
    * Enable metrics
    *
    *  @param metrics Optional metrics object to compute counters and histograms
    *  @param responseDelaySecondsHistogramBucketBoundaries Optional bucket boundaries for response delay seconds histogram
    *  @param messageSizeBytesHistogramBucketBoundaries Optional bucket boundaries for message size bytes histogram
    *  @param source Source label for prometheus metrics. If missing, class name will be taken (even being redundant with
    *  family name prefix as will ease metrics filtering anyway). A good source convention could be the process name and
    *  the endpoint identification:
    *
    *  - h2agent[_traffic_server]: optional endpoint category, as it would be deducted from family name
    *  - h2agentB
    */
    void enableMetrics(ert::metrics::Metrics *metrics,
                       const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries = {},
                       const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries = {}, const std::string &source = "");

    /**
    * Sets the server key password to use with TLS/SSL
    */
    void setServerKeyPassword(const std::string& password)
    {
        server_key_password_ = password;
    }

    /**
    * Sets the API name
    */
    void setApiName(const std::string& apiName)
    {
        api_name_ = apiName;
    }

    /**
    * Sets the API version
    */
    void setApiVersion(const std::string& apiVersion)
    {
        api_version_ = apiVersion;
    }

    // getters

    /**
    * Gets the API path (/<name>/<version>)
    */
    std::string getApiPath() const;

    /**
    * Gets the API name
    */
    const std::string &getApiName() const
    {
        return (api_name_);
    }

    /**
    * Gets the API version
    */
    const std::string &getApiVersion() const
    {
        return (api_version_);
    }

    /**
    * Virtual implementable definition of allowed methods
    */
    virtual bool checkMethodIsAllowed(
        const nghttp2::asio_http2::server::request& req,
        std::vector<std::string>& allowedMethods) = 0;

    /**
    * Virtual implementable definition of implemented methods
    */
    virtual bool checkMethodIsImplemented(
        const nghttp2::asio_http2::server::request& req) = 0;

    /**
    * Virtual implementable definition of correct headers
    */
    virtual bool checkHeaders(const nghttp2::asio_http2::server::request& req) = 0;

    /**
    * As possible optimization, our server could ignore the request body, so nghttp2 'on_data'
    * could be lightened skipping the request body internal copy. This is useful when huge
    * requests are received and they are not actually needed (for example, a dummy server
    * could mock valid static responses regardless the content received).
    *
    * @param req nghttp2-asio request structure.
    * This could be used to store data received inside a server internal indexed map.
    *
    * @return Boolean about internal copy of request body received. Default implementation returns 'true'.
    */
    virtual bool receiveDataLen(const nghttp2::asio_http2::server::request& req) {
        return true;
    }

    /**
    * By default, memory to store request data is pre reserved to minimize possible reallocations
    * when several chunks are received and then appended to the request body. The maximum request
    * body size registered at a given moment is reserved by design.
    *
    * Performance in general is better with this optimization, but specially with huge messages and
    * low standard deviation of sizes registered. A mixed traffic profile with small messages has
    * not such an obvious improvement although CPU consumption is not influenced by reservation
    * amount. Instant memory requirements however, will be higher with the time until the largest
    * message processed, but this is not a handicap under controlled environments.
    *
    * As possible simplification, our server could delegate memory allocation to inner string
    * container (std::string append()) skipping the memory reservation done before appending data.
    *
    * @return Boolean about memory pre reservation. Default implementation returns 'true'.
    */
    virtual bool preReserveRequestBody() {
        return true;
    }

    /**
    * Virtual reception callback. Implementation is mandatory.
    *
    * @param receptionId server reception identifier (monotonically increased value in every reception, from 1 to N).
    * @param req nghttp2-asio request structure.
    * @param requestBody request body received.
    * @param receptionTimestampUs microseconds timestamp of reception.
    * @param statusCode response status code to be filled by reference.
    * @param headers response headers to be filled by reference.
    * @param responseBody response body to be filled by reference.
    * @param responseDelayMs response delay in milliseconds to be filled by reference.
    */
    virtual void receive(const std::uint64_t &receptionId,
                         const nghttp2::asio_http2::server::request& req,
                         const std::string &requestBody,
                         const std::chrono::microseconds &receptionTimestampUs,
                         unsigned int& statusCode,
                         nghttp2::asio_http2::header_map& headers,
                         std::string& responseBody,
                         unsigned int &responseDelayMs) = 0;

    /**
    * Virtual error reception callback.
    * Default implementation consider this json response body:
    * <pre>
    * { "cause": "<error cause>" }
    * </pre>
    *
    * @param req nghttp2-asio request structure.
    * @param requestBody request body received (not used in default implementation, and probably never used).
    * @param statusCode response status code to be filled by reference.
    * @param headers response headers to be filled by reference.
    * @param responseBody response body to be filled by reference.
    * @param error error pair given by tuple code/description, i.e.: <415,'UNSUPPORTED_MEDIA_TYPE'>.
    * If no description provided, empty json document ({}) is sent in the default implementation.
    * @param location location header content. Empty by default.
    * @param allowedMethods allowed methods vector given by server implementation. Empty by default.
    */
    virtual void receiveError(const nghttp2::asio_http2::server::request& req,
                              const std::string &requestBody,
                              unsigned int& statusCode,
                              nghttp2::asio_http2::header_map& headers,
                              std::string& responseBody,
                              const std::pair<int, const std::string>& error,
                              const std::string &location = "",
                              const std::vector<std::string>& allowedMethods = std::vector<std::string>());

    /**
    * Virtual event for stream close under error condition (not normal close)
    *
    * @param errorCode Http2 standard errors: normal goaway (0x00), refused stream (0x07), cancel (0x08), etc., check on http://datatracker.ietf.org/doc/html/rfc7540#section-7
    * @param name Server name
    * @param receptionId Unique recepcion identifier
    * @param uriPath Original request path
    */
    virtual void streamError(uint32_t errorCode, const std::string &serverName, const std::uint64_t &receptionId, const nghttp2::asio_http2::server::request &req);

    /**
    * Virtual dynamic response delay in milliseconds
    *
    * The server could set response dynamic delays which could postpone the stream commit (answer)
    * until certain condition is fulfilled. This delay is added after the possible response delay
    * configured on receive(). A typical application could be setting a periodic check of a server
    * condition for a reception identifier to ensure some kind of synchronous scenario.
    *
    * @param receptionId Unique recepcion identifier
    *
    * @return milliseconds of delay, or zero() when no delay will be planned (default implementation).
    */
    virtual std::chrono::milliseconds responseDelayMs(const std::uint64_t &receptionId) {
        return std::chrono::milliseconds::zero();
    }

    /**
    * Server start
    *
    * @param bind_address server bind address.
    * @param listen_port server listen port.
    * @param key secure key for HTTP/2 communication.
    * @param cert secure cert for HTTP/2 communication.
    * @param numberThreads nghttp2 server threads (multi client).
    * @param asynchronous boolean for non-blocking server start.
    * @param readKeepAlive read activity keep alive period. If server does not receive data in this
    * period, then it will close the connection. Default value is 1 minute.
    *
    * @return exit code (EXIT_SUCCESS|EXIT_FAILURE)
    */
    int serve(const std::string& bind_address,
              const std::string& listen_port,
              const std::string& key,
              const std::string& cert,
              int numberThreads,
              bool asynchronous = false,
              const boost::posix_time::time_duration &readKeepAlive = boost::posix_time::seconds(60));

    /**
    * Gets the timers io context used to manage response delays
    */
    boost::asio::io_context *getTimersIoContext() const {
        return timers_io_context_;
    }

    /**
    * Server join
    */
    int join();

    /**
    * Server stop
    */
    int stop();

    friend class Stream;
};

}
}

