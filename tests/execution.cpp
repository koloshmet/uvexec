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

#include <exec/single_thread_context.hpp>
#include <exec/task.hpp>
#include <exec/async_scope.hpp>

#include <latch>


using namespace NUvExec;
using namespace std::literals;

TEST_CASE("Trivial Loop", "[loop]") {
    TLoop uvLoop;
    auto threadId = std::this_thread::get_id();
    bool executed{false};

    auto boring = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&executed]{
        executed = true;
        return std::this_thread::get_id();
    });
    REQUIRE_FALSE(executed);

    auto [innerThreadId] = stdexec::sync_wait(boring).value();
    REQUIRE(threadId == innerThreadId);
    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
}

TEST_CASE("Reuse Loop", "[loop]") {
    TLoop uvLoop;
    int executed{0};

    auto boring = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&executed]{
        ++executed;
    });

    REQUIRE(executed == 0);
    int n = 101;
    for (int i = 0; i < n; ++i) {
        stdexec::sync_wait(boring).value();
    }
    REQUIRE(executed == n);
}

TEST_CASE("Concurrent schedule", "[loop]") {
    TLoop uvLoop;
    int executedA{0};
    int executedB{0};

    auto jobA = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&executedA]{
        ++executedA;
    });
    auto jobB = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&executedB]{
        ++executedB;
    });

    REQUIRE(executedA == executedB);
    REQUIRE(executedA == 0);

    exec::async_scope root;
    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                root.spawn(jobA);
                root.spawn(jobB);
                return root.on_empty();
            }));

    REQUIRE(executedA == executedB);
    REQUIRE(executedA == 1);
}

TEST_CASE("High concurrent schedule", "[loop]") {
    TLoop uvLoop;

    int n = GENERATE(6, 17, 25, 32);

    std::vector<int> executed(n, 0);

    auto job = stdexec::then([&executed](int i) noexcept {
        ++executed[i];
    });

    REQUIRE(std::all_of(executed.begin(), executed.end(), [](int i) noexcept { return i == 0; }));

    exec::async_scope root;
    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                for (int i = 0; i < n; ++i) {
                    root.spawn(stdexec::transfer_just(uvLoop.get_scheduler(), i) | job);
                }
                return root.on_empty();
            }));

    REQUIRE(std::all_of(executed.begin(), executed.end(), [](int i) noexcept { return i == 1; }));
}

TEST_CASE("Cancelled before progress", "[loop]") {
    TLoop loop;

    exec::async_scope scope;

    bool executed{false};
    scope.spawn(stdexec::schedule(loop.get_scheduler())
            | stdexec::then([&]() noexcept {
                executed = true;
            }));

    scope.request_stop();
    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            })).value();

    REQUIRE_FALSE(executed);
}

TEST_CASE("Transfer to Loop", "[loop][mt]") {
    TLoop uvLoop;
    exec::single_thread_context ctx;

    auto threadId = std::this_thread::get_id();

    auto transfer = stdexec::schedule(ctx.get_scheduler()) | stdexec::then([&]{
        REQUIRE(threadId != std::this_thread::get_id());
    }) | stdexec::transfer(uvLoop.get_scheduler()) | stdexec::then([&]{
        REQUIRE(threadId == std::this_thread::get_id());
    });

    stdexec::sync_wait(transfer).value();
    REQUIRE(threadId == std::this_thread::get_id());
}

TEST_CASE("Parallel schedule", "[loop][mt]") {
    constexpr int iterations = 1000;

    TLoop uvLoop;
    int counter{0};

    exec::async_scope scope;
    std::latch barrier(2);
    auto routine = [&]() {
        barrier.arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
            scope.spawn(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
                ++counter;
            }));
        }
    };
    std::thread t(routine);
    routine();
    t.join();
    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            }));

    REQUIRE(counter == 2 * iterations);
}

