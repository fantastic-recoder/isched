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

// ===== Tests for GraphQL Ignored tokens =====

TEST_CASE("Ignored positive cases", "[graphql][ignored][positive]") {
    using isched::v0_0_1::Ignored;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "IgnoredGood");
        auto res = generate_ast_and_log<Ignored>(std::string("Ignored good: ")+s, in, false);
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

TEST_CASE("Ignored negative: empty", "[graphql][ignored][negative]") {
    using isched::v0_0_1::Ignored;
    string_input in(std::string(""), "IgnoredBadEmpty");
    auto res = generate_ast_and_log<Ignored>(std::string("Ignored bad: <empty>"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: letter a", "[graphql][ignored][negative]") {
    using isched::v0_0_1::Ignored;
    string_input in(std::string("a"), "IgnoredBadA");
    auto res = generate_ast_and_log<Ignored>(std::string("Ignored bad: a"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: digit 1", "[graphql][ignored][negative]") {
    using isched::v0_0_1::Ignored;
    string_input in(std::string("1"), "IgnoredBad1");
    auto res = generate_ast_and_log<Ignored>(std::string("Ignored bad: 1"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: ellipsis ...", "[graphql][ignored][negative]") {
    using isched::v0_0_1::Ignored;
    string_input in(std::string("..."), "IgnoredBadEllipsis");
    auto res = generate_ast_and_log<Ignored>(std::string("Ignored bad: ..."), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Ignored negative: double quote", "[graphql][ignored][negative]") {
    using isched::v0_0_1::Ignored;
    string_input in(std::string("\""), "IgnoredBadQuote");
    auto res = generate_ast_and_log<Ignored>(std::string("Ignored bad: \""), in, false);
    REQUIRE(std::get<0>(res) == false);
}

// Helper rule for testing sequences of Ignored (IgnoredMany is now in the grammar header)
namespace isched { namespace v0_0_1 {
    struct IgnoredAroundToken : tao::pegtl::seq< IgnoredMany, Token, IgnoredMany > {};
} }

TEST_CASE("IgnoredMany sequences and around Token", "[graphql][ignored][integration]") {
    using isched::v0_0_1::IgnoredMany;
    using isched::v0_0_1::IgnoredAroundToken;
    using isched::v0_0_1::Token;

    // A mix of different ignored pieces should be consumed by IgnoredMany
    {
        std::string s;
        s.append("\xEF\xBB\xBF", 3); // BOM
        s += " \t,  # c\n\r";       // space, tab, comma, comment, CR
        string_input in(s, "IgnoredManyGood");
        auto res = generate_ast_and_log<IgnoredMany>("IgnoredMany good", in, false);
        REQUIRE(std::get<0>(res) == true);
    }

    // Token with ignored around it should parse
    {
        std::string s = std::string("# lead\n   ,\t") + "foo" + std::string(",  # trail\n");
        string_input in(s, "IgnoredAroundTokenGood");
        auto res = generate_ast_and_log<IgnoredAroundToken>("IgnoredAroundToken good", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
}

// ===== Tests for GraphQL Name =====

namespace isched { namespace v0_0_1 {
    struct JustName : tao::pegtl::seq< Name, tao::pegtl::eof > {};
} }

TEST_CASE("Name positive cases", "[graphql][name][positive]") {
    using isched::v0_0_1::JustName;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "NameGood");
        auto res = generate_ast_and_log<JustName>(std::string("Name good: ")+s, in, false);
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

TEST_CASE("Name negative: empty", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string(""), "NameBadEmpty");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: <empty>"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: starts with digit", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("1a"), "NameBadDigit");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: 1a"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: starts with dash", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("-a"), "NameBadDash");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: -a"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dash", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("a-b"), "NameBadInnerDash");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: a-b"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains space", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("a b"), "NameBadSpace");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: a b"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dollar", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("a$"), "NameBaddollar");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: a$"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: contains dot", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("a.b"), "NameBadDot");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: a.b"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Name negative: single quote", "[graphql][name][negative]") {
    using isched::v0_0_1::JustName;
    string_input in(std::string("'"), "NameBadQuote");
    auto res = generate_ast_and_log<JustName>(std::string("Name bad: '"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

// ===== Tests for GraphQL Punctuator (per spec) =====

namespace isched { namespace v0_0_1 {
    struct JustTokenPunctuator : tao::pegtl::seq< TokenPunctuator, tao::pegtl::eof > {};
} }

TEST_CASE("Punctuator positive cases", "[graphql][punctuator][positive]") {
    using isched::v0_0_1::JustTokenPunctuator;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "PunctuatorGood");
        auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator good: ")+s, in, false);
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

TEST_CASE("Punctuator negative: empty", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string(""), "PunctuatorBadEmpty");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: <empty>"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: dot", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("."), "PunctuatorBadDot");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: ."), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: two dots", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string(".."), "PunctuatorBadTwoDots");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: .."), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: four dots", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("...."), "PunctuatorBadFourDots");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: ...."), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: ampersand", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("&"), "PunctuatorBadAmp");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: &"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: comma", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string(","), "PunctuatorBadComma");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: ,"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: semicolon", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string(";"), "PunctuatorBadSemicolon");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: ;"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: asterisk", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("*"), "PunctuatorBadAsterisk");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: *"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: plus", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("+"), "PunctuatorBadPlus");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: +"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: minus", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("-"), "PunctuatorBadMinus");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: -"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: slash", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("/"), "PunctuatorBadSlash");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: /"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: less-than", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("<"), "PunctuatorBadLT");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: <"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: greater-than", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string(">"), "PunctuatorBadGT");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: >"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Punctuator negative: question-mark", "[graphql][punctuator][negative]") {
    using isched::v0_0_1::JustTokenPunctuator;
    string_input in(std::string("?"), "PunctuatorBadQM");
    auto res = generate_ast_and_log<JustTokenPunctuator>(std::string("Punctuator bad: ?"), in, false);
    REQUIRE(std::get<0>(res) == false);
}

