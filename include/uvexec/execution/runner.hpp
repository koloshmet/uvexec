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

#include <uvexec/util/intrusive_list.hpp>
#include <atomic>

namespace NUvExec {

class TRunner : public TIntrusiveListNode<TRunner> {
public:
    TRunner() = default;

    bool Finished() const noexcept;

    bool AcquireIfNotFinished() noexcept;

    bool Acquired() const noexcept;

    void Wakeup() noexcept;

    void Finish() noexcept;

    void Wait() noexcept;

private:
    std::atomic_uint64_t Awakenings{1};
    bool Acq{false};
};

class TRunnersQueue {
public:
    TRunnersQueue() = default;

    void Add(TRunner& runner) noexcept;

    void Erase(TRunner& runner);

    void WakeupNext() noexcept;

private:
    TIntrusiveList<TRunner> Runners;
};

}
