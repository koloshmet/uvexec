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

#include "send_to_receiver.hpp"


namespace NUvExec {

template <stdexec::sender TSender, typename TSocket>
struct TSendToSender {
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TAlgorithmCompletionSignatures;

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TSendToSender s, TReceiver&& rec) {
        return stdexec::connect(std::move(s.Sender), TSendToReceiver(*s.Socket, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TSendToSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(
            stdexec::get_completion_signatures_t, const TSendToSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

    TSender Sender;
    TSocket* Socket;
};

}
