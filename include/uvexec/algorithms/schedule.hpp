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

#include "schedule_op_state.hpp"


namespace NUvExec {

class TScheduleSender {
public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures = TScheduleCompletionSignatures;

    explicit TScheduleSender(TLoop& loop) noexcept
        : Loop{&loop}
    {}

    template <stdexec::receiver_of<completion_signatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TScheduleSender s, TReceiver&& rec) {
        return TScheduleOpState<std::decay_t<TReceiver>>(*s.Loop, std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TScheduleSender& s) noexcept {
        return TLoop::TScheduler::TEnv(*s.Loop);
    }

private:
    TLoop* Loop;
};

inline auto tag_invoke(stdexec::schedule_t, TLoop::TScheduler s) noexcept {
    return TScheduleSender(TLoop::TDomain{}.GetLoop(s));
}

}
