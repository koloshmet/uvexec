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
#include <uvexec/algorithms/send.hpp>
#include <uvexec/algorithms/receive.hpp>
#include <uvexec/algorithms/close.hpp>
#include <uvexec/algorithms/connect.hpp>


namespace NUvExec {

class TUdpSocket {
public:
    using endpoints = std::variant<TIp4Addr, TIp6Addr>;

    TUdpSocket(TLoop& loop, const TIp4Addr& addr);

    TUdpSocket(TLoop& loop, const TIp6Addr& addr);

    TUdpSocket(TUdpSocket&&) noexcept = delete;

    friend auto tag_invoke(NUvUtil::TRawUvObject, TUdpSocket& socket) noexcept -> uv_udp_t&;

    auto Loop() noexcept -> TLoop&;

private:
    TUdpSocket(EErrc& err, TLoop& loop, const TIp4Addr& addr) noexcept;

    TUdpSocket(EErrc& err, TLoop& loop, const TIp6Addr& addr) noexcept;

    template <stdexec::sender TInSender, std::move_constructible TFn, stdexec::receiver TReceiver> requires
        std::is_lvalue_reference_v<NMeta::TFnParameterType<TFn>> &&
        stdexec::sender<std::invoke_result_t<TFn, NMeta::TFnParameterType<TFn>>>
    friend class TBindToOpState;

private:
    uv_udp_t UvSocket;
};

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::close_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TCloseSender<std::decay_t<TSender>, TUdpSocket>(std::move(s.Sender), std::get<0>(s.Data));
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::send_to_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TSendToSender<std::decay_t<TSender>, TUdpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::receive_from_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TReceiveFromSender<std::decay_t<TSender>, TUdpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::connect_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TConnectSender<std::decay_t<TSender>, TUdpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::receive_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TReceiveSender<std::decay_t<TSender>, TUdpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

template <stdexec::sender TSender>
auto tag_invoke(TLoop::TDomain, TSenderPackage<uvexec::send_t, TSender, std::tuple<TUdpSocket&>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    return TSendSender<std::decay_t<TSender>, TUdpSocket>{std::move(s.Sender), &std::get<0>(s.Data)};
}

}
