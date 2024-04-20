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

#include "close_receiver.hpp"


namespace NUvExec {

template <stdexec::sender TSender, typename THandle>
class TCloseSender {
public:
    using sender_concept = stdexec::sender_t;

public:
    TCloseSender(TSender sender, THandle& handle)
        : Sender(std::move(sender)), Handle{&handle}
    {}

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TCloseSender s, TReceiver&& rec) {
        return stdexec::connect(std::move(s.Sender), TCloseReceiver(*s.Handle, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TCloseSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TCloseSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TSender, TEnv,
                stdexec::completion_signatures<>, TVoidValueCompletionSignatures>{};
    }

private:
    TSender Sender;
    THandle* Handle;
};

template<typename THandle>
class TCloseSender<TJustSender<>, THandle>
        : public stdexec::sender_adaptor_closure<TCloseSender<TJustSender<>, THandle>> {
public:
    using sender_concept = stdexec::sender_t;

public:
    TCloseSender(TJustSender<> sender, THandle& handle)
        : Sender(sender), Handle{&handle}
    {}

    template <stdexec::sender TSender>
    auto operator()(TSender&& sender) const {
        return uvexec::close(std::forward<TSender>(sender), *Handle);
    }

    template <stdexec::receiver TReceiver>
    friend auto tag_invoke(stdexec::connect_t, TCloseSender s, TReceiver&& rec) {
        return stdexec::connect(s.Sender, TCloseReceiver(*s.Handle, std::forward<TReceiver>(rec)));
    }

    friend auto tag_invoke(stdexec::get_env_t, const TCloseSender& s) noexcept {
        return stdexec::get_env(s.Sender);
    }

    template <typename TEnv>
    friend auto tag_invoke(stdexec::get_completion_signatures_t, const TCloseSender&, const TEnv&) noexcept {
        return stdexec::make_completion_signatures<TJustSender<>, TEnv,
                stdexec::completion_signatures<>, TVoidValueCompletionSignatures>{};
    }

private:
    [[no_unique_address]] TJustSender<> Sender;
    THandle* Handle;
};

template <typename TAdapterClosure, typename THandle> requires std::derived_from<
        std::decay_t<TAdapterClosure>, stdexec::sender_adaptor_closure<std::decay_t<TAdapterClosure>>>
auto operator|(TCloseSender<TJustSender<>, THandle> sender, TAdapterClosure&& closure) {
    return std::forward<TAdapterClosure>(closure)(std::move(sender));
}

}
