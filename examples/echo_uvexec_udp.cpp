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

constexpr std::size_t READABLE_BUFFER_SIZE = 65'507;

struct UdpServer {
    explicit UdpServer(uvexec::loop_t& loop, const uvexec::ip_v4_addr_t& endpoint)
        : Listener(loop, endpoint)
    {}

    UdpServer(UdpServer&&) noexcept = delete;

    friend auto tag_invoke(uvexec::drop_t, UdpServer& server) noexcept {
        return stdexec::just() | stdexec::let_value([&server]() noexcept {
            server.Scope.request_stop();
            return server.Scope.on_empty();
        }) | uvexec::close(server.Listener);
    }

    auto accept_datagram() noexcept {
        return stdexec::just(std::vector<std::byte>(READABLE_BUFFER_SIZE), uvexec::ip_v4_addr_t{})
                | stdexec::let_value([this](std::vector<std::byte>& data, uvexec::ip_v4_addr_t& peer) noexcept {
                    return uvexec::receive_from(Listener, std::span(data), peer)
                            | stdexec::let_value([&](std::size_t n) noexcept {
                                spawn_accept();
                                return stdexec::just(std::span(data).first(n), std::ref(peer));
                            })
                            | uvexec::send_to(Listener)
                            | stdexec::upon_error([&](auto e) noexcept {
                                if constexpr (std::same_as<decltype(e), NUvUtil::TUvError>) {
                                    spawn_accept();
                                    if (e != UV_EOF) {
                                        fmt::println(std::cerr,
                                                "Server: Unable to accept UDP datagram -> {}", ::uv_strerror(e));
                                    }
                                } else {
                                    fmt::println(std::cerr,
                                            "Server: Unable to accept UDP datagram -> ???");
                                }
                            });
                });
    }

    void spawn_accept() noexcept {
        Scope.spawn(accept_datagram(), uvexec::scheduler_t::TEnv(Listener.Loop()));
    }

    auto request_stop() noexcept {
        return Scope.request_stop();
    }

    uvexec::udp_socket_t Listener;
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
                        | uvexec::async_value([&](UdpServer& server) noexcept {
                            return server.Scope.nest(server.accept_datagram())
                                    | stdexec::let_value([&server]() noexcept {
                                        return server.Scope.on_empty();
                                    });
                        }));
            }));
    return 0;
}
