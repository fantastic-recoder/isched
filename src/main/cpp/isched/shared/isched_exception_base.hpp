// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_exception_base.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Abstract base class for all isched exceptions.
 *
 * Defines `ExceptionBase` with `what()` and `code()` virtual accessors,
 * establishing the common interface for the isched exception hierarchy.
 */

#ifndef ISCHED_EXCEPTIONBASE_HPP
#define ISCHED_EXCEPTIONBASE_HPP


namespace isched::v0_0_1 {

    class ExceptionBase {
    public:
        virtual ~ExceptionBase() = default;

        // Return the error message
        [[nodiscard]] virtual const char* what() const noexcept = 0;

        // (optional) Return an error code associated with this exception
        [[nodiscard]] virtual int code() const noexcept { return 0; }
    };
}


#endif //EXCEPTIONBASE_HPP
