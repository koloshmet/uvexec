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
#include <uvexec/sockets/udp.hpp>

#include <exec/async_scope.hpp>
#include <exec/when_any.hpp>
#include <exec/finally.hpp>

#include <latch>
#include <array>

using namespace NUvExec;
using namespace std::literals;

namespace {

constexpr int TEST_PORT{1329};

}

TEST_CASE("Close socket", "[loop][udp]") {
    TLoop uvLoop;
    TUdpSocket socket(uvLoop, TIp4Addr("127.0.0.1", TEST_PORT));

    stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | uvexec::close(socket));
}

TEST_CASE("No incoming requests", "[loop][udp]") {
    constexpr auto timeout = 50ms;

    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);
    TUdpSocket listener(uvLoop, addr);

    TIp4Addr peer;
    std::array<std::byte, 4> req{};
    std::span<std::byte> span(req);

    bool accepted{false};
    auto conn = exec::finally(
            uvexec::receive_from(listener, span, peer) | stdexec::then([&](std::size_t) noexcept { accepted = true; }),
            uvexec::close(listener));

    auto start = std::chrono::steady_clock::now();
    stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(exec::schedule_after(uvLoop.get_scheduler(), timeout), conn);
            })).value();

    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE_FALSE(accepted);
}

TEST_CASE("Ping pong", "[loop][udp]") {
    auto asciiDecode = [](std::span<const std::byte> encoded) noexcept {
        return std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    };

    bool pingReceived{false};

    std::thread serverThread([&]{
        TLoop uvLoop;
        TIp4Addr addr("127.0.0.1", TEST_PORT);
        TUdpSocket socket(uvLoop, addr);

        std::array<std::byte, 4> req{};

        TIp4Addr peer;

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::let_value([&]() noexcept {
                    return stdexec::just(std::span(req), std::ref(peer))
                            | uvexec::receive_from(socket)
                            | stdexec::let_value([&](std::size_t n) noexcept {
                                pingReceived = asciiDecode(req) == "Ping";
                                std::memcpy(req.data(), "Pong", 4);
                                return stdexec::just(std::span(req), peer);
                            })
                            | uvexec::send_to(socket)
                            | uvexec::close(socket);
                });
        stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;
    TUdpSocket socket(uvLoop, TIp4Addr("127.0.0.1", 0));

    std::array<std::byte, 4> arr;
    std::memcpy(arr.data(), "Ping", arr.size());

    TIp4Addr server("127.0.0.1", TEST_PORT);
    TIp4Addr peer;

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::let_value([&]() noexcept {
                    return stdexec::just(std::span(arr), server)
                            | uvexec::send_to(socket)
                            | stdexec::let_value([&]() noexcept {
                                return stdexec::just(std::span(arr), std::ref(peer));
                            })
                            | uvexec::receive_from(socket)
                            | stdexec::then([&](std::size_t n) noexcept {
                                REQUIRE(arr.size() == n);
                                REQUIRE(asciiDecode(arr) == "Pong");
                            })
                            | uvexec::close(socket);
                });
    std::this_thread::sleep_for(50ms);
    REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    serverThread.join();
    REQUIRE(pingReceived);
}
