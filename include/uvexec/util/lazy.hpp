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

#include <functional>


namespace NUvExec {

template<std::invocable TF> requires std::is_nothrow_move_constructible_v<TF>
class TLazy {
public:
    explicit TLazy(TF lazy) noexcept: Lazy(std::move(lazy)) {}
    operator std::invoke_result_t<TF>() && noexcept(std::is_nothrow_invocable_v<TF>) {
        return std::invoke(std::move(Lazy));
    }

private:
    TF Lazy;
};

template <typename T, std::movable... TArgs>
    requires (std::is_nothrow_move_constructible_v<TArgs> && ...) && (std::is_nothrow_move_assignable_v<TArgs> && ...)
class TLazyConstruct {
    struct TTypeMemory {
        ~TTypeMemory() {
            std::destroy_at(std::launder(reinterpret_cast<T*>(Data)));
        }

        alignas(T) std::byte Data[sizeof(T)];
    };

public:
    explicit(sizeof...(TArgs) == 1) TLazyConstruct(TArgs... args) noexcept
        : Value(std::in_place_index<0>, std::move(args)...)
    {}

    TLazyConstruct(TLazyConstruct&& val) noexcept
        : Value(std::in_place_index<0>, std::get<0>(std::move(val.Value)))
    {}

    auto operator=(TLazyConstruct&& val) noexcept -> TLazyConstruct& {
        if (val.Constructed()) {
            std::terminate(); // Trying to move a non-movable
        }
        Value = std::move(val.Value);
    }

    operator T&() noexcept(noexcept(Transmogrify())) {
        if (!Constructed()) {
            Transmogrify();
        }
        return *std::launder(reinterpret_cast<T*>(std::get<1>(Value).Data));
    }

    operator const T&() noexcept(std::is_nothrow_convertible_v<TLazyConstruct, T&>) {
        return static_cast<T&>(*this);
    }

    operator const T&() const noexcept {
        if (!Constructed()) {
            std::terminate();
        }
        return *std::launder(reinterpret_cast<T*>(std::get<1>(Value).Data));
    }

private:
    void Transmogrify() noexcept(std::is_nothrow_constructible_v<T, TArgs&&...>) {
        auto args = std::get<0>(std::move(Value));
        Value.template emplace<1>();
        std::construct_at(reinterpret_cast<T*>(std::get<1>(Value).Data), Lazy([a = std::move(args)]{
            return std::make_from_tuple<T>(std::move(a));
        }));
    }

    [[nodiscard]]
    auto Constructed() const noexcept -> bool {
        return Value.index() == 1;
    }

private:
    std::variant<std::tuple<TArgs...>, TTypeMemory> Value;
};

template <std::invocable TF> requires std::is_nothrow_move_constructible_v<TF>
auto Lazy(TF f) noexcept {
    return TLazy<TF>(std::move(f));
}

template <typename T>
struct TLazyConstructFn {
    template <typename... TArgs>
    auto operator()(TArgs&&... args) const noexcept {
        return TLazyConstruct<T, std::decay_t<TArgs>...>(std::forward<TArgs&&>(args)...);
    }
};

template <typename T>
inline constexpr TLazyConstructFn<T> LazyConstruct;

}
