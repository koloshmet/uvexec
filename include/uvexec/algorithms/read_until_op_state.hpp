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

#include <uvexec/execution/loop.hpp>
#include <uvexec/execution/error_code.hpp>

#include <span>


namespace NUvExec {

template <typename TStream, typename TCondition, stdexec::sender TSender, stdexec::receiver TReceiver>
    requires std::is_nothrow_invocable_r_v<bool, TCondition, std::size_t>
class TReadUntilOpState {
    class TReadUntilReceiver final : public stdexec::receiver_adaptor<TReadUntilReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TReadUntilReceiver, TReceiver>;

    public:
        TReadUntilReceiver(TReadUntilOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TReadUntilReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value(std::span<std::byte>& buff) noexcept {
            Op->Buf = &buff;
            NUvUtil::RawUvObject(*Op->Stream).data = Op;
            auto err = NUvUtil::ReadStart(NUvUtil::RawUvObject(*Op->Stream), AllocateBuf, ReadCallback);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), EErrc{err});
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopOp.Setup();
            }
        }

    private:
        TReadUntilOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReadUntilReceiver>;

public:
    TReadUntilOpState(TStream& stream, TCondition condition, TSender&& sender, TReceiver receiver) noexcept
        : StopOp(StopCallback,
                *this,
                *static_cast<TLoop*>(NUvUtil::GetLoop(NUvUtil::RawUvObject(stream)).data),
                stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TReadUntilReceiver(*this, std::move(receiver))))
        , Condition(std::move(condition))
        , Stream{&stream}
        , ReadTotal{0}
    {}

    friend void tag_invoke(stdexec::start_t, TReadUntilOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void ReadCallback(uv_stream_t* tcp, std::ptrdiff_t nrd, const uv_buf_t*) {
        TReadUntilOpState* self = NUvUtil::GetData<TReadUntilOpState>(tcp);
        if (!self->StopOp) {
            return;
        }
        if (nrd < 0) {
            if (!self->StopOp.Reset()) {
                NUvUtil::ReadStop(tcp);
                if (nrd == UV_EOF) {
                    stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(self->ReadTotal));
                } else {
                    stdexec::set_error(*std::move(self->Receiver), EErrc{static_cast<NUvUtil::TUvError>(nrd)});
                }
            }
        } else if (nrd > 0) {
            self->ReadTotal += nrd;
            if (self->Condition(static_cast<std::size_t>(nrd))) {
                if (!self->StopOp.Reset()) {
                    NUvUtil::ReadStop(tcp);
                    stdexec::set_value(*std::move(self->Receiver), self->ReadTotal);
                }
            }
        }
    }

    static void AllocateBuf(uv_handle_t* tcp, std::size_t, uv_buf_t* buf) {
        TReadUntilOpState* self = NUvUtil::GetData<TReadUntilOpState>(tcp);
        buf->base = reinterpret_cast<char*>(self->Buf->data());
        buf->len = self->Buf->size();
    }

    static void StopCallback(TReadUntilOpState& op) noexcept {
        op.StopOp.ResetUnsafe();
        NUvUtil::ReadStop(NUvUtil::RawUvObject(*op.Stream));
        stdexec::set_stopped(*std::move(op.Receiver));
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TReadUntilOpState, TStopToken> StopOp;
    TOpState Op;
    TCondition Condition;
    std::span<std::byte>* Buf;
    TStream* Stream;
    std::size_t ReadTotal;
    std::optional<TReceiver> Receiver;
};

}
