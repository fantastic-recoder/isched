#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

#include "isched/isched_GqlParser.hpp"

unsigned int factorial( unsigned int number ) {
    return number <= 1 ? number : factorial(number-1)*number;
}

TEST_CASE( "Factorials are computed", "[factorial]" ) {
REQUIRE( factorial(1) == 1 );
REQUIRE( factorial(2) == 2 );
REQUIRE( factorial(3) == 6 );
REQUIRE( factorial(10) == 3628800 );
}

using isched::v0_0_1::GqlParser;
using isched::v0_0_1::IGdlParserTree;

TEST_CASE("001 Empty query", "[grammar0]" ) {
    static const auto myQuery001="{}";
    GqlParser myParser;
    auto myTree = myParser.parse(myQuery001,"myQuery001");
    REQUIRE(true == myTree->isParsingOk());
}

TEST_CASE("002 Empty query with whitespace", "[grammar0]" ) {
    static const auto myQuery002=" {} ";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery002,"myQuery002")->isParsingOk());
}

TEST_CASE( "003 Simplest GQL query", "[grammar0]" ) {

    static const auto myQuery010=R"Qry(
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery010,"myQuery010")->isParsingOk());
}

TEST_CASE( "004 Nested GQL query", "[grammar0]" ) {

    static const auto myNestedQuery=R"Qry(
    {
        hero {
                name
        }
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myNestedQuery,"myNestedQuery")->isParsingOk());
}

TEST_CASE( "005 Comments in query", "[grammar0]" ) {
    static const char* myQueryWithComments001=R"Qry(#graphql
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQueryWithComments001, "myQueryWithComments001")->isParsingOk());
    static const char* myQueryWithComments002=R"Qry(#graphql
    {
        # Hero resource
        hero
    }
    )Qry";
    REQUIRE(true == myParser.parse(myQueryWithComments002, "myQueryWithComments002")->isParsingOk());
}

TEST_CASE("006 Book grammar test","[grammar0]") {
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
