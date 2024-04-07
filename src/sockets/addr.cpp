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
#include <uvexec/sockets/addr.hpp>

#include <uvexec/uv_util/errors.hpp>


namespace NUvExec {

TIp4Addr::TIp4Addr(const char* ip, int port) {
    NUvUtil::Assert(::uv_ip4_addr(ip, port, &Addr));
}

auto tag_invoke(NUvUtil::TRawUvObject, TIp4Addr& addr) noexcept -> sockaddr_in& {
    return addr.Addr;
}

auto tag_invoke(NUvUtil::TRawUvObject, const TIp4Addr& addr) noexcept -> const sockaddr_in& {
    return addr.Addr;
}

TIp6Addr::TIp6Addr(const char* ip, int port) {
    NUvUtil::Assert(::uv_ip6_addr(ip, port, &Addr));
}

auto tag_invoke(NUvUtil::TRawUvObject, TIp6Addr& addr) noexcept -> sockaddr_in6& {
    return addr.Addr;
}

auto tag_invoke(NUvUtil::TRawUvObject, const TIp6Addr& addr) noexcept -> const sockaddr_in6& {
    return addr.Addr;
}

}
