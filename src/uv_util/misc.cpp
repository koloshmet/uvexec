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
#include <uvexec/uv_util/misc.hpp>

#include <uvexec/uv_util/safe_uv.hpp>

auto NUvUtil::NDetail::GetData(const uv_handle_t* handle) -> void* {
    return ::uv_handle_get_data(handle);
}

auto NUvUtil::NDetail::GetData(const uv_stream_t* handle) -> void* {
    return ::UvStreamGetData(handle);
}

void NUvUtil::SetData(uv_handle_t* handle, void* data) {
    ::uv_handle_set_data(handle, data);
}
