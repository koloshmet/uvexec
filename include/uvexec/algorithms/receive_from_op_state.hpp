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

template <typename TSocket, NMeta::IsIn<uvexec::endpoints_of_t<TSocket>> TEndpoint,
        stdexec::sender TSender, stdexec::receiver_of<TReadCompletionSignatures> TReceiver>
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
                Op->StopCallback.emplace(stdexec::get_stop_token(stdexec::get_env(*Op->Receiver)), TStopCallback{Op});
            }
        }

    private:
        TReceiveFromOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReceiveFromReceiver>;

public:
    TReceiveFromOpState(TSocket& socket, TSender&& sender, TReceiver receiver) noexcept
        : Op(stdexec::connect(std::move(sender), TReceiveFromReceiver(*this, std::move(receiver))))
        , Socket{&socket}
        , StopOp(*this)
    {}

    friend void tag_invoke(stdexec::start_t, TReceiveFromOpState& op) noexcept {
        stdexec::start(op.Op);
    }

    void Stop() noexcept {
        if (!Used.test_and_set(std::memory_order_relaxed)) {
            auto& loop = NUvUtil::GetLoop(NUvUtil::RawUvObject(*Socket));
            static_cast<TLoop*>(loop.data)->Schedule(StopOp);
        }
    }

    struct TStopOp : TLoop::TOpState {
        explicit TStopOp(TReceiveFromOpState& state) noexcept: State{&state} {}

        void Apply() noexcept override {
            NUvUtil::ReceiveStop(NUvUtil::RawUvObject(*State->Socket));
            State->StopCallback.reset();
            stdexec::set_stopped(*std::move(State->Receiver));
        }

        TReceiveFromOpState* State;
    };

    struct TStopCallback {
        void operator()() const {
            State->Stop();
        }

        TReceiveFromOpState* State;
    };

    using TCallback = NDetail::TCallbackOf<TReceiver, TStopCallback>;

private:
    static void ReceiveCallback(
            uv_udp_t* udp, std::ptrdiff_t nrd, const uv_buf_t*, const struct sockaddr* addr, unsigned) {
        if (addr == nullptr && nrd == 0) {
            return;
        }
        auto self = static_cast<TReceiveFromOpState*>(udp->data);
        if (!self->Used.test_and_set(std::memory_order_relaxed)) {
            NUvUtil::ReceiveStop(*udp);
            self->StopCallback.reset();
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

private:
    TOpState Op;
    std::span<std::byte> Buf;
    TSocket* Socket;
    TEndpoint* Endpoint;
    std::optional<TReceiver> Receiver;
    TStopOp StopOp;
    std::optional<TCallback> StopCallback;
    std::atomic_flag Used;
};

}
