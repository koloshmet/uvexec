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

#include <uvexec/uv_util/misc.hpp>

namespace NUvExec {

struct TIp4Addr {
    TIp4Addr() noexcept;
    TIp4Addr(const char* ip, int port);

    friend auto tag_invoke(NUvUtil::TRawUvObject, TIp4Addr& addr) noexcept -> sockaddr_in&;
    friend auto tag_invoke(NUvUtil::TRawUvObject, const TIp4Addr& addr) noexcept -> const sockaddr_in&;

    sockaddr_in Addr;
};

struct TIp6Addr {
    TIp6Addr() noexcept;
    TIp6Addr(const char* ip, int port);

    friend auto tag_invoke(NUvUtil::TRawUvObject, TIp6Addr& addr) noexcept -> sockaddr_in6&;
    friend auto tag_invoke(NUvUtil::TRawUvObject, const TIp6Addr& addr) noexcept -> const sockaddr_in6&;

    sockaddr_in6 Addr;
};

template <typename TAddr>
auto DefaultAddr() -> TAddr;

template <>
inline auto DefaultAddr<TIp4Addr>() -> TIp4Addr {
    return {"0.0.0.0", 0};
}

template <>
inline auto DefaultAddr<TIp6Addr>() -> TIp6Addr {
    return {"::", 0};
}

}
