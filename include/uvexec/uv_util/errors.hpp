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

#include <stdexcept>


namespace NUvUtil {

using TUvError = int; // TODO: Replace native errors with error_code

constexpr TUvError Ok{0};
auto IsError(TUvError uvErr) noexcept -> bool;

auto MakeRuntimeError(TUvError uvErr) -> std::runtime_error;

void Assert(TUvError uvErr);

void Panic(TUvError uvErr) noexcept;

}
