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
public:
    struct TOperation {
        virtual void Apply() noexcept = 0;

        TOperation* Next{nullptr};
    };

    class TOperationList {
    public:
        TOperationList() noexcept;
        void PushBack(TOperation& op) noexcept;
        auto Grab() noexcept -> TOperation*;

    private:
        std::atomic<TOperation*> Head;
    };

    template <typename TOpState, stdexec::stoppable_token TStopToken>
    class TStopOperation final : public TOperation {
        using TStopFn = void(*)(TOpState&) noexcept;

    public:
        TStopOperation(TStopFn fn, TOpState& opState, TLoop& loop, TStopToken stopToken)
            : Fn{fn}
            , State{&opState}
            , Loop{&loop}
            , Token(std::move(stopToken))
        {}

        void Setup() noexcept {
            StopCallback.emplace(std::move(Token), TStopCallback{this});
        }

        auto Reset() noexcept -> bool {
            if (!Used.test_and_set(std::memory_order_relaxed)) {
                StopCallback.reset();
                return false;
            }
            return true;
        }

        auto ResetUnsafe() noexcept -> bool {
            if (StopCallback) {
                StopCallback.reset();
                return false;
            }
            return true;
        }

        void Apply() noexcept override {
            std::invoke(*Fn, *State);
        }

        struct TStopCallback {
            void operator()() const noexcept {
                if (!Operation->Used.test_and_set(std::memory_order_relaxed)) {
                    Operation->Loop->Schedule(*Operation);
                }
            }

            TStopOperation* Operation;
        };

        explicit operator bool() const noexcept {
            return !Used.test(std::memory_order_relaxed);
        }

    private:
        using TCallback = typename TStopToken::template callback_type<TStopCallback>;

    private:
        TStopFn Fn;
        TOpState* State;
        TLoop* Loop;
        TStopToken Token;
        std::optional<TCallback> StopCallback;
        std::atomic_flag Used;
    };

    class TScheduler;

    struct TDomain {
        template <stdexec::sender TSender>
        static constexpr bool IsDomainCustomized = stdexec::tag_invocable<TDomain, TSender&&>;
        template <stdexec::sender TSender, typename TEnv>
        static constexpr bool IsDomainCustomizedWithEnv = stdexec::tag_invocable<TDomain, TSender&&, const TEnv&>;

        template <stdexec::sender TSender> requires IsDomainCustomized<TSender>
        auto transform_sender(TSender&& s) const noexcept -> stdexec::sender auto {
            return stdexec::tag_invoke(*this, std::forward<TSender>(s));
        }

        template <stdexec::sender TSender, typename TEnv>
            requires IsDomainCustomizedWithEnv<TSender, TEnv> || IsDomainCustomized<TSender>
        auto transform_sender(TSender&& s, const TEnv& e) const noexcept -> stdexec::sender auto {
            if constexpr (IsDomainCustomizedWithEnv<TSender, TEnv>) {
                return stdexec::tag_invoke(*this, std::forward<TSender>(s), e);
            } else {
                return this->transform_sender(std::forward<TSender>(s));
            }
        }

        template <stdexec::sender TSender, typename... TArgs>
        auto apply_sender(stdexec::sync_wait_t, TSender&& s, TArgs&&...) const {
            auto compSch = stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(s));
            return stdexec::tag_invoke(stdexec::sync_wait_t{}, compSch, std::forward<TSender>(s));
        }

        auto GetLoop(const TScheduler& sch) const noexcept -> TLoop&;
    };

    class TScheduler {
        friend struct TLoop::TDomain;

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

        friend auto tag_invoke(stdexec::get_domain_t, const TScheduler& s) noexcept -> TDomain;
        friend auto tag_invoke(exec::now_t, const TScheduler& s) noexcept -> std::chrono::time_point<TLoopClock>;

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

    TLoop();
    TLoop(TLoop&&) noexcept = delete;
    ~TLoop();

    auto get_scheduler() noexcept -> TScheduler;
    auto run() -> bool;
    auto run_once() -> bool;
    auto drain() -> bool;
    void finish() noexcept;

    void Schedule(TOperation& op) noexcept;
    void RunnerSteal(TRunner& runner);

    friend auto tag_invoke(NUvUtil::TRawUvObject, TLoop& loop) noexcept -> uv_loop_t&;
    friend auto tag_invoke(NUvUtil::TRawUvObject, const TLoop& loop) noexcept -> const uv_loop_t&;

private:
    void Walk(uv_walk_cb cb, void* arg);
    auto RunLocked(std::unique_lock<std::mutex>& lock, uv_run_mode mode) -> bool;

    static void ApplyOperations(uv_async_t* async);

private:
    uv_loop_t UvLoop;
    uv_async_t Async;
    TOperationList Scheduled;
    std::mutex RunMtx;
    TRunnersQueue Runners;
    bool Running;
};

}
