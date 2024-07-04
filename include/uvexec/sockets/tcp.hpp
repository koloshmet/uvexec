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

#include "addr.hpp"

#include <uvexec/execution/loop.hpp>
#include <uvexec/algorithms/read_until.hpp>
#include <uvexec/algorithms/read_some.hpp>
#include <uvexec/algorithms/write.hpp>
#include <uvexec/algorithms/close.hpp>
#include <uvexec/algorithms/connect.hpp>
#include <uvexec/algorithms/shutdown.hpp>


namespace NUvExec {

class TTcpSocket {
public:
    using endpoints = std::variant<TIp4Addr, TIp6Addr>;

    explicit TTcpSocket(TLoop& loop);

    TTcpSocket(TTcpSocket&&) noexcept = delete;

    friend class TTcpListener;

    friend auto tag_invoke(NUvUtil::TRawUvObject, TTcpSocket& socket) noexcept -> uv_tcp_t&;

    auto Loop() noexcept -> TLoop&;

private:
    TTcpSocket(EErrc& err, TLoop& loop);

    template <stdexec::sender TInSender, typename TListener, std::move_constructible TFn, stdexec::receiver TReceiver>
        requires
            std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
            std::same_as<std::decay_t<NMeta::TFnParameterType<TFn>>, uvexec::socket_type_t<TListener>> &&
            stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
    friend class TAcceptFromOpState;

    template <stdexec::sender TInSender, std::move_constructible TFn, stdexec::receiver TReceiver> requires
        std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
        stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
    friend class TConnectToOpState;

private:
    uv_tcp_t UvSocket;
};

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::close_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TCloseSender<std::decay_t<TSender>, TTcpSocket>(std::move(s.Sender), std::get<0>(s.Data));
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::connect_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TConnectSender<std::decay_t<TSender>, TTcpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::shutdown_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TShutdownSender<std::decay_t<TSender>, TTcpSocket>(std::move(s.Sender), std::get<0>(s.Data));
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::send_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TWriteSender<std::decay_t<TSender>, TTcpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::write_some_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TWriteSender<std::decay_t<TSender>, TTcpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::receive_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TReadSomeSender<std::decay_t<TSender>, TTcpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::read_some_t, TSender, std::tuple<TTcpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TReadSomeSender<std::decay_t<TSender>, TTcpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender, typename TCondition>
    requires std::is_nothrow_invocable_r_v<bool, TCondition, std::size_t>
auto tag_invoke(
        TLoop::TDomain,
        TSenderPackage<uvexec::read_until_t, TSender, std::tuple<TTcpSocket&, TCondition>> s) noexcept(
                std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender> &&
                std::is_nothrow_constructible_v<std::decay_t<TCondition>, TCondition>) {
    return TReadUntilSender<std::decay_t<TSender>, TTcpSocket, std::decay_t<TCondition>>{
            std::move(s.Sender), std::move(std::get<1>(s.Data)), &std::get<0>(s.Data)};
}

}
