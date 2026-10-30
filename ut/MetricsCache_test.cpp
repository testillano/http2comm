// MetricsCache: thread-safe build-once cache of metric references by labels key.

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <ert/http2comm/MetricsCache.hpp>

using ert::http2comm::MetricsCache;
using ert::http2comm::metricsKey;

TEST(MetricsCache, BuildsOncePerKeyAndReturnsStableReferences) {
    MetricsCache<int> cache;
    int builds = 0;
    const int &a = cache.get("POST", [&] { builds++; return 1; });
    const int &b = cache.get("POST", [&] { builds++; return 2; });
    EXPECT_EQ(builds, 1);
    EXPECT_EQ(&a, &b);
    EXPECT_EQ(b, 1);

    // Many other keys (forces rehash): the first reference stays valid.
    for (int i = 0; i < 1000; i++) cache.get("K" + std::to_string(i), [&] { return i; });
    EXPECT_EQ(&cache.get("POST", [&] { return 3; }), &a);
    EXPECT_EQ(a, 1);
}

TEST(MetricsCache, ConcurrentFirstUseBuildsOnce) {
    MetricsCache<int> cache;
    std::atomic<int> builds{0};
    std::vector<const int *> seen(16, nullptr);
    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < seen.size(); t++) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 1000; i++) seen[t] = &cache.get("GET|s200", [&] { builds++; return 42; });
        });
    }
    for (auto &th : threads) th.join();
    EXPECT_EQ(builds.load(), 1);
    for (auto *p : seen) EXPECT_EQ(p, seen[0]);
}

TEST(MetricsCache, KeysDistinguishMethodLabelAndValue) {
    EXPECT_EQ(metricsKey("POST"), "POST");
    EXPECT_EQ(metricsKey("POST", "status_code", 200), "POST|s200");
    EXPECT_EQ(metricsKey("POST", "rst_stream_goaway_error_code", 8), "POST|r8");
    EXPECT_NE(metricsKey("POST", "status_code", 8), metricsKey("POST", "rst_stream_goaway_error_code", 8));
    EXPECT_NE(metricsKey("GET", "status_code", 200), metricsKey("POST", "status_code", 200));
}
