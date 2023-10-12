#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>
#include "../../../main/cpp/isched/GqlParser.hpp"

unsigned int Factorial( unsigned int number ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}

TEST_CASE( "Factorials are computed", "[factorial]" ) {
REQUIRE( Factorial(1) == 1 );
REQUIRE( Factorial(2) == 2 );
REQUIRE( Factorial(3) == 6 );
REQUIRE( Factorial(10) == 3628800 );
}

using isched::v0_0_1::GqlParser;

TEST_CASE("Empty query", "[grammar0]" ) {
    static const char* myQuery001="{}";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery001,"myQuery001"));
}

TEST_CASE( "Simplest GQL query", "[grammar0]" ) {

    static const char* myQuery002=" {} ";
    static const char* myQuery010=R"Qry(
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery002,"myQuery002"));
    REQUIRE(true == myParser.parse(myQuery010,"myQuery010"));
}

TEST_CASE( "Nested GQL query", "[grammar0]" ) {

    static const char* myNestedQuery=R"Qry(
    {
        hero {
                name
        }
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myNestedQuery,"myNestedQuery"));
}

TEST_CASE( "Comments in query", "[grammar0]" ) {
    static const char* myQueryWithComments001=R"Qry(#graphql
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQueryWithComments001, "myQueryWithComments001"));
    static const char* myQueryWithComments002=R"Qry(#graphql
    {
        # Hero resource
        hero
    }
    )Qry";
    REQUIRE(true == myParser.parse(myQueryWithComments002, "myQueryWithComments002"));
}

TEST_CASE("Book grammar test","[grammar0]") {
    static const char* myBookQuery=R"Qry(#graphql
# Comments in GraphQL strings (such as this one) start with the hash (#) symbol.
# This "Book" type defines the queryable fields for every book in our data source.

    type Book {
            title: String
            author: String
    }

# The "Query" type is special: it lists all of the available queries that
# clients can execute, along with the return type for each. In this
# case, the "books" query returns an array of zero or more Books (defined above).

    type Query {
            books: [Book]
    }

})Qry";
    GqlParser myParser;
    //REQUIRE(true == myParser.parse(myBookQuery,"book"));

}