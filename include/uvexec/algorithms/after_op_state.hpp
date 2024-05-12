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

enum class ETimerType {
    After,
    At
};

template <stdexec::receiver_of<TScheduleEventuallyCompletionSignatures> TReceiver, ETimerType Type>
class TAfterScheduleOpState final : public TLoop::TOperation {
public:
    TAfterScheduleOpState(TLoop& loop, std::uint64_t timeout, TReceiver&& receiver)
        : StopOp(StopCallback, *this, loop, stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Loop{&loop}
        , Receiver(std::move(receiver))
        , Timeout{timeout}
    {}

    friend void tag_invoke(stdexec::start_t, TAfterScheduleOpState& op) noexcept {
        op.Loop->Schedule(op);
    }

    void Apply() noexcept override {
        auto err = NUvUtil::Init(Timer, NUvUtil::RawUvObject(*Loop));
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(Receiver), std::move(err));
        }
        Timer.data = this;
        std::uint64_t timeout;
        if constexpr (Type == ETimerType::At) {
            auto now = NUvUtil::Now(*Timer.loop);
            timeout = now > Timeout ? 0 : Timeout - now;
        } else {
            timeout = Timeout;
        }

        err = NUvUtil::TimerStart(Timer, AfterCallback, timeout, 0);
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(Receiver), std::move(err));
        } else {
            StopOp.Setup();
        }
    }

private:
    static void AfterCallback(uv_timer_t* timer) {
        auto opState = static_cast<TAfterScheduleOpState*>(timer->data);
        if (!opState->StopOp.Reset()) {
            NUvUtil::Close(*timer, CloseCallback);
        }
    }

    static void CloseCallback(uv_handle_t* timer) {
        auto opState = NUvUtil::GetData<TAfterScheduleOpState>(timer);
        if (!opState->StopOp.ResetUnsafe()) {
            stdexec::set_stopped(std::move(opState->Receiver));
        } else {
            stdexec::set_value(std::move(opState->Receiver));
        }
    }

    static void StopCallback(TAfterScheduleOpState& op) noexcept {
        NUvUtil::TimerStop(op.Timer);
        NUvUtil::Close(op.Timer, CloseCallback);
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TAfterScheduleOpState, TStopToken> StopOp;
    uv_timer_t Timer;
    TLoop* Loop;
    TReceiver Receiver;
    std::uint64_t Timeout;
};

template <stdexec::sender TSender, stdexec::receiver TReceiver, ETimerType Type>
class TAfterOpState {
    class TAfterReceiver final : public stdexec::receiver_adaptor<TAfterReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TAfterReceiver, TReceiver>;

    public:
        TAfterReceiver(TAfterOpState& op, TReceiver&& rec) noexcept
            : stdexec::receiver_adaptor<TAfterReceiver, TReceiver>(std::move(rec)), Op{&op}
        {}

        void set_value() noexcept {
            auto err = NUvUtil::Init(Op->Timer, NUvUtil::RawUvObject(*Op->Loop));
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), std::move(err));
            }
            Op->Timer.data = Op;
            std::uint64_t timeout;
            if constexpr (Type == ETimerType::At) {
                auto now = NUvUtil::Now(*Op->Timer.loop);
                timeout = now > Op->Timeout ? 0 : Op->Timeout - now;
            } else {
                timeout = Op->Timeout;
            }
            err = NUvUtil::TimerStart(Op->Timer, AfterCallback, timeout, 0);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(*this).base(), std::move(err));
            } else {
                Op->Receiver.emplace(std::move(*this).base());
                Op->StopOp.Setup();
            }
        }

    private:
        TAfterOpState* Op;
    };

    using TOpState = stdexec::connect_result_t<TSender, TAfterReceiver>;

public:
    TAfterOpState(TLoop& loop, std::uint64_t timeout, TSender&& sender, TReceiver&& receiver)
        : StopOp(StopCallback, *this, loop, stdexec::get_stop_token(stdexec::get_env(receiver)))
        , Op(stdexec::connect(std::move(sender), TAfterReceiver(*this, std::move(receiver))))
        , Loop{&loop}
        , Timeout{timeout}
    {}

    friend void tag_invoke(stdexec::start_t, TAfterOpState& op) noexcept {
        stdexec::start(op.Op);
    }

private:
    static void AfterCallback(uv_timer_t* timer) {
        auto opState = static_cast<TAfterOpState*>(timer->data);
        if (!opState->StopOp.Reset()) {
            NUvUtil::Close(*timer, CloseCallback);
        }
    }

    static void CloseCallback(uv_handle_t* signal) {
        auto opState = NUvUtil::GetData<TAfterOpState>(signal);
        if (!opState->StopOp.ResetUnsafe()) {
            stdexec::set_stopped(*std::move(opState->Receiver));
        } else {
            stdexec::set_value(*std::move(opState->Receiver));
        }
    }

    static void StopCallback(TAfterOpState& op) noexcept {
        NUvUtil::TimerStop(op.Timer);
        NUvUtil::Close(op.Timer, CloseCallback);
    }

    using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

private:
    TLoop::TStopOperation<TAfterOpState, TStopToken> StopOp;
    TOpState Op;
    uv_timer_t Timer;
    TLoop* Loop;
    std::optional<TReceiver> Receiver;
    std::uint64_t Timeout;
};

}
