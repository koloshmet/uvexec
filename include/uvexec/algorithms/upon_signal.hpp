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

#include "upon_signal_op_state.hpp"


namespace NUvExec {

class TUponSignalScheduleSender {
public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures = TScheduleEventuallyCompletionSignatures;

    explicit TUponSignalScheduleSender(TLoop& loop, int signum) noexcept
        : Loop{&loop}, Signum{signum}
    {}

    template <stdexec::receiver_of<completion_signatures> TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TUponSignalScheduleSender s, TReceiver&& rec) {
        return TUponSignalScheduleOpState<std::decay_t<TReceiver>>(*s.Loop, s.Signum, std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TUponSignalScheduleSender& s) noexcept -> TLoop::TScheduler::TEnv {
        return TLoop::TScheduler::TEnv(*s.Loop);
    }

private:
    TLoop* Loop;
    int Signum;
};

template <stdexec::sender TSender>
class TUponSignalSender {
public:
    using sender_concept = stdexec::sender_t;

public:
    TUponSignalSender(TSender sender, TLoop& loop, int signum)
        : Sender(std::move(sender)), Loop{&loop}, Signum{signum}
    {}

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TUponSignalSender s, TReceiver&& rec) {
        return TUponSignalOpState<TSender, std::decay_t<TReceiver>>(
                *s.Loop, s.Signum, std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TUponSignalSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TUponSignalSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<TSender, TEnv,
                TCancellableAlgorithmCompletionSignatures, TVoidValueCompletionSignatures>{};
    }

private:
    TSender Sender;
    TLoop* Loop;
    int Signum;
};

template<>
class TUponSignalSender<TJustSender<>>
        : public stdexec::sender_adaptor_closure<TUponSignalSender<TJustSender<>>> {
public:
    using sender_concept = stdexec::sender_t;

public:
    TUponSignalSender(TJustSender<> sender, TLoop& loop, int signum)
        : Sender(std::move(sender)), Loop{&loop}, Signum{signum}
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) const {
        return uvexec::upon_signal(std::forward<TSender>(sender), *Loop, Signum);
    }

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TUponSignalSender s, TReceiver&& rec) {
        return TUponSignalOpState<TJustSender<>, std::decay_t<TReceiver>>(
                *s.Loop, s.Signum, std::move(s.Sender), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TUponSignalSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TUponSignalSender&, const TEnv&) noexcept {
        return stdexec::transform_completion_signatures_of<TJustSender<>, TEnv,
                TCancellableAlgorithmCompletionSignatures, TVoidValueCompletionSignatures>{};
    }

private:
    [[no_unique_address]] TJustSender<> Sender;
    TLoop* Loop;
    int Signum;
};

template <typename TAdapterClosure> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TUponSignalSender<TJustSender<>> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

template <stdexec::sender TSender> requires std::same_as<TLoop::TScheduler,
        std::invoke_result_t<stdexec::get_completion_scheduler_t<stdexec::set_value_t>, stdexec::env_of_t<TSender>>>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::upon_signal_t, TSender, std::tuple<int>> s) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(s));
    return TUponSignalSender<std::decay_t<TSender>>(std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data));
}

template <stdexec::sender TSender, typename TEnv>
    requires std::same_as<TLoop::TScheduler, std::invoke_result_t<stdexec::get_scheduler_t, const TEnv&>>
auto tag_invoke(TLoop::TDomain d,
        TSenderPackage<uvexec::upon_signal_t, TSender, std::tuple<int>> s, const TEnv& e) noexcept(
        std::is_nothrow_constructible_v<std::decay_t<TSender>, TSender>) {
    auto sch = stdexec::get_scheduler(e);
    return TUponSignalSender<std::decay_t<TSender>>(std::move(s.Sender), d.GetLoop(sch), std::get<0>(s.Data));
}

inline auto tag_invoke(uvexec::schedule_upon_signal_t, TLoop::TScheduler s, int signum) noexcept {
    return TUponSignalScheduleSender(TLoop::TDomain{}.GetLoop(s), signum);
}

}
