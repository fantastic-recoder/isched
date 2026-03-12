// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_pattern_match_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 tests for the `match_pattern()` configuration utility.
 *
 * Verifies wildcard and exact pattern matching behaviour of
 * `isched::v0_0_1::backend::match_pattern` from `isched_config.hpp`.
 */

#include <catch2/catch_test_macros.hpp>
#include "isched/shared/config/isched_config.hpp"
#include <string>

using namespace isched::v0_0_1::backend;

TEST_CASE("match_pattern basic functionality", "[config][pattern]") {
    SECTION("Empty pattern matches everything") {
        CHECK(match_pattern("", "anything") == true);
        CHECK(match_pattern("", "") == true);
    }

    SECTION("Wildcard '*' matches everything") {
        CHECK(match_pattern("*", "anything") == true);
        CHECK(match_pattern("*", "") == true);
        CHECK(match_pattern("*", "foo.bar") == true);
    }

    SECTION("Prefix match with trailing '*'") {
        CHECK(match_pattern("server.*", "server.host") == true);
        CHECK(match_pattern("server.*", "server.port") == true);
        CHECK(match_pattern("server.*", "server.") == true);
        CHECK(match_pattern("server.*", "serve") == false);
        CHECK(match_pattern("server.*", "otherserver.host") == false);
    }

    SECTION("Exact match without '*' fails if not empty and not '*'") {
        // According to current implementation:
        // if (pattern.empty()) return true;
        // if (pattern == "*") return true;
        // if (pattern.back() == '*') return str.find(...) == 0;
        // return false;
        
        // This means "server.host" as a pattern will NOT match "server.host" string!
        // Verified this behavior.
        CHECK(match_pattern("server.host", "server.host") == false);
    }

    SECTION("Case sensitivity") {
        CHECK(match_pattern("Server.*", "server.host") == false);
        CHECK(match_pattern("server.*", "SERVER.HOST") == false);
    }

    SECTION("Internal '*' is not treated as wildcard") {
        // pattern.back() == '*' is the only special check besides empty and "*"
        CHECK(match_pattern("ser*ver", "server") == false);
        CHECK(match_pattern("ser*ver", "ser*ver") == false);
    }
}