// ===== Tests for GraphQL Description (alias of StringValue) =====

namespace isched { namespace v0_0_1 {
    struct JustDescription : tao::pegtl::seq< Description, tao::pegtl::eof > {};
} }

TEST_CASE("Description positive cases", "[graphql][description][positive]") {
    using isched::v0_0_1::JustDescription;
    auto expect_ok = [](const std::string& s){
        string_input in(std::string(s), "DescriptionGood");
        auto res = generate_ast_and_log<JustDescription>(std::string("Description good: ")+s, in, false);
        REQUIRE(std::get<0>(res) == true);
    };
    // Quoted string description
    expect_ok("\"A description\"");
    // Block string description
    expect_ok("\"\"\"Multi\nline\nDescription\"\"\"");
}

TEST_CASE("Description negative cases", "[graphql][description][negative]") {
    using isched::v0_0_1::JustDescription;
    auto expect_fail = [](const std::string& s){
        string_input in(std::string(s), "DescriptionBad");
        auto res = generate_ast_and_log<JustDescription>(std::string("Description bad: ")+s, in, false);
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

TEST_CASE("SchemaDefinition accepts optional Description", "[graphql][description][integration]") {
    using isched::v0_0_1::SchemaDefinition;
    {
        // With description (block string)
        std::string s = R"("""My schema"""
schema { query: Query })";
        string_input in(s, "SchemaWithDescription");
        auto res = generate_ast_and_log<SchemaDefinition>("Schema with description", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
    {
        // Without description
        std::string s = R"(schema{query:Query})";
        string_input in(s, "SchemaWithoutDescription");
        auto res = generate_ast_and_log<SchemaDefinition>("Schema without description", in, false);
        REQUIRE(std::get<0>(res) == true);
    }
}

// ===== Tests for SchemaDefinition with Directives =====
namespace isched { namespace v0_0_1 {
    struct JustSchemaDefinition : tao::pegtl::seq< SchemaDefinition, tao::pegtl::eof > {};
} }

TEST_CASE("SchemaDefinition with directives (no args)", "[graphql][schema][directives][positive]") {
    using isched::v0_0_1::JustSchemaDefinition;
    std::string s = R"(schema @a @b { query: Query })";
    string_input in(s, "SchemaDirectivesNoArgs");
    auto res = generate_ast_and_log<JustSchemaDefinition>("Schema directives no args", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("SchemaDefinition with directives and arguments", "[graphql][schema][directives][positive]") {
    using isched::v0_0_1::JustSchemaDefinition;
    std::string s = R"(schema @feature(flag: true, name: "X", n: null, count: 3, rate: 1.5, mode: FAST) { query: Query mutation: Mut })";
    string_input in(s, "SchemaDirectivesArgs");
    auto res = generate_ast_and_log<JustSchemaDefinition>("Schema directives with args", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("SchemaDefinition multiple root ops and whitespace/comments", "[graphql][schema][positive]") {
    using isched::v0_0_1::JustSchemaDefinition;
    std::string s = R"(
        schema  # lead comment
        {
          query: Query  # q
          
          mutation: Mutation  # m
          subscription: Sub  # s
        }
    )";
    string_input in(s, "SchemaMultiOps");
    auto res = generate_ast_and_log<JustSchemaDefinition>("Schema multiple ops", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("SchemaDefinition negative cases", "[graphql][schema][negative]") {
    using isched::v0_0_1::JustSchemaDefinition;
    auto expect_fail = [](const std::string& s, const char* name){
        string_input in(std::string(s), name);
        auto res = generate_ast_and_log<JustSchemaDefinition>(std::string("Schema bad: ")+s, in, false);
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

TEST_CASE("Document positive: single SelectionSet", "[graphql][document][positive]") {
    using isched::v0_0_1::Document;
    // Simplest executable document: just a selection set
    std::string s = R"({ hero })";
    string_input in(s, "DocSelectionSet");
    auto res = generate_ast_and_log<Document>("Document selection set", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document positive: schema definition with description", "[graphql][document][positive]") {
    using isched::v0_0_1::Document;
    std::string s = R"(
        """My awesome schema"""
        schema { query: Query }
    )";
    string_input in(s, "DocSchema");
    auto res = generate_ast_and_log<Document>("Document schema", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document positive: multiple type definitions with Ignored", "[graphql][document][positive]") {
    using isched::v0_0_1::Document;
    std::string s;
    s.append("\xEF\xBB\xBF", 3); // BOM
    s += R"(  # lead comment
        type A{ a: Int }
        ,  # comma is Ignored
        type B { b: String }
    )";
    string_input in(s, "DocTypes");
    auto res = generate_ast_and_log<Document>("Document types", in, false);
    REQUIRE(std::get<0>(res) == true);
}

TEST_CASE("Document negative: empty input", "[graphql][document][negative]") {
    using isched::v0_0_1::Document;
    string_input in(std::string(""), "DocEmpty");
    auto res = generate_ast_and_log<Document>("Document empty", in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: only Ignored", "[graphql][document][negative]") {
    using isched::v0_0_1::Document;
    std::string s = std::string("\xEF\xBB\xBF", 3) + " ,  \n# just a comment\n\r\t  ";
    string_input in(s, "DocOnlyIgnored");
    auto res = generate_ast_and_log<Document>("Document only ignored", in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: trailing garbage", "[graphql][document][negative]") {
    using isched::v0_0_1::Document;
    std::string s = R"(type A{ a: Int } ???)";
    string_input in(s, "DocTrailing");
    auto res = generate_ast_and_log<Document>("Document trailing garbage", in, false);
    REQUIRE(std::get<0>(res) == false);
}

TEST_CASE("Document negative: stray name", "[graphql][document][negative]") {
    using isched::v0_0_1::Document;
    std::string s = R"(name)";
    string_input in(s, "DocStrayName");
    auto res = generate_ast_and_log<Document>("Document stray name", in, false);
    REQUIRE(std::get<0>(res) == false);
}
