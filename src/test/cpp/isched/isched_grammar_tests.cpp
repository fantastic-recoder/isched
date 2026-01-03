#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

#include "../../../main/cpp/isched/backend/isched_gql_grammar.hpp"
#include "../../../main/cpp/isched/backend/isched_GqlParser.hpp"

namespace {
    unsigned int factorial( unsigned int number ) {
        return number <= 1 ? number : factorial(number-1)*number;
    }
}

using namespace isched::v0_0_1;
using namespace isched::v0_0_1::gql;
using namespace tao::pegtl;

TEST_CASE( "Dummy factorials are computed", "[factorial]" ) {
REQUIRE( factorial(1) == 1 );
REQUIRE( factorial(2) == 2 );
REQUIRE( factorial(3) == 6 );
REQUIRE( factorial(10) == 3628800 );
}

using isched::v0_0_1::GqlParser;
using isched::v0_0_1::IGdlParserTree;

TEST_CASE("Empty query", "[grammar][executable][positive]") {
    static const auto myQuery001="{}";
    GqlParser myParser;
    auto myTree = myParser.parse(myQuery001,"myQuery001");
    REQUIRE(true == myTree->isParsingOk());
}

TEST_CASE("Empty query with whitespace", "[grammar][executable][positive]") {
    static const auto myQuery002=" {} ";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery002,"myQuery002")->isParsingOk());
}

TEST_CASE( "Simplest GQL query", "[grammar][executable][positive]" ) {

    static const auto myQuery010=R"Qry(
    {
        hero
    }
    )Qry";
    GqlParser myParser;
    REQUIRE(true == myParser.parse(myQuery010,"myQuery010")->isParsingOk());
}

