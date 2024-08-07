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

#include <uvexec/execution/error_code.hpp>


namespace NUvUtil {

using TUvError = int;

auto IsError(TUvError uvErr) noexcept -> bool;
auto ToErrc(TUvError uvErr) noexcept -> NUvExec::EErrc;

void Assert(TUvError uvErr);

void Panic(TUvError uvErr) noexcept;

}
