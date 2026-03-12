// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_error.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief GraphQL error codes and error-list types used across the execution pipeline.
 *
 * Defines `EErrorCodes`, the `Error` struct, and `TErrorVector`.  These
 * types are the primary error-propagation currency between the parser,
 * executor, and transport layers.
 */

#ifndef ISCHED_E_ERROR_CODES_HPP
#define ISCHED_E_ERROR_CODES_HPP

#include <string>
#include <vector>

namespace isched::v0_0_1::gql {
    enum class EErrorCodes {
        OK = 0,
        UNKNOWN_ERROR = 1,
        MISSING_GQL_RESOLVER = 2, PARSE_ERROR = 3, EXECUTABLE_DEF_NOT_ALLOWED =4, ARGUMENT_ERROR
    };

    struct Error {
        EErrorCodes code = EErrorCodes::UNKNOWN_ERROR;
        std::string message{};
    };

    using TErrorVector = std::vector<Error>;

}

#endif //ISCHED_E_ERROR_CODES_HPP