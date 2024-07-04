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

#include <uvexec/execution/error_code.hpp>

#include <uvexec/uv_util/reqs.hpp>
#include <uvexec/uv_util/misc.hpp>

#include <span>


namespace NUvExec {

template <stdexec::receiver TReceiver, typename TSocket>
class TSendReceiver : public stdexec::receiver_adaptor<TSendReceiver<TReceiver, TSocket>, TReceiver> {
    friend stdexec::receiver_adaptor<TSendReceiver, TReceiver>;

public:
    TSendReceiver(TSocket& socket, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TSendReceiver, TReceiver>(std::move(rec)), Handle{&socket}
    {}

    void set_value(std::span<const uv_buf_t> buffs) noexcept {
        SendReq.data = this;
        auto err = NUvUtil::Send(SendReq, NUvUtil::RawUvObject(*Handle), buffs, SendCallback);
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(*this).base(), EErrc{err});
        }
    }

    void set_value(std::span<const std::byte> buff) noexcept {
        if (buff.empty()) {
            stdexec::set_value(std::move(*this).base());
        }
        Buf.base = const_cast<char*>(reinterpret_cast<const char*>(buff.data()));
        Buf.len = buff.size();
        set_value(std::span(&Buf, 1));
    }

private:
    static void SendCallback(uv_udp_send_t* req, NUvUtil::TUvError status) {
        auto self = static_cast<TSendReceiver*>(req->data);
        if (NUvUtil::IsError(status)) {
            stdexec::set_error(std::move(*self).base(), EErrc{status});
        } else {
            stdexec::set_value(std::move(*self).base());
        }
    }

private:
    uv_udp_send_t SendReq;
    uv_buf_t Buf;
    TSocket* Handle;
};

template <stdexec::receiver TReceiver, typename TSocket>
class TSendToReceiver : public stdexec::receiver_adaptor<TSendToReceiver<TReceiver, TSocket>, TReceiver> {
    friend stdexec::receiver_adaptor<TSendToReceiver, TReceiver>;

public:
    TSendToReceiver(TSocket& socket, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TSendToReceiver, TReceiver>(std::move(rec)), Handle{&socket}
    {}

    template <typename TEp>
    void set_value(std::span<const uv_buf_t> buffs, const TEp& ep) noexcept {
        SendReq.data = this;
        auto err = NUvUtil::Send(SendReq, NUvUtil::RawUvObject(*Handle), buffs, SendCallback, NUvUtil::RawUvObject(ep));
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(*this).base(), EErrc{err});
        }
    }

    template <typename TEp>
    void set_value(std::span<std::byte> buff, const TEp& ep) noexcept {
        Buf.base = reinterpret_cast<char*>(buff.data());
        Buf.len = buff.size();
        set_value(std::span(&Buf, 1), ep);
    }

private:
    static void SendCallback(uv_udp_send_t* req, NUvUtil::TUvError status) {
        auto self = static_cast<TSendToReceiver*>(req->data);
        if (NUvUtil::IsError(status)) {
            stdexec::set_error(std::move(*self).base(), EErrc{status});
        } else {
            stdexec::set_value(std::move(*self).base());
        }
    }

private:
    uv_udp_send_t SendReq;
    uv_buf_t Buf;
    TSocket* Handle;
};

}
