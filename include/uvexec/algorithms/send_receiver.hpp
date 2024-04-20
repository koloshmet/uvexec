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

#include "completion_signatures.hpp"

#include <uvexec/uv_util/reqs.hpp>

#include <span>


namespace NUvExec {

template <stdexec::receiver TReceiver, typename TSocket>
class TSendReceiver : public stdexec::receiver_adaptor<TSendReceiver<TReceiver, TSocket>, TReceiver> {
    friend stdexec::receiver_adaptor<TSendReceiver, TReceiver>;

public:
    TSendReceiver(TSocket& socket, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TSendReceiver, TReceiver>(std::move(rec)), Handle{&socket}
    {}

    void set_value(std::span<const uv_buf_t> buffs) noexcept {
        WriteReq.data = this;
        auto err = NUvUtil::Write(WriteReq, NUvUtil::RawUvObject(*Handle), buffs, WriteCallback);
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(*this).base(), err);
        }
    }

    void set_value(std::span<std::byte> buff) noexcept {
        Buf.base = reinterpret_cast<char*>(buff.data());
        Buf.len = buff.size();
        set_value(std::span(&Buf, 1));
    }

private:
    static void WriteCallback(uv_write_t* req, NUvUtil::TUvError status) {
        auto self = static_cast<TSendReceiver*>(req->data);
        if (NUvUtil::IsError(status)) {
            stdexec::set_error(std::move(*self).base(), status);
        } else {
            stdexec::set_value(std::move(*self).base());
        }
    }

private:
    uv_write_t WriteReq;
    uv_buf_t Buf;
    TSocket* Handle;
};

}
