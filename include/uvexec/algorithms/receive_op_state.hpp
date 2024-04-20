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

template <typename TStream, stdexec::sender TSender, stdexec::receiver TReceiver>
class TReceiveOpState {
    class TReceiveReceiver final : public stdexec::receiver_adaptor<TReceiveReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TReceiveReceiver, TReceiver>;

    public:
        TReceiveReceiver(TReceiveOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TReceiveReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value(std::span<std::byte> buff) noexcept {
            Op->Buf = buff;
            StartRead();
        }

    private:
        void StartRead() noexcept {
            NUvUtil::RawUvObject(*Op->Stream).data = Op;
            auto err = NUvUtil::ReadStart(NUvUtil::RawUvObject(*Op->Stream), AllocateBuf, ReadCallback);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), err);
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopCallback.emplace(stdexec::get_stop_token(stdexec::get_env(*Op->Receiver)), TStopCallback{Op});
            }
        }

    private:
        TReceiveOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReceiveReceiver>;

public:
    TReceiveOpState(TStream& stream, TSender&& sender, TReceiver receiver) noexcept
        : Op(stdexec::connect(std::move(sender), TReceiveReceiver(*this, std::move(receiver))))
        , Stream{&stream}
        , StopOp(*this)
    {}

    friend void tag_invoke(stdexec::start_t, TReceiveOpState& op) noexcept {
        stdexec::start(op.Op);
    }

    void Stop() noexcept {
        if (!Used.test_and_set(std::memory_order_relaxed)) {
            auto& loop = NUvUtil::GetLoop(NUvUtil::RawUvObject(*Stream));
            static_cast<TLoop*>(loop.data)->Schedule(StopOp);
        }
    }

    struct TStopOp : TLoop::TOpState {
        explicit TStopOp(TReceiveOpState& state) noexcept: State{&state} {}

        void Apply() noexcept override {
            NUvUtil::ReadStop(NUvUtil::RawUvObject(*State->Stream));
            State->StopCallback.reset();
            stdexec::set_stopped(*std::move(State->Receiver));
        }

        TReceiveOpState* State;
    };

    struct TStopCallback {
        void operator()() const {
            State->Stop();
        }

        TReceiveOpState* State;
    };

    using TCallback = NDetail::TCallbackOf<TReceiver, TStopCallback>;

private:
    static void ReadCallback(uv_stream_t* tcp, std::ptrdiff_t nrd, const uv_buf_t*) {
        TReceiveOpState* self = NUvUtil::GetData<TReceiveOpState>(tcp);
        if (!self->Used.test_and_set(std::memory_order_relaxed)) {
            NUvUtil::ReadStop(tcp);
            self->StopCallback.reset();
            if (nrd < 0) {
                stdexec::set_error(*std::move(self->Receiver), static_cast<NUvUtil::TUvError>(nrd));
            } else {
                stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(nrd));
            }
        }
    }

    static void AllocateBuf(uv_handle_t* tcp, std::size_t, uv_buf_t* buf) {
        TReceiveOpState* self = NUvUtil::GetData<TReceiveOpState>(tcp);
        buf->base = reinterpret_cast<char*>(self->Buf.data());
        buf->len = self->Buf.size();
    }

private:
    TOpState Op;
    std::span<std::byte> Buf;
    TStream* Stream;
    std::optional<TReceiver> Receiver;
    TStopOp StopOp;
    std::optional<TCallback> StopCallback;
    std::atomic_flag Used;
};

}
