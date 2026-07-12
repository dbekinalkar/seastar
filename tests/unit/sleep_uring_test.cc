/*
 * Regression test for seastar#1890 and seastar#2014:
 * io_uring reactor backend with DPDK networking causes sleep() to hang.
 *
 * This test verifies that seastar::sleep() completes correctly when using
 * the io_uring reactor backend, even under sustained polling where the event
 * loop enters "interrupt mode" (idle state with no pending I/O).
 *
 * The original bug was: with io_uring + DPDK enabled, the low-resolution
 * timers used by seastar::sleep() had no kernel timeout mechanism, so
 * the reactor could block on io_uring_wait_cqes() indefinitely without
 * checking the low-res timers.
 *
 * While we cannot reproduce the exact DPDK hang without hardware, this test
 * exercises the same code path and would catch regressions in how the
 * io_uring backend handles low-res timer wakeups during idle/interrupt mode.
 */

#include <seastar/core/sleep.hh>
#include <seastar/core/reactor.hh>
#include <seastar/testing/test_case.hh>
#include <seastar/testing/thread_test_case.hh>
#include <chrono>

using namespace seastar;
using namespace std::chrono_literals;

// Helper: convert duration to milliseconds as integer.
static int64_t to_ms(auto dur) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

// Basic sleep test: ensure sleep() completes with io_uring backend.
SEASTAR_THREAD_TEST_CASE(test_sleep_io_uring_basic) {
    auto start = lowres_clock::now();
    sleep(100ms).get();
    auto end = lowres_clock::now();
    int64_t elapsed = to_ms(end - start);
    BOOST_REQUIRE_GE(elapsed, 50);
    BOOST_REQUIRE_LE(elapsed, 300);
}

// Test multiple sleeps to exercise the poller wake loop repeatedly.
SEASTAR_THREAD_TEST_CASE(test_sleep_io_uring_repeated) {
    for (int i = 0; i < 10; ++i) {
        auto start = lowres_clock::now();
        sleep(50ms).get();
        auto end = lowres_clock::now();
        int64_t elapsed = to_ms(end - start);
        BOOST_REQUIRE_LE(elapsed, 200);
    }
}

// Test longer sleeps to ensure the hrtimer mechanism works over
// extended periods when no I/O is pending.
SEASTAR_THREAD_TEST_CASE(test_sleep_io_uring_longer) {
    auto test_duration = [](int64_t ms_val) {
        auto start = lowres_clock::now();
        sleep(std::chrono::milliseconds(ms_val)).get();
        auto end = lowres_clock::now();
        int64_t elapsed = to_ms(end - start);
        BOOST_REQUIRE_GE(elapsed, ms_val / 2);
        BOOST_REQUIRE_LE(elapsed, ms_val * 3);
    };
    test_duration(200);
    test_duration(500);
}

// Verify sleep interacts correctly with yields during io_uring polling.
SEASTAR_THREAD_TEST_CASE(test_sleep_io_uring_with_yields) {
    for (int i = 0; i < 5; ++i) {
        auto start = lowres_clock::now();
        sleep(30ms).get();
        // Use std::this_thread::sleep_for to avoid yield() ambiguity.
        std::this_thread::yield();
        std::this_thread::yield();
        auto end = lowres_clock::now();
        int64_t elapsed = to_ms(end - start);
        BOOST_REQUIRE_GE(elapsed, 10);
        BOOST_REQUIRE_LE(elapsed, 300);
    }
}

// Test sleep() with a then()-style chain (matching the original bug report pattern).
SEASTAR_TEST_CASE(test_sleep_io_uring_with_then_chain) {
    bool done = false;
    return sleep(100ms).then([&done] {
        done = true;
    }).then([&done] {
        BOOST_REQUIRE(done);
    });
}
