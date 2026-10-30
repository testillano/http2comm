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

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ert
{
namespace http2comm
{

/**
 * Thread-safe cache of prometheus metric references, grouped by a label key.
 *
 * prometheus-cpp 'Family<T>::Add(labels)' builds and hashes the labels map and
 * locks the family on every call: on the traffic hot path (several metrics per
 * request or response) that dominates the cost of the metrics. The references
 * it returns are stable for the family lifetime (metrics are never removed
 * here), so each group of metrics sharing the same labels is resolved once and
 * reused: the hot path only builds a short key and does a shared-lock lookup.
 *
 * Groups hold the references of the metrics which are always created together
 * (so that no new series appears in the exposition compared to resolving them
 * on demand).
 */
template <class Group>
class MetricsCache
{
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Group> groups_{}; // element references are stable on rehash

public:
    /**
     * Gets the group for the key, building it on first use.
     *
     * @param key Labels key (unique per labels combination).
     * @param build Callable returning the Group (resolves the metrics with Family::Add).
     */
    template <class Build>
    const Group &get(const std::string &key, Build &&build)
    {
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = groups_.find(key);
            if (it != groups_.end()) return it->second;
        }
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = groups_.find(key);
        if (it != groups_.end()) return it->second; // built meanwhile by another thread
        return groups_.emplace(key, build()).first->second;
    }
};

/** Labels key for a method and an optional result label/value ("POST", "POST|s|200"). */
inline std::string metricsKey(const std::string &method, const char *resultLabel = nullptr, int resultValue = 0)
{
    if (!resultLabel) return method;
    std::string key = method;
    key += '|';
    key += resultLabel[0]; // 's'tatus_code / 'r'st_stream_goaway_error_code
    key += std::to_string(resultValue);
    return key;
}

}
}
