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

#include "accept_from_op_state.hpp"
#include "completion_signatures.hpp"


namespace NUvExec {

template <stdexec::sender TInSender, typename TListener, std::move_constructible TFn>
class TAcceptFromSender {
    using TValue = std::decay_t<NMeta::TFnParameterType<TFn>>;

    template <typename... TArgs>
    using TFnCompletionSignatures = stdexec::completion_signatures_of_t<
            std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>;

public:
    using sender_concept = stdexec::sender_t;

    TAcceptFromSender(TInSender inSender, TListener& listener, TFn fun)
        : InSender(std::move(inSender)), Listener{&listener}, Fun(std::move(fun))
    {}

    template <stdexec::receiver TRec>
    friend auto tag_invoke(stdexec::connect_t, TAcceptFromSender s, TRec&& rec)
            noexcept(std::is_nothrow_move_constructible_v<TFn>) {
        return TAcceptFromOpState<TInSender, TListener, TFn, std::decay_t<TRec>>{
                std::move(s.InSender), *s.Listener, std::move(s.Fun), std::forward<TRec>(rec)};
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAcceptFromSender& s) noexcept {
        return stdexec::get_env(s.InSender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAcceptFromSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<
                std::invoke_result_t<uvexec::accept_t, TInSender, TListener&, TValue&>,
                TEnv,
                TExceptionCompletionSignatures,
                TFnCompletionSignatures>{};
    }

private:
    TInSender InSender;
    TListener* Listener;
    TFn Fun;
};

template <typename TListener, std::move_constructible TFn>
class TAcceptFromSender<TJustSender<>, TListener, TFn>
        : public stdexec::sender_adaptor_closure<TAcceptFromSender<TJustSender<>, TListener, TFn>> {
    using TValue = std::decay_t<NMeta::TFnParameterType<TFn>>;

    template <typename... TArgs>
    using TFnCompletionSignatures = stdexec::completion_signatures_of_t<
            std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>;

public:
    using sender_concept = stdexec::sender_t;

    TAcceptFromSender(TJustSender<> inSender, TListener& listener, TFn fun)
        : InSender(std::move(inSender)), Listener{&listener}, Fun(std::move(fun))
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) {
        return TAcceptFromSender<std::decay_t<TSender>, TListener, TFn>(
                std::forward<TSender>(sender), *Listener, std::move(Fun));
    }

    template <stdexec::receiver TRec>
    friend auto tag_invoke(stdexec::connect_t, TAcceptFromSender s, TRec&& rec)
            noexcept(std::is_nothrow_move_constructible_v<TFn>) {
        return TAcceptFromOpState<TJustSender<>, TListener, TFn, std::decay_t<TRec>>{
                std::move(s.InSender), *s.Listener, std::move(s.Fun), std::forward<TRec>(rec)};
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAcceptFromSender& s) noexcept {
        return stdexec::get_env(s.InSender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAcceptFromSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<
                std::invoke_result_t<uvexec::accept_t, TJustSender<>, TListener&, TValue&>,
                TEnv,
                TExceptionCompletionSignatures,
                TFnCompletionSignatures>{};
    }

private:
    TJustSender<> InSender;
    TListener* Listener;
    TFn Fun;
};

template <typename TAdapterClosure, typename TListener, typename TFn> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TAcceptFromSender<TJustSender<>, TListener, TFn> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

}

namespace uvexec {

struct accept_from_t {
    template <typename TListener, typename TFn>
    auto operator()(TListener& listener, TFn fn) const noexcept(
            std::is_nothrow_invocable_v<accept_from_t, NUvExec::TJustSender<>, TFn>) -> stdexec::sender auto {
        return (*this)(stdexec::just(), listener, fn);
    }

    template <stdexec::sender TSender, typename TListener, std::move_constructible TFn>
    auto operator()(TSender&& sender, TListener& listener, TFn fn) const
            noexcept(std::is_nothrow_move_constructible_v<TFn>) -> stdexec::sender auto {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d,
                NUvExec::TAcceptFromSender<TSender, TListener, TFn>{
                        std::forward<TSender>(sender), listener, std::move(fn)});
    }
};

inline constexpr accept_from_t accept_from;

}
