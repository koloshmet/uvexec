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

#include "write_receiver.hpp"
#include "completion_signatures.hpp"


namespace NUvExec {

template <stdexec::sender TSender, typename TSocket>
struct TWriteSender {
    using sender_concept = stdexec::sender_t;

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TWriteSender s, TReceiver&& rec) {
        return stdexec::connect(std::move(s.Sender), TWriteReceiver(*s.Socket, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TWriteSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TWriteSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TSender, TEnv,
                TAlgorithmCompletionSignatures, TVoidValueCompletionSignatures>{};
    }

    TSender Sender;
    TSocket* Socket;
};

}
