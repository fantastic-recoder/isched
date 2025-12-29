//
// Created by groby on 2025-12-19.
//

#ifndef ISCHED_ISCHED_EXECUTIONRESULT_HPP
#define ISCHED_ISCHED_EXECUTIONRESULT_HPP

#include <nlohmann/json.hpp>
#include <vector>

namespace isched::v0_0_1::backend {
    /**
 * @brief GraphQL execution result
 */
    struct ExecutionResult {
        nlohmann::json data;                             ///< Query result data
        std::vector<std::string> errors;                 ///< Execution errors
        nlohmann::json extensions;                       ///< Optional extensions
        std::chrono::milliseconds execution_time{0};     ///< Execution duration

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
                result["errors"] = errors;
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
#endif //ISCHED_ISCHED_EXECUTIONRESULT_HPP