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

#include <uvexec/interface/uvexec.hpp>

#include <exec/finally.hpp>
#include <uvexec/util/lazy.hpp>


namespace NUvExec {

template <stdexec::sender TInSender, std::move_constructible TFn, stdexec::receiver TReceiver> requires
        std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
        stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
class TAsyncValueOpState {
    using TValue = std::decay_t<NMeta::TFnParameterType<TFn>>;

    static constexpr bool IsValueStopSource = requires(TValue& val) { val.request_stop(); };

    struct TDestroyer {
        void operator()() const noexcept {
            op->Value.reset();
            op->StopCallback.reset();
        }

        TAsyncValueOpState* op;
    };

    using TOutOpState = stdexec::connect_result_t<
            std::invoke_result_t<exec::finally_t,
                    std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>,
                    std::invoke_result_t<stdexec::then_t, std::invoke_result_t<uvexec::drop_t, TValue&>, TDestroyer>>,
            TReceiver>;

    class TAsyncValueReceiver : public stdexec::receiver_adaptor<TAsyncValueReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TAsyncValueReceiver, TReceiver>;

    public:
        explicit TAsyncValueReceiver(TAsyncValueOpState* opState, TReceiver receiver) noexcept
            : stdexec::receiver_adaptor<TAsyncValueReceiver, TReceiver>(std::move(receiver))
            , OpState{opState}
        {}

        template <typename... TArgs>
        void set_value(TArgs&&... args) noexcept {
            auto op = OpState;
            op->Value.emplace(std::forward<TArgs>(args)...);
            op->StopCallback.emplace(stdexec::get_stop_token(stdexec::get_env(this->base())), TStopCallback{op});
            try {
                op->OpState.template emplace<1>(Lazy([op, this]{
                    return stdexec::connect(
                            exec::finally(
                                    std::invoke(std::move(op->Fn), *op->Value),
                                    stdexec::then(uvexec::drop(*op->Value), TDestroyer{op})),
                            std::move(*this).base());
                }));
                stdexec::start(std::get<1>(op->OpState));
            } catch (...) {
                if constexpr (!std::is_nothrow_invocable_v<TFn, NMeta::TFnParameterType<TFn>>) {
                    stdexec::set_error(std::move(*this).base(), std::current_exception());
                }
            }
        }

    private:
        TAsyncValueOpState* OpState;
    };

    using TInOpState = stdexec::connect_result_t<TInSender, TAsyncValueReceiver>;

public:
    TAsyncValueOpState(TInSender inSender, TFn fun, TReceiver receiver) noexcept
        : OpState(std::in_place_index<0>,
            Lazy([&]{
                return stdexec::connect(std::move(inSender), TAsyncValueReceiver(this, std::move(receiver)));
            }))
        , Fn(std::move(fun))
    {}

    friend void tag_invoke(stdexec::start_t, TAsyncValueOpState& op) noexcept {
        stdexec::start(std::get<0>(op.OpState));
    }

private:
    struct TStopCallback {
        void operator()() const noexcept {
            if constexpr (IsValueStopSource) {
                State->Value->request_stop();
            }
        }

        TAsyncValueOpState* State;
    };

    using TCallback = typename stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>::
            template callback_type<TStopCallback>;

private:
    std::variant<TInOpState, TOutOpState> OpState;
    TFn Fn;
    std::optional<TValue> Value;
    std::optional<TCallback> StopCallback;
};

}
