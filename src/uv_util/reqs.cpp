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
#include <uvexec/uv_util/reqs.hpp>

#include <uvexec/uv_util/safe_uv.hpp>


namespace NUvUtil {

auto Fire(uv_async_t& req) -> TUvError {
    return ::uv_async_send(&req);
}

auto Now(const uv_loop_t& loop) -> std::uint64_t {
    return ::uv_now(&loop);
}

auto TimerStart(uv_timer_t& req, uv_timer_cb cb, std::uint64_t timeout, std::uint64_t repeat) -> TUvError {
    return ::uv_timer_start(&req, cb, timeout, repeat);
}

auto TimerStop(uv_timer_t& req) -> TUvError {
    return ::uv_timer_stop(&req);
}

auto SignalOnce(uv_signal_t& req, uv_signal_cb cb, int signum) -> TUvError {
    return ::uv_signal_start_oneshot(&req, cb, signum);
}

auto SignalStop(uv_signal_t& req) -> TUvError {
    return ::uv_signal_stop(&req);
}

auto Bind(uv_tcp_t& tcp, const sockaddr_in& addr) -> TUvError {
    return ::UvTcpInBind(&tcp, &addr);
}

auto Bind(uv_tcp_t& tcp, const sockaddr_in6& addr) -> TUvError {
    return ::UvTcpIn6Bind(&tcp, &addr);
}

auto Accept(uv_tcp_t& server, uv_tcp_t& client) -> TUvError {
    return ::UvTcpAccept(&server, &client);
}

auto Connect(uv_connect_t& req, uv_tcp_t& tcp, const sockaddr_in& addr, uv_connect_cb cb) -> TUvError {
    return ::UvTcpInConnect(&req, &tcp, &addr, cb);
}

auto Connect(uv_connect_t& req, uv_tcp_t& tcp, const sockaddr_in6& addr, uv_connect_cb cb) -> TUvError {
    return ::UvTcpIn6Connect(&req, &tcp, &addr, cb);
}

auto Shutdown(uv_shutdown_t& req, uv_tcp_t& tcp, uv_shutdown_cb cb) -> TUvError {
    return ::UvTcpShutdown(&req, &tcp, cb);
}

auto CloseReset(uv_tcp_t& handle, uv_close_cb cb) -> TUvError {
    return ::uv_tcp_close_reset(&handle, cb);
}

auto ReadStart(uv_tcp_t& tcp, uv_alloc_cb acb, uv_read_cb rcb) -> TUvError {
    return ::UvTcpReadStart(&tcp, acb, rcb);
}

auto ReadStop(uv_tcp_t& tcp) -> TUvError {
    return ::UvTcpReadStop(&tcp);
}

auto ReadStop(uv_stream_t* tcp) -> TUvError {
    return ::uv_read_stop(tcp);
}

auto Write(uv_write_t& req, uv_tcp_t& tcp, std::span<const uv_buf_t> bufs, uv_write_cb cb) -> TUvError {
    return ::UvTcpWrite(&req, &tcp, bufs.data(), static_cast<unsigned>(bufs.size()), cb);
}

void Close(uv_handle_t* handle, uv_close_cb cb) {
    ::uv_close(handle, cb);
}

void Close(uv_async_t& handle, uv_close_cb cb) {
    ::UvAsyncClose(&handle, cb);
}

void Close(uv_timer_t& handle, uv_close_cb cb) {
    ::UvTimerClose(&handle, cb);
}

void Close(uv_signal_t& handle, uv_close_cb cb) {
    ::UvSignalClose(&handle, cb);
}

void Close(uv_tcp_t& handle, uv_close_cb cb) {
    ::UvTcpClose(&handle, cb);
}

void Close(uv_idle_t& handle, uv_close_cb cb) {
    ::UvIdleClose(&handle, cb);
}

}
