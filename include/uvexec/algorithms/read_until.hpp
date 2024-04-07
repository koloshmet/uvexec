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

#include "read_until_op_state.hpp"

#include <uvexec/interface/uvexec.hpp>


namespace NUvExec {

template <stdexec::sender TSender, typename TSocket, typename TCondition>
    requires std::is_nothrow_invocable_r_v<bool, TCondition, std::size_t>
struct TReadUntilSender {
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TReadCompletionSignatures;

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TReadUntilSender s, TReceiver&& rec) {
        return TReadUntilOpState(*s.Socket, std::move(s.Condition), std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TReadUntilSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TReadUntilSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

    TSender Sender;
    TCondition Condition;
    TSocket* Socket;
};

}
