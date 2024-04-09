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

#include "deduce.hpp"


namespace NMeta::NDetail {

template <typename>
struct TMethodArg{};

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg)> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) &> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) &&> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const&> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const&&> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) noexcept> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) & noexcept> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) && noexcept> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const noexcept> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const& noexcept> { using TType = TArg; };

template <typename TRes, typename TFn, typename TArg>
struct TMethodArg<TRes(TFn::*)(TArg) const&& noexcept> { using TType = TArg; };

template <typename T, template <typename...> typename TCont, typename... Ts>
auto BindFront(TDeduce<T>, TDeduce<TCont<Ts...>>) -> TCont<T, Ts...>;

template <typename TSeek, std::size_t Idx, typename... Ts>
struct TIndexOf {};

template <typename TSeek, std::size_t Idx>
struct TIndexOf<TSeek, Idx> {
    static constexpr auto Value = Idx;
};

template <typename TSeek, std::size_t Idx, typename T, typename... Ts>
struct TIndexOf<TSeek, Idx, T, Ts...> {
    static constexpr std::size_t Value{std::is_same_v<TSeek, T> ? Idx : TIndexOf<TSeek, Idx + 1, Ts...>::Value};
};

template <typename TSeek, typename... Ts>
constexpr auto IndexOf = NDetail::TIndexOf<TSeek, 0, Ts...>::Value;

template <typename TSeek, template <typename...> typename TCont, typename... Ts>
constexpr auto IndexOfImpl(TDeduce<TSeek>, TDeduce<TCont<Ts...>>) noexcept -> std::size_t {
    return IndexOf<TSeek, Ts...>;
}

template <typename TSeek, template <typename...> typename TCont, typename... Ts>
constexpr auto ContainsImpl(TDeduce<TSeek>, TDeduce<TCont<Ts...>>) noexcept -> bool {
    return IndexOf<TSeek, Ts...> < sizeof...(Ts);
}

}
