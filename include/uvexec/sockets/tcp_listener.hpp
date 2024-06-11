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

#include "tcp.hpp"
#include "uvexec/execution/error_code.hpp"


namespace NUvExec {

class TTcpListener {
public:
    using socket_type = TTcpSocket;
    using endpoints = typename socket_type::endpoints;

    struct TAccept : TIntrusiveListNode<TAccept> {
        virtual void Accept() noexcept = 0;
        virtual void Error(NUvUtil::TUvError) noexcept = 0;
    };

    template <stdexec::sender TSender, stdexec::receiver_of<TAlgorithmCompletionSignatures> TReceiver>
    class TAcceptOpState final : public TAccept {
        class TAcceptReceiver final : public stdexec::receiver_adaptor<TAcceptReceiver, TReceiver> {
            friend stdexec::receiver_adaptor<TAcceptReceiver, TReceiver>;

        public:
            TAcceptReceiver(TAcceptOpState& op, TReceiver&& rec) noexcept
                : stdexec::receiver_adaptor<TAcceptReceiver, TReceiver>(std::move(rec)), Op{&op}
            {}

            void set_value() noexcept {
                Op->Receiver.emplace(std::move(*this).base());
                Op->Listener->RegisterAccept(*Op);
                Op->StopOp.Setup();
            }

        private:
            TAcceptOpState* Op;
        };

        using TOpState = stdexec::connect_result_t<TSender, TAcceptReceiver>;

    public:
        TAcceptOpState(TTcpListener& listener, TTcpSocket& socket, TSender&& sender, TReceiver&& receiver) noexcept
            : StopOp(StopCallback,
                *this,
                *static_cast<TLoop*>(NUvUtil::GetLoop(NUvUtil::RawUvObject(socket)).data),
                stdexec::get_stop_token(stdexec::get_env(receiver)))
            , Op(stdexec::connect(std::move(sender), TAcceptReceiver(*this, std::move(receiver))))
            , Listener{&listener}
            , Socket{&socket}
        {}

        friend void tag_invoke(stdexec::start_t, TAcceptOpState& op) noexcept {
            stdexec::start(op.Op);
        }

        void Accept() noexcept override {
            if (StopOp.Reset()) {
                return;
            }
            auto err = NUvUtil::Accept(NUvUtil::RawUvObject(*Listener), NUvUtil::RawUvObject(*Socket));
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(*std::move(Receiver), EErrc{err});
            } else {
                stdexec::set_value(*std::move(Receiver));
            }
        }
        void Error(NUvUtil::TUvError err) noexcept override {
            if (StopOp.Reset()) {
                return;
            }
            stdexec::set_error(*std::move(Receiver), EErrc{err});
        }

        static void StopCallback(TAcceptOpState& op) noexcept {
            op.Listener->AcceptList.Erase(op);
            op.StopOp.ResetUnsafe();
            stdexec::set_stopped(*std::move(op.Receiver));
        }

        using TStopToken = stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>;

    private:
        TLoop::TStopOperation<TAcceptOpState, TStopToken> StopOp;
        TOpState Op;
        TTcpListener* Listener;
        TTcpSocket* Socket;
        std::optional<TReceiver> Receiver;
    };

    template <NMeta::IsIn<endpoints> TEp>
    TTcpListener(TLoop& loop, const TEp& ep, unsigned short backlog = 4096)
        : Socket(loop)
        , AcceptList{}
        , PendingConnections{-static_cast<int>(backlog)}
    {
        NUvUtil::Assert(NUvUtil::Bind(NUvUtil::RawUvObject(Socket), NUvUtil::RawUvObject(ep)));
    }

    void RegisterAccept(TAccept& accept);

    friend auto tag_invoke(NUvUtil::TRawUvObject, TTcpListener& listener) noexcept -> uv_tcp_t&;

    template <stdexec::sender TSender>
    friend auto tag_invoke(
            TLoop::TDomain, TSenderPackage<uvexec::close_t, TSender, std::tuple<TTcpListener&>> s) noexcept(
            std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
        return TCloseSender<std::decay_t<TSender>, TTcpSocket>(std::move(s.Sender), std::get<0>(s.Data).Socket);
    }

    auto Loop() noexcept -> TLoop&;

private:
    auto StartListening() -> NUvUtil::TUvError;

    static void ConnectionCallback(uv_stream_t* server, int status);

private:
    socket_type Socket;
    TIntrusiveList<TAccept> AcceptList;
    int PendingConnections;
};

}
