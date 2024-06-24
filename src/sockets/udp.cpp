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
#include <uvexec/sockets/udp.hpp>

#include <uvexec/uv_util/safe_uv.hpp>


namespace NUvExec {

TUdpSocket::TUdpSocket(TLoop& loop, const TIp4Addr& addr) {
    NUvUtil::Assert(::uv_udp_init(&NUvUtil::RawUvObject(loop), &UvSocket));
    NUvUtil::Assert(::UvUdpInBind(&UvSocket, &NUvUtil::RawUvObject(addr)));
}

TUdpSocket::TUdpSocket(TLoop& loop, const TIp6Addr& addr) {
    NUvUtil::Assert(::uv_udp_init(&NUvUtil::RawUvObject(loop), &UvSocket));
    NUvUtil::Assert(::UvUdpIn6Bind(&UvSocket, &NUvUtil::RawUvObject(addr)));
}

auto TUdpSocket::Loop() noexcept -> TLoop& {
    return *static_cast<TLoop*>(UvSocket.loop->data);
}

auto tag_invoke(NUvUtil::TRawUvObject, TUdpSocket& socket) noexcept -> uv_udp_t& {
    return socket.UvSocket;
}

}
