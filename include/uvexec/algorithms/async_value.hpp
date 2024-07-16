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

#include "async_value_op_state.hpp"
#include "completion_signatures.hpp"


namespace NUvExec {

template <stdexec::sender TInSender, std::move_constructible TFn>
class TAsyncValueSender {
    template <typename... TArgs>
    using TFnCompletionSignatures = stdexec::completion_signatures_of_t<
            std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>;

    using TNoexceptCompletionSignatures = TExceptionCompletionSignatures;

public:
    using sender_concept = stdexec::sender_t;

    TAsyncValueSender(TInSender inSender, TFn fun)
        : InSender(std::move(inSender)), Fn(std::move(fun))
    {}

    template <stdexec::receiver TRec>
    friend auto tag_invoke(stdexec::connect_t, TAsyncValueSender s, TRec&& rec) {
        return TAsyncValueOpState<TInSender, TFn, std::decay_t<TRec>>{
                std::move(s.InSender), std::move(s.Fn), std::forward<TRec>(rec)};
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAsyncValueSender& s) noexcept {
        return stdexec::get_env(s.InSender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAsyncValueSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<TInSender, TEnv,
                TNoexceptCompletionSignatures, TFnCompletionSignatures>{};
    }

private:
    TInSender InSender;
    TFn Fn;
};

}

namespace uvexec {

struct async_value_t {
    template <stdexec::sender TSender, std::move_constructible TFn>
    auto operator()(TSender&& sender, TFn fun) const noexcept(std::is_nothrow_move_constructible_v<TFn>)
            -> stdexec::sender auto {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(
                d, NUvExec::TAsyncValueSender<TSender, TFn>{std::forward<TSender>(sender), std::move(fun)});
    }

    template <std::move_constructible TFn>
    auto operator()(TFn fun) const noexcept(std::is_nothrow_move_constructible_v<TFn>) {
        return NUvExec::TArgBinder<TFn, async_value_t>(std::move(fun));
    }
};

inline constexpr async_value_t async_value;

}
