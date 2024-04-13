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

#include "domain.hpp"
#include "closure.hpp"

#include <uvexec/meta/meta.hpp>


namespace uvexec {

struct drop_t {
    template <typename TValue>
    stdexec::sender auto operator()(TValue& value) const noexcept(
            stdexec::nothrow_tag_invocable<drop_t, TValue&>) {
        return stdexec::tag_invoke(drop_t{}, value);
    }
};

struct schedule_upon_signal_t {
    template <stdexec::scheduler TScheduler, typename TSignal>
    stdexec::sender auto operator()(TScheduler&& scheduler, TSignal signal) const noexcept(
            stdexec::nothrow_tag_invocable<schedule_upon_signal_t, TScheduler, TSignal>) {
        return stdexec::tag_invoke(*this, std::forward<TScheduler>(scheduler), signal);
    }
};


template <typename TSocket>
using endpoints_of_t = typename TSocket::endpoints;
template <typename TSocketListener>
using socket_type_t = typename TSocketListener::socket_type;

struct connect_t {
    template <typename TSocket, NMeta::IsIn<endpoints_of_t<TSocket>> TEp>
    stdexec::sender auto operator()(TSocket& socket, TEp&& ep) const noexcept(
            std::is_nothrow_invocable_v<connect_t, NUvExec::TJustSender<std::decay_t<TEp>>, TSocket&>) {
        return (*this)(stdexec::just(std::forward<TEp>(ep)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<TSocket, connect_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<connect_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<connect_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct accept_t {
    template <typename TSocketListener>
    stdexec::sender auto operator()(TSocketListener& listener, socket_type_t<TSocketListener>& socket) const noexcept(
            std::is_nothrow_invocable_v<accept_t, NUvExec::TJustSender<>, socket_type_t<TSocketListener>&>) {
        return (*this)(stdexec::just(), listener, socket);
    }

    template <stdexec::sender TSender, typename TSocketListener>
    stdexec::sender auto operator()(
            TSender&& sender, TSocketListener& listener, socket_type_t<TSocketListener>& socket) const noexcept(
            stdexec::nothrow_tag_invocable<accept_t, TSender, socket_type_t<TSocketListener>&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<accept_t>(
                std::forward<TSender>(sender), std::make_tuple(std::ref(listener), std::ref(socket))));
    }
};

struct shutdown_t {
    template <typename TSocket>
    stdexec::sender auto operator()(TSocket& socket) const noexcept(
            std::is_nothrow_invocable_v<shutdown_t, NUvExec::TJustSender<>, TSocket&>) {
        return (*this)(stdexec::just(), socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<shutdown_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<shutdown_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct close_t {
    template <typename TSocket>
    stdexec::sender auto operator()(TSocket& socket) const noexcept(
            std::is_nothrow_invocable_v<close_t, NUvExec::TJustSender<>, TSocket&>) {
        return (*this)(stdexec::just(), socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<close_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<close_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct receive_t {
    template <typename TSocket, typename TMutableBufferSequence>
    stdexec::sender auto operator()(TSocket& socket, TMutableBufferSequence buffer) const noexcept(
            std::is_nothrow_invocable_v<receive_t, NUvExec::TJustSender<TMutableBufferSequence>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffer)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<TSocket, receive_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<receive_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<receive_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct read_some_t {
    template <typename TSocket, typename TMutableBufferSequence>
    stdexec::sender auto operator()(TSocket& socket, TMutableBufferSequence buffer) const noexcept(
            std::is_nothrow_invocable_v<read_some_t, NUvExec::TJustSender<TMutableBufferSequence>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffer)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<TSocket, read_some_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<read_some_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<read_some_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct read_until_t {
    template <typename TSocket, typename TMutableBufferSequence, typename TCondition>
        requires std::is_nothrow_invocable_r_v<bool, TCondition, std::size_t>
    stdexec::sender auto operator()(
            TSocket& socket, TMutableBufferSequence& buffer, TCondition&& condition) const noexcept(
            std::is_nothrow_invocable_v<read_until_t,
                    NUvExec::TJustSender<std::reference_wrapper<TMutableBufferSequence>>,
                    TSocket&, TCondition&&>) {
        return (*this)(stdexec::just(std::ref(buffer)), socket, std::forward<TCondition>(condition));
    }

    template <typename TSocket, typename TCondition>
    auto operator()(TSocket& socket, TCondition&& cond) const noexcept {
        return NUvExec::TSocketArgBinder<TSocket, std::decay_t<TCondition>, read_until_t>(
                std::forward<TCondition>(cond), socket);
    }

    template <stdexec::sender TSender, typename TSocket, typename TCondition>
        requires std::is_nothrow_invocable_r_v<bool, TCondition, std::size_t>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket, TCondition&& cond) const noexcept(
            stdexec::nothrow_tag_invocable<read_until_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d,
                NUvExec::MakeSenderPackage<read_until_t>(std::forward<TSender>(sender),
                        std::make_tuple(std::ref(socket), std::forward<TCondition>(cond))));
    }
};

struct receive_from_t {
    template <typename TSocket, typename TMutableBufferSequence, NMeta::IsIn<endpoints_of_t<TSocket>> TEp>
    stdexec::sender auto operator()(TSocket& socket, TMutableBufferSequence buffers, TEp& ep) const noexcept(
            std::is_nothrow_invocable_v<receive_from_t,
                    NUvExec::TJustSender<TMutableBufferSequence, std::reference_wrapper<TEp>>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffers), std::ref(ep)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<std::decay_t<TSocket>, receive_from_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<receive_from_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<receive_from_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct send_t {
    template <typename TSocket, typename TConstBufferSequence>
    stdexec::sender auto operator()(TSocket& socket, TConstBufferSequence buffers) const noexcept(
            std::is_nothrow_invocable_v<send_t, NUvExec::TJustSender<TConstBufferSequence>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffers)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<std::decay_t<TSocket>, send_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<send_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<send_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct write_some_t {
    template <typename TSocket, typename TConstBufferSequence>
    stdexec::sender auto operator()(TSocket& socket, TConstBufferSequence buffers) const noexcept(
            std::is_nothrow_invocable_v<write_some_t, NUvExec::TJustSender<TConstBufferSequence>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffers)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<std::decay_t<TSocket>, write_some_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<write_some_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<write_some_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

struct send_to_t {
    template <typename TSocket, typename TConstBufferSequence, NMeta::IsIn<endpoints_of_t<TSocket>> TEp>
    stdexec::sender auto operator()(TSocket& socket, TConstBufferSequence buffers, TEp&& ep) const noexcept(
            std::is_nothrow_invocable_v<send_to_t,
                    NUvExec::TJustSender<TConstBufferSequence, std::decay_t<TEp>>, TSocket&>) {
        return (*this)(stdexec::just(std::move(buffers), std::forward<TEp>(ep)), socket);
    }

    template <typename TSocket>
    auto operator()(TSocket& socket) const noexcept {
        return NUvExec::TSocketBinder<std::decay_t<TSocket>, send_to_t>(socket);
    }

    template <stdexec::sender TSender, typename TSocket>
    stdexec::sender auto operator()(TSender&& sender, TSocket& socket) const noexcept(
            stdexec::nothrow_tag_invocable<send_t, TSender, TSocket&>) {
        auto d = NUvExec::GetEarlyDomain(sender);
        return stdexec::transform_sender(d, NUvExec::MakeSenderPackage<send_to_t>(
                std::forward<TSender>(sender), std::tuple<TSocket&>(socket)));
    }
};

// Generic async destructor
inline constexpr drop_t drop;

// Signal handling
inline constexpr schedule_upon_signal_t schedule_upon_signal;

// Generic data stream operations
inline constexpr close_t close;
inline constexpr read_some_t read_some;
inline constexpr read_until_t read_until;
inline constexpr write_some_t write_some;

// Socket stream operations
inline constexpr connect_t connect;
inline constexpr accept_t accept;
inline constexpr shutdown_t shutdown;
inline constexpr receive_t receive;
inline constexpr send_t send;

// Socket datagram operations
inline constexpr receive_from_t receive_from;
inline constexpr send_to_t send_to;

}
