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
#include <uvexec/execution/error_code.hpp>


namespace NUvExec {

class TCategory final : public std::error_category {
public:
    const char* name() const noexcept override {
        return "uv";
    }
    std::string message(int ec) const override {
        return ::uv_strerror(ec);
    }
};

const std::error_category& Category() noexcept {
    static TCategory c;
    return c;
}

auto make_error_code(EErrc e) noexcept -> std::error_code {
    return {static_cast<int>(e), Category()};
}

}