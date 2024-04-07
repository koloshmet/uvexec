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

#include <concepts>

namespace NUvExec {

template <typename T>
struct TIntrusiveListNode {
    TIntrusiveListNode* Next{nullptr};
    TIntrusiveListNode* Prev{nullptr};
};

template <typename T>
class TIntrusiveList {
    using TNode = TIntrusiveListNode<T>;
    static_assert(std::derived_from<T, TNode>);

public:
    TIntrusiveList() noexcept = default;

    void Add(T& node) noexcept {
        node.Next = Head;
        if (Head != nullptr) {
            Head->Prev = &node;
        }
        Head = &node;
    }

    auto Pop() -> T& {
        auto head = Head;
        Erase(static_cast<T&>(*Head));
        return static_cast<T&>(*head);
    }

    void Erase(T& node) {
        if (node.Next != nullptr) {
            node.Next->Prev = node.Prev;
        }
        if (node.Prev != nullptr) {
            node.Prev->Next = node.Next;
        }
        if (&node == Head) {
            Head = node.Next;
        }
        node.Next = node.Prev = nullptr;
    }

    [[nodiscard]]
    auto Empty() const noexcept -> bool {
        return Head == nullptr;
    }

private:
    TNode* Head{nullptr};
};

}
