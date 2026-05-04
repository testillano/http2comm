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

// SFINAE shim to access the underlying boost::asio executor of an
// nghttp2-asio session/request/response, regardless of how the accessor
// is named in the nghttp2-asio variant in use:
//
//   - Upstream nghttp2-asio (tatsuhiro-t, main branch, archived) exposes
//     io_service() returning boost::asio::io_service& (which is a typedef
//     for boost::asio::io_context since Boost 1.66).
//   - Some downstream forks rename the accessor to io_context(), aligning
//     with the Asio type that has superseded io_service.
//
// Using this helper keeps http2comm callers agnostic to the variant.

namespace ert
{
namespace http2comm
{
namespace asio_compat
{

// Preferred overload: picked when the object exposes io_context().
template <class T>
auto ioContext(T& x, int) -> decltype((x.io_context())) {
    return x.io_context();
}

// Fallback overload: picked when only io_service() is available.
template <class T>
auto ioContext(T& x, long) -> decltype((x.io_service())) {
    return x.io_service();
}

// Public entry point; callers write asio_compat::io_context(obj).
template <class T>
auto io_context(T& x) -> decltype(ioContext(x, 0)) {
    return ioContext(x, 0);
}

// Plural variant, for objects that return a collection of io_contexts
// (e.g. nghttp2::asio_http2::server::http2::io_services()).
template <class T>
auto ioContexts(T& x, int) -> decltype((x.io_contexts())) {
    return x.io_contexts();
}

template <class T>
auto ioContexts(T& x, long) -> decltype((x.io_services())) {
    return x.io_services();
}

template <class T>
auto io_contexts(T& x) -> decltype(ioContexts(x, 0)) {
    return ioContexts(x, 0);
}

}
}
}
