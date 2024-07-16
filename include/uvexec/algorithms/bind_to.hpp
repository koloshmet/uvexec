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

#include "bind_to_op_state.hpp"
#include "completion_signatures.hpp"


namespace NUvExec {

template <stdexec::sender TInSender, std::move_constructible TFn>
class TBindToSender {
    using TValue = std::decay_t<NMeta::TFnParameterType<TFn>>;

    template <typename... TArgs>
    using TFnCompletionSignatures = stdexec::completion_signatures_of_t<
            std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>;

public:
    using sender_concept = stdexec::sender_t;

    TBindToSender(TInSender inSender, TFn fun)
        : InSender(std::move(inSender)), Fun(std::move(fun))
    {}

    template <stdexec::receiver TRec>
    friend auto tag_invoke(stdexec::connect_t, TBindToSender s, TRec&& rec)
            noexcept(std::is_nothrow_move_constructible_v<TFn>) {
        return TBindToOpState<TInSender, TFn, std::decay_t<TRec>>{
                std::move(s.InSender), std::move(s.Fun), std::forward<TRec>(rec)};
    }

    friend auto tag_invoke(stdexec::get_env_t, const TBindToSender& s) noexcept {
        return stdexec::get_env(s.InSender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TBindToSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<TInSender, TEnv,
                TExceptionCompletionSignatures, TFnCompletionSignatures>{};
    }

private:
    TInSender InSender;
    TFn Fun;
};

}

namespace uvexec {

struct bind_to_t {
    template <typename TFn, NMeta::IsInDecayed<endpoints_of_t<std::decay_t<NMeta::TFnParameterType<TFn>>>> TEp>
    auto operator()(TEp&& ep, TFn fn) const noexcept(
            std::is_nothrow_invocable_v<bind_to_t, NUvExec::TJustSender<std::decay_t<TEp>>, TFn>)
            -> stdexec::sender auto {
        return (*this)(stdexec::just(std::forward<TEp>(ep)), fn);
    }

    template <std::move_constructible TFn>
    auto operator()(TFn fun) const noexcept(std::is_nothrow_move_constructible_v<TFn>) {
        return NUvExec::TArgBinder<TFn, bind_to_t>(std::move(fun));
    }

    template <stdexec::sender TSender, std::move_constructible TFn>
    auto operator()(TSender&& sender, TFn fn) const noexcept(std::is_nothrow_move_constructible_v<TFn>)
            -> stdexec::sender auto {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(
                d, NUvExec::TBindToSender<TSender, TFn>{std::forward<TSender>(sender), std::move(fn)});
    }
};

inline constexpr bind_to_t bind_to;

}