TEST_CASE( "Nested GQL query", "[grammar][executable][positive]" ) {

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

TEST_CASE( "Comments in query", "[grammar][executable][positive]" ) {
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

TEST_CASE("Test WS and comments","[grammar][lexical][positive]") {
    {
        string_input in0(std::move(std::string("#graphql ")),"single line comment");
        auto myRoot = generate_ast_and_log<TSeps>(
            in0,"Parsing field",true);
        REQUIRE(std::get<0>(myRoot) == true );
    }
    {
        string_input in0(std::move(std::string(R"(#graphql


    # Comments in GraphQL strings (such as this one) start with the hash (#) symbol.
    # This "Book" type defines the queryable fields for every book in our data source.


)")),"multiple lines comment");
        auto myRoot = generate_ast_and_log<TSeps>(
            in0,"Parsing field",true);
        REQUIRE(std::get<0>(myRoot) == true );
    }
}

TEST_CASE("Test field with arguments","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("hello_who(p_name: String): String")), "field with arguments");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,
        "field with arguments",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse simple type field","[grammar][type-system][positive]")
{
    string_input in(std::move(std::string("title: String")), "simple string field");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,
        "simple string field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse simple non null type field","[grammar][type-system][positive]")
{
    string_input in(std::move(std::string("title: String!")), "simple non null string field");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,
        "simple non null string field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse non null type","[grammar][type-system][positive]") {
    string_input in1{std::move(std::string(R"(String!)")), "parse non null type"};
    auto myRoot = generate_ast_and_log<Type>(in1,
        "parse non null type",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse type fields with sigle non null field","[grammar][type-system][positive]") {
    string_input in1{std::move(std::string(R"(
{
    description: String!
}
)")), "type fields with sigle non null field"};
    auto myRoot = generate_ast_and_log<FieldsDefinition>(in1,
        "type fields with sigle non null field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse type fields","[grammar][type-system][positive]") {
        string_input in1{std::move(std::string(R"(
{
    title: String
    author: String
    description: String!
}
)")), "multiple fields"};
        auto myRoot = generate_ast_and_log<FieldsDefinition>(in1,"Parsing multiple fields",true);
        REQUIRE(std::get<0>(myRoot) == true );
        const auto& fields = std::get<1>(myRoot);
        REQUIRE(fields->children.size() == 3);
        const auto& field00 = fields->children[0]->children[0];
        REQUIRE(field00->string_view() == "title");
        const auto& field01 = fields->children[0]->children[1];
        REQUIRE(field01->string_view() == "String");
        const auto& field10 = fields->children[1]->children[0];
        REQUIRE(field10->string_view() == "author");
        const auto& field11 = fields->children[1]->children[1];
        REQUIRE(field11->string_view() == "String");
    }


TEST_CASE("Parse type fields with comments","[grammar][type-system][positive]")
{
    static const char* myFieldsWithComments=R"Qry(#graphql

    # Comments in GraphQL strings (such as this one) start with the hash (#) symbol.
    # This "Book" type defines the queryable fields for every book in our data source.

    {
            title: String
            author: String
    }
)Qry";
    string_input in2{std::move(myFieldsWithComments), "multiple fields with comments"};
    auto myRoot = generate_ast_and_log<FieldsDefinition>(in2,"Parsing multiple fields with comments",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parsing array field","[grammar][type-system][positive]") {
    //[Episode!]!
    string_input in(std::move(std::string("appearsIn: [Episode!]!")), "array field");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"array field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse simple type","[grammar][type-system][positive]") {
    static const char* myBookQuery=R"Qry(#graphql
    type Book {
            title: String
            author: String
    }
)Qry";
    string_input in(myBookQuery, "simple type I");
    auto myRoot = generate_ast_and_log<ObjectTypeDefinition>(in,"Parsing simple type",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse simple type system definition","[grammar][type-system][positive]") {
    static const char* myBookQuery=R"Qry(#graphql
    type Book {
            title: String
            author: String
    }
)Qry";
    string_input in(myBookQuery, "simple type system definition");
    auto myRoot = generate_ast_and_log<TypeSystemDefinition>(in,
        "Parsing simple type system definition",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse simple query","[grammar][type-system][positive]") {
    static const char* myBookQuery=R"Qry(#graphql
    type Query {
            books: [Book]
    }
)Qry";
    string_input in(myBookQuery, "simple query");
    auto myRoot = generate_ast_and_log<ObjectTypeDefinition>(in,
        "Parsing simple query",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Book grammar test - no comments","[grammar][type-system][positive]") {
    static const char* myBookQuery=R"Qry(
    type Book {
            title: String
            author: String
    }
    type Author {
      name: String
    }
    type Query {
            books: [Book]
    }
)Qry";
    string_input in(myBookQuery, "simple Book type-system no comments");
    auto myRoot = generate_ast_and_log<TypeSystemDocument>(in,
        "simple Book type-system no comments",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Book grammar test","[grammar][type-system][positive]") {
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
    string_input in(myBookQuery, "simple Book type-system");
    auto myRoot = generate_ast_and_log<TypeSystemDocument>(in,
        "simple Book type-system",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse integer array","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("my_int_array: [Int]!")), "Query");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"Parsing int array",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse type reference","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("appearsIn: [Episode!]!")), "Query");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"Parsing type reference",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Character type grammar test","[grammar][type-system][positive]") {
    static const char* myCharacterQry=R"Qry(#graphql
    type Character {
        name: String!
        appearsIn: [Episode!]!
    }
)Qry";
    string_input in(myCharacterQry, "Character type grammar");
    auto myRoot = generate_ast_and_log<TypeSystemDocument>(in,
        "Character type grammar",true);
    REQUIRE(std::get<0>(myRoot) == true );
}



TEST_CASE("Parse float field","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("rating: Float")), "Query");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"Parsing float field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse boolean field","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("isPublished: Boolean!")), "Query");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"Parsing boolean field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Parse ID field","[grammar][type-system][positive]") {
    string_input in(std::move(std::string("nodeId: ID")), "Query");
    auto myRoot = generate_ast_and_log<FieldDefinition>(in,"Parsing ID field",true);
    REQUIRE(std::get<0>(myRoot) == true );
}

TEST_CASE("Misc type with built-ins","[grammar][type-system][positive]") {
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

TEST_CASE("IntValue positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::IntValue;
    {
        string_input in(std::string("0"), "IntValue0");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue 0", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("7"), "IntValue7");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue 7", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("42"), "IntValue42");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue 42", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("1234567890"), "IntValueLong");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue long", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("-1"), "IntValueNeg1");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue -1", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        string_input in(std::string("-999"), "IntValueNeg999");
        auto res = generate_ast_and_log<IntValue>(in, "IntValue -999", false);
        REQUIRE(std::get<0>(res) == true);
    }
}

TEST_CASE("IntValue negative cases", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::IntValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "IntValueBad");
        auto res = generate_ast_and_log<IntValue>
        (in, std::string("IntValue bad: ")+s, false);
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

TEST_CASE("FloatValue positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::FloatValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "FloatValueGood");
        auto res = generate_ast_and_log<FloatValue>(in, std::string("FloatValue good: ")+s, false);
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

TEST_CASE("FloatValue negative cases", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::FloatValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "FloatValueBad");
        auto res = generate_ast_and_log<FloatValue>(in, std::string("FloatValue bad: ")+s, false);
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

TEST_CASE("StringValue positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::StringValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "StringValueGood");
        auto res = generate_ast_and_log<StringValue>(in, std::string("StringValue good: ")+s, false);
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

TEST_CASE("StringValue negative cases", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::StringValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "StringValueBad");
        auto res = generate_ast_and_log<StringValue>(in, std::string("StringValue bad: ")+s, false);
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

TEST_CASE("BlockString positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::StringValue;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "BlockStringGood");
        auto res = generate_ast_and_log<StringValue>(in, std::string("BlockString good: ")+s, false);
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

TEST_CASE("BlockString negative cases", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::StringValue;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "BlockStringBad");
        auto res = generate_ast_and_log<StringValue>(in, std::string("BlockString bad: ")+s, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Unterminated block string
    expect_fail("\"\"\"unterminated");
}

TEST_CASE("Token positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::Token;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "TokenGood");
        auto res = generate_ast_and_log<Token>(in, std::string("Token good: ")+s, false);
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

TEST_CASE("Token negative cases", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Token;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "TokenBad");
        auto res = generate_ast_and_log<Token>(in, std::string("Token bad: ")+s, false);
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

// ===== Tests for GraphQL Ignored tokens =====

TEST_CASE("Ignored positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::Ignored;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "IgnoredGood");
        auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored good: ")+s, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Space and tab
    expect_ok(" ");
    expect_ok("\t");
    // Line terminators
    expect_ok("\n");
    expect_ok("\r");
    // Comma
    expect_ok(",");
    // Comment (must end with a newline per our Comment rule)
    expect_ok("# just a comment\n");
    // Comment may also end at end-of-file (no trailing newline) per GraphQL spec
    expect_ok("# just a comment at EOF");
    // Unicode BOM (UTF-8: EF BB BF)
    expect_ok(std::string("\xEF\xBB\xBF", 3));
}

TEST_CASE("Ignored negative: empty", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Ignored;
    string_input in(std::string(""), "IgnoredBadEmpty");
    auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored bad: <empty>"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: letter a", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Ignored;
    string_input in(std::string("a"), "IgnoredBadA");
    auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored bad: a"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: digit 1", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Ignored;
    string_input in(std::string("1"), "IgnoredBad1");
    auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored bad: 1"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: ellipsis ...", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Ignored;
    string_input in(std::string("..."), "IgnoredBadEllipsis");
    auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored bad: ..."), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: double quote", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::Ignored;
    string_input in(std::string("\""), "IgnoredBadQuote");
    auto res = generate_ast_and_log<Ignored>(in, std::string("Ignored bad: \""), false);
    REQUIRE(std::get<0>(res) == false);
}

// Helper rule for testing sequences of Ignored (IgnoredMany is now in the grammar header)
namespace isched { namespace v0_0_1 { namespace gql {
    struct IgnoredAroundToken : tao::pegtl::seq< IgnoredMany, Token, IgnoredMany > {};
} } }

TEST_CASE("IgnoredMany sequences and around Token", "[grammar][lexical][integration]") {
    using isched::v0_0_1::gql::IgnoredMany;
    using isched::v0_0_1::gql::IgnoredAroundToken;
    using isched::v0_0_1::gql::Token;

    // A mix of different ignored pieces should be consumed by IgnoredMany
    {
        std::string s;
        s.append("\xEF\xBB\xBF", 3); // BOM
        s += " \t,  # c\n\r";       // space, tab, comma, comment, CR
        string_input in(s, "IgnoredManyGood");
        auto res = generate_ast_and_log<IgnoredMany>(in, "IgnoredMany good", false);
        REQUIRE(std::get<0>(res) == true);
    }

    // Token with ignored around it should parse
    {
        std::string s = std::string("# lead\n   ,\t") + "foo" + std::string(",  # trail\n");
        string_input in(s, "IgnoredAroundTokenGood");
        auto res = generate_ast_and_log<IgnoredAroundToken>(in, "IgnoredAroundToken good", false);
        REQUIRE(std::get<0>(res) == true);
    }
}

// ===== Tests for GraphQL Name =====

namespace isched { namespace v0_0_1 { namespace gql {
    struct JustName : tao::pegtl::seq< Name, tao::pegtl::eof > {};
} } }

TEST_CASE("Name positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::JustName;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "NameGood");
        auto res = generate_ast_and_log<JustName>(in, std::string("Name good: ")+s, false);
        REQUIRE(std::get<0>(res) == true);
    };
    expect_ok("a");
    expect_ok("A");
    expect_ok("_");
    expect_ok("a1");
    expect_ok("_9");
    expect_ok("some_name");
    expect_ok("CamelCase");
    expect_ok("__typename");
    expect_ok("foo_bar_123");
}

TEST_CASE("Name negative: empty", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string(""), "NameBadEmpty");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: <empty>"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: starts with digit", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("1a"), "NameBadDigit");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: 1a"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: starts with dash", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("-a"), "NameBadDash");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: -a"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dash", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("a-b"), "NameBadInnerDash");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: a-b"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains space", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("a b"), "NameBadSpace");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: a b"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dollar", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("a$"), "NameBaddollar");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: a$"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dot", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("a.b"), "NameBadDot");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: a.b"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: single quote", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustName;
    string_input in(std::string("'"), "NameBadQuote");
    auto res = generate_ast_and_log<JustName>(in, std::string("Name bad: '"), false);
    REQUIRE(std::get<0>(res) == false);
}

