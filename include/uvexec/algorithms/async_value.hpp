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

namespace NDetail {

struct TAsyncValueTypeImpl{
    template <typename TRet, typename TArg>
    constexpr auto operator()(TRet(*)(TArg)) -> TArg;

    template <typename TFn>
    constexpr auto operator()(TFn) -> NMeta::TMethodArgType<decltype(&TFn::operator())>;
};

template <typename TFn>
using TAsyncValueType = std::invoke_result_t<TAsyncValueTypeImpl, TFn>;

}

template <stdexec::sender TInSender, std::move_constructible TFun, stdexec::receiver TReceiver>
    requires std::is_lvalue_reference_v<NDetail::TAsyncValueType<TFun>> &&
            stdexec::sender<std::invoke_result_t<TFun, NDetail::TAsyncValueType<TFun>>>
class TAsyncValueOpState {
    using TValue = std::remove_reference_t<NDetail::TAsyncValueType<TFun>>;

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
                    std::invoke_result_t<TFun, TValue&>,
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
            op->OpState.template emplace<1>(Lazy([op, this] {
                return stdexec::connect(
                        exec::finally(
                                std::invoke(std::move(op->Fun), *op->Value),
                                stdexec::then(uvexec::drop(*op->Value), TDestroyer{op})),
                        std::move(*this).base());
            }));
            stdexec::start(std::get<1>(op->OpState));
        }

    private:
        TAsyncValueOpState* OpState;
    };

    using TInOpState = stdexec::connect_result_t<TInSender, TAsyncValueReceiver>;

public:
    TAsyncValueOpState(TInSender inSender, TFun fun, TReceiver receiver) noexcept
        : OpState(std::in_place_index<0>,
                Lazy([&]{
                    return stdexec::connect(std::move(inSender), TAsyncValueReceiver(this, std::move(receiver)));
                }))
        , Fun(std::move(fun))
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
    TFun Fun;
    std::optional<TValue> Value;
    std::optional<TCallback> StopCallback;
};

template <stdexec::sender TInSender, std::move_constructible TFun>
class TAsyncValueSender {
    using TValue = NDetail::TAsyncValueType<TFun>;
    
public:
    using sender_concept = stdexec::sender_t;

    TAsyncValueSender(TInSender inSender, TFun fun)
        : InSender(std::move(inSender)), Fun(std::move(fun))
    {}

    template <stdexec::receiver TRec>
    friend auto tag_invoke(stdexec::connect_t, TAsyncValueSender s, TRec&& rec) {
        return TAsyncValueOpState<TInSender, TFun, std::decay_t<TRec>>{
                std::move(s.InSender), std::move(s.Fun), std::forward<TRec>(rec)};
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAsyncValueSender& s) noexcept {
        return stdexec::get_env(s.InSender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAsyncValueSender&, const TEnv&) noexcept {
        return stdexec::completion_signatures_of_t<std::invoke_result_t<TFun, TValue&>, TEnv>{};
    }

private:
    TInSender InSender;
    TFun Fun;
};

}

namespace uvexec {

struct async_value_t {
    template <stdexec::sender TSender, std::move_constructible TFun>
    stdexec::sender auto operator()(TSender&& sender, TFun fun) const noexcept {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(
                d, NUvExec::TAsyncValueSender<TSender, TFun>{std::forward<TSender>(sender), std::move(fun)});
    }

    template <std::move_constructible TFun>
    auto operator()(TFun fun) const noexcept {
        return NUvExec::TArgBinder<TFun, async_value_t>(std::move(fun));
    }
};

inline constexpr async_value_t async_value;

}
