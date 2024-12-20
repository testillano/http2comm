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
#include <regex>
//#include <iomanip>
//#include <cctype>


#include <ert/http2comm/URLFunctions.hpp>


const char UPPER_XDIGITS[] = "0123456789ABCDEF";

namespace ert
{
namespace http2comm
{
std::string URLFunctions::encode(const std::string& decodedUrl)
{
    std::string result;
    result.reserve(decodedUrl.length() * 3); // minimize reallocations in std::string
    // (we multiply by 3 because it is the worst case: %XX for every character)

    for (unsigned char c : decodedUrl) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') { // unreserved (RFC3986)
            result += c;
        } else {
            // encode as %XX
            result += '%';
            result += UPPER_XDIGITS[c >> 4]; // first digit
            result += UPPER_XDIGITS[c & 0x0F]; // second digit
        }
    }

    return result;
}

std::string URLFunctions::decode(const std::string& encodedUrl)
{
    std::string result;
    result.reserve(encodedUrl.size());

    for (std::size_t i = 0; i < encodedUrl.size(); ++i)
    {
        auto ch = encodedUrl[i];

        if (ch == '%' && (i + 2) < encodedUrl.size())
        {
            auto hex = encodedUrl.substr(i + 1, 2);
            auto dec = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result.push_back(dec);
            i += 2;
        }
        else if (ch == '+')
        {
            result.push_back(' ');
        }
        else
        {
            result.push_back(ch);
        }
    }

    return result;
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
