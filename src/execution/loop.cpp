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
#include <uvexec/execution/loop.hpp>

#include <uvexec/uv_util/safe_uv.hpp>


namespace NUvExec {

TLoop::TLoop(): Scheduled{}, Running{false} {
    NUvUtil::Assert(::uv_loop_init(&UvLoop));
    UvLoop.data = this;
    NUvUtil::Assert(::uv_async_init(&UvLoop, &Async, ApplyOperations));
    Async.data = &Scheduled;
}

TLoop::~TLoop() {
    NUvUtil::Close(Async);
    ::uv_run(&UvLoop, UV_RUN_ONCE);
    NUvUtil::Panic(::uv_loop_close(&UvLoop)); // Loop is busy, rip
}

auto TLoop::get_scheduler() noexcept -> TLoop::TScheduler {
    return TScheduler(*this);
}

auto TLoop::run() -> bool {
    TRunner runner;
    while (true) {
        std::unique_lock lock(RunMtx);
        if (!Running) {
            auto stopped = RunLocked(lock, UV_RUN_DEFAULT);
            Runners.WakeupNext();
            return stopped;
        } else {
            Runners.Add(runner);
            lock.unlock();
            runner.Wait();
            lock.lock();
            Runners.Erase(runner);
        }
    }
}

auto TLoop::run_once() -> bool {
    TRunner runner;
    while (true) {
        std::unique_lock lock(RunMtx);
        if (!Running) {
            auto stopped = RunLocked(lock, UV_RUN_ONCE);
            Runners.WakeupNext();
            return stopped;
        } else {
            Runners.Add(runner);
            lock.unlock();
            runner.Wait();
            lock.lock();
            Runners.Erase(runner);
        }
    }
}

auto TLoop::drain() -> bool {
    TRunner runner;
    while (true) {
        std::unique_lock lock(RunMtx);
        if (!Running) {
            auto stopped = RunLocked(lock, UV_RUN_NOWAIT);
            Runners.WakeupNext();
            return stopped;
        } else {
            Runners.Add(runner);
            lock.unlock();
            runner.Wait();
            lock.lock();
            Runners.Erase(runner);
        }
    }
}

void TLoop::finish() noexcept {
    ::uv_stop(&UvLoop);
}

void TLoop::Schedule(NUvExec::TLoop::TOperation& op) noexcept {
    Scheduled.PushBack(op);
    NUvUtil::Fire(Async); // never returns error
}

void TLoop::RunnerSteal(TRunner& runner) {
    while (!runner.Finished()) {
        std::unique_lock lock(RunMtx);
        if (!Running) {
            if (runner.AcquireIfNotFinished()) {
                RunLocked(lock, UV_RUN_DEFAULT);
            }
            Runners.WakeupNext();
        } else {
            Runners.Add(runner);
            lock.unlock();
            runner.Wait();
            lock.lock();
            Runners.Erase(runner);
        }
    }
}

TLoop::TOperationList::TOperationList() noexcept
    : Head{nullptr}
{}

void TLoop::TOperationList::PushBack(TOperation& op) noexcept {
    auto curTop = Head.load(std::memory_order_relaxed);
    do {
        op.Next = curTop;
    } while (!Head.compare_exchange_weak(curTop, &op, std::memory_order_release, std::memory_order_relaxed));
}

auto TLoop::TOperationList::Grab() noexcept -> TLoop::TOperation* {
    auto opStates = Head.exchange(nullptr, std::memory_order_acquire);
    TOperation* newTop = nullptr;
    auto cur = opStates;
    while (cur != nullptr) {
        auto next = cur->Next;
        cur->Next = newTop;
        newTop = cur;
        cur = next;
    }
    return newTop;
}

void TLoop::ApplyOperations(uv_async_t* async) {
    auto opStates = static_cast<TOperationList*>(async->data)->Grab();

    auto op = opStates;
    while (op != nullptr) {
        auto next = op->Next;
        op->Apply();
        op = next;
    }
}


auto tag_invoke(NUvUtil::TRawUvObject, TLoop& loop) noexcept -> uv_loop_t& {
    return loop.UvLoop;
}

auto tag_invoke(NUvUtil::TRawUvObject, const TLoop& loop) noexcept -> const uv_loop_t& {
    return loop.UvLoop;
}

void TLoop::Walk(uv_walk_cb cb, void* arg) {
    ::uv_walk(&UvLoop, cb, arg);
}

auto TLoop::RunLocked(std::unique_lock<std::mutex>& lock, uv_run_mode mode) -> bool {
    Running = true;
    lock.unlock();
    auto stopped = ::uv_run(&UvLoop, mode);
    lock.lock();
    Running = false;
    return stopped != 0;
}

auto TLoop::TDomain::GetLoop(const TLoop::TScheduler& sch) const noexcept -> TLoop& {
    return *sch.Loop;
}

TLoop::TScheduler::TScheduler(TLoop& loop) noexcept
    : Loop{&loop}
{}

TLoop::TScheduler::TLoopEnv::TLoopEnv(TLoop& loop) noexcept
        : Loop{&loop}
{}

TLoop::TScheduler::TEnv::TEnv(TLoop& loop) noexcept
    : Loop{&loop}
{}

auto tag_invoke(stdexec::get_domain_t, const TLoop::TScheduler&) noexcept -> TLoop::TDomain {
    return {};
}

auto tag_invoke(exec::now_t, const TLoop::TScheduler& s) noexcept -> std::chrono::time_point<TLoopClock> {
    return TLoopClock::time_point(std::chrono::milliseconds(NUvUtil::Now(NUvUtil::RawUvObject(*s.Loop))));
}

auto tag_invoke(stdexec::get_scheduler_t, const TLoop::TScheduler::TLoopEnv& env) noexcept -> TLoop::TScheduler {
    return env.Loop->get_scheduler();
}

auto tag_invoke(stdexec::get_domain_t, const TLoop::TScheduler::TLoopEnv&) noexcept -> TLoop::TDomain {
    return {};
}

auto tag_invoke(stdexec::get_domain_t, const TLoop::TScheduler::TEnv&) noexcept -> TLoop::TDomain {
    return {};
}

}
