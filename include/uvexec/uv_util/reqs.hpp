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
#pragma once

#include "errors.hpp"

#include <uv.h>

#include <span>


namespace NUvUtil {

auto Fire(uv_async_t& req) -> TUvError;

auto Now(const uv_loop_t& loop) -> std::uint64_t;

auto TimerStart(uv_timer_t& req, uv_timer_cb cb, std::uint64_t timeout, std::uint64_t repeat) -> TUvError;

auto TimerStop(uv_timer_t& req) -> TUvError;

auto SignalOnce(uv_signal_t& req, uv_signal_cb cb, int signum) -> TUvError;

auto SignalStop(uv_signal_t& req) -> TUvError;

auto Bind(uv_tcp_t& tcp, const sockaddr_in& addr) -> TUvError;

auto Bind(uv_tcp_t& tcp, const sockaddr_in6& addr) -> TUvError;

auto Accept(uv_tcp_t& server, uv_tcp_t& client) -> TUvError;

auto Connect(uv_connect_t& req, uv_tcp_t& tcp, const sockaddr_in& addr, uv_connect_cb cb) -> TUvError;

auto Connect(uv_connect_t& req, uv_tcp_t& tcp, const sockaddr_in6& addr, uv_connect_cb cb) -> TUvError;

auto Shutdown(uv_shutdown_t& req, uv_tcp_t& tcp, uv_shutdown_cb cb) -> TUvError;

auto CloseReset(uv_tcp_t& handle, uv_close_cb cb) -> TUvError;

auto ReadStart(uv_tcp_t& tcp, uv_alloc_cb acb, uv_read_cb rcb) -> TUvError;

auto ReadStop(uv_tcp_t& tcp) -> TUvError;

auto ReadStop(uv_stream_t* tcp) -> TUvError;

auto Write(uv_write_t& req, uv_tcp_t& tcp, std::span<const uv_buf_t> bufs, uv_write_cb cb) -> TUvError;

void Close(uv_handle_t* handle, uv_close_cb cb);

void Close(uv_async_t& handle, uv_close_cb cb);

void Close(uv_timer_t& handle, uv_close_cb cb);

void Close(uv_signal_t& handle, uv_close_cb cb);

void Close(uv_tcp_t& handle, uv_close_cb cb);

void Close(uv_idle_t& handle, uv_close_cb cb);


template <typename TUvHandle>
concept UvHandle = requires (TUvHandle& handle) {
    { handle.type } -> std::same_as<uv_handle_type&>;
};

template <UvHandle TUvHandle>
void Close(TUvHandle& handle) {
    NUvUtil::Close(handle, nullptr);
}

}
