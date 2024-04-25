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

#include <stdexec/execution.hpp>

#include <uvexec/uv_util/errors.hpp>


namespace NUvExec {

template <typename... TArgs>
using TVoidValueCompletionSignatures = stdexec::completion_signatures<stdexec::set_value_t()>;

template <typename... TArgs>
using TLengthValueCompletionSignatures = stdexec::completion_signatures<stdexec::set_value_t(std::size_t)>;

using TAlgorithmCompletionSignatures = stdexec::completion_signatures<stdexec::set_error_t(NUvUtil::TUvError)>;

using TCancellableAlgorithmCompletionSignatures = stdexec::completion_signatures<
        stdexec::set_error_t(NUvUtil::TUvError), stdexec::set_stopped_t()>;

}
