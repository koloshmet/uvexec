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

#include "receive_op_state.hpp"


namespace NUvExec {

template <stdexec::sender TSender, typename TSocket>
struct TReceiveSender {
    using sender_concept = stdexec::sender_t;

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TReceiveSender s, TReceiver&& rec) {
        return TReceiveOpState(*s.Socket, std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TReceiveSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TReceiveSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TSender, TEnv,
                TCancellableAlgorithmCompletionSignatures, TLengthValueCompletionSignatures>{};
    }

    TSender Sender;
    TSocket* Socket;
};

}
