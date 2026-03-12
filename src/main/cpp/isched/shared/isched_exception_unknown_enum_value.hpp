// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_exception_unknown_enum_value.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Exception thrown on unhandled enum-to-string conversion.
 *
 * `ExceptionUnknownEnumValue` is raised by `toString()` helpers (e.g.
 * `EHttpMethods::toString`) when an unexpected enum value is encountered.
 */

#ifndef ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
#define ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
#include <stdexcept>

#include "isched_exception_base.hpp"

namespace isched::v0_0_1 {
    class ExceptionUnknownEnumValue final : public ExceptionBase, public std::runtime_error {

        std::string message{};
        int error_code;

    public:
        explicit ExceptionUnknownEnumValue(const std::string& message, int error_code = 0)
        : std::runtime_error(message), message(message), error_code(error_code) {
        }


        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
        [[nodiscard]] int code() const noexcept override { return error_code; }

    };
}
#endif //ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
