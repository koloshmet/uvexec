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

#include <uvexec/algorithms/async_value.hpp>
#include <uvexec/algorithms/after.hpp>


#include <exec/async_scope.hpp>
#include <exec/when_any.hpp>
#include <exec/finally.hpp>


using namespace std::literals;

namespace {

struct TS {
    explicit TS(bool& destroyed) noexcept: Destroyed{&destroyed} {}
    TS(TS&& s) noexcept: Destroyed{s.Destroyed} {
        s.Owner = false;
    }
    ~TS() {
        if (Owner) {
            *Destroyed = true;
        }
    }

    friend auto tag_invoke(uvexec::drop_t, TS&) noexcept {
        return stdexec::just();
    }

    bool* Destroyed;
    bool Owner{true};
};

}

TEST_CASE("Let value doesn't destroy", "[async_value]") {
    stdexec::run_loop loop;
    bool destroyed{false};
    stdexec::start_detached(
            stdexec::let_value(stdexec::transfer_just(loop.get_scheduler(), TS(destroyed)), [&](TS&) noexcept {
                REQUIRE(!destroyed);
                return stdexec::just();
            }) | stdexec::then([&]() noexcept {
                REQUIRE(!destroyed);
                loop.finish();
            }));
    loop.run();
    REQUIRE(destroyed);
}

TEST_CASE("Async value destroys", "[async_value]") {
    stdexec::run_loop loop;
    bool destroyed{false};
    stdexec::start_detached(
            uvexec::async_value(stdexec::transfer_just(loop.get_scheduler(), TS(destroyed)), [&](TS&) noexcept {
                REQUIRE(!destroyed);
                return stdexec::just();
            }) | stdexec::then([&]() noexcept {
                REQUIRE(destroyed);
                loop.finish();
            }));
    loop.run();
    REQUIRE(destroyed);
}

namespace uvexec {

auto tag_invoke(uvexec::drop_t, exec::async_scope& scope) noexcept {
    return stdexec::let_value(stdexec::just(), [&scope]() noexcept {
        scope.request_stop();
        return scope.on_empty();
    });
}

}

TEST_CASE("Async scope cancellation", "[async_value]") {
    NUvExec::TLoop loop;

    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(
                        stdexec::schedule(loop.get_scheduler()),
                        uvexec::async_value(stdexec::just(), [&](exec::async_scope& scope) noexcept {
                            return scope.nest(exec::schedule_after(loop.get_scheduler(), 1s))
                                    | stdexec::let_value([&]() noexcept {
                                        return scope.on_empty();
                                    });
                        }));
            }));
}
