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
/*
TEST_CASE("Empty query", "grammar0" ) {
    static const auto myQuery001="{}";
    GqlParser myParser;
    auto myTree = myParser.parse(myQuery001,"myQuery001");
    REQUIRE(true == myTree->isParsingOk());
}*/

/*
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
*/
TEST_CASE("Parse type fields","grammar0") {
    string_input in(std::move(std::string("title: String")), "Query");
    auto myRoot = generate_ast_and_log<GqlTypeField>("Parsing field",in,true);
    REQUIRE(std::get<0>(myRoot) == true );
}

/*
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
#1#

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
#1#


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
*/

TEST_CASE("IntValue positive cases", "[graphql][intvalue][positive]") {
    using isched::v0_0_1::IntValue;
    {
        string_input in(std::string("0"), "IntValue0");
        auto res = generate_ast_and_log<IntValue>("IntValue 0", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("7"), "IntValue7");
        auto res = generate_ast_and_log<IntValue>("IntValue 7", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("42"), "IntValue42");
        auto res = generate_ast_and_log<IntValue>("IntValue 42", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("1234567890"), "IntValueLong");
        auto res = generate_ast_and_log<IntValue>("IntValue long", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("-1"), "IntValueNeg1");
        auto res = generate_ast_and_log<IntValue>("IntValue -1", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("-999"), "IntValueNeg999");
        auto res = generate_ast_and_log<IntValue>("IntValue -999", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
}

TEST_CASE("IntValue negative cases", "[graphql][intvalue][negative]") {
    using isched::v0_0_1::IntValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "IntValueBad");
        auto res = generate_ast_and_log<IntValue>
        (std::string("IntValue bad: ")+s, in, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Leading zeros not allowed
    expect_fail("01");
    expect_fail("-01");
    // Plus sign not allowed for IntValue
    expect_fail("+1");
    // Float-like numbers are not IntValue
    expect_fail("1.0");
    expect_fail("-2.5");
    // Exponents are floats, not ints
    expect_fail("1e10");
    expect_fail("-3E5");
    // Incomplete or empty
    expect_fail("");
    expect_fail("-");
    // Trailing garbage
    expect_fail("0x10");
}

TEST_CASE("FloatValue positive cases", "[graphql][floatvalue][positive]") {
    using isched::v0_0_1::FloatValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "FloatValueGood");
        auto res = generate_ast_and_log<FloatValue>(std::string("FloatValue good: ")+s, in, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Fractional part variants
    expect_ok("0.0");
    expect_ok("1.0");
    expect_ok("123.456");
    expect_ok("-0.123");
    // Exponent variants without fraction
    expect_ok("1e10");
    expect_ok("9E0");
    expect_ok("10e+3");
    expect_ok("-3E5");
    // Fraction + exponent
    expect_ok("10.0e-3");
    expect_ok("-3.5E-2");
    expect_ok("0e10");
}

TEST_CASE("FloatValue negative cases", "[graphql][floatvalue][negative]") {
    using isched::v0_0_1::FloatValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "FloatValueBad");
        auto res = generate_ast_and_log<FloatValue>(std::string("FloatValue bad: ")+s, in, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Leading zeros before non-zero integer part are not allowed
    expect_fail("01.0");
    expect_fail("-01.0");
    // Missing integer or fractional digits
    expect_fail(".5");
    expect_fail("1.");
    // Invalid exponents
    expect_fail("1e");
    expect_fail("1E+");
    // Plus sign is not allowed
    expect_fail("+1.0");
    // Garbage / identifiers
    expect_fail("NaN");
    expect_fail("Infinity");
    expect_fail("1.0a");
    // Malformed numbers
    expect_fail("1..0");
    expect_fail("e10");
    expect_fail("--1.0");
}

TEST_CASE("StringValue positive cases", "[graphql][stringvalue][positive]") {
    using isched::v0_0_1::StringValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "StringValueGood");
        auto res = generate_ast_and_log<StringValue>(std::string("StringValue good: ")+s, in, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Empty string
    expect_ok("\"\"");
    // Simple text
    expect_ok("\"hello\"");
    // Escaped quote and backslash
    expect_ok("\"\\\"quote\\\"\\\\\\\\\""); // "quote"\\
    // Forward slash and common escapes
    expect_ok("\"a\\/b\\nline\\tTab\\rR\\fF\\bB\\\\\\\"\"");
    // Unicode escape
    expect_ok("\"Unicode A: \\u0041\"");
}

TEST_CASE("StringValue negative cases", "[graphql][stringvalue][negative]") {
    using isched::v0_0_1::StringValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "StringValueBad");
        auto res = generate_ast_and_log<StringValue>(std::string("StringValue bad: ")+s, in, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Unclosed quote
    expect_fail("\"unterminated");
    // Newline not allowed inside quoted string
    expect_fail("\"line1\nline2\"");
    // Invalid escape sequence
    expect_fail("\"bad \\x escape\"");
    // Bad unicode escape (non-hex)
    expect_fail("\"bad unicode \\u12G4\"");
    // Incomplete unicode escape
    expect_fail("\"incomplete \\u123\"");
}

TEST_CASE("BlockString positive cases", "[graphql][blockstring][positive]") {
    using isched::v0_0_1::StringValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "BlockStringGood");
        auto res = generate_ast_and_log<StringValue>(std::string("BlockString good: ")+s, in, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Empty block string
    expect_ok("\"\"\"\"\"\"");
    // Simple text
    expect_ok("\"\"\"hello\"\"\"");
    // Multiline content
    expect_ok("\"\"\"line1\nline2\nline3\"\"\"");
    // Escaped triple quotes inside
    expect_ok("\"\"\"before \\\"\"\" after\"\"\"");
    // Backslashes and quotes around
    expect_ok("\"\"\"path \\ file \"name\"\"\"\"");
}

TEST_CASE("BlockString negative cases", "[graphql][blockstring][negative]") {
    using isched::v0_0_1::StringValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "BlockStringBad");
        auto res = generate_ast_and_log<StringValue>(std::string("BlockString bad: ")+s, in, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Unterminated block string
    expect_fail("\"\"\"unterminated");
}

TEST_CASE("Token positive cases", "[graphql][token][positive]") {
    using isched::v0_0_1::Token;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "TokenGood");
        auto res = generate_ast_and_log<Token>(std::string("Token good: ")+s, in, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Name
    expect_ok("foo");
    expect_ok("_bar9");
    // IntValue
    expect_ok("0");
    expect_ok("-12");
    // FloatValue
    expect_ok("3.14");
    expect_ok("1e10");
    // StringValue (quoted)
    expect_ok("\"hi\"");
    // StringValue (block string)
    expect_ok("\"\"\"block\nstring\"\"\"");
    // Punctuators
    expect_ok("!");
    expect_ok("$");
    expect_ok("(");
    expect_ok(")");
    expect_ok(":");
    expect_ok("=");
    expect_ok("@");
    expect_ok("[");
    expect_ok("]");
    expect_ok("{");
    expect_ok("}");
    expect_ok("|");
    // Ellipsis
    expect_ok("...");
}

TEST_CASE("Token negative cases", "[graphql][token][negative]") {
    using isched::v0_0_1::Token;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "TokenBad");
        auto res = generate_ast_and_log<Token>(std::string("Token bad: ")+s, in, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Empty / whitespace only
    expect_fail("");
    expect_fail("   ");
    // Comments are not tokens alone
    expect_fail("# just a comment");
    // Invalid numbers or symbols
    expect_fail("+1");
    expect_fail("01");
    expect_fail(".");
    expect_fail("..");
    // Unterminated string
    expect_fail("\"unterminated");
}
