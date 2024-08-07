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
#include <uvexec/sockets/tcp_listener.hpp>

#include <uvexec/uv_util/safe_uv.hpp>


namespace NUvExec {

void TTcpListener::RegisterAccept(TAccept& accept) {
    if (PendingConnections > 0) {
        --PendingConnections;
        accept.Accept();
    } else {
        AcceptList.Add(accept);
        if (PendingConnections < 0) [[unlikely]] {
            if (auto err = StartListening(); err != EErrc{0}) {
                accept.Error(err);
                return;
            }
            PendingConnections = 0;
        }
    }
}

auto TTcpListener::Loop() noexcept -> TLoop& {
    return Socket.Loop();
}

auto tag_invoke(NUvUtil::TRawUvObject, TTcpListener& listener) noexcept -> uv_tcp_t& {
    return NUvUtil::RawUvObject(listener.Socket);
}

auto TTcpListener::StartListening() -> EErrc {
    auto& tcp = NUvUtil::RawUvObject(Socket);
    tcp.data = this;
    auto err = ::UvTcpListen(&tcp, -PendingConnections, ConnectionCallback);
    if (NUvUtil::IsError(err)) {
        return EErrc{err};
    }
    return EErrc{0};
}

void TTcpListener::ConnectionCallback(uv_stream_t* server, int status) {
    if (NUvUtil::IsError(status)) {
        return;
    }
    auto self = NUvUtil::GetData<TTcpListener>(server);
    if (self->AcceptList.Empty()) {
        ++self->PendingConnections;
    } else {
        auto& op = self->AcceptList.Pop();
        op.Accept();
    }
}

}
