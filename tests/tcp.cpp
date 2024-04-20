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
#include <uvexec/sockets/tcp_listener.hpp>
#include <uvexec/algorithms/accept.hpp>

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

TEST_CASE("Close socket", "[loop][tcp]") {
    TLoop uvLoop;
    TTcpSocket socket(uvLoop);

    stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | uvexec::close(socket));
}

TEST_CASE("Bind and close listener", "[loop][tcp]") {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);
    TTcpListener listener(uvLoop, addr, 1);

    stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | uvexec::close(listener));
}

TEST_CASE("No incoming connection", "[loop][tcp]") {
    constexpr auto timeout = 50ms;

    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);
    TTcpListener listener(uvLoop, addr, 1);

    TTcpSocket socket(uvLoop);

    bool accepted{false};
    auto conn = exec::finally(
            uvexec::accept(listener, socket) | stdexec::then([&]() noexcept { accepted = true; }),
            uvexec::close(socket) | uvexec::close(listener));

    auto start = std::chrono::steady_clock::now();
    stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(exec::schedule_after(uvLoop.get_scheduler(), timeout), conn);
            })).value();

    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE_FALSE(accepted);
}

TEST_CASE("No data to read_until", "[loop][tcp]") {
    constexpr auto timeout = 50ms;

    bool dataReceived{false};
    bool connectionAccepted{false};

    std::thread serverThread([&]{
        TLoop uvLoop;
        TIp4Addr addr("127.0.0.1", TEST_PORT);
        TTcpListener listener(uvLoop, addr, 1);

        std::array<std::byte, 4> resp{};
        std::span<std::byte> span(resp);

        TTcpSocket socket(uvLoop);

        auto conn = exec::finally(
                uvexec::accept(listener, socket)
                | stdexec::then([&]() noexcept {
                    connectionAccepted = true;
                    return std::ref(span);
                })
                | uvexec::read_until(socket, [](std::size_t) noexcept { return false; })
                | stdexec::then([&](std::size_t n) noexcept {
                    dataReceived = true;
                }),
                uvexec::close(socket) | uvexec::close(listener));

        stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::let_value([&]() noexcept {
                    return exec::when_any(exec::schedule_after(uvLoop.get_scheduler(), timeout), conn);
                })).value();
    });
    TLoop uvLoop;
    TTcpSocket socket(uvLoop);
    TIp4Addr addr("127.0.0.1", TEST_PORT);

    bool connected{false};

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return uvexec::connect(socket, addr)
                    | stdexec::then([&]() noexcept {
                        connected = true;
                        std::this_thread::sleep_for(timeout);
                    })
                    | exec::finally(uvexec::close(socket));
            });

    std::this_thread::sleep_for(10ms);
    std::ignore = stdexec::sync_wait(conn).value();
    serverThread.join();
    REQUIRE(connected);
    REQUIRE(connectionAccepted);
    REQUIRE_FALSE(dataReceived);
}

TEST_CASE("Ping pong", "[loop][tcp]") {
    auto asciiDecode = [](std::span<const std::byte> encoded) noexcept {
        return std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    };

    bool pingReceived{false};

    std::thread serverThread([&]{
        TLoop uvLoop;
        TIp4Addr addr("127.0.0.1", TEST_PORT);
        TTcpListener listener(uvLoop, addr, 1);

        std::array<std::byte, 4> resp{};

        TTcpSocket socket(uvLoop);

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | uvexec::accept(listener, socket)
                | stdexec::then([&resp]() noexcept {
                    return std::span(resp);
                })
                | uvexec::receive(socket)
                | stdexec::then([&](std::size_t n) noexcept {
                    pingReceived = asciiDecode(resp) == "Ping";
                    std::memcpy(resp.data(), "Pong", 4);
                    return std::span(resp);
                })
                | uvexec::send(socket)
                | uvexec::close(socket)
                | uvexec::close(listener);

        stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;
    TTcpSocket socket(uvLoop);

    TIp4Addr addr("127.0.0.1", TEST_PORT);

    std::array<std::byte, 4> arr;
    std::memcpy(arr.data(), "Ping", arr.size());

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::then([&addr]() noexcept {
                    return std::ref(addr);
                })
                | uvexec::connect(socket)
                | stdexec::then([&arr]() noexcept {
                    return std::span(arr);
                })
                | uvexec::send(socket)
                | stdexec::then([&arr]() noexcept {
                    return std::span(arr);
                })
                | uvexec::receive(socket)
                | stdexec::then([&](std::size_t n) noexcept {
                    REQUIRE(arr.size() == n);
                    REQUIRE(asciiDecode(arr) == "Pong");
                })
                | uvexec::close(socket);

    std::this_thread::sleep_for(50ms);
    REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    serverThread.join();
    REQUIRE(pingReceived);
}
