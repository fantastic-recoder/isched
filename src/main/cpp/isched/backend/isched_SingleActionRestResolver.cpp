// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_SingleActionRestResolver.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of a resolver that responds to a single REST action with a fixed answer.
 */

#include "isched_SingleActionRestResolver.hpp"


namespace isched::v0_0_1 {
    SingleActionRestResolver::SingleActionRestResolver(const EHttpMethods pMethod, const std::string &pPath,
        std::string&& pAnsver) : mMethod(pMethod),
    mMethodStr(toString(pMethod)),
    mPath(pPath),
    mAnsver(std::move(pAnsver))
    {
    }

    SingleActionRestResolver::~SingleActionRestResolver() = default;
} // isched::v0_0_1
