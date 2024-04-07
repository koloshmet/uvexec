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

#include <stdexec/execution.hpp>


namespace NUvExec {

template <typename TSocket, typename TAlgorithm>
class TSocketBinder : public stdexec::sender_adaptor_closure<TSocketBinder<TSocket, TAlgorithm>> {
public:
    explicit TSocketBinder(TSocket& socket) noexcept: Socket(&socket) {}

    template <stdexec::sender TSender>
    stdexec::sender auto operator()(TSender&& sender) const noexcept(
            std::is_nothrow_invocable_v<TAlgorithm, TSender&&, TSocket&>) {
        return TAlgorithm{}(std::forward<TSender>(sender), *Socket);
    }

private:
    TSocket* Socket;
};

template <typename TSocket, typename TArg, typename TAlgorithm>
    requires std::is_nothrow_move_constructible_v<TArg>
class TSocketArgBinder : public stdexec::sender_adaptor_closure<TSocketArgBinder<TSocket, TArg, TAlgorithm>> {
public:
    TSocketArgBinder(TArg arg, TSocket& socket) noexcept: Arg(std::move(arg)), Socket(&socket) {}

    template <stdexec::sender TSender>
    stdexec::sender auto operator()(TSender&& sender) && noexcept(
            std::is_nothrow_invocable_v<TAlgorithm, TSender&&, TSocket&, TArg&&>) {
        return TAlgorithm{}(std::forward<TSender>(sender), *Socket, std::move(Arg));
    }

private:
    TArg Arg;
    TSocket* Socket;
};

template <typename TArg, typename TAlgorithm> requires std::is_nothrow_move_constructible_v<TArg>
class TArgBinder : public stdexec::sender_adaptor_closure<TArgBinder<TArg, TAlgorithm>> {
public:
    explicit TArgBinder(TArg arg) noexcept: Arg(std::move(arg)) {}

    template <stdexec::sender TSender>
    stdexec::sender auto operator()(TSender&& sender) && noexcept(
            std::is_nothrow_invocable_v<TAlgorithm, TSender&&, TArg&&>) {
        return TAlgorithm{}(std::forward<TSender>(sender), std::move(Arg));
    }

private:
    TArg Arg;
};

}
