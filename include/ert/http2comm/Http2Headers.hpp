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
#include <vector>

#include <nghttp2/asio_http2_server.h>

namespace ert
{
namespace http2comm
{

/**
 * Prints headers list for traces. For example:
 * '[content-length: 200][content-type: application/json; charset=utf-8]'
 *
 * @param headers nghttp2 headers map
 *
 * @return sorted query parameters URI part
 */
std::string headersAsString(const nghttp2::asio_http2::header_map &headers);


class Http2Headers
{
    nghttp2::asio_http2::header_map headers_{};

public:
    Http2Headers() {};

    // setters

    /**
    * Adds version header
    */
    void addVersion(const std::string& value, const std::string& header = "x-version");

    /**
    * Adds location header
    */
    void addLocation(const std::string& value, const std::string& header = "location");

    /**
    * Adds allow header
    */
    void addAllowedMethods(const std::vector<std::string>& value, const std::string& header = "Allow");

    /**
    * Adds content-length header
    */
    void addContentLength(size_t value, const std::string& header = "content-length");

    /**
    * Adds content-type header
    */
    void addContentType(const std::string& value, const std::string& header = "content-type");

    /**
    * Generic method to add a header
    * Added value must be non-empty to be added (internally checked).
    *
    * @param hKey header field name, i.e. 'content-type'
    * @param hVal header value
    * @param sensitiveInformation If 'true' the header is not indexed by HPACK (but still huffman-encoded)
    */
    void emplace(const std::string& hKey, const std::string& hVal, bool sensitiveInformation = false);

    // getters

    /**
    * Gets current built header map
    */
    const nghttp2::asio_http2::header_map& getHeaders() const;

    /**
    * Class string representation
    */
    std::string asString() const;
};

}
}

