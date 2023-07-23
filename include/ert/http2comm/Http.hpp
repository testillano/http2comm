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

namespace ert
{
namespace http2comm
{

class ResponseCode
{
public:
    enum Value
    {
        OK = 200,
        CREATED = 201,
        ACCEPTED = 202,
        NO_CONTENT = 204,
        MULTIPLE_CHOICES = 300,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        LENGTH_REQUIRED = 411,
        URI_TOO_LONG = 414,
        UNSUPPORTED_MEDIA_TYPE = 415,
        UNPROCESSABLE = 422,
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        SERVICE_UNAVAILABLE = 503
    };
};

const std::pair<int, std::string> WRONG_URI (
    ResponseCode::NOT_FOUND,
    "");

const std::pair<int, std::string> WRONG_API_NAME_OR_VERSION(
    ResponseCode::BAD_REQUEST,
    "INVALID_API");

const std::pair<int, std::string> SYSTEM_FAILURE (
    ResponseCode::INTERNAL_SERVER_ERROR,
    "SYSTEM_FAILURE");

const std::pair<int, std::string> INCORRECT_LENGTH (
    ResponseCode::LENGTH_REQUIRED,
    "INCORRECT_LENGTH");

const std::pair<int, std::string> UNSUPPORTED_MEDIA_TYPE (
    ResponseCode::UNSUPPORTED_MEDIA_TYPE,
    "UNSUPPORTED_MEDIA_TYPE");

const std::pair<int, std::string> METHOD_NOT_ALLOWED (
    ResponseCode::METHOD_NOT_ALLOWED,
    "METHOD_NOT_ALLOWED");

const std::pair<int, std::string> METHOD_NOT_IMPLEMENTED (
    ResponseCode::NOT_IMPLEMENTED,
    "METHOD_NOT_IMPLEMENTED");

const std::pair<int, std::string> SERVICE_UNAVAILABLE (
    ResponseCode::SERVICE_UNAVAILABLE,
    "SERVICE_UNAVAILABLE");
}
}

