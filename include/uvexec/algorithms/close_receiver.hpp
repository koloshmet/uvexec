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

template <stdexec::receiver TReceiver, typename THandle>
class TCloseReceiver : public stdexec::receiver_adaptor<TCloseReceiver<TReceiver, THandle>, TReceiver> {
    friend stdexec::receiver_adaptor<TCloseReceiver, TReceiver>;

public:
    TCloseReceiver(THandle& handle, TReceiver rec) noexcept
        : stdexec::receiver_adaptor<TCloseReceiver, TReceiver>(std::move(rec)), Handle{&handle}
    {}

    void set_value() noexcept {
        NUvUtil::RawUvObject(*Handle).data = this;
        NUvUtil::Close(NUvUtil::RawUvObject(*Handle), CloseCallback);
    }

private:
    static void CloseCallback(uv_handle_t* handle) {
        auto self = NUvUtil::GetData<TCloseReceiver>(handle);
        stdexec::set_value(std::move(*self).base());
    }

private:
    THandle* Handle;
};

}
