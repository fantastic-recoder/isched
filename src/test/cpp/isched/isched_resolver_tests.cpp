#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

#include "isched/isched_GqlParser.hpp"

unsigned int factorial( unsigned int number ) {
    return number <= 1 ? number : factorial(number-1)*number;
}

TEST_CASE( "001", "[resolvers]" ) {
REQUIRE( factorial(1) == 1 );
REQUIRE( factorial(2) == 2 );
REQUIRE( factorial(3) == 6 );
REQUIRE( factorial(10) == 3628800 );
}

