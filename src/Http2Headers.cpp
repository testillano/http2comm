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

#include <ert/http2comm/Http2Headers.hpp>
#include <iostream>


namespace ert
{
namespace http2comm
{

std::string headersAsString(const nghttp2::asio_http2::header_map &headers) {
    std::string result = "";

    for(auto it = headers.begin(); it != headers.end(); it ++) {
        result += "[";
        result += it->first;
        result += ": ";
        result += it->second.value;
        result += "]";
    }

    return result;
}


const nghttp2::asio_http2::header_map& Http2Headers::getHeaders() const {
    return headers_;
}

void Http2Headers::emplace(const std::string& hKey, const std::string& hVal, bool sensitiveInformation)
{
    if (!hVal.empty()) {
        headers_.emplace(hKey, nghttp2::asio_http2::header_value{hVal, sensitiveInformation});
    }
}

void Http2Headers::addVersion(const std::string& value, const std::string& header)
{
    emplace(header, value);
}

void Http2Headers::addLocation(const std::string& value, const std::string& header)
{
    emplace(header, value);
}

void Http2Headers::addAllowedMethods(const std::vector<std::string>& value, const std::string& header)
{
    if (value.empty()) return;

    std::string serialized = value[0];
    for (auto method = value.begin() + 1; method != value.end(); method++)
    {
        serialized = serialized + ", " + *method;
    }

    emplace(header, serialized);
}

void Http2Headers::addContentLength(size_t value, const std::string& header)
{
    emplace(header, std::to_string(value));
}

void Http2Headers::addContentType(const std::string& value, const std::string& header)
{
    emplace(header, value);
}

std::string Http2Headers::asString() const {
    return headersAsString(headers_);
}

}
}

