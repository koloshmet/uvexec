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
#include <uvexec/uv_util/misc.hpp>


namespace NUvExec {

template <stdexec::receiver TReceiver, typename TStreamSocket>
class TShutdownReceiver : public stdexec::receiver_adaptor<TShutdownReceiver<TReceiver, TStreamSocket>, TReceiver> {
    friend stdexec::receiver_adaptor<TShutdownReceiver, TReceiver>;

public:
    TShutdownReceiver(TStreamSocket& socket, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TShutdownReceiver, TReceiver>(std::move(rec)), Socket{&socket}
    {}

    void set_value() noexcept {
        ShutdownReq.data = this;
        auto err = NUvUtil::Shutdown(ShutdownReq, NUvUtil::RawUvObject(*Socket), ShutdownCallback);
        if (NUvUtil::IsError(err)) {
            stdexec::set_error(std::move(*this).base(), err);
        }
    }

private:
    static void ShutdownCallback(uv_shutdown_t* req, NUvUtil::TUvError status) {
        auto self = static_cast<TShutdownReceiver*>(req->data);
        if (NUvUtil::IsError(status)) {
            stdexec::set_error(std::move(*self).base(), status);
        } else {
            stdexec::set_value(std::move(*self).base());
        }
    }

private:
    uv_shutdown_t ShutdownReq;
    TStreamSocket* Socket;
};

}
