# UVEXEC - A Senders API for Uv Asynchronous Loop

`uvexec` is an implementation of the _Senders_ api proposed by [**P2300**](http://wg21.link/p2300) for [libuv](https://github.com/libuv/libuv)

## Example

Below is a simple server that accepts single connection, echoes it and stops.

```c++
#include <uvexec/execution/loop.hpp>
#include <uvexec/sockets/tcp_listener.hpp>
#include <uvexec/algorithms/accept.hpp>
#include <array>

using namespace NUvExec;

int main() {
    TLoop uvLoop;
    TIp4Addr addr("127.0.0.1", 1329);
    TTcpListener listener(uvLoop, addr, 1);

    TTcpSocket socket(uvLoop);
    std::array<std::byte, 1024> data{}; // 1KB buffer

    auto server = stdexec::schedule(uvLoop.get_scheduler())
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

## Requirements

C++20

Supported compilers:
- Clang 14+
- GCC 11+
- Apple Clang 15+
