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
#include <uvexec/execution/loop.hpp>

#include <span>


namespace NUvExec {

template <typename TSocket, stdexec::sender TSender, stdexec::receiver TReceiver>
class TReceiveOpState {
    class TReceiveFromReceiver final : public stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver>;

    public:
        TReceiveFromReceiver(TReceiveOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value(std::span<std::byte> buff) noexcept {
            Op->Buf = buff;
            StartReceive();
        }

    private:
        void StartReceive() noexcept {
            NUvUtil::RawUvObject(*Op->Socket).data = Op;
            auto err = NUvUtil::ReceiveStart(NUvUtil::RawUvObject(*Op->Socket), AllocateBuf, ReceiveCallback);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), err);
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopOp.Setup();
            }
        }

    private:
        TReceiveOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReceiveFromReceiver>;

public:
    TReceiveOpState(TSocket& socket, TSender&& sender, TReceiver receiver) noexcept
        : StopOp(StopCallback,
                *this,
                *static_cast<TLoop*>(NUvUtil::GetLoop(NUvUtil::RawUvObject(socket)).data),
                stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TReceiveFromReceiver(*this, std::move(receiver))))
        , Socket{&socket}
    {}

    friend void tag_invoke(stdexec::start_t, TReceiveOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void ReceiveCallback(
            uv_udp_t* udp, std::ptrdiff_t nrd, const uv_buf_t*, const struct sockaddr* addr, unsigned) {
        if (addr == nullptr && nrd == 0) {
            return;
        }
        auto self = static_cast<TReceiveOpState*>(udp->data);
        if (!self->StopOp.Reset()) {
            NUvUtil::ReceiveStop(*udp);
            if (nrd < 0) {
                stdexec::set_error(*std::move(self->Receiver), static_cast<NUvUtil::TUvError>(nrd));
            } else {
                stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(nrd));
            }
        }
    }

    static void AllocateBuf(uv_handle_t* udp, std::size_t, uv_buf_t* buf) {
        TReceiveOpState* self = NUvUtil::GetData<TReceiveOpState>(udp);
        buf->base = reinterpret_cast<char*>(self->Buf.data());
        buf->len = self->Buf.size();
    }

    static void StopCallback(TReceiveOpState& op) noexcept {
        op.StopOp.ResetUnsafe();
        NUvUtil::ReceiveStop(NUvUtil::RawUvObject(*op.Socket));
        stdexec::set_stopped(*std::move(op.Receiver));
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TReceiveOpState, TStopToken> StopOp;
    TOpState Op;
    std::span<std::byte> Buf;
    TSocket* Socket;
    std::optional<TReceiver> Receiver;
};

template <typename TSocket, NMeta::IsIn<uvexec::endpoints_of_t<TSocket>> TEndpoint,
        stdexec::sender TSender, stdexec::receiver TReceiver>
class TReceiveFromOpState {
    class TReceiveFromReceiver final : public stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver>;

    public:
        TReceiveFromReceiver(TReceiveFromOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TReceiveFromReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value(std::span<std::byte> buff, std::reference_wrapper<TEndpoint> ep) noexcept {
            Op->Buf = buff;
            Op->Endpoint = &ep.get();
            StartReceive();
        }

    private:
        void StartReceive() noexcept {
            NUvUtil::RawUvObject(*Op->Socket).data = Op;
            auto err = NUvUtil::ReceiveStart(NUvUtil::RawUvObject(*Op->Socket), AllocateBuf, ReceiveCallback);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), err);
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopOp.Setup();
            }
        }

    private:
        TReceiveFromOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReceiveFromReceiver>;

public:
    TReceiveFromOpState(TSocket& socket, TSender&& sender, TReceiver receiver) noexcept
        : StopOp(StopCallback,
                *this,
                *static_cast<TLoop*>(NUvUtil::GetLoop(NUvUtil::RawUvObject(socket)).data),
                stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TReceiveFromReceiver(*this, std::move(receiver))))
        , Socket{&socket}
    {}

    friend void tag_invoke(stdexec::start_t, TReceiveFromOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void ReceiveCallback(
            uv_udp_t* udp, std::ptrdiff_t nrd, const uv_buf_t*, const struct sockaddr* addr, unsigned) {
        if (addr == nullptr && nrd == 0) {
            return;
        }
        auto self = static_cast<TReceiveFromOpState*>(udp->data);
        if (!self->StopOp.Reset()) {
            NUvUtil::ReceiveStop(*udp);
            if (nrd < 0) {
                stdexec::set_error(*std::move(self->Receiver), static_cast<NUvUtil::TUvError>(nrd));
            } else {
                auto err = NUvUtil::CopyAddress(addr, NUvUtil::RawUvObject(*self->Endpoint));
                if (NUvUtil::IsError(err)) {
                    stdexec::set_error(*std::move(self->Receiver), err);
                } else {
                    stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(nrd));
                }
            }
        }
    }

    static void AllocateBuf(uv_handle_t* udp, std::size_t, uv_buf_t* buf) {
        TReceiveFromOpState* self = NUvUtil::GetData<TReceiveFromOpState>(udp);
        buf->base = reinterpret_cast<char*>(self->Buf.data());
        buf->len = self->Buf.size();
    }

    static void StopCallback(TReceiveFromOpState& op) noexcept {
        op.StopOp.ResetUnsafe();
        NUvUtil::ReceiveStop(NUvUtil::RawUvObject(*op.Socket));
        stdexec::set_stopped(*std::move(op.Receiver));
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TReceiveFromOpState, TStopToken> StopOp;
    TOpState Op;
    std::span<std::byte> Buf;
    TSocket* Socket;
    TEndpoint* Endpoint;
    std::optional<TReceiver> Receiver;
};

}