// ===== Tests for GraphQL Punctuator (per spec) =====

namespace isched { namespace v0_0_1 { namespace gql {
    struct JustTokenPunctuator : tao::pegtl::seq< TokenPunctuator, tao::pegtl::eof > {};
} } }

TEST_CASE("Punctuator positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "PunctuatorGood");
        auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator good: ")+s, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Single-character punctuators
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

TEST_CASE("Punctuator negative: empty", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string(""), "PunctuatorBadEmpty");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: <empty>"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: dot", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("."), "PunctuatorBadDot");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: ."), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: two dots", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string(".."), "PunctuatorBadTwoDots");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: .."), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: four dots", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("...."), "PunctuatorBadFourDots");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: ...."), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: ampersand", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("&"), "PunctuatorBadAmp");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: &"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: comma", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string(","), "PunctuatorBadComma");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: ,"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: semicolon", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string(";"), "PunctuatorBadSemicolon");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: ;"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: asterisk", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("*"), "PunctuatorBadAsterisk");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: *"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: plus", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("+"), "PunctuatorBadPlus");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: +"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: minus", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("-"), "PunctuatorBadMinus");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: -"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: slash", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("/"), "PunctuatorBadSlash");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: /"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: less-than", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("<"), "PunctuatorBadLT");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: <"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: greater-than", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string(">"), "PunctuatorBadGT");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: >"), false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: question-mark", "[grammar][lexical][negative]") {
    using isched::v0_0_1::gql::JustTokenPunctuator;
    string_input in(std::string("?"), "PunctuatorBadQM");
    auto res = generate_ast_and_log<JustTokenPunctuator>(in, std::string("Punctuator bad: ?"), false);
    REQUIRE(std::get<0>(res) == false);
}

