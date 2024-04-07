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

#include <forward_list>

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
    auto stopped = ::uv_run(&UvLoop, UV_RUN_DEFAULT);
    return stopped != 0;
}

auto TLoop::run_once() -> bool {
    auto stopped = ::uv_run(&UvLoop, UV_RUN_ONCE);
    return stopped != 0;
}

auto TLoop::drain() -> bool {
    auto stopped = ::uv_run(&UvLoop, UV_RUN_NOWAIT);
    return stopped != 0;
}

void TLoop::Schedule(NUvExec::TLoop::TOpState& op) noexcept {
    Scheduled.PushBack(op);
    NUvUtil::Fire(Async); // never returns error
}

void TLoop::Stop() noexcept {
    ::uv_stop(&UvLoop);
}

void TLoop::RunnerSteal(TRunner& runner) {
    while (runner.Awakenings.load() != 0) {
        std::unique_lock lock(RunMtx);
        if (!Running) {
            if (runner.Awakenings.load() != 0) {
                Running = true;
                lock.unlock();
                runner.Acquired = true;
                run();
                lock.lock();
                Running = false;
            }
            while (!Runners.Empty()) {
                auto& next = Runners.Pop();
                if (next.Awakenings.load() != 0) {
                    next.Wakeup();
                    break;
                }
            }
        } else {
            Runners.Add(runner);
            lock.unlock();
            auto wakeups = runner.Awakenings.load();
            while (wakeups != 0 && runner.Awakenings.load() == wakeups) {
                runner.Awakenings.wait(wakeups);
            }
            lock.lock();
            Runners.Erase(runner);
        }
    }
}

TLoop::TOpStateList::TOpStateList() noexcept
    : Head{nullptr}
{}

void TLoop::TOpStateList::PushBack(TOpState& op) noexcept {
    auto curTop = Head.load();
    do {
        op.Next.store(curTop);
    } while (!Head.compare_exchange_weak(curTop, &op));
}

auto TLoop::TOpStateList::Grab() noexcept -> TLoop::TOpState* {
    auto opStates = Head.exchange(nullptr);
    TOpState* newTop = nullptr;
    auto cur = opStates;
    while (cur != nullptr) {
        auto next = cur->Next.load();
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
        auto next = op->Next.load();
        op->Apply();
        op = next;
    }
}

void TLoop::TRunner::Wakeup() noexcept {
    Awakenings.fetch_add(1);
    Awakenings.notify_one();
}

void TLoop::TRunner::WakeupFinished() noexcept {
    Awakenings.store(0);
    Awakenings.notify_one();
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

void TLoop::TTimer::Init(NUvExec::TLoop& loop) {
    NUvUtil::Assert(::uv_timer_init(&loop.UvLoop, &UvTimer));
}

void TLoop::TSignal::Init(NUvExec::TLoop& loop) {
    NUvUtil::Assert(::uv_signal_init(&loop.UvLoop, &UvSignal));
}

TLoop::TScheduler::TScheduler(TLoop& loop) noexcept
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

auto tag_invoke(stdexec::get_scheduler_t, const TLoop::TScheduler::TEnv& env) noexcept -> TLoop::TScheduler {
    return env.Loop->get_scheduler();
}

auto tag_invoke(stdexec::get_domain_t, const TLoop::TScheduler::TEnv&) noexcept -> TLoop::TDomain {
    return {};
}

}
