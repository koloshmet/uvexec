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

#include "after_op_state.hpp"
#include "schedule.hpp"


namespace NUvExec {

template <ETimerType Type>
class TAfterScheduleSender {
public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures = TScheduleEventuallyCompletionSignatures;

    explicit TAfterScheduleSender(TLoop& loop, std::chrono::milliseconds timeout) noexcept
        : Timeout(std::max(timeout, std::chrono::milliseconds{0})), Loop{&loop}
    {}

    template <stdexec::receiver_of<completion_signatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TAfterScheduleSender s, TReceiver&& rec) {
        return TAfterScheduleOpState<std::decay_t<TReceiver>, Type>(
                *s.Loop, static_cast<std::uint64_t>(s.Timeout.count()), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAfterScheduleSender& s) noexcept {
        return TLoop::TScheduler::TEnv(*s.Loop);
    }

private:
    std::chrono::milliseconds Timeout;
    TLoop* Loop;
};

template <stdexec::sender TSender, ETimerType Type>
class TAfterSender {
public:
    using sender_concept = stdexec::sender_t;

public:
    TAfterSender(TSender sender, TLoop& loop, std::chrono::milliseconds timeout)
        : Sender(std::move(sender)), Loop{&loop}, Timeout(std::max(timeout, std::chrono::milliseconds{0}))
    {}

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TAfterSender s, TReceiver&& rec) {
        return TAfterOpState<TSender, std::decay_t<TReceiver>, Type>(*s.Loop,
                static_cast<std::uint64_t>(s.Timeout.count()), std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAfterSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAfterSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TSender, TEnv,
                TCancellableAlgorithmCompletionSignatures, TVoidValueCompletionSignatures>{};
    }

private:
    TSender Sender;
    TLoop* Loop;
    std::chrono::milliseconds Timeout;
};

template<ETimerType Type>
class TAfterSender<TJustSender<>, Type>
        : public stdexec::sender_adaptor_closure<TAfterSender<TJustSender<>, Type>> {
public:
    using sender_concept = stdexec::sender_t;

public:
    TAfterSender(TJustSender<> sender, TLoop& loop, std::chrono::milliseconds timeout)
        : Sender(std::move(sender)), Loop{&loop}, Timeout(std::max(timeout, std::chrono::milliseconds{0}))
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) const {
        if constexpr (Type == ETimerType::At) {
            return uvexec::at(std::forward<TSender>(sender), *Loop, Timeout);
        } else {
            return uvexec::after(std::forward<TSender>(sender), *Loop, Timeout);
        }
    }

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TAfterSender s, TReceiver&& rec) {
        return TAfterOpState<TJustSender<>, std::decay_t<TReceiver>, Type>(*s.Loop,
                static_cast<std::uint64_t>(s.Timeout.count()), std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TAfterSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TAfterSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TJustSender<>, TEnv,
                TCancellableAlgorithmCompletionSignatures, TVoidValueCompletionSignatures>{};
    }

private:
    [[no_unique_address]] TJustSender<> Sender;
    TLoop* Loop;
    std::chrono::milliseconds Timeout;
};

template <typename TAdapterClosure, ETimerType Type> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TAfterSender<TJustSender<>, Type> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

template <stdexec::sender TSender, typename TRep, typename TPeriod> requires
        std::same_as<TLoop::TScheduler, std::invoke_result_t<
                stdexec::get_completion_scheduler_t<stdexec::set_value_t>, stdexec::env_of_t<TSender>>> &&
        std::convertible_to<std::chrono::duration<TRep, TPeriod>, std::chrono::milliseconds>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::after_t, TSender, std::tuple<std::chrono::duration<TRep, TPeriod>>> s)
        noexcept(std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(s));
    return TAfterSender<std::decay_t<TSender>, ETimerType::After>(
            std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data));
}

template <stdexec::sender TSender, typename TEnv, typename TRep, typename TPeriod> requires
        std::same_as<TLoop::TScheduler, std::invoke_result_t<stdexec::get_scheduler_t, const TEnv&>> &&
        std::convertible_to<std::chrono::duration<TRep, TPeriod>, std::chrono::milliseconds>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::after_t, TSender, std::tuple<std::chrono::duration<TRep, TPeriod>>> s,
        const TEnv& e) noexcept(std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_scheduler(e);
    return TAfterSender<std::decay_t<TSender>, ETimerType::After>(
            std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data));
}

template <stdexec::sender TSender, typename TDuration> requires
        std::same_as<TLoop::TScheduler, std::invoke_result_t<
                stdexec::get_completion_scheduler_t<stdexec::set_value_t>, stdexec::env_of_t<TSender>>> &&
        std::convertible_to<TDuration, std::chrono::milliseconds>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::at_t, TSender, std::tuple<std::chrono::time_point<TLoopClock, TDuration>>> s)
        noexcept(std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(s));
    return TAfterSender<std::decay_t<TSender>, ETimerType::At>(
            std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data).time_since_epoch());
}

template <stdexec::sender TSender, typename TEnv, typename TDuration> requires
        std::same_as<TLoop::TScheduler, std::invoke_result_t<stdexec::get_scheduler_t, const TEnv&>> &&
        std::convertible_to<TDuration, std::chrono::milliseconds>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::at_t, TSender, std::tuple<std::chrono::time_point<TLoopClock, TDuration>>> s,
        const TEnv& e) noexcept(std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_scheduler(e);
    return TAfterSender<std::decay_t<TSender>, ETimerType::At>(
            std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data).time_since_epoch());
}

inline auto tag_invoke(exec::schedule_after_t,
        TLoop::TScheduler s,
        TLoopClock::duration timeout) noexcept {
    return TAfterScheduleSender<ETimerType::After>(TLoop::TDomain{}.GetLoop(s), timeout);
}

inline auto tag_invoke(exec::schedule_at_t,
        TLoop::TScheduler s,
        TLoopClock::time_point timeout) noexcept {
    return TAfterScheduleSender<ETimerType::At>(TLoop::TDomain{}.GetLoop(s), timeout.time_since_epoch());
}

}
