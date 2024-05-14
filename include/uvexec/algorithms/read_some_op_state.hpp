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
class TReadSomeOpState {
    class TReadSomeReceiver final : public stdexec::receiver_adaptor<TReadSomeReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TReadSomeReceiver, TReceiver>;

    public:
        TReadSomeReceiver(TReadSomeOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TReadSomeReceiver, TReceiver>(std::move(rec)), Op{&op}
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
                Op->StopOp.Setup();
            }
        }

    private:
        TReadSomeOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TReadSomeReceiver>;

public:
    TReadSomeOpState(TStream& stream, TSender&& sender, TReceiver receiver) noexcept
        : StopOp(StopCallback,
                *this,
                *static_cast<TLoop*>(NUvUtil::GetLoop(NUvUtil::RawUvObject(stream)).data),
                stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TReadSomeReceiver(*this, std::move(receiver))))
        , Stream{&stream}
    {}

    friend void tag_invoke(stdexec::start_t, TReadSomeOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void ReadCallback(uv_stream_t* tcp, std::ptrdiff_t nrd, const uv_buf_t*) {
        TReadSomeOpState* self = NUvUtil::GetData<TReadSomeOpState>(tcp);
        if (nrd == 0) {
            return;
        }
        if (!self->StopOp.Reset()) {
            NUvUtil::ReadStop(tcp);
            if (nrd < 0) {
                if (nrd == UV_EOF) {
                    stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(0));
                } else {
                    stdexec::set_error(*std::move(self->Receiver), static_cast<NUvUtil::TUvError>(nrd));
                }
            } else {
                stdexec::set_value(*std::move(self->Receiver), static_cast<std::size_t>(nrd));
            }
        }
    }

    static void AllocateBuf(uv_handle_t* tcp, std::size_t, uv_buf_t* buf) {
        TReadSomeOpState* self = NUvUtil::GetData<TReadSomeOpState>(tcp);
        buf->base = reinterpret_cast<char*>(self->Buf.data());
        buf->len = self->Buf.size();
    }

    static void StopCallback(TReadSomeOpState& op) noexcept {
        op.StopOp.ResetUnsafe();
        NUvUtil::ReadStop(NUvUtil::RawUvObject(*op.Stream));
        stdexec::set_stopped(*std::move(op.Receiver));
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TReadSomeOpState, TStopToken> StopOp;
    TOpState Op;
    std::span<std::byte> Buf;
    TStream* Stream;
    std::optional<TReceiver> Receiver;
};

}
