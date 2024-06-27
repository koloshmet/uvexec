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

#include "sockets/tcp_listener.hpp"
#include "sockets/udp.hpp"
#include "algorithms/accept.hpp"
#include "algorithms/schedule.hpp"
#include "algorithms/after.hpp"
#include "algorithms/upon_signal.hpp"
#include "algorithms/bind_to.hpp"
#include "algorithms/connect_to.hpp"
#include "algorithms/accept_from.hpp"

#include "algorithms/async_value.hpp"


namespace uvexec {

using errc = NUvExec::EErrc;

using loop_t = NUvExec::TLoop;
using scheduler_t = NUvExec::TLoop::TScheduler;

using clock_t = NUvExec::TLoopClock;

using tcp_socket_t = NUvExec::TTcpSocket;
using tcp_listener_t = NUvExec::TTcpListener;
using udp_socket_t = NUvExec::TUdpSocket;

using ip_v4_addr_t = NUvExec::TIp4Addr;
using ip_v6_addr_t = NUvExec::TIp6Addr;

}
