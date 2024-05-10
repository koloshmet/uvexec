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

#include "detail.hpp"

#include <tuple>


namespace NMeta {

template <typename... Ts>
using TDecayedTuple = std::tuple<std::decay_t<Ts>...>;

template <typename TFn>
using TMethodArgType = typename NDetail::TMethodArg<TFn>::TType;

template <typename TFn>
using TFnParameterType = std::invoke_result_t<NDetail::TFunctionParameterTypeImpl, TFn>;

template <typename T>
using TRemoveReferenceWrapperType = typename NDetail::TRemoveReferenceWrapper<T>::TType;

template <typename T, typename TContainer>
using TBindFront = decltype(NDetail::BindFront(Deduce<T>(), Deduce<TContainer>()));

template <typename TSeek, typename TContainer>
constexpr std::size_t IndexOf = NDetail::IndexOfImpl(Deduce<TSeek>(), Deduce<TContainer>());

template <typename T, typename TContainer>
concept IsIn = NDetail::ContainsImpl(Deduce<T>(), Deduce<TContainer>());

template <typename T, typename TContainer>
concept IsInDecayed = NDetail::ContainsImpl(Deduce<std::decay_t<T>>(), Deduce<TContainer>());

}
