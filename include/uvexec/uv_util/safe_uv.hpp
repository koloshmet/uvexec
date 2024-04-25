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

#include <uv.h>


extern "C" {

void* UvStreamGetData(const uv_stream_t* handle);

int UvCopyInAddress(const struct sockaddr* from, struct sockaddr_in* to);

int UvCopyIn6Address(const struct sockaddr* from, struct sockaddr_in6* to);

int UvTcpInBind(uv_tcp_t* tcp, const struct sockaddr_in* addr);

int UvTcpIn6Bind(uv_tcp_t* tcp, const struct sockaddr_in6* addr);

int UvUdpInBind(uv_udp_t* udp, const struct sockaddr_in* addr);

int UvUdpIn6Bind(uv_udp_t* udp, const struct sockaddr_in6* addr);

int UvTcpListen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb);

int UvTcpAccept(uv_tcp_t* server, uv_tcp_t* client);

int UvTcpInConnect(uv_connect_t* req, uv_tcp_t* tcp, const struct sockaddr_in* addr, uv_connect_cb cb);

int UvTcpIn6Connect(uv_connect_t* req, uv_tcp_t* tcp, const struct sockaddr_in6* addr, uv_connect_cb cb);

int UvUdpInConnect(uv_udp_t* udp, const struct sockaddr_in* addr);

int UvUdpIn6Connect(uv_udp_t* udp, const struct sockaddr_in6* addr);

int UvTcpShutdown(uv_shutdown_t* req, uv_tcp_t* tcp, uv_shutdown_cb cb);

int UvTcpReadStart(uv_tcp_t* tcp, uv_alloc_cb acb, uv_read_cb rcb);

int UvTcpReadStop(uv_tcp_t* tcp);

int UvTcpWrite(uv_write_t* req, uv_tcp_t* tcp, const uv_buf_t* bufs, unsigned nbufs, uv_write_cb cb);

int UvUdpInSend(
        uv_udp_send_t* req, uv_udp_t* udp, const uv_buf_t* bufs, unsigned nbufs,
        const struct sockaddr_in* addr, uv_udp_send_cb cb);

int UvUdpIn6Send(
        uv_udp_send_t* req, uv_udp_t* udp, const uv_buf_t* bufs, unsigned nbufs,
        const struct sockaddr_in6* addr, uv_udp_send_cb cb);

void UvAsyncClose(uv_async_t* handle, uv_close_cb close_cb);

void UvTimerClose(uv_timer_t* handle, uv_close_cb close_cb);

void UvSignalClose(uv_signal_t* handle, uv_close_cb close_cb);

void UvTcpClose(uv_tcp_t* handle, uv_close_cb close_cb);

void UvUdpClose(uv_udp_t* handle, uv_close_cb close_cb);

void UvIdleClose(uv_idle_t* handle, uv_close_cb close_cb);

}
