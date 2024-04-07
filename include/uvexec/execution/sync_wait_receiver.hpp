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

#include <uvexec/uv_util/errors.hpp>
#include <uvexec/meta/meta.hpp>

#include <stdexec/execution.hpp>


namespace NUvExec {

template <typename TSyncWaitEnv, stdexec::sender_in<TSyncWaitEnv> TSender>
using TSyncWaitType = stdexec::value_types_of_t<TSender, TSyncWaitEnv, NMeta::TDecayedTuple, std::type_identity_t>;

template <typename TSyncWaitEnv, stdexec::sender_in<TSyncWaitEnv> TSender, std::invocable TWakeup>
class TSyncWaitReceiver : public stdexec::receiver_adaptor<TSyncWaitReceiver<TSyncWaitEnv, TSender, TWakeup>> {
public:
    using T = TSyncWaitType<TSyncWaitEnv, TSender>;
    using TState = std::variant<std::monostate, T, stdexec::set_stopped_t, std::exception_ptr>;

    explicit TSyncWaitReceiver(TState& dest, TSyncWaitEnv env, TWakeup wakeup) noexcept
        : Destination{&dest}
        , Env(std::move(env))
        , Wakeup(std::move(wakeup))
    {}

    friend stdexec::receiver_adaptor<TSyncWaitReceiver>;

    template <typename TErr>
    void set_error(TErr err) noexcept {
        if constexpr (std::same_as<std::decay_t<TErr>, std::exception_ptr>) {
            Destination->template emplace<3>(std::move(err));
        } else if constexpr (std::same_as<std::decay_t<TErr>, std::error_code>) {
            Destination->template emplace<3>(std::make_exception_ptr(std::system_error(err)));
        } else if constexpr (std::same_as<std::decay_t<TErr>, NUvUtil::TUvError>) {
            Destination->template emplace<3>(std::make_exception_ptr(NUvUtil::MakeRuntimeError(err)));
        } else {
            Destination->template emplace<3>(std::make_exception_ptr(std::move(err)));
        }
        std::invoke(std::move(Wakeup));
    }

    template <typename... Ts> requires std::constructible_from<T, Ts...>
    void set_value(Ts&&... value) noexcept {
        try {
            Destination->template emplace<1>(std::forward<Ts>(value)...);
        } catch (...) {
            set_error(std::current_exception());
        }
        Wakeup();
    }
    void set_stopped() noexcept {
        Destination->template emplace<2>();
        std::invoke(std::move(Wakeup));
    }

    [[nodiscard]]
    TSyncWaitEnv get_env() const noexcept {
        return Env;
    }

private:
    TState* Destination;
    TSyncWaitEnv Env;
    TWakeup Wakeup;
};

template <typename TSyncWaitEnv, stdexec::sender_in<TSyncWaitEnv> TSender, std::invocable TWakeup>
using TSyncWaitReceiverState = typename TSyncWaitReceiver<TSyncWaitEnv, TSender, TWakeup>::TState;

template <typename TSyncWaitEnv, stdexec::sender_in<TSyncWaitEnv> TSender, std::invocable TWakeup>
std::optional<TSyncWaitType<TSyncWaitEnv, TSender>> UnwrapSyncWaitReceiverState(
        TSyncWaitReceiverState<TSyncWaitEnv, TSender, TWakeup> state) {
    switch (state.index()) {
        case 3:
            std::rethrow_exception(std::get<3>(state));
        case 2:
            return std::nullopt;
        default:
            return std::get<1>(std::move(state));
    }
}

}
