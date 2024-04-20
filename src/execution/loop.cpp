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
    NUvUtil::Assert(::uv_async_init(&UvLoop, &Async, ApplyOpStates));
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

void TLoop::Schedule(NUvExec::TLoop::TOpState& op) noexcept {
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

TLoop::TOpStateList::TOpStateList() noexcept
    : Head{nullptr}
{}

void TLoop::TOpStateList::PushBack(TOpState& op) noexcept {
    auto curTop = Head.load(std::memory_order_relaxed);
    do {
        op.Next = curTop;
    } while (!Head.compare_exchange_weak(curTop, &op, std::memory_order_release, std::memory_order_relaxed));
}

auto TLoop::TOpStateList::Grab() noexcept -> TLoop::TOpState* {
    auto opStates = Head.exchange(nullptr, std::memory_order_acquire);
    TOpState* newTop = nullptr;
    auto cur = opStates;
    while (cur != nullptr) {
        auto next = cur->Next;
        cur->Next = newTop;
        newTop = cur;
        cur = next;
    }
    return newTop;
}

void TLoop::ApplyOpStates(uv_async_t* async) {
    auto opStates = static_cast<TOpStateList*>(async->data)->Grab();

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

void TLoop::TTimer::Init(NUvExec::TLoop& loop) {
    NUvUtil::Assert(::uv_timer_init(&loop.UvLoop, &UvTimer));
}

void TLoop::TSignal::Init(NUvExec::TLoop& loop) {
    NUvUtil::Assert(::uv_signal_init(&loop.UvLoop, &UvSignal));
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

TLoop::TScheduler::TSender::TSender(TLoop& loop) noexcept
    : Loop{&loop}
{}

auto tag_invoke(stdexec::get_env_t, const TLoop::TScheduler::TSender& s) noexcept -> TLoop::TScheduler::TEnv {
    return TLoop::TScheduler::TEnv(*s.Loop);
}

TLoop::TScheduler::TSignalSender::TSignalSender(TLoop& loop, int signum) noexcept
    : Loop{&loop}, Signum{signum}
{}

auto tag_invoke(stdexec::get_env_t, const TLoop::TScheduler::TSignalSender& s) noexcept -> TLoop::TScheduler::TEnv {
    return TLoop::TScheduler::TEnv(*s.Loop);
}

auto tag_invoke(stdexec::get_domain_t, const TLoop::TScheduler&) noexcept -> TLoop::TDomain {
    return {};
}

auto tag_invoke(exec::now_t, const TLoop::TScheduler& s) noexcept -> std::chrono::time_point<TLoopClock> {
    return TLoopClock::time_point(std::chrono::milliseconds(NUvUtil::Now(NUvUtil::RawUvObject(*s.Loop))));
}

auto tag_invoke(stdexec::schedule_t, TLoop::TScheduler s) noexcept -> TLoop::TScheduler::TSender {
    return TLoop::TScheduler::TSender(*s.Loop);
}

auto tag_invoke(uvexec::schedule_upon_signal_t,
        TLoop::TScheduler s, int signum) noexcept -> TLoop::TScheduler::TSignalSender {
    return TLoop::TScheduler::TSignalSender(*s.Loop, signum);
}

auto tag_invoke(exec::schedule_after_t,
        TLoop::TScheduler s,
        TLoopClock::duration timeout) noexcept -> TLoop::TScheduler::TTimedSender<TLoop::ETimerType::After> {
    return TLoop::TScheduler::TTimedSender<TLoop::ETimerType::After>(*s.Loop, timeout);
}

auto tag_invoke(exec::schedule_at_t,
        TLoop::TScheduler s,
        TLoopClock::time_point timeout) noexcept -> TLoop::TScheduler::TTimedSender<TLoop::ETimerType::At> {
    return TLoop::TScheduler::TTimedSender<TLoop::ETimerType::At>(*s.Loop, timeout.time_since_epoch());
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
