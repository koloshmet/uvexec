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

#include "loop_clock.hpp"
#include "sync_wait_receiver.hpp"
#include "runner.hpp"

#include <uvexec/interface/uvexec.hpp>
#include <exec/timed_scheduler.hpp>

#include <uvexec/uv_util/reqs.hpp>
#include <uvexec/uv_util/errors.hpp>
#include <uvexec/uv_util/misc.hpp>

#include <stdexec/execution.hpp>

#include <uvexec/util/intrusive_list.hpp>


namespace NUvExec {

namespace NDetail {

template <typename TReceiver, typename TStopCallback>
using TCallbackOf = typename stdexec::stop_token_of_t<stdexec::env_of_t<TReceiver>>::
        template callback_type<TStopCallback>;

}

class TLoop {
    struct TTimer {
        TTimer() = default;
        void Init(TLoop& loop);

        uv_timer_t UvTimer;
    };

    struct TSignal {
        TSignal() = default;
        void Init(TLoop& loop);

        uv_signal_t UvSignal;
    };

public:
    TLoop();
    TLoop(TLoop&&) noexcept = delete;
    ~TLoop();

    using TScheduleCompletionSignatures = stdexec::completion_signatures<
            stdexec::set_value_t(), stdexec::set_stopped_t()>;

    using TScheduleEventuallyCompletionSignatures = stdexec::completion_signatures<
            stdexec::set_value_t(), stdexec::set_error_t(NUvUtil::TUvError), stdexec::set_stopped_t()>;

    struct TOpState {
        virtual void Apply() noexcept = 0;

        TOpState* Next{nullptr};
    };

    class TOpStateList {
    public:
        TOpStateList() noexcept;
        void PushBack(TOpState& op) noexcept;
        auto Grab() noexcept -> TOpState*;

    private:
        std::atomic<TOpState*> Head;
    };

    template <stdexec::receiver_of<TScheduleCompletionSignatures> TReceiver>
    class TScheduleOpState final : public TOpState {
    public:
        TScheduleOpState(TLoop& loop, TReceiver&& receiver)
            : Async{&loop.Async}, Receiver(std::move(receiver))
        {}

        friend void tag_invoke(stdexec::start_t, TScheduleOpState& op) noexcept {
            auto opList = static_cast<TOpStateList*>(op.Async->data);
            opList->PushBack(op);
            NUvUtil::Fire(*op.Async); // never returns error
        }

        void Apply() noexcept override {
            if (stdexec::get_stop_token(stdexec::get_env(Receiver)).stop_requested()) {
                stdexec::set_stopped(std::move(Receiver));
            } else {
                stdexec::set_value(std::move(Receiver));
            }
        }

    private:
        uv_async_t* Async;
        TReceiver Receiver;
    };

    enum class ETimerType {
        After,
        At
    };

    template <stdexec::receiver_of<TScheduleEventuallyCompletionSignatures> TReceiver, ETimerType Type>
    class TTimedScheduleOpState final : public TOpState {
    public:
        TTimedScheduleOpState(TLoop& loop, std::uint64_t timeout, TReceiver&& receiver)
            : Loop{&loop}, Receiver(std::move(receiver)), StopOp(*this), Timeout{timeout}
        {}

        friend void tag_invoke(stdexec::start_t, TTimedScheduleOpState& op) noexcept {
            op.Loop->Schedule(op);
        }

        void Apply() noexcept override {
            Timer.Init(*Loop);
            Timer.UvTimer.data = this;
            std::uint64_t timeout;
            if constexpr (Type == ETimerType::At) {
                auto now = NUvUtil::Now(*Timer.UvTimer.loop);
                timeout = now > Timeout ? 0 : Timeout - now;
            } else {
                timeout = Timeout;
            }

            auto err = NUvUtil::TimerStart(Timer.UvTimer, AfterCallback, timeout, 0);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(Receiver), std::move(err));
            } else {
                StopCallback.emplace(stdexec::get_stop_token(stdexec::get_env(Receiver)), TStopCallback{this});
            }
        }

