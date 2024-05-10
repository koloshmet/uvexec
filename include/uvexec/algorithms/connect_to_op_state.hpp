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

#include <uvexec/execution/loop.hpp>

#include <exec/finally.hpp>
#include <uvexec/util/lazy.hpp>


namespace NUvExec {

template <stdexec::sender TInSender, std::move_constructible TFn, stdexec::receiver TReceiver> requires
        std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
        stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
class TConnectToOpState {
    using TSocket = std::decay_t<NMeta::TFnParameterType<TFn>>;
    using TEnv = stdexec::env_of_t<TReceiver>;
    using TConnectSender = std::invoke_result_t<uvexec::connect_t, TInSender, TSocket&>;
    using TConnectErrors = stdexec::error_types_of_t<TConnectSender, TEnv>;
    using TConnectErrorsResult = std::conditional_t<
            NMeta::IsIn<std::exception_ptr, TConnectErrors>,
            TConnectErrors,
            NMeta::TBindFront<std::exception_ptr, TConnectErrors>>;
    using TConnectResult = std::conditional_t<
            NMeta::IsIn<stdexec::set_stopped_t(), stdexec::completion_signatures_of_t<TConnectSender, TEnv>>,
            NMeta::TBindFront<stdexec::set_stopped_t, TConnectErrorsResult>,
            TConnectErrorsResult>;

    class TConnectErrorReceiver : public stdexec::receiver_adaptor<TConnectErrorReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TConnectErrorReceiver, TReceiver>;

    public:
        explicit TConnectErrorReceiver(TConnectResult&& result, TReceiver receiver) noexcept
            : stdexec::receiver_adaptor<TConnectErrorReceiver, TReceiver>(std::move(receiver))
            , ConnectResult(std::move(result))
        {}

        void set_value() noexcept {
            std::visit([this]<typename T>(T&& err) noexcept {
                if constexpr (std::same_as<std::decay_t<T>, stdexec::set_stopped_t>) {
                    stdexec::set_stopped(std::move(*this).base());
                } else {
                    stdexec::set_error(std::move(*this).base(), std::forward<T>(err));
                }
            }, std::move(ConnectResult));
        }

    private:
        TConnectResult ConnectResult;
    };

    using TOutOpState = stdexec::connect_result_t<
            std::invoke_result_t<exec::finally_t,
                    std::invoke_result_t<TFn, TSocket&>,
                    std::invoke_result_t<uvexec::close_t, TSocket&>>,
            TReceiver>;

    class TConnectToReceiver {
    public:
        using receiver_concept = stdexec::receiver_t;

        explicit TConnectToReceiver(TConnectToOpState* opState, TReceiver receiver) noexcept
            : Receiver(std::move(receiver))
            , OpState{opState}
        {}

        friend auto tag_invoke(stdexec::get_env_t, const TConnectToReceiver& rec) noexcept {
            return stdexec::get_env(rec.Receiver);
        }

        template <typename TError>
        friend void tag_invoke(stdexec::set_error_t, TConnectToReceiver&& rec, TError error) noexcept {
            rec.OpState->EmplaceAndStartClose(std::move(rec.Receiver), std::move(error));
        }
        template <std::same_as<stdexec::set_stopped_t> TTag>
        friend void tag_invoke(TTag, TConnectToReceiver&& rec) noexcept {
            rec.OpState->EmplaceAndStartClose(std::move(rec.Receiver), stdexec::set_stopped_t{});
        }
        template <std::same_as<stdexec::set_value_t> TTag>
        friend void tag_invoke(TTag, TConnectToReceiver&& rec) noexcept {
            auto op = rec.OpState;
            try {
                op->EmplaceAndStartBody(std::move(rec.Receiver));
            } catch (...) {
                op->EmplaceAndStartClose(std::move(rec.Receiver), std::current_exception());
            }
        }

    private:
        TReceiver Receiver;
        TConnectToOpState* OpState;
    };

    using TInOpState = stdexec::connect_result_t<
            std::invoke_result_t<uvexec::connect_t, TInSender, TSocket&>, TConnectToReceiver>;
    using TCloseOnErrorOpState = stdexec::connect_result_t<
            std::invoke_result_t<uvexec::close_t, TSocket&>, TConnectErrorReceiver>;

public:
    TConnectToOpState(TInSender inSender, TFn fun, TReceiver receiver) noexcept
        : InSender(std::move(inSender))
        , Receiver(std::move(receiver))
        , Fn(std::move(fun))
    {}

    friend void tag_invoke(stdexec::start_t, TConnectToOpState& op) noexcept {
        try {
            auto env = stdexec::get_env(op.Receiver);
            using TEndpoint = NMeta::TRemoveReferenceWrapperType<std::tuple_element_t<
                    0, stdexec::value_types_of_t<TInSender, TEnv, NMeta::TDecayedTuple, std::type_identity_t>>>;
            if constexpr (std::constructible_from<TSocket, TLoop&>) {
                op.Socket.emplace(TLoop::TDomain{}.GetLoop(stdexec::get_scheduler(env)));
            } else {
                op.Socket.emplace(TLoop::TDomain{}.GetLoop(stdexec::get_scheduler(env)), DefaultAddr<TEndpoint>());
            }
            op.OpState.template emplace<1>(Lazy([&]{
                return stdexec::connect(
                        uvexec::connect(std::move(op.InSender), *op.Socket),
                        TConnectToReceiver(&op, std::move(op.Receiver)));
            }));
            stdexec::start(std::get<1>(op.OpState));
        } catch (...) {
            stdexec::set_error(std::move(op.Receiver), std::current_exception());
        }
    }

private:
    void EmplaceAndStartBody(TReceiver&& rec) {
        OpState.template emplace<2>(Lazy([&] {
            return stdexec::connect(
                    exec::finally(
                            std::invoke(std::move(Fn), *Socket),
                            uvexec::close(*Socket)),
                    std::move(rec));
        }));
        stdexec::start(std::get<2>(OpState));
    }

    template <typename TFin>
    void EmplaceAndStartClose(TReceiver rec, TFin fin) {
        OpState.template emplace<3>(Lazy([&] {
            return stdexec::connect(
                    uvexec::close(*Socket),
                    TConnectErrorReceiver(TConnectResult(std::move(fin)), std::move(rec)));
        }));
        stdexec::start(std::get<3>(OpState));
    }

private:
    std::optional<TSocket> Socket;
    std::variant<std::monostate, TInOpState, TOutOpState, TCloseOnErrorOpState> OpState;
    TInSender InSender;
    TReceiver Receiver;
    TFn Fn;
};

}
