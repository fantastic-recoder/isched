// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_resolver_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 tests for resolver integration.
 *
 * Placeholder tests for future resolver coverage.
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

unsigned int factorial( unsigned int number ) {
    return number <= 1 ? number : factorial(number-1)*number;
}

TEST_CASE( "001", "[resolvers]" ) {
REQUIRE( factorial(1) == 1 );
REQUIRE( factorial(2) == 2 );
REQUIRE( factorial(3) == 6 );
REQUIRE( factorial(10) == 3628800 );
}

