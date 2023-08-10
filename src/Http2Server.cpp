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

#include <string>
#include <thread>
#include <sstream>
#include <iterator>
#include <iostream>
#include <boost/exception/diagnostic_information.hpp>

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/Http2Server.hpp>
#include <ert/http2comm/URLFunctions.hpp>
#include <ert/http2comm/Stream.hpp>
#include <ert/http2comm/Http.hpp>
#include <ert/http2comm/Http2Headers.hpp>


namespace ert
{
namespace http2comm
{

Http2Server::Http2Server(const std::string& name, size_t workerThreads, size_t maxWorkerThreads, boost::asio::io_context *timersIoContext, int queueDispatcherMaxSize):
    name_(name), timers_io_context_(timersIoContext), reception_id_(0), maximum_request_body_size_(0), queue_dispatcher_max_size_(queueDispatcherMaxSize) {

    queue_dispatcher_ = (workerThreads > 1) ? new ert::queuedispatcher::QueueDispatcher(name + "_queueDispatcher", workerThreads, maxWorkerThreads) : nullptr;
}

int Http2Server::getQueueDispatcherBusyThreads() const {
    return (queue_dispatcher_ ? queue_dispatcher_->getBusyThreads():0);
}

int Http2Server::getQueueDispatcherThreads() const {
    return (queue_dispatcher_ ? queue_dispatcher_->getThreads():0);
}

int Http2Server::getQueueDispatcherSize() const {
    return (queue_dispatcher_ ? queue_dispatcher_->getSize():0);
}

int Http2Server::getQueueDispatcherMaxSize() const {
    return queue_dispatcher_max_size_;
}

void Http2Server::enableMetrics(ert::metrics::Metrics *metrics,
                                const ert::metrics::bucket_boundaries_t &responseDelaySecondsHistogramBucketBoundaries,
                                const ert::metrics::bucket_boundaries_t &messageSizeBytesHistogramBucketBoundaries) {

    metrics_ = metrics;

    if (metrics_) {
        ert::metrics::labels_t familyLabels = {{"source", name_}};

        observed_requests_accepted_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_resquests_accepted_counter"), std::string("Requests accepted observed counter in ") + name_, familyLabels));
        observed_requests_errored_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_resquests_errored_counter"), std::string("Requests errored observed counter in ") + name_, familyLabels));
        observed_responses_counter_family_ptr_ = &(metrics_->addCounterFamily(name_ + std::string("_observed_responses_counter"), std::string("Responses observed counter in ") + name_, familyLabels));

        responses_delay_seconds_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_responses_delay_seconds_gauge"), std::string("Message responses delay gauge (seconds) in ") + name_, familyLabels));
        responses_delay_seconds_gauge_ = &(responses_delay_seconds_gauge_family_ptr_->Add({})); // we could create ad-hoc gauges for each http2 method, but it is probably overkilling
        received_messages_size_bytes_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_received_messages_size_bytes_gauge"), std::string("Received messages sizes gauge (bytes) in ") + name_, familyLabels));
        received_messages_size_bytes_gauge_ = &(received_messages_size_bytes_gauge_family_ptr_->Add({})); // we could create ad-hoc gauges for each http2 method, but it is probably overkilling
        sent_messages_size_bytes_gauge_family_ptr_ = &(metrics_->addGaugeFamily(name_ + std::string("_sent_messages_size_bytes_gauge"), std::string("Sent messages sizes gauge (bytes) in ") + name_, familyLabels));
        sent_messages_size_bytes_gauge_ = &(sent_messages_size_bytes_gauge_family_ptr_->Add({})); // we could create ad-hoc gauges for each http2 method, but it is probably overkilling

        responses_delay_seconds_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_responses_delay_seconds_histogram"), std::string("Message responses delay histogram (seconds) in ") + name_, familyLabels));
        responses_delay_seconds_histogram_ = &(responses_delay_seconds_histogram_family_ptr_->Add({}, responseDelaySecondsHistogramBucketBoundaries)); // we could create ad-hoc histograms for each http2 method, but it is probably overkilling
        received_messages_size_bytes_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_received_messages_size_bytes_histogram"), std::string("Received messages sizes histogram (bytes) in ") + name_, familyLabels));
        received_messages_size_bytes_histogram_ = &(received_messages_size_bytes_histogram_family_ptr_->Add({}, messageSizeBytesHistogramBucketBoundaries)); // we could create ad-hoc histograms for each http2 method, but it is probably overkilling
        sent_messages_size_bytes_histogram_family_ptr_ = &(metrics_->addHistogramFamily(name_ + std::string("_sent_messages_size_bytes_histogram"), std::string("Sent messages sizes histogram (bytes) in ") + name_, familyLabels));
        sent_messages_size_bytes_histogram_ = &(sent_messages_size_bytes_histogram_family_ptr_->Add({}, messageSizeBytesHistogramBucketBoundaries)); // we could create ad-hoc histograms for each http2 method, but it is probably overkilling
    }
}

Http2Server::~Http2Server() {
    delete queue_dispatcher_;
}

std::string Http2Server::getApiPath() const
{
    std::string result{};
    if (api_name_.empty()) return result;

    result += "/";
    result += api_name_;

    if (api_version_.empty()) return result;

    result += "/";
    result += api_version_;

    return result;
}

void Http2Server::receiveError(const nghttp2::asio_http2::server::request& req,
                               const std::string &requestBody,
                               unsigned int& statusCode,
                               nghttp2::asio_http2::header_map& headers,
                               std::string& responseBody,
                               const std::pair<int, const std::string>& error,
                               const std::string &location,
                               const std::vector<std::string>& allowedMethods)
{
    // metrics
    if (metrics_) {
        auto& counter = observed_requests_errored_counter_family_ptr_->Add({{"method", req.method()}});
        counter.Increment();
    }

    statusCode = error.first;
    responseBody = "{}";

    std::string s_errorCause = "<none>";
    if (error.second != "")
    {
        s_errorCause = error.second;
        responseBody = "{\"cause\":\"" + (s_errorCause + "\"}");
    }

    ert::tracing::Logger::error(ert::tracing::Logger::asString(
                                    "UNSUCCESSFUL REQUEST: path %s, code %d, error cause %s",
                                    req.uri().path.c_str(), statusCode, s_errorCause.c_str()), ERT_FILE_LOCATION);

    // Headers:
    Http2Headers hdrs;
    hdrs.addVersion(getApiVersion());
    hdrs.addLocation(location);
    hdrs.addAllowedMethods(allowedMethods);
    hdrs.addContentLength(responseBody.size());
    hdrs.addContentType(((statusCode >= 200) && (statusCode < 300)) ? "application/json" : "application/problem+json");
    headers = hdrs.getHeaders();
}

nghttp2::asio_http2::server::request_cb Http2Server::handler()
{
    return [&](const nghttp2::asio_http2::server::request & req,
               const nghttp2::asio_http2::server::response & res)
    {
        auto stream = std::make_shared<Stream>(req, res, this);
        req.on_data([stream, this](const uint8_t* data, std::size_t len)
        {
            if (len > 0) // https://stackoverflow.com/a/72925875/2576671
            {
                if (receiveDataLen(stream->getReq())) {
                    // https://github.com/testillano/h2agent/issues/6 is caused when this is enabled, on high load and broke client connections:
                    // (mutexes does not solves the problem neither std::move of data, and does not matter is shared_ptr requestBody is replaced
                    // by static type; it seems that data is not correctly protected on lower layers, probably tatsuhiro-t nghttp2)
                    stream->appendData(data, len);

                    // Update maximum request body size registered
                    //if (len > maximum_request_body_size_.load()) { // this allows traffic profile learning
                    if (preReserveRequestBody() && (len > maximum_request_body_size_.load())) {
                        maximum_request_body_size_.store(len);
                    }
                }
            }
            else
            {
                reception_id_++;
                stream->setReceptionId(reception_id_.load());

                if (queue_dispatcher_) {
                    queue_dispatcher_->dispatch(stream);
                }
                else {
                    stream->reception();
                    stream->commit();
                }
            }
        });

        res.on_close([stream](uint32_t error_code)
        {
            if (error_code != 0) {
                stream->error();
                ert::tracing::Logger::error(ert::tracing::Logger::asString("Client connection error: %d", error_code), ERT_FILE_LOCATION);
            }
            else {
                stream->close();
            }
        });
    };
}

int Http2Server::serve(const std::string& bind_address,
                       const std::string& listen_port,
                       const std::string& key,
                       const std::string& cert,
                       int numberThreads,
                       bool asynchronous,
                       const boost::posix_time::time_duration &readKeepAlive)
{
    bool secure = !key.empty() && !cert.empty();

    boost::system::error_code ec;

    server_.handle("/", handler());
    server_.num_threads(numberThreads);
    server_.read_timeout(readKeepAlive);

    if (secure)
    {
        try {
            boost::asio::ssl::context tls(boost::asio::ssl::context::sslv23);

            if (!server_key_password_.empty()) {
                tls.set_password_callback ([this](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                    return server_key_password_;
                });
            }

            tls.use_private_key_file(key, boost::asio::ssl::context::pem);
            tls.use_certificate_chain_file(cert);
            nghttp2::asio_http2::server::configure_tls_context_easy(ec, tls);
            server_.listen_and_serve(ec, tls, bind_address, listen_port, asynchronous);
        }
        catch (const boost::exception& e)
        {
            std::cerr << boost::diagnostic_information(e) << '\n';
            stop();
            return EXIT_FAILURE;
        }
        catch (...) {
            std::cerr << "Server initialization failure in " << name_ << '\n';
            stop();
            return EXIT_FAILURE;
        }

    }
    else
    {
        server_.listen_and_serve(ec, bind_address, listen_port, asynchronous);
    }

    if (ec)
    {
        std::stringstream ss;
        ss << "Initialization error in " << name_ << " (" << bind_address << ":" <<
           listen_port << "): " << ec.message();
        ert::tracing::Logger::error(ss.str().c_str(), ERT_FILE_LOCATION);
        std::cerr << ss.str() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int Http2Server::join()
{
    try
    {
        server_.join();
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int Http2Server::stop()
{
    try
    {
        // stop internal io contexts
        for (auto &ii: server_.io_services()) if (!ii->stopped()) ii->stop();
        server_.stop();
    }
    catch (std::exception& e)
    {
        ert::tracing::Logger::error(e.what(), ERT_FILE_LOCATION);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

}
}
