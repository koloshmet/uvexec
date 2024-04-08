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
#include <catch2/catch.hpp>

#include <uvexec/execution/loop.hpp>

#include <exec/task.hpp>
#include <exec/async_scope.hpp>
#include <exec/when_any.hpp>
#include <exec/repeat_n.hpp>

#include <latch>


using namespace NUvExec;
using namespace std::literals;

TEST_CASE("Clock", "[loop][timer]") {
    STATIC_REQUIRE(std::same_as<TLoopClock::duration, std::chrono::milliseconds>);
}

TEST_CASE("Trivial after", "[loop][timer]") {
    constexpr auto timeout = 50ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();
    auto loopStart = exec::now(loop.get_scheduler());

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_after(loop.get_scheduler(), timeout)
            | stdexec::then([&]() noexcept {
                executed = true;
                return std::this_thread::get_id();
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE(loopStart + timeout <= exec::now(loop.get_scheduler()));
}

TEST_CASE("After 0", "[loop][timer]") {
    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_after(loop.get_scheduler(), 0s)
            | stdexec::then([&]() noexcept {
                executed = true;
                return std::this_thread::get_id();
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    CHECK(start + 1ms > std::chrono::steady_clock::now());
}

TEST_CASE("After negative", "[loop][timer]") {
    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_after(loop.get_scheduler(), -1s)
            | stdexec::then([&]() noexcept {
                executed = true;
                return std::this_thread::get_id();
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    CHECK(start + 1ms > std::chrono::steady_clock::now());
}

TEST_CASE("Trivial at", "[loop][timer]") {
    constexpr auto timeout = 50ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();
    auto loopStart = exec::now(loop.get_scheduler());

    auto alarm = exec::now(loop.get_scheduler()) + timeout;

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_at(loop.get_scheduler(), alarm)
            | stdexec::then([&]() noexcept {
                executed = true;
                return std::this_thread::get_id();
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE(loopStart + timeout <= exec::now(loop.get_scheduler()));
}

TEST_CASE("At in the past", "[loop][timer]") {
    constexpr auto delay = 100ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto missedAlarm = exec::now(loop.get_scheduler());

    std::this_thread::sleep_for(delay);

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_at(loop.get_scheduler(), missedAlarm)
            | stdexec::then([&]() noexcept {
                executed = true;
                return std::this_thread::get_id();
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    CHECK(start + delay > std::chrono::steady_clock::now());
}

TEST_CASE("Chained after", "[loop][timer]") {
    constexpr auto timeout = 30ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_after(loop.get_scheduler(), timeout)
            | stdexec::let_value([&]() noexcept {
                return exec::schedule_after(loop.get_scheduler(), timeout)
                        | stdexec::then([&]() noexcept {
                            executed = true;
                            return std::this_thread::get_id();
                        });
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    REQUIRE(start + 2 * timeout <= std::chrono::steady_clock::now() + 1ms);
}

TEST_CASE("At follows after", "[loop][timer]") {
    constexpr auto timeout = 50ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto alarm = exec::now(loop.get_scheduler()) + timeout;

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_after(loop.get_scheduler(), timeout)
            | stdexec::let_value([&]() noexcept {
                return exec::schedule_at(loop.get_scheduler(), alarm)
                        | stdexec::then([&]() noexcept {
                            executed = true;
                            return std::this_thread::get_id();
                        });
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE(start + 2 * timeout > std::chrono::steady_clock::now());
}

TEST_CASE("After follows at", "[loop][timer]") {
    constexpr auto timeout = 30ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto alarm = exec::now(loop.get_scheduler()) + timeout;

    auto start = std::chrono::steady_clock::now();

    bool executed{false};
    auto [innerThreadId] = stdexec::sync_wait(
            exec::schedule_at(loop.get_scheduler(), alarm)
            | stdexec::let_value([&]() noexcept {
                return exec::schedule_after(loop.get_scheduler(), timeout)
                        | stdexec::then([&]() noexcept {
                            executed = true;
                            return std::this_thread::get_id();
                        });
            })).value();

    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
    REQUIRE(start + 2 * timeout <= std::chrono::steady_clock::now() + 1ms);
}

TEST_CASE("When any", "[loop][timer]") {
    constexpr auto timeout = 50ms;

    TLoop loop;
    auto threadId = std::this_thread::get_id();

    auto start = std::chrono::steady_clock::now();

    int executed{0};
    int stopped{0};

    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                return exec::when_any(
                        exec::schedule_after(loop.get_scheduler(), timeout)
                                | stdexec::then([&]() noexcept {
                                    ++executed;
                                })
                                | stdexec::upon_stopped([&]() noexcept {
                                    ++stopped;
                                }),
                        exec::schedule_after(loop.get_scheduler(), 2 * timeout)
                                | stdexec::then([&]() noexcept {
                                    ++executed;
                                })
                                | stdexec::upon_stopped([&]() noexcept {
                                    ++stopped;
                                }));
            })).value();

    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE(start + 2 * timeout > std::chrono::steady_clock::now());
}

TEST_CASE("Timer cancelled before progress", "[loop][timer]") {
    TLoop loop;

    exec::async_scope scope;

    bool executed{false};
    scope.spawn(exec::schedule_after(loop.get_scheduler(), 0s)
            | stdexec::then([&]() noexcept {
                executed = true;
            }) | stdexec::upon_error([](auto) noexcept {}));

    scope.request_stop();
    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            })).value();

    REQUIRE_FALSE(executed);
}

TEST_CASE("Racy timer cancellation", "[loop][timer][mt]") {
    TLoop loop;

    exec::async_scope scope;
    std::latch barrier(2);

    std::thread t([&] {
        barrier.arrive_and_wait();
        scope.request_stop();
    });

    int executed{0};
    for (int i = 0; i < 100; ++i) {
        scope.spawn(
                exec::schedule_after(loop.get_scheduler(), 0s)
                | stdexec::then([&executed]() noexcept { ++executed; })
                | exec::repeat_n(100)
                | stdexec::upon_error([](auto) noexcept {}));
    }

    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                barrier.arrive_and_wait();
                return scope.on_empty();
            }));
    t.join();
    CHECK(executed > 0);
    CHECK(executed < 10'000);
}
