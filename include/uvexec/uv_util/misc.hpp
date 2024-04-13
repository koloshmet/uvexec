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

#include "errors.hpp"
#include <uv.h>

#include <stdexec/execution.hpp>


namespace NUvUtil {

struct TRawUvObject {
    template <typename TObject>
    auto operator()(TObject& obj) const noexcept -> auto& {
        return stdexec::tag_invoke(TRawUvObject{}, obj);
    }
};

inline constexpr TRawUvObject RawUvObject;

template <typename TObject>
using TRawUvObjectType = stdexec::tag_invoke_result_t<TRawUvObject, TObject&>;

namespace NDetail {

auto GetData(const uv_handle_t* handle) -> void*;
auto GetData(const uv_stream_t* handle) -> void*;

}

template <typename TData>
auto GetData(const uv_handle_t* handle) -> TData* {
    return static_cast<TData*>(NDetail::GetData(handle));
}
template <typename TData>
auto GetData(const uv_stream_t* handle) -> TData* {
    return static_cast<TData*>(NDetail::GetData(handle));
}

void SetData(uv_handle_t* handle, void* data);

template <typename THandle>
auto GetLoop(THandle& handle) -> uv_loop_t& {
    return *handle.loop;
}

auto CopyAddress(const sockaddr* from, sockaddr_in& to) -> TUvError;
auto CopyAddress(const sockaddr* from, sockaddr_in6& to) -> TUvError;

}
