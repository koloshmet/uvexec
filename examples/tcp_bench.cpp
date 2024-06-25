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

#include <uvexec/uvexec.hpp>
#include <thread>

#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/std.h>


#define ATTR_NOINLINE __attribute__((noinline))

extern "C" {

void UvEchoServer(int port);
void UvEchoServerStop();
long long UvEchoClient(int port, int connections, int init_conn, const char* data, size_t data_len);

}

void UvExecEchoServer(int port);
void UvExecEchoServerStop();
auto UvExecEchoClient(int port, int connections, int init_conn, std::span<const char> payload) -> long long;

using namespace std::literals;

auto main(int argc, char* argv[]) -> int {
    constexpr int PORT = 1329;
    constexpr int DATA_LEN = 4 * 1000 * 1000; // K x 1M
    constexpr int CONNECTIONS = 4 * 1000;
    constexpr int IN_CONN = 128;
    std::vector<char> data(DATA_LEN, 'a');

    // UvExec server
    {
        std::thread serverThread([] { UvExecEchoServer(PORT); });

        std::this_thread::sleep_for(50ms);
        [&]() ATTR_NOINLINE {
            auto start = std::chrono::steady_clock::now();
            auto bytes_received = UvEchoClient(PORT, CONNECTIONS, IN_CONN, data.data(), data.size());
            fmt::println("Uv -> UvExec: transferred {}B in {}",
                    bytes_received,
                    std::chrono::ceil<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
        }();
        std::this_thread::sleep_for(50ms);

        [&]() ATTR_NOINLINE {
            auto start = std::chrono::steady_clock::now();
            auto bytes_received = UvExecEchoClient(PORT, CONNECTIONS, IN_CONN, data);
            fmt::println("UvExec -> UvExec: transferred {}B in {}",
                    bytes_received,
                    std::chrono::ceil<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
        }();

        std::this_thread::sleep_for(50ms);
        UvExecEchoServerStop();

        serverThread.join();
    }

    std::this_thread::sleep_for(100ms);

    // Raw UV server as reference value
    {
        std::thread serverThread([] { UvEchoServer(PORT); });

        std::this_thread::sleep_for(50ms);
        [&]() ATTR_NOINLINE -> void {
            auto start = std::chrono::steady_clock::now();
            auto bytes_received = UvEchoClient(PORT, CONNECTIONS, IN_CONN, data.data(), data.size());
            fmt::println("Uv -> Uv: transferred {}B in {}",
                    bytes_received,
                    std::chrono::ceil<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
        }();
        std::this_thread::sleep_for(50ms);

        [&]() ATTR_NOINLINE -> void {
            auto start = std::chrono::steady_clock::now();
            auto bytes_received = UvExecEchoClient(PORT, CONNECTIONS, IN_CONN, data);
            fmt::println("UvExec -> Uv: transferred {}B in {}",
                    bytes_received,
                    std::chrono::ceil<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
        }();
        std::this_thread::sleep_for(50ms);
        UvEchoServerStop();

        serverThread.join();
    }
}
