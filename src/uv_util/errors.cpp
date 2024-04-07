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
#include <uvexec/uv_util/errors.hpp>

#include <uv.h>

#include <cstdio>


auto NUvUtil::IsError(TUvError uvErr) noexcept -> bool {
    return uvErr < 0;
}

auto NUvUtil::MakeRuntimeError(TUvError uvErr) -> std::runtime_error {
    return std::runtime_error(::uv_strerror(uvErr));
}

void NUvUtil::Assert(NUvUtil::TUvError uvErr) {
    if (IsError(uvErr)) [[unlikely]] {
        throw MakeRuntimeError(uvErr);
    }
}

void NUvUtil::Panic(NUvUtil::TUvError uvErr) noexcept {
    if (IsError(uvErr)) [[unlikely]] {
        std::fprintf(stderr, "Panicked with UV error: %s\n", ::uv_strerror(uvErr));
        std::terminate();
    }
}
