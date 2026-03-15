// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_log_result.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Logging helpers for `ExecutionResult` and string-vector formatting.
 *
 * Provides `log_result()` (emits data/errors via spdlog at error/debug level)
 * and `concat_vector()` (joins elements of a vector into a delimited string).
 */

#ifndef ISCHED_LOG_RESULT_HPP
#define ISCHED_LOG_RESULT_HPP

#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include "isched/backend/isched_ExecutionResult.hpp"

namespace isched::v0_0_1::backend
{
    inline void log_result(const ExecutionResult& p_result) {
        if (!p_result.is_success()) {
            spdlog::error("Result: failed, data: {}", p_result.data.dump(4,'.'));
            for (int aI=0;const auto& err : p_result.errors) {
                spdlog::error( "Result error no=\t{}: {}.",++aI,err.message);
            }
        } else {
            spdlog::debug("Result: ok, data: {}", p_result.data.dump(4,'.'));
        }
    }

    template<typename T>
    constexpr std::string concat_vector(const std::vector<T>& p_vec, const std::string& p_sep=", ") {
        std::string ret;
        for (int pI=0;const auto& elem : p_vec) {
            if (++pI>1) {
                ret +=p_sep;
            }
            ret += elem ;
        }
        return ret;
    }
}

#endif //ISCHED_LOG_RESULT_HPP