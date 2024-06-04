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

#include <exec/finally.hpp>
#include <exec/async_scope.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>
#include <vector>
#include <array>

using namespace NUvExec;

namespace {

constexpr int READABLE_BUFFER_SIZE = 65536;

struct TcpConnection {
    explicit TcpConnection(uvexec::tcp_socket_t& socket)
        : Socket{&socket}, Data(READABLE_BUFFER_SIZE), Buf(Data), Received{0}, Sent{0}
    {}

    auto process_connection() noexcept {
        return uvexec::read_until(*Socket, Buf, [this](std::size_t n) noexcept {
            auto tmpBuf = std::exchange(Data, std::vector<std::byte>(READABLE_BUFFER_SIZE));
            tmpBuf.resize(n);
            Buf = Data;
            Scope.spawn(stdexec::just(std::move(tmpBuf))
                    | stdexec::let_value([this](std::vector<std::byte>& tmpBuf) noexcept {
                        return uvexec::send(*Socket, std::span(tmpBuf))
                                | stdexec::upon_error([&](NUvUtil::TUvError e) noexcept {
                                    fmt::println(std::cerr, "Server: Unable to response -> {}", ::uv_strerror(e));
                                });
                    }), uvexec::scheduler_t::TLoopEnv(Socket->Loop()));
            return false;
        })
        | stdexec::then([](std::size_t) noexcept {})
        | stdexec::upon_error([&](NUvUtil::TUvError e) noexcept {
            fmt::println(std::cerr, "Server: Unable to process connection -> {}", ::uv_strerror(e));
        });
    };

    auto process_connection_sequentially() noexcept {
        return uvexec::receive(*Socket, Buf)
                | stdexec::let_value([this](std::size_t n) noexcept {
                    Received += n;
                    return uvexec::send(*Socket, Buf.first(n))
                            | stdexec::then([n, this]() noexcept {
                                Sent += n;
                                return n == 0;
                            });
                })
                | stdexec::upon_error([this](auto e) noexcept {
                    if constexpr (std::same_as<decltype(e), NUvUtil::TUvError>) {
                        fmt::println(std::cerr, "Server: Unable to process connection -> {}", ::uv_strerror(e));
                    }
                    return true;
                })
                | stdexec::then([this](bool finish) noexcept {
                    if (!finish) {
                        spawn_process_connection_sequentially();
                    }
                });
    };

    void spawn_process_connection_sequentially() noexcept {
        Scope.spawn(process_connection_sequentially(), uvexec::scheduler_t::TLoopEnv(Socket->Loop()));
    }

    uvexec::tcp_socket_t* Socket;
    exec::async_scope Scope;
    std::vector<std::byte> Data;
    std::span<std::byte> Buf;
    std::size_t Received;
    std::size_t Sent;
};

auto RootScope() -> exec::async_scope& {
    static exec::async_scope scope;
    return scope;
}

void spawn_accept(uvexec::tcp_listener_t& listener) noexcept;

auto accept_connection(uvexec::tcp_listener_t& listener) noexcept {
    return uvexec::accept_from(listener, [&](uvexec::tcp_socket_t& socket) {
        spawn_accept(listener);
        auto connection = new TcpConnection(socket);
        return connection->Scope.nest(connection->process_connection_sequentially())
               | stdexec::let_value([connection]() {
                   return connection->Scope.on_empty();
               })
               | exec::finally(stdexec::just() | stdexec::then([connection]() noexcept {
                   delete connection;
               }));
    });
}

void spawn_accept(uvexec::tcp_listener_t& listener) noexcept {
    RootScope().spawn(
            accept_connection(listener) | stdexec::upon_error([](auto e) noexcept {
                if constexpr (std::same_as<decltype(e), NUvUtil::TUvError>) {
                    fmt::println(std::cerr, "Server: Unable to accept connection -> {}", ::uv_strerror(e));
                }
            }),
            uvexec::scheduler_t::TLoopEnv(listener.Loop()));
}

}



void UvExecEchoServer(int port) {
    uvexec::loop_t loop;

    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler())
            | stdexec::then([&port]() { return uvexec::ip_v4_addr_t("127.0.0.1", port); })
            | uvexec::bind_to([&](uvexec::tcp_listener_t& listener) {
                return RootScope().nest(accept_connection(listener))
                       | stdexec::let_value([&]() {
                           return RootScope().on_empty();
                       });
            }));

    std::cerr.flush();
}

void UvExecEchoServerStop() {
    RootScope().request_stop();
}

void UvExecEchoClient(int port, int connections) {
    uvexec::loop_t loop;

    const char message[] = "Rise and shine";
    std::array<std::byte, sizeof(message) - 1> arr;
    std::memcpy(arr.data(), message, arr.size());

    auto conn = stdexec::schedule(loop.get_scheduler())
            | stdexec::then([&port]() { return uvexec::ip_v4_addr_t("127.0.0.1", port); })
            | uvexec::connect_to([&](uvexec::tcp_socket_t& socket) noexcept {
                return uvexec::send(socket, std::span(arr))
                        | stdexec::then([&]() noexcept { return std::span(arr); })
                        | uvexec::receive(socket);
            });

    try {
        stdexec::sync_wait(conn).value();
    } catch (const std::exception& e) {
        fmt::println(std::cerr, "Unable to wakeup server, error: {}", e.what());
        throw;
    } catch (...) {
        fmt::println(std::cerr, "Unable to wakeup server");
        throw;
    }
}
