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
#include <uv.h>
#include <string.h>


void* UvStreamGetData(const uv_stream_t* handle) {
    return uv_handle_get_data((uv_handle_t*)handle);
}

int UvCopyInAddress(const struct sockaddr* from, struct sockaddr_in* to) {
    if (from->sa_family != AF_INET) {
        return UV_EINVAL;
    }
    memcpy(to, from, sizeof(struct sockaddr_in));
    return 0;
}

int UvCopyIn6Address(const struct sockaddr* from, struct sockaddr_in6* to) {
    if (from->sa_family != AF_INET6) {
        return UV_EINVAL;
    }
    memcpy(to, from, sizeof(struct sockaddr_in6));
    return 0;
}

int UvTcpInBind(uv_tcp_t* tcp, const struct sockaddr_in* addr) {
    return uv_tcp_bind(tcp, (const struct sockaddr*)addr, 0);
}

int UvTcpIn6Bind(uv_tcp_t* tcp, const struct sockaddr_in6* addr) {
    return uv_tcp_bind(tcp, (const struct sockaddr*)addr, 0);
}

int UvUdpInBind(uv_udp_t* udp, const struct sockaddr_in* addr) {
    return uv_udp_bind(udp, (const struct sockaddr*)addr, 0);
}

int UvUdpIn6Bind(uv_udp_t* udp, const struct sockaddr_in6* addr) {
    return uv_udp_bind(udp, (const struct sockaddr*)addr, 0);
}

int UvTcpListen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb) {
    return uv_listen((uv_stream_t*)tcp, backlog, cb);
}

int UvTcpAccept(uv_tcp_t* server, uv_tcp_t* client) {
    return uv_accept((uv_stream_t*)server, (uv_stream_t*)client);
}

int UvTcpInConnect(uv_connect_t* req, uv_tcp_t* tcp, const struct sockaddr_in* addr, uv_connect_cb cb) {
    return uv_tcp_connect(req, tcp, (const struct sockaddr*)addr, cb);
}

int UvTcpIn6Connect(uv_connect_t* req, uv_tcp_t* tcp, const struct sockaddr_in6* addr, uv_connect_cb cb) {
    return uv_tcp_connect(req, tcp, (const struct sockaddr*)addr, cb);
}

int UvUdpInConnect(uv_udp_t* udp, const struct sockaddr_in* addr) {
    return uv_udp_connect(udp, (const struct sockaddr*)addr);
}

int UvUdpIn6Connect(uv_udp_t* udp, const struct sockaddr_in6* addr) {
    return uv_udp_connect(udp, (const struct sockaddr*)addr);
}

int UvTcpShutdown(uv_shutdown_t* req, uv_tcp_t* tcp, uv_shutdown_cb cb) {
    return uv_shutdown(req, (uv_stream_t*)tcp, cb);
}

int UvTcpReadStart(uv_tcp_t* tcp, uv_alloc_cb acb, uv_read_cb rcb) {
    return uv_read_start((uv_stream_t*)tcp, acb, rcb);
}

int UvTcpReadStop(uv_tcp_t* tcp) {
    return uv_read_stop((uv_stream_t*)tcp);
}

int UvTcpWrite(uv_write_t* req, uv_tcp_t* tcp, const uv_buf_t* bufs, unsigned nbufs, uv_write_cb cb) {
    return uv_write(req, (uv_stream_t*)tcp, bufs, nbufs, cb);
}

int UvUdpInSend(
        uv_udp_send_t* req, uv_udp_t* udp, const uv_buf_t* bufs, unsigned nbufs,
        const struct sockaddr_in* addr, uv_udp_send_cb cb) {
    return uv_udp_send(req, udp, bufs, nbufs, (struct sockaddr*)addr, cb);
}

int UvUdpIn6Send(
        uv_udp_send_t* req, uv_udp_t* udp, const uv_buf_t* bufs, unsigned nbufs,
        const struct sockaddr_in6* addr, uv_udp_send_cb cb) {
    return uv_udp_send(req, udp, bufs, nbufs, (struct sockaddr*)addr, cb);
}

void UvAsyncClose(uv_async_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}

void UvTimerClose(uv_timer_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}

void UvSignalClose(uv_signal_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}

void UvTcpClose(uv_tcp_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}

void UvUdpClose(uv_udp_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}

void UvIdleClose(uv_idle_t* handle, uv_close_cb close_cb) {
    uv_close((uv_handle_t*)handle, close_cb);
}