    private:
        void Stop() noexcept {
            if (!Used.test_and_set(std::memory_order_relaxed)) {
                Loop->Schedule(StopOp);
            }
        }

        static void AfterCallback(uv_timer_t* timer) {
            auto opState = static_cast<TTimedScheduleOpState*>(timer->data);
            if (!opState->Used.test_and_set(std::memory_order_relaxed)) {
                opState->StopCallback.reset();
                NUvUtil::Close(*timer, CloseCallback);
            }
        }

        static void CloseCallback(uv_handle_t* timer) {
            auto opState = NUvUtil::GetData<TTimedScheduleOpState>(timer);
            if (opState->StopCallback) {
                opState->StopCallback.reset();
                stdexec::set_stopped(std::move(opState->Receiver));
            } else {
                stdexec::set_value(std::move(opState->Receiver));
            }
        }

        struct TStopOp : TOpState {
            explicit TStopOp(TTimedScheduleOpState& state) noexcept: State{&state} {}

            void Apply() noexcept override {
                NUvUtil::TimerStop(State->Timer.UvTimer);
                NUvUtil::Close(State->Timer.UvTimer, CloseCallback);
            }

            TTimedScheduleOpState* State;
        };

        struct TStopCallback {
            void operator()() const {
                State->Stop();
            }

            TTimedScheduleOpState* State;
        };

        using TCallback = NDetail::TCallbackOf<TReceiver, TStopCallback>;

