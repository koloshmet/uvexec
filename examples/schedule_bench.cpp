/*
 * Copyright (c) 2024 Michael Guzov
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>

#include <uvexec/execution/loop.hpp>

#include <exec/single_thread_context.hpp>
#include <exec/repeat_n.hpp>


using namespace std::literals;

TEST_CASE("Schedule benchmark", "[tcp][bench]") {
    NUvExec::TLoop loop;
    exec::single_thread_context thread;

    constexpr int n = 100;
    auto schedule_n = stdexec::schedule(loop.get_scheduler()) | exec::repeat_n(n);
    auto schedule_thread_n = stdexec::schedule(thread.get_scheduler()) | exec::repeat_n(n);
    auto schedule_after0_n = exec::schedule_after(loop.get_scheduler(), 0ms)| exec::repeat_n(n);

    BENCHMARK("Schedule thread") {
        return stdexec::sync_wait(schedule_thread_n).value();
    };
    BENCHMARK("Schedule") {
        return stdexec::sync_wait(schedule_n).value();
    };
    BENCHMARK("Schedule after 0ms") {
        return stdexec::sync_wait(schedule_after0_n).value();
    };
}