// ===== Tests for GraphQL Description (alias of StringValue) =====

namespace isched { namespace v0_0_1 { namespace gql {
    struct JustDescription : tao::pegtl::seq< Description, tao::pegtl::eof > {};
} } }

TEST_CASE("Description positive cases", "[grammar][lexical][positive]") {
    using isched::v0_0_1::gql::JustDescription;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "DescriptionGood");
        auto res = generate_ast_and_log<JustDescription>(in, std::string("Description good: ")+s, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Quoted string description
    expect_ok("\"A description\"");
    // Block string description
    expect_ok("\"\"\"Multi\nline\nDescription\"\"\"");
}

TEST_CASE("Description negative cases", "[grammar][lexical][negative]") {
    using gql::JustDescription;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "DescriptionBad");
        auto res = generate_ast_and_log<JustDescription>(in, std::string("Description bad: ")+s, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Empty input
    expect_fail("");
    // Not a string
    expect_fail("name");
    expect_fail("123");
    // Unterminated quoted string
    expect_fail("\"unterminated");
}

TEST_CASE("SchemaDefinition accepts optional Description", "[grammar][type-system][integration]") {
    using isched::v0_0_1::gql::SchemaDefinition;
    {
        // With description (block string)
        std::string s = R"("""My schema"""
schema { query: Query })";
        string_input in(s, "SchemaWithDescription");
        auto res = generate_ast_and_log<SchemaDefinition>(in, "Schema with description", false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        // Without description
        std::string s = R"(schema{query:Query})";
        string_input in(s, "SchemaWithoutDescription");
        auto res = generate_ast_and_log<SchemaDefinition>(in, "Schema without description", false);
        REQUIRE(std::get<0>(res) == true);
    }
}


TEST_CASE("Test list value argument/input value","[grammar][type-system][positive]") {
    SECTION("ListValue") {
        std::string s = "[1,2,3]";
        string_input in(s, "ListValue");
        auto res = generate_ast_and_log<ListValue>(in, "ListValue", false);
        REQUIRE(std::get<0>(res) == true);
    }
    SECTION("ListValuePlusWs") {
        std::string s = "[1,2,3 , 4 ]";
        string_input in(s, "ListValuePlusWs");
        auto res = generate_ast_and_log<ListValue>(in, "ListValuePlusWs", false);
        REQUIRE(std::get<0>(res) == true);
    }
    SECTION("Value of List of Int") {
        std::string s = "[1,2,3]";
        string_input in(s, "Value");
        auto res = generate_ast_and_log<Value>(in, "Value", false);
        REQUIRE(std::get<0>(res) == true);
    }
    SECTION("Arguments value with list of ints") {
        std::string s = "(l: [1, 2])";
        string_input in(s, "Arguments");
        auto res = generate_ast_and_log<Arguments>(in, "Arguments", false);
        REQUIRE(std::get<0>(res) == true);
    }
    SECTION("OperationDefinition with list of ints") {
        std::string s = "query { test_args(l: [1, 2]) }";
        string_input in(s, "OperationDefinition");
        auto res = generate_ast_and_log<OperationDefinition>(in, "OperationDefinition", false);
        REQUIRE(std::get<0>(res) == true);
    }
    SECTION("Document ListValueArgument") {
        std::string s = "query { test_args(l: [1, 2]) }";
        string_input in(s, "ListValueArgument");
        auto res = generate_ast_and_log<Document>(in, "ListValueArgument", false);
        REQUIRE(std::get<0>(res) == true);
    }
}

// ===== Tests for SchemaDefinition with Directives =====
namespace isched { namespace v0_0_1 { namespace gql {
    struct JustSchemaDefinition : tao::pegtl::seq< SchemaDefinition, tao::pegtl::eof > {};
} } }

TEST_CASE("SchemaDefinition with directives (no args)", "[grammar][type-system][positive]") {
    using isched::v0_0_1::gql::JustSchemaDefinition;
    std::string s = R"(schema @a @b { query: Query })";
    string_input in(s, "SchemaDirectivesNoArgs");
    auto res = generate_ast_and_log<JustSchemaDefinition>(in, "Schema directives no args", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("SchemaDefinition with directives and arguments", "[grammar][type-system][positive]") {
    using isched::v0_0_1::gql::JustSchemaDefinition;
    std::string s = R"(schema @feature(flag: true, name: "X", n: null, count: 3, rate: 1.5, mode: FAST) { query: Query mutation: Mut })";
    string_input in(s, "SchemaDirectivesArgs");
    auto res = generate_ast_and_log<JustSchemaDefinition>(in, "Schema directives with args", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Test RootOperationTypeDefinition","[grammar][type-system][positive]") {
    {
        using isched::v0_0_1::gql::RootOperationTypeDefinition;
        std::string s = R"(query: Query  # q)";
        string_input in(s, "RootOperationTypeDefinition query");
        auto res = generate_ast_and_log<RootOperationTypeDefinition>(in,
            "RootOperationTypeDefinition query", false);
    }
    {
        using isched::v0_0_1::gql::RootOperationTypeDefinition;
        std::string s = R"(mutation: Mutation  # m)";
        string_input in(s, "RootOperationTypeDefinition mutation");
        auto res = generate_ast_and_log<RootOperationTypeDefinition>(in,
            "RootOperationTypeDefinition mutation", false);
    }
    {
        using isched::v0_0_1::gql::RootOperationTypeDefinition;
        std::string s = R"(subscription: Sub  # s)";
        string_input in(s, "RootOperationTypeDefinition sub");
        auto res = generate_ast_and_log<RootOperationTypeDefinition>(in,
            "RootOperationTypeDefinition sub", false);
    }
}

TEST_CASE("SchemaDefinition multiple root ops and whitespace/comments", "[grammar][type-system][positive]") {
    using isched::v0_0_1::gql::JustSchemaDefinition;
    std::string s = R"(
        schema  # lead comment
        {
          query: Query  # q
          
          mutation: Mutation  # m
          subscription: Sub  # s
        }
    )";
    string_input in(s, "SchemaMultiOps");
    auto res = generate_ast_and_log<JustSchemaDefinition>(in, "Schema multiple ops", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("SchemaDefinition negative cases", "[grammar][type-system][negative]") {
    using isched::v0_0_1::gql::JustSchemaDefinition;
    auto expect_fail = [](const std::string& s, const char* name){
        string_input in(std::string(s), name);
        auto res = generate_ast_and_log<JustSchemaDefinition>(in, std::string("Schema bad: ")+s, false);
        REQUIRE(std::get<0>(res) == false);
    };
    // Missing braces
    expect_fail("schema query: Query", "SchemaNoBraces");
    // Empty root operation list
    expect_fail("schema { }", "SchemaEmptyOps");
    // Missing colon
    expect_fail("schema { query Query }", "SchemaMissingColon");
    // Missing type name
    expect_fail("schema { query: }", "SchemaMissingType");
    // Unterminated description
    expect_fail("\"unterminated schema { query: Q }", "SchemaBadDesc");
}

// ===== Tests for GraphQL Document =====

TEST_CASE("Document positive: single SelectionSet", "[grammar][executable][positive]") {
    using isched::v0_0_1::gql::Document;
    // Simplest executable document: just a selection set
    std::string s = R"({ hero })";
    string_input in(s, "DocSelectionSet");
    auto res = generate_ast_and_log<Document>(in, "Document selection set", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document positive: schema definition with description", "[grammar][type-system][positive]") {
    using isched::v0_0_1::gql::Document;
    std::string s = R"(
        """My awesome schema"""
        schema { query: Query }
    )";
    string_input in(s, "DocSchema");
    auto res = generate_ast_and_log<Document>(in, "Document schema", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document positive: multiple type definitions with Ignored", "[grammar][type-system][positive]") {
    using isched::v0_0_1::gql::Document;
    std::string s;
    s.append("\xEF\xBB\xBF", 3); // BOM
    s += R"(  # lead comment
        type A{ a: Int }
        ,  # comma is Ignored
        type B { b: String }
    )";
    string_input in(s, "DocTypes");
    auto res = generate_ast_and_log<Document>(in, "Document types", false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document negative: empty input", "[grammar][internal][negative]") {
    using isched::v0_0_1::gql::Document;
    string_input in(std::string(""), "DocEmpty");
    auto res = generate_ast_and_log<Document>(in, "Document empty", false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: only Ignored", "[grammar][internal][negative]") {
    using isched::v0_0_1::gql::Document;
    std::string s = std::string("\xEF\xBB\xBF", 3) + " ,  \n# just a comment\n\r\t  ";
    string_input in(s, "DocOnlyIgnored");
    auto res = generate_ast_and_log<Document>(in, "Document only ignored", false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: trailing garbage", "[grammar][internal][negative]") {
    using isched::v0_0_1::gql::Document;
    std::string s = R"(type A{ a: Int } ???)";
    string_input in(s, "DocTrailing");
    auto res = generate_ast_and_log<Document>(in, "Document trailing garbage", false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: stray name", "[grammar][internal][negative]") {
    using isched::v0_0_1::gql::Document;
    std::string s = R"(name)";
    string_input in(s, "DocStrayName");
    auto res = generate_ast_and_log<Document>(in, "Document stray name", false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Test scalar","[grammar][type-system][positive]") {
    std::string s = R"(scalar UUID @specifiedBy(url: "https://tools.ietf.org/html/rfc4122"))";
    string_input in(s, "Scalar UUID");
    auto res = generate_ast_and_log<ScalarTypeDefinition>(in, "Scalar UUID", false);
    REQUIRE(std::get<0>(res) == true);
    const auto& ast = std::get<1>(res);
    REQUIRE(ast->children.size() == 1);
    REQUIRE(ast->children[0]->type == "isched::v0_0_1::gql::ScalarTypeDefinition");
    const auto& scalar = ast->children[0];
    REQUIRE(scalar->children.size() == 2);
    REQUIRE(scalar->children[0]->type == "isched::v0_0_1::gql::Name");
    REQUIRE(scalar->children[0]->source == "Scalar UUID");
}

TEST_CASE("Test hello world Query", "[grammar][executable][positive]") {
    std::string s = R"(

type Query {
   hello: String
   hello_who(p_name: String): String
}
)";
    string_input in1(s, "HelloWorldQuery");
    auto res1 = generate_ast_and_log<Document>(in1, "Hello world query", false);
    REQUIRE(std::get<0>(res1) == true);
}

TEST_CASE("Parse Example 9 from spec", "[grammar][spec-examples][positive]") {
    static const char* example9 = R"Qry(
    {
      user(id: 4) {
        id
        name
        profilePic(size: 100)
      }
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example9, "Example 9");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 9", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 1 from spec", "[grammar][spec-examples][positive]") {
    static const char* example1 = R"Qry(
    {
      user(id: 4) {
        name
      }
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example1, "Example 1");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 1", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 5 from spec", "[grammar][spec-examples][positive]") {
    static const char* example5 = R"Qry(
    {
      field
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example5, "Example 5");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 5", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 10 from spec", "[grammar][spec-examples][positive]") {
    static const char* example10 = R"Qry(
    {
      user(id: 4) {
        id
        name
        profilePic(width: 100, height: 50)
      }
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example10, "Example 10");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 10", true);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 17 from spec", "[grammar][spec-examples][positive]") {
    static const char* example17 = R"Qry(
    query noFragments {
      user(id: 4) {
        friends(first: 10) {
          id
          name
          profilePic(size: 50)
        }
        mutualFriends(first: 10) {
          id
          name
          profilePic(size: 50)
        }
      }
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example17, "Example 17");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 17", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 24 from spec", "[grammar][spec-examples][positive]") {
    static const char* example24 = R"Qry(
    mutation {
      sendEmail(message: """
        Hello,
          World!

        Yours,
          GraphQL.
      """)
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example24, "Example 24");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 24", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 26 from spec", "[grammar][spec-examples][positive]") {
    static const char* example26 = R"Qry("""
    This starts with and ends with an empty line,
    which makes it easier to read.
    """)Qry";

    using isched::v0_0_1::gql::StringValue;
    tao::pegtl::string_input in(example26, "Example 26");
    auto res = generate_ast_and_log<StringValue>(in, "Parsing Example 26", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 35 from spec", "[grammar][spec-examples][positive]") {
    static const char* example35 = R"Qry(
    """
    A simple GraphQL schema which is well described.
    """
    schema {
      query: Query
    }

    """
    Root type for all your query operations
    """
    type Query {
      """
      Translates a string from a given language into a different language.
      """
      translate(
        "The original language that `text` is provided in."
        fromLanguage: Language

        "The translated language to be returned."
        toLanguage: Language

        "The text to be translated."
        text: String
      ): String
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example35, "Example 35");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 35", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 39 from spec", "[grammar][spec-examples][positive]") {
    static const char* example39 = R"Qry(
    schema {
      query: MyQueryRootType
      mutation: MyMutationRootType
    }

    type MyQueryRootType {
      someField: String
    }

    type MyMutationRootType {
      setSomeField(to: String): String
    }
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example39, "Example 39");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 39", false);

    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Test multiple arguments", "[grammar][executable][positive]") {
    std::string s = "(a: 1, b: 2)";
    using isched::v0_0_1::gql::Arguments;
    tao::pegtl::string_input in(s, "MultipleArgs");
    auto res = generate_ast_and_log<Arguments>(in, "Parsing multiple args", true);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Parse Example 43 from spec", "[grammar][spec-examples][positive]") {
    static const char* example43 = R"Qry(
    scalar UUID @specifiedBy(url: "https://tools.ietf.org/html/rfc4122")
    scalar URL @specifiedBy(url: "https://tools.ietf.org/html/rfc3986")
    )Qry";

    using isched::v0_0_1::gql::Document;
    tao::pegtl::string_input in(example43, "Example 43");
    auto res = generate_ast_and_log<Document>(in, "Parsing Example 43", false);

    REQUIRE(std::get<0>(res) == true);
}