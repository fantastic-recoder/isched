#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

#include "isched/isched_gql_grammar.hpp"
#include "isched/isched_gql_parser.hpp"

namespace {
    unsigned int factorial( unsigned int number ) {
        return number <= 1 ? number : factorial(number-1)*number;
    }
}

using namespace isched::v0_0_1;
using namespace tao::pegtl;

TEST_CASE( "Dummy factorials are computed", "[factorial]" ) {
REQUIRE( factorial(1) == 1 );
REQUIRE( factorial(2) == 2 );
REQUIRE( factorial(3) == 6 );
REQUIRE( factorial(10) == 3628800 );
}

using isched::v0_0_1::GqlParser;
using isched::v0_0_1::IGdlParserTree;

TEST_CASE("Empty query", "grammar0" ) {
    static const auto myQuery001="{}";
    GqlParser myParser;
    auto myTree = myParser.parse(myQuery001,"myQuery001");
    REQUIRE(true == myTree->isParsingOk());
}

TEST_CASE("Empty query with whitespace", "grammar0" ) {
    static const auto myQuery002=" {} ";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery002,"myQuery002")->isParsingOk());
}

TEST_CASE( "Simplest GQL query", "grammar0" ) {

    static const auto myQuery010=R"Qry(
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery010,"myQuery010")->isParsingOk());
}

TEST_CASE( "Nested GQL query", "grammar0" ) {

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

TEST_CASE( "Comments in query", "grammar0" ) {
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

TEST_CASE("Parse type fields","grammar0") {
    string_input in(std::move(std::string("title: String")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing field",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Book grammar test","grammar0") {
    static const char* myBookQuery=R"Qry(#graphql

    # Comments in GraphQL strings (such as this one) start with the hash (#) symbol.
    # This "Book" type defines the queryable fields for every book in our data source.

    type Book {
            title: String
            author: String
    }

    type Author {
      name: String
    }

    # The "Query" type is special: it lists all of the available queries that
    # clients can execute, along with the return type for each. In this
    # case, the "books" query returns an array of zero or more Books (defined above).

    type Query {
            books: [Book]
    }


)Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myBookQuery,"book")->isParsingOk());
}

TEST_CASE("Parse integer array","grammar0") {
    string_input in(std::move(std::string("my_int_array: [Int]!")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing int array",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse type reference","grammar0") {
    string_input in(std::move(std::string("appearsIn: [Episode!]!")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing type reference",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Character type grammar test","grammar0") {
    static const char* myCharacterQry=R"Qry(#graphql
    type Character {
        name: String!
        appearsIn: [Episode!]!
    }
)Qry";
    GqlParser myParser;
    auto myTree = myParser.parse(myCharacterQry,"Charecter");
    REQUIRE(true == myTree->isParsingOk());
}


TEST_CASE("Parse float field","grammar0") {
    string_input in(std::move(std::string("rating: Float")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing float field",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse boolean field","grammar0") {
    string_input in(std::move(std::string("isPublished: Boolean!")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing boolean field",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse ID field","grammar0") {
    string_input in(std::move(std::string("nodeId: ID")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing ID field",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Misc type with built-ins","grammar0") {
    static const char* myMiscType=R"Qry(#graphql
    type Misc {
        rating: Float
        isPublished: Boolean!
        nodeId: ID!
    }
)Qry";
    GqlParser myParser;
    auto myTree = myParser.parse(myMiscType,"Misc");
    REQUIRE(true == myTree->isParsingOk());
}
