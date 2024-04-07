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

#include <stdexec/execution.hpp>

#include <uvexec/meta/deduce.hpp>

namespace NUvExec {

struct TGetEarlyDomain {
    template <stdexec::sender TSender>
    auto operator()(const TSender& sender) const noexcept {
        if constexpr (std::invocable<stdexec::get_domain_t, stdexec::env_of_t<TSender>>) {
            return stdexec::get_domain(stdexec::get_env(sender));
        } else {
            return stdexec::default_domain{};
        }
    }
};

inline constexpr TGetEarlyDomain GetEarlyDomain;

template <typename... TArgs>
using TJustSender = std::invoke_result_t<stdexec::just_t, TArgs...>;

template <typename TTag, stdexec::sender TSender, typename TTupleOfData>
struct TSenderPackageBase {
    template <typename... Ts>
    static constexpr auto TagInvocableWith(NMeta::TDeduce<std::tuple<Ts...>>) noexcept -> bool {
        return stdexec::tag_invocable<TTag, TSender&&, Ts...>;
    }

    template <typename... Ts>
    static constexpr auto NothrowTagInvocableWith(NMeta::TDeduce<std::tuple<Ts...>>) noexcept -> bool {
        return stdexec::nothrow_tag_invocable<TTag, TSender&&, Ts...>;
    }

    struct TTagInvokeWithResult {
        template <typename... Ts>
        constexpr auto operator()(std::tuple<Ts...>) -> stdexec::tag_invoke_result_t<TTag, TSender&&, Ts...>;
    };

    TSenderPackageBase(TSender&& sender, TTupleOfData&& data) noexcept
        : Sender(std::move(sender)), Data(std::move(data))
    {}

    auto TagInvoke() noexcept(NothrowTagInvocableWith(NMeta::Deduce<TTupleOfData>())) {
        static_assert(TagInvocableWith(NMeta::Deduce<TTupleOfData>()),
                "Algorithm has no default implementation and must be customized via tag_invoke");
        return std::apply(stdexec::tag_invoke,
                std::tuple_cat(
                        std::tuple<TTag>{},
                        std::forward_as_tuple(std::move(Sender)),
                        std::move(Data)));
    }

    template <typename TEnv>
    auto GetCompSigs(const TEnv&) const noexcept {
        if constexpr (TagInvocableWith(NMeta::Deduce<TTupleOfData>())) {
            using TTagInvokedSender = std::invoke_result_t<TTagInvokeWithResult, TTupleOfData>;
            return std::invoke_result_t<stdexec::get_completion_signatures_t, const TTagInvokedSender&, const TEnv&>{};
        } else {
            return stdexec::completion_signatures<>{};
        }
    };

    TSender Sender;
    TTupleOfData Data;
};

template <typename TTag, stdexec::sender TSender, typename TTupleOfData>
struct TSenderPackage : public TSenderPackageBase<TTag, TSender, TTupleOfData> {
    using sender_concept = stdexec::sender_t;

    TSenderPackage(TSender&& sender, TTupleOfData&& data) noexcept
        : TSenderPackageBase<TTag, TSender, TTupleOfData>{std::move(sender), std::move(data)}
    {}

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TSenderPackage p, TReceiver&& rec) {
        return stdexec::connect(p.TagInvoke(), std::forward<TReceiver>(rec));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TSenderPackage& p) noexcept {
        return stdexec::get_env(p.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TSenderPackage& s, const TEnv& e) noexcept {
        return s.GetCompSigs(e);
    }
};

template <typename TTag, typename TTupleOfData>
struct TSenderPackage<TTag, TJustSender<>, TTupleOfData>
        : public TSenderPackageBase<TTag, TJustSender<>, TTupleOfData>
        , public stdexec::sender_adaptor_closure<TSenderPackage<TTag, TJustSender<>, TTupleOfData>> {
    using sender_concept = stdexec::sender_t;
    using TSender = TJustSender<>;

    TSenderPackage(TSender&& sender, TTupleOfData&& data) noexcept
        : TSenderPackageBase<TTag, TSender , TTupleOfData>(std::move(sender), std::move(data))
    {}

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TSenderPackage p, TReceiver&& rec) {
        return stdexec::connect(p.TagInvoke(), std::forward<TReceiver>(rec));
    }

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) && {
        return std::apply(TTag{}, std::tuple_cat(
                std::forward_as_tuple(std::forward<TSender>(sender)), std::move(this->Data)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TSenderPackage& p) noexcept {
        return stdexec::get_env(p.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TSenderPackage& s, const TEnv& e) noexcept {
        return s.GetCompSigs(e);
    }
};

template <typename TAdapterClosure, typename TTag, typename TTupleOfData> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TSenderPackage<TTag, TJustSender<>, TTupleOfData> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

template <typename TTag>
struct TMakeSenderPackage {
    template <stdexec::sender TSender, typename TTupleOfData>
    auto operator()(TSender&& sender, TTupleOfData&& data) const noexcept {
        return TSenderPackage<TTag, std::decay_t<TSender>, std::decay_t<TTupleOfData>>(
                std::forward<TSender>(sender), std::forward<TTupleOfData>(data));
    }
};

template <typename TTag>
inline constexpr TMakeSenderPackage<TTag> MakeSenderPackage;

}