    private:
        TTimer Timer;
        TLoop* Loop;
        TReceiver Receiver;
        TStopOp StopOp;
        std::optional<TCallback> StopCallback;
        std::atomic_flag Used;
        std::uint64_t Timeout;
    };

    template <stdexec::receiver_of<TScheduleEventuallyCompletionSignatures> TReceiver>
    class TSignalScheduleOpState final : public TOpState {
    public:
        TSignalScheduleOpState(TLoop& loop, int signum, TReceiver&& receiver)
            : Loop{&loop}, Receiver(std::move(receiver)), StopOp(*this), Signum{signum}
        {}

        friend void tag_invoke(stdexec::start_t, TSignalScheduleOpState& op) noexcept {
            op.Loop->Schedule(op);
        }

        void Apply() noexcept override {
            Signal.Init(*Loop);
            Signal.UvSignal.data = this;
            auto err = NUvUtil::SignalOnce(Signal.UvSignal, SignalCallback, Signum);
            if (NUvUtil::IsError(err)) {
                stdexec::set_error(std::move(Receiver), std::move(err));
            } else {
                StopCallback.emplace(stdexec::get_stop_token(stdexec::get_env(Receiver)), TStopCallback{this});
            }
        }

    private:
        void Stop() noexcept {
            if (!Used.test_and_set(std::memory_order_relaxed)) {
                Loop->Schedule(StopOp);
            }
        }

        static void SignalCallback(uv_signal_t* signal, int) {
            auto opState = static_cast<TSignalScheduleOpState*>(signal->data);
            if (!opState->Used.test_and_set(std::memory_order_relaxed)) {
                opState->StopCallback.reset();
                NUvUtil::Close(*signal, CloseCallback);
            }
        }

        static void CloseCallback(uv_handle_t* signal) {
            auto opState = NUvUtil::GetData<TSignalScheduleOpState>(signal);
            if (opState->StopCallback) {
                opState->StopCallback.reset();
                stdexec::set_stopped(std::move(opState->Receiver));
            } else {
                stdexec::set_value(std::move(opState->Receiver));
            }
        }

        struct TStopOp : TOpState {
            explicit TStopOp(TSignalScheduleOpState& state) noexcept: State{&state} {}

            void Apply() noexcept override {
                NUvUtil::SignalStop(State->Signal.UvSignal);
                NUvUtil::Close(State->Signal.UvSignal, CloseCallback);
            }

            TSignalScheduleOpState* State;
        };

        struct TStopCallback {
            void operator()() const {
                State->Stop();
            }

            TSignalScheduleOpState* State;
        };

        using TCallback = NDetail::TCallbackOf<TReceiver, TStopCallback>;

    private:
        TSignal Signal;
        TLoop* Loop;
        TReceiver Receiver;
        TStopOp StopOp;
        std::optional<TCallback> StopCallback;
        std::atomic_flag Used;
        int Signum;
    };

    struct TDomain {
        template <stdexec::sender TSender> requires stdexec::tag_invocable<TDomain, TSender&&>
        auto transform_sender(TSender&& s) const noexcept -> stdexec::sender auto {
            return stdexec::tag_invoke(*this, std::forward<TSender>(s));
        }

        template <stdexec::sender TSender, typename TEnv> requires stdexec::tag_invocable<TDomain, TSender&&>
        auto transform_sender(TSender&& s, const TEnv&) const noexcept -> stdexec::sender auto {
            return this->transform_sender(std::forward<TSender>(s));
        }

        template <stdexec::sender TSender, typename... TEnvs> requires stdexec::tag_invocable<TDomain, TSender&&>
        auto transform_sender(TSender&& s, const TEnvs&...) const noexcept -> stdexec::sender auto {
            return this->transform_sender(std::forward<TSender>(s));
        }

        template <stdexec::sender TSender, typename... TArgs>
        auto apply_sender(stdexec::sync_wait_t, TSender&& s, TArgs&&...) const noexcept {
            auto compSch = stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(s));
            return stdexec::tag_invoke(stdexec::sync_wait_t{}, compSch, std::forward<TSender>(s));
        }
    };

    class TScheduler {
    public:
        explicit TScheduler(TLoop& loop) noexcept;

        class TEnv {
        public:
            explicit TEnv(TLoop& loop) noexcept;

            template <typename T>
            friend auto tag_invoke(stdexec::get_completion_scheduler_t<T>, const TEnv& env) noexcept -> TScheduler {
                return env.Loop->get_scheduler();
            }

            friend auto tag_invoke(stdexec::get_domain_t, const TEnv& env) noexcept -> TDomain;

        private:
            TLoop* Loop;
        };

        class TSender {
        public:
            using sender_concept = stdexec::sender_t;
            using completion_signatures = TScheduleCompletionSignatures;

            explicit TSender(TLoop& loop) noexcept;

            template <stdexec::receiver_of<completion_signatures> TReceiver>
            friend auto tag_invoke(stdexec::connect_t, TSender s, TReceiver&& rec) {
                return TScheduleOpState<std::decay_t<TReceiver>>(*s.Loop, std::forward<TReceiver>(rec));
            }

            friend auto tag_invoke(stdexec::get_env_t, const TSender& s) noexcept -> TEnv;

        private:
            TLoop* Loop;
        };

        template <ETimerType Type>
        class TTimedSender {
        public:
            using sender_concept = stdexec::sender_t;
            using completion_signatures = TScheduleEventuallyCompletionSignatures;

            explicit TTimedSender(TLoop& loop, std::chrono::milliseconds timeout) noexcept
                : Timeout(std::max(timeout, std::chrono::milliseconds{0})), Loop{&loop}
            {}

            template <stdexec::receiver_of<completion_signatures> TReceiver>
            friend auto tag_invoke(stdexec::connect_t, TTimedSender s, TReceiver&& rec) {
                return TTimedScheduleOpState<std::decay_t<TReceiver>, Type>(
                        *s.Loop, static_cast<std::uint64_t>(s.Timeout.count()), std::forward<TReceiver>(rec));
            }

            friend auto tag_invoke(stdexec::get_env_t, const TTimedSender& s) noexcept {
                return TEnv(*s.Loop);
            }

        private:
            std::chrono::milliseconds Timeout;
            TLoop* Loop;
        };

        class TSignalSender {
        public:
            using sender_concept = stdexec::sender_t;
            using completion_signatures = TScheduleEventuallyCompletionSignatures;

            explicit TSignalSender(TLoop& loop, int signum) noexcept;

            template <stdexec::receiver_of<completion_signatures> TReceiver>
            friend auto tag_invoke(stdexec::connect_t, TSignalSender s, TReceiver&& rec) {
                return TSignalScheduleOpState<std::decay_t<TReceiver>>(*s.Loop, s.Signum, std::forward<TReceiver>(rec));
            }

            friend auto tag_invoke(stdexec::get_env_t, const TSignalSender& s) noexcept -> TEnv;

        private:
            TLoop* Loop;
            int Signum;
        };

        friend auto tag_invoke(stdexec::get_domain_t, const TScheduler& s) noexcept -> TDomain;
        friend auto tag_invoke(exec::now_t, const TScheduler& s) noexcept -> std::chrono::time_point<TLoopClock>;
        friend auto tag_invoke(stdexec::schedule_t, TScheduler s) noexcept -> TSender;
        friend auto tag_invoke(uvexec::schedule_upon_signal_t, TScheduler s, int signum) noexcept -> TSignalSender;
        friend auto tag_invoke(exec::schedule_after_t,
                TScheduler s, TLoopClock::duration t) noexcept -> TTimedSender<ETimerType::After>;
        friend auto tag_invoke(exec::schedule_at_t,
                TScheduler s, TLoopClock::time_point t) noexcept -> TTimedSender<ETimerType::At>;

        class TLoopEnv {
        public:
            explicit TLoopEnv(TLoop& loop) noexcept;

            friend auto tag_invoke(stdexec::get_scheduler_t, const TLoopEnv& env) noexcept -> TScheduler;

            friend auto tag_invoke(stdexec::get_domain_t, const TLoopEnv& env) noexcept -> TDomain;

        private:
            TLoop* Loop;
        };

        template <stdexec::sender_in<TLoopEnv> TS>
        friend auto tag_invoke(stdexec::sync_wait_t, const TScheduler& sched, TS&& sender) {
            auto& loop = *sched.Loop;
            TRunner runner;
            auto wakeup = [&runner, &loop]() noexcept {
                if (runner.Acquired()) {
                    loop.finish();
                }
                runner.Finish();
            };

            using TWakeup = decltype(wakeup);
            using TWaitR = TSyncWaitReceiver<TLoopEnv, TS, TWakeup>;

            TLoopEnv env(*sched.Loop);
            TSyncWaitReceiverState<TLoopEnv, TS, TWakeup> dest;
            auto op = stdexec::connect(std::forward<TS>(sender), TWaitR(dest, env, std::move(wakeup)));
            stdexec::start(op);
            sched.Loop->RunnerSteal(runner);
            return UnwrapSyncWaitReceiverState<TLoopEnv, TS, TWakeup>(std::move(dest));
        }

        friend auto tag_invoke(stdexec::get_forward_progress_guarantee_t, const TScheduler&) noexcept {
            return stdexec::forward_progress_guarantee::parallel;
        }

        auto operator==(const TScheduler&) const noexcept -> bool = default;

    private:
        TLoop* Loop;
    };

    auto get_scheduler() noexcept -> TScheduler;
    auto run() -> bool;
    auto run_once() -> bool;
    auto drain() -> bool;
    void finish() noexcept;

    void Schedule(TOpState& op) noexcept;
    void RunnerSteal(TRunner& runner);

    friend auto tag_invoke(NUvUtil::TRawUvObject, TLoop& loop) noexcept -> uv_loop_t&;
    friend auto tag_invoke(NUvUtil::TRawUvObject, const TLoop& loop) noexcept -> const uv_loop_t&;

private:
    void Walk(uv_walk_cb cb, void* arg);
    auto RunLocked(std::unique_lock<std::mutex>& lock, uv_run_mode mode) -> bool;

    static void ApplyOpStates(uv_async_t* async);

private:
    uv_loop_t UvLoop;
    uv_async_t Async;
    TOpStateList Scheduled;
    std::mutex RunMtx;
    TRunnersQueue Runners;
    bool Running;
};

}
