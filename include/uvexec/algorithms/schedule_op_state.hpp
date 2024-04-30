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

template <stdexec::receiver_of<TScheduleCompletionSignatures> TReceiver>
class TScheduleOpState final : public TLoop::TOperation {
public:
    TScheduleOpState(TLoop& loop, TReceiver&& receiver)
        : Loop{&loop}, Receiver(std::move(receiver))
    {}

    friend void tag_invoke(stdexec::start_t, TScheduleOpState& op) noexcept {
        op.Loop->Schedule(op);
    }

    void Apply() noexcept override {
        if (stdexec::get_stop_token(stdexec::get_env(Receiver)).stop_requested()) {
            stdexec::set_stopped(std::move(Receiver));
        } else {
            stdexec::set_value(std::move(Receiver));
        }
    }

private:
    TLoop* Loop;
    TReceiver Receiver;
};

}
