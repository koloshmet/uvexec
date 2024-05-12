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
#include <uvexec/algorithms/upon_signal.hpp>

#include <exec/task.hpp>
#include <exec/async_scope.hpp>

#include <csignal>


using namespace NUvExec;
using namespace std::literals;

TEST_CASE("Raise SIGINT", "[loop][signal]") {
    constexpr auto signal = SIGINT;

    TLoop loop;
    exec::async_scope scope;
    auto threadId = std::this_thread::get_id();

    bool executed{false};
    scope.spawn(
            uvexec::schedule_upon_signal(loop.get_scheduler(), signal)
            | stdexec::then([&]() noexcept {
                executed = true;
                REQUIRE(threadId == std::this_thread::get_id());
            }) | stdexec::upon_error([](auto) noexcept {}));

    stdexec::sync_wait(stdexec::schedule(loop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                std::raise(signal);
                return scope.on_empty();
            })).value();

    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
}

TEST_CASE("Facade raise SIGINT", "[loop][signal]") {
    constexpr auto signal = SIGINT;

    TLoop loop;
    exec::async_scope scope;
    auto threadId = std::this_thread::get_id();

    bool executed{false};
    scope.spawn(
            stdexec::schedule(loop.get_scheduler())
            | uvexec::upon_signal(signal)
            | stdexec::then([&]() noexcept {
                executed = true;
                REQUIRE(threadId == std::this_thread::get_id());
            }) | stdexec::upon_error([](auto) noexcept {}));

    stdexec::sync_wait(stdexec::schedule(loop.get_scheduler())
                       | stdexec::let_value([&]() noexcept {
        std::raise(signal);
        return scope.on_empty();
    })).value();

    REQUIRE(threadId == std::this_thread::get_id());
    REQUIRE(executed);
}

TEST_CASE("Signal cancelled before progress", "[loop][signal]") {
    constexpr auto signal = SIGINT;

    TLoop loop;

    exec::async_scope scope;

    bool executed{false};
    scope.spawn(uvexec::schedule_upon_signal(loop.get_scheduler(), signal)
            | stdexec::then([&]() noexcept {
                executed = true;
            }) | stdexec::upon_error([](auto) noexcept {}));

    scope.request_stop();
    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            })).value();

    REQUIRE(!executed);
}

TEST_CASE("Facade signal cancelled before progress", "[loop][signal]") {
    constexpr auto signal = SIGINT;

    TLoop loop;

    exec::async_scope scope;

    bool executed{false};
    scope.spawn(stdexec::schedule(loop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return uvexec::upon_signal(signal);
            })
            | stdexec::then([&]() noexcept { executed = true; })
            | stdexec::upon_error([](auto) noexcept {}));

    scope.request_stop();
    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler()) | stdexec::let_value([&]() noexcept {
                return scope.on_empty();
            })).value();

    REQUIRE(!executed);
}
