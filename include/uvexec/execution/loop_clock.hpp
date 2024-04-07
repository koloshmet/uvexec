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

#include <chrono>

namespace NUvExec {

struct TLoopClock {
    using rep = std::chrono::milliseconds::rep;
    using period = std::chrono::milliseconds::period;
    using duration = std::chrono::milliseconds;
    using time_point = std::chrono::time_point<TLoopClock, duration>;

    static constexpr bool is_steady = true;
    [[noreturn]] static time_point now() noexcept { // Non-static, use exec::now(uvLoop.get_scheduler()) instead
        std::terminate();
    }
};

}
