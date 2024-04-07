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

#include "shutdown_receiver.hpp"


namespace NUvExec {

template <stdexec::sender TSender, typename TStreamSocket>
class TShutdownSender {
public:
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TAlgorithmCompletionSignatures;

public:
    TShutdownSender(TSender sender, TStreamSocket& socket)
        : Sender(std::move(sender)), Socket{&socket}
    {}

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TShutdownSender s, TReceiver&& rec) {
        return stdexec::connect(
                std::move(s.Sender), TShutdownReceiver(*s.Socket, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TShutdownSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TShutdownSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

private:
    TSender Sender;
    TStreamSocket* Socket;
};

template<typename TStreamSocket>
class TShutdownSender<TJustSender<>, TStreamSocket>
        : public stdexec::sender_adaptor_closure<TShutdownSender<TJustSender<>, TStreamSocket>> {
public:
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TAlgorithmCompletionSignatures;

public:
    TShutdownSender(TJustSender<> sender, TStreamSocket& socket)
        : Sender(sender), Socket{&socket}
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) const {
        return uvexec::shutdown(std::forward<TSender>(sender), *Socket);
    }

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TShutdownSender s, TReceiver&& rec) {
        return stdexec::connect(s.Sender, TShutdownReceiver(*s.Stream, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TShutdownSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TShutdownSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

private:
    [[no_unique_address]] TJustSender<> Sender;
    TStreamSocket* Socket;
};

template <typename TAdapterClosure, typename TStreamSocket> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TShutdownSender<TJustSender<>, TStreamSocket> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

}
