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

#include <ert/tracing/Logger.hpp>

#include <ert/http2comm/QueueDispatcher.hpp>

namespace ert
{
namespace http2comm
{


QueueDispatcher::QueueDispatcher(std::string name, size_t thread_cnt, size_t max_threads) :
    name_{std::move(name)}, threads_(thread_cnt), max_threads_(max_threads)
{
    max_threads_ = (max_threads != 0) ? max_threads:thread_cnt;

    LOGINFORMATIONAL(
        std::string msg = ert::tracing::Logger::asString("Creating dispatch queue '%s' with '%zu' ", name_.c_str(), thread_cnt);
        msg += (max_threads > thread_cnt) ? ert::tracing::Logger::asString("threads and a maximum of '%zu'.", max_threads_):"fixed threads.";
        ert::tracing::Logger::informational(msg, ERT_FILE_LOCATION));

    for (size_t i = 0; i < threads_.size(); i++)
    {
        threads_[i] = std::thread(&QueueDispatcher::dispatch_thread_handler, this);
    }
}

QueueDispatcher::~QueueDispatcher()
{
    LOGINFORMATIONAL(ert::tracing::Logger::informational(
                         ert::tracing::Logger::asString("Destroying dispatch threads ..."), ERT_FILE_LOCATION));

    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(lock_);
    quit_ = true;
    lock.unlock();
    cv_.notify_all();

    // Wait for threads to finish before we exit
    for (size_t i = 0; i < threads_.size(); i++)
    {
        if (threads_[i].joinable())
        {
            LOGINFORMATIONAL(ert::tracing::Logger::informational(
                                 ert::tracing::Logger::asString("Joining thread %zu until completion", i), ERT_FILE_LOCATION));
            threads_[i].join();
        }
    }
}

void QueueDispatcher::create_thread(void)
{
    threads_.push_back(std::thread(&QueueDispatcher::dispatch_thread_handler, this));
}

void QueueDispatcher::dispatch_thread_handler(void)
{
    std::unique_lock<std::mutex> lock(lock_);

    do
    {
        //Wait until we have data or a quit signal
        cv_.wait(lock, [this]
        {
            return (q_.size() || quit_);
        });

        //after wait, we own the lock
        if (!quit_ && q_.size())
        {
            auto st = std::move(q_.front());
            q_.pop();

            busy_threads_++;

            //unlock now that we're done messing with the queue
            lock.unlock();

            st->process();
            st->commit();

            lock.lock();
            busy_threads_--;
        }
    }
    while (!quit_);
}

void QueueDispatcher::dispatch(std::shared_ptr<Stream> st)
{
    if (busy_threads_.load() == threads_.size() && threads_.size() < max_threads_)
    {
        create_thread();
    }

    std::unique_lock<std::mutex> lock(lock_);
    //    q_.push(std::move(st));
    //
    //    // Manual unlocking is done before notifying, to avoid waking up
    //    // the waiting thread only to block again (see notify_one for details)
    //    lock.unlock();
    //    cv_.notify_one();

    q_.push(st);
    lock.unlock();
    cv_.notify_one();
}


}
}
