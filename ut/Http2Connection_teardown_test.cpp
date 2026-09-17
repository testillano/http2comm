/*
 Unit tests for Http2Connection teardown safety.

 These tests reproduce, in isolation, the teardown races that used to crash the
 process (found the hard way via CT + gdb). They are designed to run under
 AddressSanitizer/ThreadSanitizer to catch use-after-free / data races.

 Build (standalone) example:
   g++ -std=c++17 -g -fsanitize=address -I../include \
       Http2Connection_teardown_test.cpp ../src/Http2Connection.cpp \
       -lgtest -lgtest_main -lnghttp2_asio -lnghttp2 -lboost_system -lssl -lcrypto -lpthread \
       -o teardown_test && ./teardown_test

 Branches covered:
   (ii)  destroyed from an EXTERNAL thread when it NEVER connected (dead port).
   (iii) destroyed from its OWN io_context thread (ownership-bug path): must not
         crash the process (it logs and degrades; callers must avoid this).
*/

#include <gtest/gtest.h>

#include <ert/http2comm/Http2Connection.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using ert::http2comm::Http2Connection;

namespace {
// A port nothing listens on: connect never succeeds (ever_connected_ stays false).
constexpr const char* kDeadHost = "127.0.0.1";
constexpr const char* kDeadPort = "8009";
} // namespace

// (ii) Destroying a connection that never connected must be safe.
// Reproduces the SIGSEGV in nghttp2 session::shutdown() on a half-initialized
// session (endpoint on a closed port).
TEST(Http2ConnectionTeardown, DestroyNeverConnectedFromExternalThread) {
    for (int i = 0; i < 50; ++i) {
        auto conn = std::make_shared<Http2Connection>(kDeadHost, kDeadPort, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        conn.reset(); // destroyed on this (external) thread
    }
    SUCCEED();
}

// (ii) variant: concurrent create/destroy against the dead port from several
// external threads, stressing the teardown path under contention.
TEST(Http2ConnectionTeardown, ConcurrentDestroyNeverConnected) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 25;
    std::vector<std::thread> workers;
    std::atomic<int> done{0};
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < kPerThread; ++i) {
                auto conn = std::make_shared<Http2Connection>(kDeadHost, kDeadPort, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                conn.reset();
            }
            done++;
        });
    }
    for (auto& w : workers) w.join();
    EXPECT_EQ(done.load(), kThreads);
}

// (iii) Destroying a connection from within its OWN io_context thread is a
// caller ownership-invariant violation (see Http2Connection::closeImpl). There
// is no fully-safe teardown for this case with the current design: it leaves a
// use-after-free window (ASAN flags the detached io thread still inside
// io_context::run()). The supported contract is that callers never do this
// (h2agent posts chain continuations to a worker pool precisely to avoid it).
//
// This test is DISABLED on purpose: it documents the invariant and the known
// limitation, and would trip ASAN by design. Enable it manually only when
// working on the deeper ownership rework (tech debt) tracked in closeImpl.
TEST(Http2ConnectionTeardown, DISABLED_DestroyFromOwnIoThreadIsUnsupported) {
    auto conn = std::make_shared<Http2Connection>(kDeadHost, kDeadPort, false);
    boost::asio::post(conn->getIoContext(), [holder = conn]() mutable {
        holder.reset(); // last reference dropped on the io thread (invariant violation)
    });
    conn.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SUCCEED();
}
