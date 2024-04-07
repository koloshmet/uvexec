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
#include <uvexec/execution/loop.hpp>
#include <uvexec/algorithms/accept.hpp>
#include <uvexec/algorithms/async_value.hpp>

#include <uvexec/util/lazy.hpp>

#include <exec/async_scope.hpp>
#include <exec/finally.hpp>
#include <exec/when_any.hpp>

#include <iostream>
#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace NUvExec;
using namespace std::literals;

namespace {

constexpr std::size_t READABLE_BUFFER_SIZE = 65536;

struct TTcpConnection {
    explicit TTcpConnection(TLoop& loop)
        : Socket(loop), Data(READABLE_BUFFER_SIZE), Buf(Data)
    {}

    TTcpConnection(TTcpConnection&&) noexcept = delete;

    friend auto tag_invoke(uvexec::drop_t, TTcpConnection& connection) noexcept {
        return stdexec::let_value(stdexec::just(), [&connection]() noexcept {
            connection.Scope.request_stop();
            return connection.Scope.on_empty() | uvexec::close(connection.Socket);
        });
    }

    auto Process() noexcept {
        return uvexec::read_until(Socket, Buf, [this](std::size_t n) noexcept {
            if (n == 0) {
                return false;
            }
            auto tmpBuf = std::exchange(Data, std::vector<std::byte>(READABLE_BUFFER_SIZE));
            tmpBuf.resize(n);
            Buf = std::span(Data);
            Spawn(stdexec::just(std::move(tmpBuf))
                    | stdexec::let_value([this](std::vector<std::byte>& tmpBuf) noexcept {
                        return uvexec::send(Socket, std::span(tmpBuf))
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

    template <stdexec::sender TSender>
    void Spawn(TSender&& sender) {
        Scope.spawn(std::forward<TSender>(sender), TLoop::TScheduler::TEnv(Socket.Loop()));
    }

    auto request_stop() noexcept {
        return Scope.request_stop();
    }

    TTcpSocket Socket;
    exec::async_scope Scope;
    std::vector<std::byte> Data;
    std::span<std::byte> Buf;
};

struct TTcpServer {
    explicit TTcpServer(TLoop& loop, const TIp4Addr& endpoint)
        : Listener(loop, endpoint, 128)
    {}

    TTcpServer(TTcpServer&&) noexcept = delete;

    friend auto tag_invoke(uvexec::drop_t, TTcpServer& server) noexcept {
        return stdexec::let_value(stdexec::just(), [&server]() noexcept {
            server.Scope.request_stop();
            return server.Scope.on_empty() | uvexec::close(server.Listener);
        });
    }

    auto AcceptConnection() noexcept {
        return stdexec::just(std::ref(Listener.Loop()))
                | uvexec::async_value([this](TTcpConnection& connection) noexcept {
                    return uvexec::accept(Listener, connection.Socket)
                            | stdexec::let_value([&]() noexcept {
                                SpawnAccept();
                                return connection.Scope.nest(connection.Process());
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

    void SpawnAccept() noexcept {
        Scope.spawn(AcceptConnection(), TLoop::TScheduler::TEnv(Listener.Loop()));
    }

    auto request_stop() noexcept {
        return Scope.request_stop();
    }

    TTcpListener Listener;
    exec::async_scope Scope;
};

}

int main() {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", 1329);

    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(
                        uvexec::schedule_upon_signal(uvLoop.get_scheduler(), SIGINT),
                        stdexec::just(std::ref(uvLoop), std::ref(addr))
                        | uvexec::async_value([&](TTcpServer& server) noexcept {
                            return server.Scope.nest(server.AcceptConnection())
                                    | stdexec::let_value([&server]() noexcept {
                                        return server.Scope.on_empty();
                                    });
                        }));
            }));
    return 0;
}
