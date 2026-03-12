// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_EHttpMethods.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief HTTP method enumeration and `toString()` helper (legacy).
 *
 * Provides `EHttpMethods` (GET, POST) consumed by the legacy REST resolver
 * layer.  Scheduled for removal in Phase 7.
 *
 * @deprecated Used only by legacy REST resolvers scheduled for Phase 7 removal.
 */

#ifndef ISCHED_EHTTPMETHODS_HPP
#define ISCHED_EHTTPMETHODS_HPP

#include <string>

#include "../shared/isched_exception_unknown_enum_value.hpp"

namespace isched::v0_0_1 {
    enum class EHttpMethods {
        GET,
        POST
    };

    constexpr  const char* toString(const EHttpMethods pMethod)
    {
        switch(pMethod)
        {
            case EHttpMethods::GET: return "GET";
            case EHttpMethods::POST: return "POST";
        }
        throw new ExceptionUnknownEnumValue("Unknown value in toString conversion of EHttpMethods.");
    }
}

#endif //ISCHED_EHTTPMETHODS_HPP
