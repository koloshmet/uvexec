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

#include <uvexec/sockets/tcp_listener.hpp>

#include <uvexec/algorithms/accept.hpp>

#include <exec/finally.hpp>
#include <exec/async_scope.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>

using namespace NUvExec;

namespace {

constexpr int READABLE_BUFFER_SIZE = 65536;

struct TTcpConnection {
    explicit TTcpConnection(TLoop& loop)
        : Socket(loop), Data(READABLE_BUFFER_SIZE), Buf(Data)
    {}

    TTcpConnection(TTcpConnection&&) noexcept = delete;

    TTcpSocket Socket;
    exec::async_scope Scope;
    std::vector<std::byte> Data;
    std::span<std::byte> Buf;
};

}

auto RootScope() -> exec::async_scope& {
    static exec::async_scope scope;
    return scope;
}

void UvExecEchoServer(int port) {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", port);
    TTcpListener listener(uvLoop, addr, 128);

    auto processConnection = [](TTcpConnection* connection) noexcept {
        return uvexec::read_until(connection->Socket, connection->Buf, [connection](std::size_t n) noexcept {
                    if (n == 0) {
                        return false;
                    }
                    auto tmpBuf = std::exchange(connection->Data, std::vector<std::byte>(READABLE_BUFFER_SIZE));
                    tmpBuf.resize(n);
                    connection->Buf = connection->Data;
                    connection->Scope.spawn(stdexec::just(std::move(tmpBuf))
                            | stdexec::let_value([connection](std::vector<std::byte>& tmpBuf) noexcept {
                                return uvexec::send(connection->Socket, std::span(tmpBuf))
                                        | stdexec::upon_error([&](NUvUtil::TUvError e) noexcept {
                                            fmt::println(std::cerr, "Server: Unable to response -> {}", ::uv_strerror(e));
                                        });
                            }), TLoop::TScheduler::TEnv(connection->Socket.Loop()));
                    return false;
                })
                | stdexec::then([](std::size_t) noexcept {})
                | stdexec::upon_error([&](NUvUtil::TUvError e) noexcept {
                    if (e != UV_EOF) {
                        fmt::println(std::cerr, "Server: Unable to process connection -> {}", ::uv_strerror(e));
                    }
                });
        };

    auto acceptConnection = [&](auto& spawnAccept) noexcept {
        auto connection = new TTcpConnection(uvLoop);
        return exec::finally(
                uvexec::accept(listener, connection->Socket)
                | stdexec::let_value([&, connection]() noexcept {
                    spawnAccept(spawnAccept);
                    return connection->Scope.nest(processConnection(connection));
                })
                | stdexec::let_value([connection]() noexcept {
                    return connection->Scope.on_empty();
                })
                | stdexec::upon_error([](auto e) noexcept {
                    if constexpr (std::same_as<decltype(e), NUvUtil::TUvError>) {
                        fmt::println(std::cerr, "Server: Unable to accept TCP connection -> {}", ::uv_strerror(e));
                    } else {
                        fmt::println(std::cerr, "Server: Unable to accept TCP connection -> ???");
                    }
                }),
                stdexec::just()
                | stdexec::let_value([connection]() noexcept {
                    connection->Scope.request_stop();
                    return connection->Scope.on_empty();
                })
                | uvexec::close(connection->Socket)
                | stdexec::then([connection]() noexcept {
                    delete connection;
                }));
    };

    auto spawnAccept = [&acceptConnection, &uvLoop](auto& self) noexcept -> void {
        RootScope().spawn(acceptConnection(self), TLoop::TScheduler::TEnv(uvLoop));
    };

    stdexec::sync_wait(
            stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::finally(
                        RootScope().nest(acceptConnection(spawnAccept))
                        | stdexec::let_value([&]() noexcept {
                            return RootScope().on_empty();
                        }),
                        stdexec::just()
                        | stdexec::let_value([&]() noexcept {
                            RootScope().request_stop();
                            return RootScope().on_empty();
                        })
                        | uvexec::close(listener));
            }));

    std::cerr.flush();
}

void UvExecEchoServerStop() {
    RootScope().request_stop();
}

void UvExecEchoClient(int port, int connections) {
    TLoop uvLoop;
    TTcpSocket socket(uvLoop);

    TIp4Addr dest("127.0.0.1", port);

    const char message[] = "Rise and shine";
    std::array<std::byte, sizeof(message) - 1> arr;
    std::memcpy(arr.data(), message, arr.size());

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([&dest]() noexcept { return std::ref(dest); })
            | uvexec::connect(socket)
            | stdexec::then([&]() noexcept { return std::span(arr); })
            | uvexec::send(socket)
            | stdexec::then([&]() noexcept { return std::span(arr); })
            | uvexec::receive(socket)
            | stdexec::then([](std::size_t) noexcept {})
            | uvexec::close(socket);

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
