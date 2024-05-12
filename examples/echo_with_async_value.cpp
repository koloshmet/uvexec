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

#include <exec/async_scope.hpp>
#include <exec/when_any.hpp>

#include <iostream>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <vector>

using namespace std::literals;

namespace {

constexpr std::size_t READABLE_BUFFER_SIZE = 65536;

struct TcpConnection {
    explicit TcpConnection(uvexec::loop_t& loop)
        : Socket(loop), Data(READABLE_BUFFER_SIZE), Buf(Data)
    {}

    TcpConnection(TcpConnection&&) noexcept = delete;

    friend auto tag_invoke(uvexec::drop_t, TcpConnection& connection) noexcept {
        return stdexec::just() | stdexec::let_value([&connection]() noexcept {
            connection.Scope.request_stop();
            return connection.Scope.on_empty();
        }) | uvexec::close(connection.Socket);
    }

    auto process() noexcept {
        return uvexec::read_until(Socket, Buf, [this](std::size_t n) noexcept {
            if (n == 0) {
                return false;
            }
            auto sndBuf = std::exchange(Data, std::vector<std::byte>(READABLE_BUFFER_SIZE));
            sndBuf.resize(n);
            Buf = std::span(Data);
            spawn(stdexec::just(std::move(sndBuf))
                    | stdexec::let_value([this](std::vector<std::byte>& sndBuf) noexcept {
                        return uvexec::send(Socket, std::span(sndBuf))
                                | stdexec::upon_error([](NUvUtil::TUvError e) noexcept {
                                    fmt::println(std::cerr, "Server: Unable to response -> {}", ::uv_strerror(e));
                                });
                    }));
            return false;
        })
        | stdexec::then([](std::size_t) noexcept {})
        | stdexec::upon_error([](NUvUtil::TUvError e) noexcept {
            if (e != UV_EOF) {
                fmt::println(std::cerr, "Server: Unable to process connection -> {}", ::uv_strerror(e));
            }
        });
    }

    template <stdexec::sender Sender>
    void spawn(Sender&& sender) {
        Scope.spawn(std::forward<Sender>(sender), uvexec::scheduler_t::TLoopEnv(Socket.Loop()));
    }

    auto request_stop() noexcept {
        return Scope.request_stop();
    }

    uvexec::tcp_socket_t Socket;
    exec::async_scope Scope;
    std::vector<std::byte> Data;
    std::span<std::byte> Buf;
};

struct TcpServer {
    explicit TcpServer(uvexec::loop_t& loop, const uvexec::ip_v4_addr_t& endpoint)
        : Listener(loop, endpoint, 128)
    {}

    TcpServer(TcpServer&&) noexcept = delete;

    friend auto tag_invoke(uvexec::drop_t, TcpServer& server) noexcept {
        return stdexec::just() | stdexec::let_value([&server]() noexcept {
            server.Scope.request_stop();
            return server.Scope.on_empty();
        }) | uvexec::close(server.Listener);
    }

    auto accept_connection() noexcept {
        return stdexec::just(std::ref(Listener.Loop()))
                | uvexec::async_value([this](TcpConnection& connection) noexcept {
                    return uvexec::accept(Listener, connection.Socket)
                            | stdexec::let_value([&]() noexcept {
                                spawn_accept();
                                return connection.Scope.nest(connection.process());
                            })
                            | stdexec::let_value([&connection]() noexcept {
                                return connection.Scope.on_empty();
                            })
                            | stdexec::upon_error([](auto e) noexcept {
                                if constexpr (std::same_as<decltype(e), NUvUtil::TUvError>) {
                                    fmt::println(std::cerr,
                                            "Server: Unable to accept TCP connection -> {}", ::uv_strerror(e));
                                } else {
                                    fmt::println(std::cerr,
                                            "Server: Unable to accept TCP connection -> ???");
                                }
                            });
                });
    }

    void spawn_accept() noexcept {
        Scope.spawn(accept_connection(), uvexec::scheduler_t::TLoopEnv(Listener.Loop()));
    }

    auto request_stop() noexcept {
        return Scope.request_stop();
    }

    uvexec::tcp_listener_t Listener;
    exec::async_scope Scope;
};

}

auto main() -> int {
    uvexec::loop_t loop;
    uvexec::ip_v4_addr_t addr("127.0.0.1", 1329);

    stdexec::sync_wait(
            stdexec::schedule(loop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(
                        uvexec::upon_signal(SIGINT),
                        stdexec::just(std::ref(loop), std::ref(addr))
                        | uvexec::async_value([&](TcpServer& server) noexcept {
                            return server.Scope.nest(server.accept_connection())
                                    | stdexec::let_value([&server]() noexcept {
                                        return server.Scope.on_empty();
                                    });
                        }));
            }));
    return 0;
}
