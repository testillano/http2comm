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

#include <nghttp2/asio_http2.h>

#include <string>
#include <regex>

#include <ert/http2comm/URLFunctions.hpp>


namespace nghttp2
{
namespace util
{
// This percent_encode function is declared in util.h file from
// nghttp2 source code and is present in the library itself, but
// the util.h header file is not installed in /usr/local/include/nghttp2
// We declare it here so it can be used in the
// add_query_parameters_to_uri anonymous function
extern std::string percent_encode(const std::string& target);
}
}


namespace ert
{
namespace http2comm
{
std::string URLFunctions::encode(const std::string& rawUrl)
{
    return nghttp2::util::percent_encode(rawUrl);
}

bool URLFunctions::matchPrefix(const std::string& urlPath,
                               const std::string& pathPrefix)
{
    // Normalize urlPath (add slashes at both sides):
    std::string pathNormalized = urlPath;

    if (urlPath.front() != '/')
    {
        pathNormalized.insert(pathNormalized.begin(), '/');
    }

    if (urlPath.back() != '/')
    {
        pathNormalized += "/";
    }

    // Normalize pathPrefix (add slashes at both sides):
    std::string prefixNormalized = pathPrefix;

    if (pathPrefix.front() != '/')
    {
        prefixNormalized.insert(prefixNormalized.begin(), '/');
    }

    if (pathPrefix.back() != '/')
    {
        prefixNormalized += "/";
    }

    // Match:
    std::size_t pos = pathNormalized.find(prefixNormalized);
    return (pos == 0);
}

}
}
