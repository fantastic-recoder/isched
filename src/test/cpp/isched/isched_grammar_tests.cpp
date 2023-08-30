#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>
#include "../../../main/cpp/isched/GraphQlParser.hpp"

unsigned int Factorial( unsigned int number ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}

TEST_CASE( "Factorials are computed", "[factorial]" ) {
REQUIRE( Factorial(1) == 1 );
REQUIRE( Factorial(2) == 2 );
REQUIRE( Factorial(3) == 6 );
REQUIRE( Factorial(10) == 3628800 );
}

using isched::v0_0_1::GraphQlParser;

TEST_CASE( "Simple grammar", "[grammar0]" ) {
    static const char* myQuery001="{}";
    static const char* myQuery002=" {} ";
    static const char* myQuery010=R"Qry(
    {
        hero
    }
    )Qry";
    static const char* myQuery011=R"Qry(
    {
        hero {
                name
        }
    }
    )Qry";
    GraphQlParser myParser;
    REQUIRE(true == myParser.parse(myQuery001));
    REQUIRE(true == myParser.parse(myQuery002));
    REQUIRE(true == myParser.parse(myQuery010));
}