//
// Created by groby on 2025-12-19.
//

#ifndef ISCHED_ISCHED_EXECUTION_RESULT_HPP
#define ISCHED_ISCHED_EXECUTION_RESULT_HPP

#include <nlohmann/json.hpp>
#include <vector>

#include "isched_EErrorCodes.hpp"

namespace isched::v0_0_1::backend {

    struct ExecutionError {
        EErrorCodes code;
        std::string message;
    };

    inline nlohmann::json ec_to_json(const std::vector<ExecutionError>& pErrors) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& error : pErrors) {
            result.push_back(nlohmann::json{
                {"message", error.message},
                {"code", static_cast<int>(error.code)}
            });
        }
        return result;
    }
    /**
     * @brief GraphQL execution result
     */
    struct ExecutionResult {
        nlohmann::json data; ///< Query result data
        std::vector<ExecutionError> errors; ///< Execution errors
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
