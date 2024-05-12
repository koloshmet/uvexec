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


namespace NUvExec {

template <stdexec::receiver_of<TScheduleEventuallyCompletionSignatures> TReceiver>
class TUponSignalScheduleOpState final : public TLoop::TOperation {
public:
    TUponSignalScheduleOpState(TLoop& loop, int signum, TReceiver&& receiver)
        : StopOp(StopCallback, *this, loop, stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Loop{&loop}
        , Receiver(std::move(receiver))
        , Signum{signum}
    {}

    friend void tag_invoke(stdexec::start_t, TUponSignalScheduleOpState& op) noexcept {
        op.Loop->Schedule(op);
    }

    void Apply() noexcept override {
        auto err = NUvUtil::Init(Signal, NUvUtil::RawUvObject(*Loop));
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(Receiver), std::move(err));
        }
        Signal.data = this;
        err = NUvUtil::SignalOnce(Signal, SignalCallback, Signum);
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(Receiver), std::move(err));
        } else {
            StopOp.Setup();
        }
    }

private:
    static void SignalCallback(uv_signal_t* signal, int) {
        auto opState = static_cast<TUponSignalScheduleOpState*>(signal->data);
        if (!opState->StopOp.Reset()) {
            NUvUtil::Close(*signal, CloseCallback);
        }
    }

    static void CloseCallback(uv_handle_t* signal) {
        auto opState = NUvUtil::GetData<TUponSignalScheduleOpState>(signal);
        if (!opState->StopOp.ResetUnsafe()) {
            stdexec::set_stopped(std::move(opState->Receiver));
        } else {
            stdexec::set_value(std::move(opState->Receiver));
        }
    }

    static void StopCallback(TUponSignalScheduleOpState& op) noexcept {
        NUvUtil::SignalStop(op.Signal);
        NUvUtil::Close(op.Signal, CloseCallback);
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TUponSignalScheduleOpState, TStopToken> StopOp;
    uv_signal_t Signal;
    TLoop* Loop;
    TReceiver Receiver;
    int Signum;
};

template <stdexec::sender TSender, stdexec::receiver TReceiver>
class TUponSignalOpState {
    class TUponSignalReceiver final : public stdexec::receiver_adaptor<TUponSignalReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TUponSignalReceiver, TReceiver>;

    public:
        TUponSignalReceiver(TUponSignalOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TUponSignalReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value() noexcept {
            auto err = NUvUtil::Init(Op->Signal, NUvUtil::RawUvObject(*Op->Loop));
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), std::move(err));
            }
            Op->Signal.data = Op;
            err = NUvUtil::SignalOnce(Op->Signal, SignalCallback, Op->Signum);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), std::move(err));
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopOp.Setup();
            }
        }

    private:
        TUponSignalOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TUponSignalReceiver>;

public:
    TUponSignalOpState(TLoop& loop, int signum, TSender&& sender, TReceiver&& receiver)
        : StopOp(StopCallback, *this, loop, stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TUponSignalReceiver(*this, std::move(receiver))))
        , Loop{&loop}
        , Signum{signum}
    {}

    friend void tag_invoke(stdexec::start_t, TUponSignalOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void SignalCallback(uv_signal_t* signal, int) {
        auto opState = static_cast<TUponSignalOpState*>(signal->data);
        if (!opState->StopOp.Reset()) {
            NUvUtil::Close(*signal, CloseCallback);
        }
    }

    static void CloseCallback(uv_handle_t* signal) {
        auto opState = NUvUtil::GetData<TUponSignalOpState>(signal);
        if (!opState->StopOp.ResetUnsafe()) {
            stdexec::set_stopped(*std::move(opState->Receiver));
        } else {
            stdexec::set_value(*std::move(opState->Receiver));
        }
    }

    static void StopCallback(TUponSignalOpState& op) noexcept {
        NUvUtil::SignalStop(op.Signal);
        NUvUtil::Close(op.Signal, CloseCallback);
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TUponSignalOpState, TStopToken> StopOp;
    TOpState Op;
    uv_signal_t Signal;
    TLoop* Loop;
    std::optional<TReceiver> Receiver;
    int Signum;
};

}
