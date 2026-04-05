// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_ExecutionResult.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief GraphQL execution result type and JSON serialisation helpers.
 *
 * Defines `ExecutionResult` (data + errors vector) and `ec_to_json()` which
 * converts a `TErrorVector` into the JSON `errors` array required by the
 * GraphQL over HTTP specification.
 */

#ifndef ISCHED_ISCHED_EXECUTION_RESULT_HPP
#define ISCHED_ISCHED_EXECUTION_RESULT_HPP

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <charconv>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "isched_gql_error.hpp"

namespace isched::v0_0_1::backend {

    inline const char* gql_error_code_name(const gql::EErrorCodes code) {
        switch (code) {
            case gql::EErrorCodes::OK: return "OK";
            case gql::EErrorCodes::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            case gql::EErrorCodes::MISSING_GQL_RESOLVER: return "MISSING_GQL_RESOLVER";
            case gql::EErrorCodes::PARSE_ERROR: return "PARSE_ERROR";
            case gql::EErrorCodes::EXECUTABLE_DEF_NOT_ALLOWED: return "EXECUTABLE_DEF_NOT_ALLOWED";
            case gql::EErrorCodes::ARGUMENT_ERROR: return "ARGUMENT_ERROR";
            case gql::EErrorCodes::FORBIDDEN: return "FORBIDDEN";
            case gql::EErrorCodes::RATE_LIMITED: return "RATE_LIMITED";
            case gql::EErrorCodes::UNAUTHENTICATED: return "UNAUTHENTICATED";
            case gql::EErrorCodes::VALIDATION_FAILED: return "VALIDATION_FAILED";
            case gql::EErrorCodes::CONFLICT: return "CONFLICT";
            case gql::EErrorCodes::CONTEXT_MISMATCH: return "CONTEXT_MISMATCH";
            case gql::EErrorCodes::TRANSIENT_NETWORK: return "TRANSIENT_NETWORK";
            case gql::EErrorCodes::CSRF_FAILED: return "CSRF_FAILED";
        }
        return "UNKNOWN_ERROR";
    }

    inline std::optional<int> extract_retry_after_ms(const std::string& message) {
        constexpr std::string_view marker = "retryAfterMs=";
        const auto idx = message.find(marker);
        if (idx == std::string::npos) {
            return std::nullopt;
        }

        const char* begin = message.data() + static_cast<std::ptrdiff_t>(idx + marker.size());
        const char* end = message.data() + static_cast<std::ptrdiff_t>(message.size());
        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if (ec != std::errc{} || ptr == begin || parsed < 0) {
            return std::nullopt;
        }
        return parsed;
    }

    inline nlohmann::json ec_to_json(const gql::TErrorVector& pErrors) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& error : pErrors) {
            nlohmann::json err{
                {"message", error.message},
                {"code", static_cast<int>(error.code)}
            };
            nlohmann::json extensions{{"code", gql_error_code_name(error.code)}};
            if (error.code == gql::EErrorCodes::RATE_LIMITED) {
                if (const auto retry_after_ms = extract_retry_after_ms(error.message); retry_after_ms.has_value()) {
                    extensions["retryAfterMs"] = *retry_after_ms;
                }
            }
            err["extensions"] = std::move(extensions);
            if (!error.locations.empty()) {
                auto locs = nlohmann::json::array();
                for (const auto& loc : error.locations) {
                    locs.push_back({{"line", loc.line}, {"column", loc.column}});
                }
                err["locations"] = std::move(locs);
            }
            if (!error.path.empty()) {
                auto path_arr = nlohmann::json::array();
                for (const auto& elem : error.path) {
                    std::visit([&path_arr](const auto& v) { path_arr.push_back(v); }, elem);
                }
                err["path"] = std::move(path_arr);
            }
            result.push_back(std::move(err));
        }
        return result;
    }
    /**
     * @brief GraphQL execution result
     */
    struct ExecutionResult {
        nlohmann::json data; ///< Query result data
        gql::TErrorVector errors; ///< Execution errors
        nlohmann::json extensions; ///< Optional extensions
        std::chrono::milliseconds execution_time{0}; ///< Execution duration

        /**
         * @brief Convert result to JSON response
         * @return JSON response according to GraphQL spec
         */
        [[nodiscard]] nlohmann::json to_json() const {
            nlohmann::json result;

            if (!data.is_null()) {
                result["data"] = data;
            }

            if (!errors.empty()) {
                result["errors"] = ec_to_json(errors);
            }

            if (!extensions.is_null()) {
                result["extensions"] = extensions;
            }

            return result;
        }

        /**
         * @brief Check if execution was successful
         * @return true if no errors occurred
         */
        [[nodiscard]] bool is_success() const noexcept {
            return errors.empty();
        }

    };
}
#endif //ISCHED_ISCHED_EXECUTION_RESULT_HPP
