# UVEXEC - A Senders API for Uv Asynchronous Loop

`uvexec` is an implementation of the _Senders_ api proposed by [**P2300**](http://wg21.link/p2300) for [**libuv**](https://github.com/libuv/libuv)🦄🦖

[![CI](https://github.com/koloshmet/uvexec/actions/workflows/ci.yml/badge.svg)](https://github.com/koloshmet/uvexec/actions/workflows/ci.yml)

## Overview
Uvexec provides a few major components:

- A thread-safe [event loop](examples/schedule_bench.cpp).
- Oneshot timers and signal handlers.
- Asynchronous [TCP](examples/echo_with_async_value.cpp) and [UDP](examples/echo_uvexec_udp.cpp) sockets 
with an api similar to the one described in [P2762](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2762r2.pdf).

Uvexec supports all the platforms that libuv does and can be used to build cross-platform asynchronous applications
using modern C++

## Example

Below is a simple server that accepts single connection, echoes it and stops.

```c++
#include <uvexec/uvexec.hpp>
#include <array>

int main() {
    uvexec::loop_t loop;
    uvexec::ip_v4_addr_t addr("127.0.0.1", 1329);
    uvexec::tcp_listener_t listener(loop, addr, 1);

    uvexec::tcp_socket_t socket(loop);
    std::array<std::byte, 1024> data{}; // 1KB buffer

    auto server = stdexec::schedule(loop.get_scheduler())
            | uvexec::accept(listener, socket) // Accepts connection from listener
            | stdexec::then([&]() noexcept {
                return std::span(data); // Passes buffer to receive
            })
            | uvexec::receive(socket) // Reads from socket to buffer
            | stdexec::then([&](std::size_t n) noexcept {
                return std::span(data).first(n); // Passes buffer to send
            })
            | uvexec::send(socket) // Replies back
            | uvexec::close(socket) // Closes connection
            | uvexec::close(listener); // Stops accepting new connection

    stdexec::sync_wait(server).value(); // Runs loop
    return 0;
}
```

## How to get it

#### Without CPM
Clone it into your project, add uvexec root as subdirectory to your cmake project and link `uvexec::uvexec` to your target 


#### Using CPM

```cmake
cmake_minimum_required(VERSION 3.22.1)

project(UvexecExample)

include(CPM.cmake)

CPMAddPackage(
    NAME uvexec
    GITHUB_REPOSITORY koloshmet/uvexec
    GIT_TAG main
)

add_executable(example example.cpp)
target_link_libraries(example uvexec::uvexec)
```

## Requirements

C++20

#### Supported compilers:
- Clang 14+
- GCC 11+
- Apple Clang 15+
- MSVC 19.38+

It may compile on earlier versions of compilers, but hasn't been tested on them.
This is especially true for MSVC
