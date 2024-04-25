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

#include "completion_signatures.hpp"

#include <uvexec/uv_util/reqs.hpp>
#include <uvexec/uv_util/misc.hpp>


namespace NUvExec {

template <stdexec::receiver TReceiver, typename TStreamSocket>
class TConnectReceiver : public stdexec::receiver_adaptor<TConnectReceiver<TReceiver, TStreamSocket>, TReceiver> {
    friend stdexec::receiver_adaptor<TConnectReceiver, TReceiver>;

public:
    TConnectReceiver(TStreamSocket& socket, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TConnectReceiver, TReceiver>(std::move(rec)), Socket{&socket}
    {}

    template <typename TEp>
    void set_value(const TEp& ep) noexcept {
        ConnectReq.data = this;
        auto& socket = NUvUtil::RawUvObject(*Socket);
        if constexpr (std::same_as<decltype(socket), uv_udp_t&>) {
            auto err = NUvUtil::Connect(socket, NUvUtil::RawUvObject(ep));
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), err);
            } else {
                stdexec::set_value(std::move(*this).base());
            }
        } else {
            auto err = NUvUtil::Connect(ConnectReq, socket, NUvUtil::RawUvObject(ep), ConnectCallback);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), err);
            }
        }
    }

private:
    static void ConnectCallback(uv_connect_t* req, NUvUtil::TUvError status) {
        auto self = static_cast<TConnectReceiver*>(req->data);
        if (NUvUtil::IsError(status)) {
            stdexec::set_error(std::move(*self).base(), status);
        } else {
            stdexec::set_value(std::move(*self).base());
        }
    }

private:
    uv_connect_t ConnectReq;
    TStreamSocket* Socket;
};

}
