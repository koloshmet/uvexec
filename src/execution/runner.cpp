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
#include <uvexec/execution/runner.hpp>


namespace NUvExec {

bool TRunner::Finished() const noexcept {
    return Awakenings.load(std::memory_order_acquire) == 0;
}

bool TRunner::AcquireIfNotFinished() noexcept {
    if (Awakenings.load(std::memory_order_relaxed) != 0) {
        Acq = true;
    }
    return Acq;
}

bool TRunner::Acquired() const noexcept {
    return Acq;
}

void TRunner::Wakeup() noexcept {
    Awakenings.fetch_add(1, std::memory_order_relaxed);
    Awakenings.notify_one();
}

void TRunner::Finish() noexcept {
    Awakenings.store(0, std::memory_order_release);
    Awakenings.notify_one();
}

void TRunner::Wait() noexcept {
    auto wakeups = Awakenings.load(std::memory_order_relaxed);
    while (wakeups != 0 && Awakenings.load(std::memory_order_relaxed) == wakeups) {
        Awakenings.wait(wakeups, std::memory_order_relaxed);
    }
}

void TRunnersQueue::Add(NUvExec::TRunner& runner) noexcept {
    Runners.Add(runner);
}

void TRunnersQueue::Erase(NUvExec::TRunner& runner) {
    Runners.Erase(runner);
}

void TRunnersQueue::WakeupNext() noexcept {
    while (!Runners.Empty()) {
        auto& next = Runners.Pop();
        if (!next.Finished()) {
            next.Wakeup();
            break;
        }
    }
}

}
