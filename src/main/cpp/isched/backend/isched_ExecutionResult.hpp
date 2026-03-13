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
#include <variant>
#include <vector>

#include "isched_gql_error.hpp"

namespace isched::v0_0_1::backend {

    inline nlohmann::json ec_to_json(const gql::TErrorVector& pErrors) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& error : pErrors) {
            nlohmann::json err{
                {"message", error.message},
                {"code", static_cast<int>(error.code)}
            };
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
