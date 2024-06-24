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
#include "stdexec/execution.hpp"
#include "uvexec/sockets/tcp_listener.hpp"
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <uvexec/algorithms/accept.hpp>
#include <uvexec/algorithms/after.hpp>
#include <uvexec/algorithms/schedule.hpp>
#include <uvexec/algorithms/connect_to.hpp>
#include <uvexec/algorithms/accept_from.hpp>
#include <uvexec/algorithms/bind_to.hpp>

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

    std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | uvexec::close(socket));
}

TEST_CASE("Bind and close listener", "[loop][tcp]") {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);
    TTcpListener listener(uvLoop, addr, 1);

    std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler()) | uvexec::close(listener));
}

TEST_CASE("Bind and close listener facade", "[loop][tcp]") {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);

    std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([&] { return addr; })
            | uvexec::bind_to([&](TTcpListener& listener) { return stdexec::just(); }));
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
    std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::let_value([&]() noexcept {
                return exec::when_any(exec::schedule_after(uvLoop.get_scheduler(), timeout), conn);
            })).value();

    REQUIRE(start + timeout <= std::chrono::steady_clock::now() + 1ms);
    REQUIRE_FALSE(accepted);
}

TEST_CASE("No incoming connection facade", "[loop][tcp]") {
    constexpr auto timeout = 50ms;

    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", TEST_PORT);

    bool accepted{false};
    auto conn = stdexec::just(addr) | uvexec::bind_to([&](TTcpListener& listener) {
                return uvexec::accept_from(listener, [&](TTcpSocket&) noexcept {
                    accepted = true;
                    return stdexec::just();
                });
            });

    auto start = std::chrono::steady_clock::now();
    std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
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
                | stdexec::let_value([&]() noexcept {
                    connectionAccepted = true;
                    return exec::when_any(
                            uvexec::after(timeout),
                            stdexec::just(std::ref(span))
                            | uvexec::read_until(socket, [](std::size_t) noexcept { return false; })
                            | stdexec::then([&](std::size_t n) noexcept {
                                if (n > 0) {
                                    dataReceived = true;
                                }
                            }));
                }),
                uvexec::close(socket) | uvexec::close(listener));

        std::ignore = stdexec::sync_wait(stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::let_value([&]() noexcept {
                    return conn;
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

    std::this_thread::sleep_for(50ms);
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

        std::ignore = stdexec::sync_wait(conn).value();
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

TEST_CASE("Ping pong facade", "[loop][tcp]") {
    auto asciiDecode = [](std::span<const std::byte> encoded) noexcept {
        return std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    };

    bool pingReceived{false};

    std::thread serverThread([&]{
        TLoop uvLoop;

        std::array<std::byte, 4> resp{};

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::then([]() {
                    return TIp4Addr("127.0.0.1", TEST_PORT);
                })
                | uvexec::bind_to([&](TTcpListener& listener) noexcept {
                    return uvexec::accept_from(listener, [&](TTcpSocket& socket) {
                        return uvexec::receive(socket, std::span(resp))
                               | stdexec::then([&](std::size_t n) noexcept {
                                    pingReceived = asciiDecode(resp) == "Ping";
                                    std::memcpy(resp.data(), "Pong", 4);
                                    return std::span(resp);
                                })
                               | uvexec::send(socket);
                    });
                });

        std::ignore = stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;

    std::array<std::byte, 4> arr;
    std::memcpy(arr.data(), "Ping", arr.size());

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([]() {
                return TIp4Addr("127.0.0.1", TEST_PORT);
            })
            | uvexec::connect_to([&](TTcpSocket& socket) noexcept {
                return uvexec::send(socket, std::span(arr))
                        | stdexec::then([&arr]() noexcept {
                            return std::span(arr);
                        })
                        | uvexec::receive(socket)
                        | stdexec::then([&](std::size_t n) noexcept {
                            REQUIRE(arr.size() == n);
                            REQUIRE(asciiDecode(arr) == "Pong");
                        });
            });

    std::this_thread::sleep_for(50ms);
    REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    serverThread.join();
    REQUIRE(pingReceived);
}

namespace {

template <typename TFn>
class TServer {
public:
    TServer(TTcpListener& listener, exec::async_scope& scope, TFn processor)
        : Scope(scope), Listener(listener), Processor(std::move(processor))
    {}

    auto AcceptConnection() {
        return uvexec::accept_from(Listener, [&](TTcpSocket& socket) {
            SpawnAccept();
            auto resp = new std::array<std::byte, 4>;
            return uvexec::receive(socket, std::span(*resp))
                   | stdexec::then([&, resp, this](std::size_t n) noexcept {
                       return Processor(std::span(*resp).first(n));
                   })
                   | uvexec::send(socket)
                   | stdexec::then([resp]() { delete resp; });
        });
    }

    void SpawnAccept() {
        Scope.spawn(
                AcceptConnection() | stdexec::upon_error([](auto e){ std::abort(); }),
                TLoop::TScheduler::TLoopEnv(Listener.Loop()));
    }

    auto Run() {
        return Scope.nest(AcceptConnection()) | stdexec::let_value([this]() {
            return Scope.on_empty();
        });
    }

    exec::async_scope& Scope;
    TTcpListener& Listener;
    TFn Processor;
};

}

TEST_CASE("Ping pong multi", "[loop][tcp]") {
    constexpr auto connections = 100;

    auto asciiDecode = [](std::span<const std::byte> encoded) noexcept {
        return std::string_view(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    };

    int pingReceived{0};
    exec::async_scope serverScope;

    std::thread serverThread([&]{
        TLoop uvLoop;

        std::array<std::byte, 4> resp{};

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::then([]() {
                    return TIp4Addr("127.0.0.1", TEST_PORT);
                })
                | uvexec::bind_to([&](TTcpListener& listener) noexcept {
                    auto server = new TServer(listener, serverScope, [&](std::span<std::byte> data) {
                        if (asciiDecode(data) == "Ping") {
                            ++pingReceived;
                            std::memcpy(data.data(), "Pong", 4);
                        }
                        return data;
                    });
                    return server->Run()
                            | stdexec::then([server]() {
                                delete server;
                            });
                });

        std::ignore = stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;
    exec::async_scope scope;

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([&]() {
                for (int i = 0; i < connections; ++i) {
                    scope.spawn(
                            uvexec::connect_to(TIp4Addr("127.0.0.1", TEST_PORT), [&](TTcpSocket& socket) noexcept {
                                auto arr = new std::array<std::byte, 4>;
                                std::memcpy(arr->data(), "Ping", arr->size());
                                return uvexec::send(socket, std::span(*arr))
                                        | stdexec::then([arr]() noexcept {
                                            return std::span(*arr);
                                        })
                                        | uvexec::receive(socket)
                                        | stdexec::then([&, arr](std::size_t n) noexcept {
                                            REQUIRE(arr->size() == n);
                                            REQUIRE(asciiDecode(*arr) == "Pong");
                                            delete arr;
                                        })
                                        | stdexec::upon_error([](auto e) noexcept {
                                            REQUIRE(false);
                                        });
                            })
                            | stdexec::upon_error([](auto e) noexcept {
                                REQUIRE(false);
                            }),
                            TLoop::TScheduler::TLoopEnv(uvLoop));
                }
            })
            | stdexec::let_value([&]() {
                return scope.on_empty();
            });

    std::this_thread::sleep_for(50ms);
    REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    serverScope.request_stop();
    serverThread.join();
    REQUIRE(pingReceived == connections);
}

namespace {

class TClientConnection {
public:
    TClientConnection(TTcpSocket& socket, std::span<std::byte> buff, std::size_t sent) noexcept
        : Socket{socket}
        , Buffer(buff)
        , DataLeft{static_cast<std::ptrdiff_t>(sent)}
    {}

    auto ReceiveData() {
        return uvexec::receive(Socket, Buffer) | stdexec::then([&](std::size_t rd) {
            DataLeft -= static_cast<std::ptrdiff_t>(rd);
            if (rd > 0 && DataLeft > 0) {
                SpawnReceive();
            }
        }) | stdexec::upon_error([](auto e) noexcept {});
    }

    void SpawnReceive() {
        Scope.spawn(ReceiveData(), TLoop::TScheduler::TLoopEnv(Socket.Loop()));
    }

    exec::async_scope Scope;
    TTcpSocket& Socket;
    std::span<std::byte> Buffer;
    std::ptrdiff_t DataLeft;
};

template <typename TFn>
class TEchoServerConnection {
public:
    TEchoServerConnection(TTcpSocket& socket, std::span<std::byte> buff, TFn process) noexcept
        : Socket{socket}, Buffer(buff), Process(std::move(process))
    {}

    auto ReceiveData() {
        return uvexec::receive(Socket, Buffer) | stdexec::let_value([&](std::size_t rd) {
            Process(Buffer.first(rd));
            return uvexec::send(Socket, Buffer.first(rd)) | stdexec::then([rd, this]() {
                if (rd > 0) {
                    SpawnReceive();
                }
            });
        }) | stdexec::upon_error([](auto e) noexcept { std::abort(); });
    }

    void SpawnReceive() {
        Scope.spawn(ReceiveData(), TLoop::TScheduler::TLoopEnv(Socket.Loop()));
    }

    exec::async_scope Scope;
    TTcpSocket& Socket;
    std::span<std::byte> Buffer;
    TFn Process;
};

}

TEST_CASE("Continuous transmission", "[loop][tcp]") {
    std::thread serverThread([&]{
        TLoop uvLoop;

        std::array<std::byte, 1000> arr;

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::then([]() {
                    return TIp4Addr("127.0.0.1", TEST_PORT);
                })
                | uvexec::bind_to([&](TTcpListener& listener) noexcept {
                    return uvexec::accept_from(listener, [&](TTcpSocket& socket) {
                        auto connection = new TEchoServerConnection(socket, arr, [](auto data) noexcept {
                            if (data.size() > 4) {
                                std::uint32_t i;
                                std::memcpy(&i, data.data(), 4);
                                if (i % 250 != 0) {
                                    std::abort();
                                }
                            }
                        });
                        return connection->Scope.nest(connection->ReceiveData())
                                | stdexec::let_value([&, connection]() {
                                    return connection->Scope.on_empty();
                                })
                                | stdexec::then([connection]() { delete connection; });
                    });
                });

        std::ignore = stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;

    auto data = std::make_unique<std::uint32_t[]>(25'000);
    std::span dataBuf(data.get(), 25'000);
    std::iota(dataBuf.begin(), dataBuf.end(), 0);

    std::array<std::byte, 1000> arr;

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([]() {
                return TIp4Addr("127.0.0.1", TEST_PORT);
            })
            | uvexec::connect_to([&](TTcpSocket& socket) noexcept {
                auto bytes = std::as_writable_bytes(dataBuf);
                auto connection = new TClientConnection(socket, arr, bytes.size());
                return uvexec::send(socket, bytes)
                        | stdexec::let_value([&, connection]() {
                            return connection->Scope.nest(connection->ReceiveData());
                        })
                        | stdexec::let_value([&, connection]() {
                            return connection->Scope.on_empty();
                        })
                        | stdexec::then([connection]() {
                            delete connection;
                        })
                        | stdexec::upon_error([](auto e) {
                            REQUIRE(false);
                        });
            });

    std::this_thread::sleep_for(50ms);
    REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    serverThread.join();
}

namespace {

template <typename TFn>
class TContinuousServer {
public:
    TContinuousServer(TTcpListener& listener, exec::async_scope& scope, TFn processor)
        : Scope(scope), Listener(listener), Processor(std::move(processor))
    {}

    auto AcceptConnection() {
        return uvexec::accept_from(Listener, [&](TTcpSocket& socket) {
            SpawnAccept();
            auto arr = new std::array<std::byte, 1000>;
            auto connection = new TEchoServerConnection(socket, *arr, [this](auto data) noexcept {
                Processor(data);
            });
            return connection->Scope.nest(connection->ReceiveData())
                    | stdexec::let_value([&, connection]() {
                        return connection->Scope.on_empty();
                    })
                    | stdexec::then([connection, arr]() { delete connection; delete arr; });
        });
    }

    void SpawnAccept() {
        Scope.spawn(
                AcceptConnection() | stdexec::upon_error([](auto e){ std::abort(); }),
                TLoop::TScheduler::TLoopEnv(Listener.Loop()));
    }

    auto Run() {
        return Scope.nest(AcceptConnection()) | stdexec::let_value([this]() {
            return Scope.on_empty();
        });
    }

    exec::async_scope& Scope;
    TTcpListener& Listener;
    TFn Processor;
};

}

TEST_CASE("Multiple continuous transmissions", "[loop][tcp]") {
    constexpr auto connections = 100;

    exec::async_scope serverScope;
    int validSegments{0};

    std::thread serverThread([&]{
        TLoop uvLoop;

        auto conn = stdexec::schedule(uvLoop.get_scheduler())
                | stdexec::then([]() {
                    return TIp4Addr("127.0.0.1", TEST_PORT);
                })
                | uvexec::bind_to([&](TTcpListener& listener) noexcept {
                    auto server = new TContinuousServer(listener, serverScope, [&](auto data) {
                        if (data.size() > 4) {
                            std::uint32_t i;
                            std::memcpy(&i, data.data(), 4);
                            if (i % 250 == 0) {
                                ++validSegments;
                            }
                        }
                        return data;
                    });
                    return server->Run()
                            | stdexec::then([server]() {
                                delete server;
                            });
                });

        std::ignore = stdexec::sync_wait(conn).value();
    });
    TLoop uvLoop;
    exec::async_scope scope;

    auto data = std::make_unique<std::uint32_t[]>(25'000);
    std::span dataBuf(data.get(), 25'000);
    std::iota(dataBuf.begin(), dataBuf.end(), 0);

    auto conn = stdexec::schedule(uvLoop.get_scheduler())
            | stdexec::then([&]() {
                for (int i = 0; i < connections; ++i) {
                    scope.spawn(
                        uvexec::connect_to(TIp4Addr("127.0.0.1", TEST_PORT), [&](TTcpSocket& socket) noexcept {
                            auto arr = new std::array<std::byte, 1000>;
                            auto connection = new TClientConnection(socket, *arr, dataBuf.size_bytes());
                            return uvexec::send(socket, std::as_bytes(dataBuf))
                                | stdexec::let_value([&, connection]() {
                                    return connection->Scope.nest(connection->ReceiveData());
                                })
                                | stdexec::let_value([&, connection]() {
                                    return connection->Scope.on_empty();
                                })
                                | stdexec::then([connection, arr]() {
                                    delete connection; delete arr;
                                });
                        }) | stdexec::upon_error([](auto e) { REQUIRE(false); }),
                        TLoop::TScheduler::TLoopEnv(uvLoop));
                }
            })
            | stdexec::let_value([&]() {
                return scope.on_empty();
            });

    std::this_thread::sleep_for(50ms);
    for (int i = 0; i < 10; ++i) {
        REQUIRE_NOTHROW(stdexec::sync_wait(conn).value());
    }
    serverScope.request_stop();
    serverThread.join();
    REQUIRE(validSegments == 100'000);
}