TEST_CASE("High parallel schedule", "[loop][mt]") {
    constexpr int threadsCount = 8;
    constexpr int iterations = 1000;

    TLoop uvLoop;
    int counter{0};

    exec::async_scope scope;
    std::latch barrier(threadsCount);
    auto routine = [&]() {
        barrier.arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
            scope.spawn(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
                ++counter;
            }));
        }
    };
    std::vector<std::thread> threads;
    threads.reserve(threadsCount - 1);
    for (std::size_t i = 0; i < threads.capacity(); ++i) {
        threads.emplace_back(routine);
    }
    routine();
    for (auto&& t : threads) {
        t.join();
    }
    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            }));

    REQUIRE(counter == threadsCount * iterations);
}

TEST_CASE("Parallel sync_wait", "[loop][mt]") {
    constexpr int iterations = 1000;

    TLoop uvLoop;
    int counter{0};

    std::latch barrier(2);
    auto routine = [&]() {
        barrier.arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
            stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
                ++counter;
            }));
        }
    };
    std::thread t(routine);
    routine();
    t.join();

    REQUIRE(counter == 2 * iterations);
}

TEST_CASE("High parallel sync_wait", "[loop][mt]") {
    constexpr int threadsCount = 8;
    constexpr int iterations = 1000;

    TLoop uvLoop;
    int counter{0};

    std::latch barrier(threadsCount);
    auto routine = [&]() {
        barrier.arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
            stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
                ++counter;
            }));
        }
    };
    std::vector<std::thread> threads;
    threads.reserve(threadsCount - 1);
    for (std::size_t i = 0; i < threads.capacity(); ++i) {
        threads.emplace_back(routine);
    }
    routine();
    for (auto&& t : threads) {
        t.join();
    }

    REQUIRE(counter == threadsCount * iterations);
}

TEST_CASE("Concurrent run and sync_wait", "[loop][mt]") {
    constexpr int iterations = 1000;

    TLoop uvLoop;
    int counter{0};

    std::latch barrier(2);
    auto routine = [&]() {
        barrier.arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
            stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
                ++counter;
            }));
        }
        std::this_thread::sleep_for(50ms);
        stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | stdexec::then([&]() noexcept {
            uvLoop.finish();
        }));
    };
    std::thread t(routine);
    barrier.arrive_and_wait();
    uvLoop.run();
    t.join();

    REQUIRE(counter == iterations);
}

TEST_CASE("Coroutines", "[loop][coro]") {
    TLoop uvLoop;

    std::size_t cnt{0};
    uv_idle_t idler; // TODO: Make proper debug/test utility
    ::uv_idle_init(&NUvUtil::RawUvObject(uvLoop), &idler);
    idler.data = &cnt;
    ::uv_idle_start(&idler, [](uv_idle_t* handle){
        auto& cnt = *static_cast<std::size_t*>(handle->data);
        ++cnt;
    });

    auto task = [](int n) -> exec::task<void> {
        while (n > 0) {
            auto _ = co_await stdexec::get_scheduler();
            --n;
        }
    };
    int n = GENERATE(6, 13, 57);

    SECTION("Simple coroutine scheduler propagation") {
        auto transfer = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::let_value([&]() noexcept {
            return task(n);
        });
        stdexec::sync_wait(transfer).value();
        REQUIRE(cnt == n + 1);
    }

    SECTION("Nested coroutine scheduler propagation") {
        auto outerTask = [&task](int n) -> exec::task<void> {
            for (int i = 0; i < n; ++i) {
                co_await task(n);
            }
        };

        auto transfer = stdexec::schedule(uvLoop.get_scheduler()) | stdexec::let_value([&]() noexcept {
            return outerTask(n);
        });
        stdexec::sync_wait(transfer).value();
        REQUIRE(cnt == n * (n + 1) + 1);
    }
    NUvUtil::Close(idler);
    uvLoop.run_once(); // Error idle closing
}
