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
class TBindToOpState {
    using TSocket = std::decay_t<NMeta::TFnParameterType<TFn>>;

    using TOutOpState = stdexec::connect_result_t<
            std::invoke_result_t<exec::finally_t,
                    std::invoke_result_t<TFn, TSocket&>,
                    std::invoke_result_t<uvexec::close_t, TSocket&>>,
            TReceiver>;

    class TBindToReceiver : public stdexec::receiver_adaptor<TBindToReceiver, TReceiver> {
        friend stdexec::receiver_adaptor<TBindToReceiver, TReceiver>;

    public:
        explicit TBindToReceiver(TBindToOpState* opState, TReceiver receiver) noexcept
            : stdexec::receiver_adaptor<TBindToReceiver, TReceiver>(std::move(receiver))
            , OpState{opState}
        {}

        template <typename TEp>
        void set_value(const TEp& ep) noexcept {
            auto err = OpState->EmplaceAndStartBody(std::move(*this).base(), ep);
            if (err != EErrc{0}) {
                stdexec::set_error(std::move(*this).base(), std::move(err));
            }
        }

    private:
        TBindToOpState* OpState;
    };

    using TInOpState = stdexec::connect_result_t<TInSender, TBindToReceiver>;

public:
    TBindToOpState(TInSender inSender, TFn fun, TReceiver receiver) noexcept
        : OpState(std::in_place_index<0>, Lazy([&]{
            return stdexec::connect(
                    std::move(inSender),
                    TBindToReceiver(this, std::move(receiver)));
        }))
        , Fn(std::move(fun))
    {}

    friend void tag_invoke(stdexec::start_t, TBindToOpState& op) noexcept {
        stdexec::start(std::get<0>(op.OpState));
    }

    template <typename TEp>
    auto EmplaceAndStartBody(TReceiver&& rec, const TEp& ep) -> EErrc {
        EErrc err;
        Socket.emplace(Lazy([&] {
            return TSocket(err, TLoop::TDomain{}.GetLoop(stdexec::get_scheduler(stdexec::get_env(rec))), ep);
        }));
        if (err != EErrc{0}) {
            return err;
        }
        OpState.template emplace<1>(Lazy([&] {
            return stdexec::connect(
                    exec::finally(
                            std::invoke(std::move(Fn), *Socket),
                            uvexec::close(*Socket)),
                    std::move(rec));
        }));
        stdexec::start(std::get<1>(OpState));
        return EErrc{0};
    }

private:
    std::optional<TSocket> Socket;
    std::variant<TInOpState, TOutOpState> OpState;
    TFn Fn;
};

}
