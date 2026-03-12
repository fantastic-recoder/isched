// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_BaseRestResolver.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Abstract base interface for REST request resolvers (legacy).
 *
 * Defines `BaseRestResolver`, the polymorphic interface implemented by all
 * REST resolvers in the legacy HTTP service layer.  Scheduled for removal
 * in Phase 7 once the GraphQL-only transport replaces the REST surface.
 *
 * @deprecated Superseded by the GraphQL transport in `isched_Server.hpp`.
 */

#ifndef BASERESOLVER_HPP
#define BASERESOLVER_HPP
#include <string>


namespace isched::v0_0_1 {
    /**
     * \brief base class for all resolvers.
    */
    class BaseRestResolver {
    public:
        virtual ~BaseRestResolver() = default;

        virtual std::string &getPath() = 0;

        virtual std::string &getMethod() = 0;

        virtual std::string handle(std::string&&) = 0;
    };
}

// v0_0_1::isched

#endif //BASERESOLVER_HPP
