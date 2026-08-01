/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <seastar/core/coroutine.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/timer.hh>
#include <seastar/testing/test_case.hh>

#include <chrono>
#include <string>

using namespace seastar;
using namespace std::chrono_literals;

// The first timer callback arms a second high-resolution timer while the
// preempt AIO hrtimer completion is still being processed. With io_uring this
// must rearm the hrtimer completion from the polling path, because poll mode
// never enters wait_and_process_events().
SEASTAR_TEST_CASE(highres_timer_rearm_from_callback_test) {
    BOOST_REQUIRE_EQUAL(std::string(engine().get_backend_name()), "io_uring");

    promise<> done;
    timer<> first;
    timer<> second;

    first.set_callback([&] {
        second.set_callback([&] {
            done.set_value();
        });
        second.arm(100ms);
    });

    first.arm(100ms);
    co_await done.get_future();
}
