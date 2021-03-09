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

#include <ert/http2comm/ResponseHeader.hpp>


namespace ert
{
namespace http2comm
{

ResponseHeader::ResponseHeader(const std::string& version,
                               const std::string& location,
                               const std::vector<std::string>& allowedMethods)
{
    version_ = version;
    location_ = location;
    allowed_methods_ = allowedMethods;
}


nghttp2::asio_http2::header_map
ResponseHeader::getResponseHeader(size_t contentLength, unsigned int status)
{
    std::string contentType = ((status >= 200)
                               && (status < 300)) ? "application/json" :
                              "application/problem+json";

    auto headers = nghttp2::asio_http2::header_map();

    if (version_.size() != 0)
    {
        headers.emplace("x-version", nghttp2::asio_http2::header_value{version_});
    }

    if (location_.size() != 0)
    {
        headers.emplace("location", nghttp2::asio_http2::header_value{location_, false /* non sensitive */});
    }

    if (contentLength != 0)
    {
        headers.emplace("content-type", nghttp2::asio_http2::header_value{contentType});
    }

    headers.emplace("content-length", nghttp2::asio_http2::header_value{std::to_string(contentLength)});

    if (allowed_methods_.empty() != true)
    {
        std::string allowedSerialized = allowed_methods_[0];

        for (auto method = allowed_methods_.begin() + 1;
                method != allowed_methods_.end();
                method++)
        {
            allowedSerialized = allowedSerialized + ", " + *method;
        }

        headers.emplace("Allow", nghttp2::asio_http2::header_value{allowedSerialized});
    }

    return headers;
}

}
}
