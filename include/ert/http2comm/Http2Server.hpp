/*
 __________________________________________________________________________________
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

#include <boost/asio.hpp>

#include <nghttp2/asio_http2_server.h>

#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http2Headers.hpp>
#include <ert/http2comm/QueueDispatcher.hpp>

#include <ert/metrics/Metrics.hpp>

namespace ert
{
namespace http2comm
{

class Http2Server
{
    std::string server_key_password_{};
    std::string name_{};
    std::string api_name_{};
    std::string api_version_{};
    boost::asio::io_service *timers_io_service_;
    QueueDispatcher *queue_dispatcher_;

    nghttp2::asio_http2::server::request_cb handler();

    // metrics:
    ert::metrics::Metrics *metrics_{};

    ert::metrics::counter_t *observed_requests_post_counter_{};
    ert::metrics::counter_t *observed_requests_get_counter_{};
    ert::metrics::counter_t *observed_requests_put_counter_{};
    ert::metrics::counter_t *observed_requests_delete_counter_{};
    ert::metrics::counter_t *observed_requests_head_counter_{};
    ert::metrics::counter_t *observed_requests_other_counter_{};
    ert::metrics::counter_t *observed_requests_error_post_counter_{};
    ert::metrics::counter_t *observed_requests_error_get_counter_{};
    ert::metrics::counter_t *observed_requests_error_put_counter_{};
    ert::metrics::counter_t *observed_requests_error_delete_counter_{};
    ert::metrics::counter_t *observed_requests_error_head_counter_{};
    ert::metrics::counter_t *observed_requests_error_other_counter_{};

    ert::metrics::histogram_t *responses_delay_seconds_histogram_{};
    ert::metrics::histogram_t *messages_size_bytes_rx_histogram_{};
    ert::metrics::histogram_t *messages_size_bytes_tx_histogram_{};

protected:

    nghttp2::asio_http2::server::http2 server_;

public:

    /**
    *  Class constructor
    *
    *  @param name Server name.
    *  @param workerThreads number of worker threads.
    *  @param timerIoService Optional io service to manage response delays
    */
    Http2Server(const std::string& name, size_t workerThreads, boost::asio::io_service *timerIoService = nullptr);
    ~Http2Server();

    // setters

    /**
    * Enable metrics
    *
    *  @param metrics Optional metrics object to compute counters and histograms
    *  @param responseDelaySecondsHistogramBucketBoundaries Optional bucket boundaries for response delay seconds histogram
    *  @param messageSizeBytesHistogramBucketBoundaries Optional bucket boundaries for message size bytes histogram
    */
    void enableMetrics(ert::metrics::Metrics *metrics,
                       const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries = {},
                       const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries = {});

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
    * Virtual reception callback. Implementation is mandatory.
    *
    * @param req nghttp2-asio request structure.
    * @param requestBody request body received.
    * @param statusCode response status code to be filled by reference.
    * @param headers reponse headers to be filled by reference.
    * @param responseBody response body to be filled by reference.
    * @param responseDelayMs reponse delay in milliseconds to be filled by reference.
    */
    virtual void receive(const nghttp2::asio_http2::server::request& req,
                         const std::string& requestBody,
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
    * @param requestBody request body received.
    * @param statusCode response status code to be filled by reference.
    * @param headers reponse headers to be filled by reference.
    * @param responseBody response body to be filled by reference.
    * @param error error pair given by tuple code/description, i.e.: <415,'UNSUPPORTED_MEDIA_TYPE'>.
    * If no description provided, empty json document ({}) is sent in the default implementation.
    * @param location location header content. Empty by default.
    * @param allowedMethods allowed methods vector given by server implementation. Empty by default.
    */
    virtual void receiveError(const nghttp2::asio_http2::server::request& req,
                              const std::string& requestBody,
                              unsigned int& statusCode,
                              nghttp2::asio_http2::header_map& headers,
                              std::string& responseBody,
                              const std::pair<int, const std::string>& error,
                              const std::string &location = "",
                              const std::vector<std::string>& allowedMethods = std::vector<std::string>());

    // Default error handler
    //virtual nghttp2::asio_http2::server::request_cb errorHandler();

    /**
    * Server start
    *
    * @param bind_address server bind address.
    * @param listen_port server listen port.
    * @param key secure key for HTTP/2 communication.
    * @param cert secure cert for HTTP/2 communication.
    * @param numberThreads nghttp2 server threads (multi client).
    * @param asynchronous boolean for non-blocking server start.
    */
    int serve(const std::string& bind_address,
              const std::string& listen_port,
              const std::string& key,
              const std::string& cert,
              int numberThreads,
              bool asynchronous = false);

    /**
    * Gets the timers io service used to manage response delays
    */
    boost::asio::io_service *getTimersIoService() const {
        return timers_io_service_;
    }

    /**
    * Server stop
    */
    int stop();

    friend class Stream;
};

}
}

