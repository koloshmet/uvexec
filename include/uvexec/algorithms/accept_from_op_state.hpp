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

template <stdexec::sender TInSender, typename TListener, std::move_constructible TFn, stdexec::receiver TReceiver>
    requires
        std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
        std::same_as<std::decay_t<NMeta::TFnParameterType<TFn>>, uvexec::socket_type_t<TListener>> &&
        stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
class TAcceptFromOpState {
    using TSocket = uvexec::socket_type_t<TListener>;
    using TEnv = stdexec::env_of_t<TReceiver>;
    using TAcceptSender = std::invoke_result_t<uvexec::accept_t, TInSender, TListener&, TSocket&>;
    using TAcceptErrors = stdexec::error_types_of_t<TAcceptSender, TEnv>;
    using TAcceptResult = NMeta::TBindFront<
            stdexec::set_stopped_t,
            std::conditional_t<
                    NMeta::IsIn<std::exception_ptr, TAcceptErrors>,
                    TAcceptErrors,
                    NMeta::TBindFront<std::exception_ptr, TAcceptErrors>>>;

    class TAcceptErrorReceiver : public stdexec::receiver_adaptor<TAcceptErrorReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TAcceptErrorReceiver, TReceiver>;

    public:
        explicit TAcceptErrorReceiver(TAcceptResult&& result, TReceiver receiver) noexcept
            : stdexec::receiver_adaptor<TAcceptErrorReceiver, TReceiver>(std::move(receiver))
            , AcceptResult(std::move(result))
        {}

        void set_value() noexcept {
            std::visit([this]<typename T>(T&& err) noexcept {
                if constexpr (std::same_as<std::decay_t<T>, stdexec::set_stopped_t>) {
                    stdexec::set_stopped(std::move(*this).base());
                } else {
                    stdexec::set_error(std::move(*this).base(), std::forward<T>(err));
                }
            }, std::move(AcceptResult));
        }

    private:
        TAcceptResult AcceptResult;
    };

    using TOutOpState = stdexec::connect_result_t<
            std::invoke_result_t<exec::finally_t,
                    std::invoke_result_t<TFn, TSocket&>,
                    std::invoke_result_t<uvexec::close_t, TSocket&>>,
            TReceiver>;

    class TAcceptFromReceiver : public stdexec::receiver_adaptor<TAcceptFromReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TAcceptFromReceiver, TReceiver>;

    public:
        explicit TAcceptFromReceiver(TAcceptFromOpState* opState, TReceiver receiver) noexcept
            : stdexec::receiver_adaptor<TAcceptFromReceiver, TReceiver>(std::move(receiver)), OpState{opState}
        {}

        void set_value() noexcept {
            auto op = OpState;
            try {
                op->EmplaceAndStartBody(std::move(*this).base());
            } catch (...) {
                set_error(std::current_exception());
            }
        }

        template <typename TError>
        void set_error(TError error) noexcept {
            auto op = OpState;
            op->EmplaceAndStartClose(std::move(*this).base(), std::move(error));
        }

        void set_stopped() noexcept {
            auto op = OpState;
            op->EmplaceAndStartClose(std::move(*this).base(), stdexec::set_stopped_t{});
        }

    private:
        TAcceptFromOpState* OpState;
    };

    using TInOpState = stdexec::connect_result_t<TAcceptSender, TAcceptFromReceiver>;
    using TCloseOnErrorOpState = stdexec::connect_result_t<
            std::invoke_result_t<uvexec::close_t, TSocket&>, TAcceptErrorReceiver>;

public:
    TAcceptFromOpState(TInSender inSender, TListener& listener, TFn fun, TReceiver receiver) noexcept
        : InSender(std::move(inSender))
        , Receiver(std::move(receiver))
        , Fn(std::move(fun))
        , Listener{&listener}
    {}

    friend void tag_invoke(stdexec::start_t, TAcceptFromOpState& op) noexcept {
        op.EmplaceAndStartInitially();
    }

    void EmplaceAndStartInitially() {
        EErrc err;
        Socket.emplace(Lazy([&]() noexcept {
            return TSocket(err, TLoop::TDomain{}.GetLoop(stdexec::get_scheduler(stdexec::get_env(Receiver))));
        }));
        if (err != EErrc{0}) {
            stdexec::set_error(std::move(Receiver), std::move(err));
            return;
        }
        try {
            OpState.template emplace<1>(Lazy([&] {
                return stdexec::connect(
                        uvexec::accept(std::move(InSender), *Listener, *Socket),
                        TAcceptFromReceiver(this, std::move(Receiver)));
            }));
            stdexec::start(std::get<1>(OpState));
        } catch (...) {
            stdexec::set_error(std::move(Receiver), std::current_exception());
        }
    }

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
    void EmplaceAndStartClose(TReceiver&& rec, TFin fin) {
        OpState.template emplace<3>(Lazy([&] {
            return stdexec::connect(
                    uvexec::close(*Socket),
                    TAcceptErrorReceiver(TAcceptResult(std::move(fin)), std::move(rec)));
        }));
        stdexec::start(std::get<3>(OpState));
    }

private:
    std::optional<TSocket> Socket;
    std::variant<std::monostate, TInOpState, TOutOpState, TCloseOnErrorOpState> OpState;
    TInSender InSender;
    TReceiver Receiver;
    TFn Fn;
    TListener* Listener;
};

}
