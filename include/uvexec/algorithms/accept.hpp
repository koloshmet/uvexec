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

#include <uvexec/sockets/tcp_listener.hpp>

#include <uvexec/interface/uvexec.hpp>


namespace NUvExec {

template <stdexec::sender TSender>
class TTcpAcceptSender {
public:
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TAlgorithmCompletionSignatures;

public:
    TTcpAcceptSender(TSender sender, TTcpListener& listener, TTcpSocket& socket)
        : Sender(std::move(sender)), Listener{&listener}, Socket{&socket}
    {}

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TTcpAcceptSender s, TReceiver&& rec) {
        return TTcpListener::TAcceptOpState(
                *s.Listener, *s.Socket, std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TTcpAcceptSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TTcpAcceptSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

private:
    TSender Sender;
    TTcpListener* Listener;
    TTcpSocket* Socket;
};

template<>
class TTcpAcceptSender<TJustSender<>>
        : public stdexec::sender_adaptor_closure<TTcpAcceptSender<TJustSender<>>> {
public:
    using sender_concept = stdexec::sender_t;
    using TCompletionSignatures = TAlgorithmCompletionSignatures;

public:
    TTcpAcceptSender(TJustSender<> sender, TTcpListener& listener, TTcpSocket& socket)
        : Sender(sender), Listener{&listener}, Socket{&socket}
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) const {
        return uvexec::accept(std::forward<TSender>(sender), *Listener, *Socket);
    }

    template <stdexec::receiver_of<TCompletionSignatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TTcpAcceptSender s, TReceiver&& rec) {
        return TTcpListener::TAcceptOpState(
                *s.Listener, *s.Socket, std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TTcpAcceptSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TTcpAcceptSender&, const TEnv&) noexcept {
        return TCompletionSignatures{};
    }

private:
    [[no_unique_address]] TJustSender<> Sender;
    TTcpListener* Listener;
    TTcpSocket* Socket;
};

template <typename TAdapterClosure> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TTcpAcceptSender<TJustSender<>> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

template <stdexec::sender TSender>
auto tag_invoke(
        TLoop::TDomain,
        TSenderPackage<uvexec::accept_t, TSender, std::tuple<TTcpListener&, TTcpSocket&>>&& s) noexcept(
                std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TTcpAcceptSender<std::decay_t<TSender>>(std::move(s.Sender), std::get<0>(s.Data), std::get<1>(s.Data));
}

}
