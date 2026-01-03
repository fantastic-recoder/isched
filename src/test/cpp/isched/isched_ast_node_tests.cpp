//
// Created by groby on 2026-01-03.
//

#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include <tao/pegtl/eol_pair.hpp>
#include <tao/pegtl/string_input.hpp>

using tao::pegtl::string_input;
using isched::v0_0_1::gql::generate_ast_and_log;
using isched::v0_0_1::gql::ast_node_to_str;

namespace isched::v0_0_1 {
    TEST_CASE("AST Node to String Conversion", "[isched][ast][string]") {
        SECTION("Schema with description") {
            std::string s = R"("""My schema"""
schema { query: Query })";
            string_input in(s, "SchemaWithDescription");
            auto res = generate_ast_and_log<gql::Document>(in, "Schema with description", false);
            REQUIRE(std::get<0>(res) == true);
            REQUIRE(std::get<1>(res) != nullptr);
            const auto &ast = std::get<1>(res);
            auto s2 = ast_node_to_str(ast);
            REQUIRE(s2.has_value());
            REQUIRE(s == s2.value());
        }

        SECTION("Simple Query") {
            std::string s = "{ hero { name } }";
            string_input in(s, "SimpleQuery");
            auto res = generate_ast_and_log<gql::Document>(in, "Simple Query", false);
            REQUIRE(std::get<0>(res) == true);
            const auto &ast = std::get<1>(res);
            auto s2 = ast_node_to_str(ast);
            REQUIRE(s2.has_value());
            // The output might not exactly match 's' if whitespaces are not in AST
            // Let's see what happens.
            REQUIRE(s2.value() == s);
        }
        SECTION("Complex Mutation") {
            std::string s = R"(mutation {
  sendEmail(message: """
    Hello,
    World!
  """)
})";
            string_input in(s, "ComplexMutation");
            auto res = generate_ast_and_log<gql::Document>(in, "Complex Mutation", false);
            REQUIRE(std::get<0>(res) == true);
            const auto &ast = std::get<1>(res);
            auto s2 = ast_node_to_str(ast);
            REQUIRE(s2.has_value());
            REQUIRE(s2.value() == s);
        }

        SECTION("Type System Definitions") {
            std::string s = R"(
"""
Description of Scalar
"""
scalar MyScalar

type MyType {
  field(arg: Int): String
}
)";
            string_input in(s, "TypeSystemDefinitions");
            auto res = generate_ast_and_log<gql::Document>(in, "Type System Definitions", false);
            REQUIRE(std::get<0>(res) == true);
            const auto &ast = std::get<1>(res);
            auto s2 = ast_node_to_str(ast);
            REQUIRE(s2.has_value());
            REQUIRE(s2.value() == s);
        }
        SECTION("Directives and Arguments") {
            std::string s = R"(
query MyQuery @deprecated(reason: "old") {
  field(arg: 123) @include(if: true)
}
)";
            string_input in(s, "DirectivesAndArguments");
            auto res = generate_ast_and_log<gql::Document>(in, "Directives and Arguments", false);
            REQUIRE(std::get<0>(res) == true);
            const auto &ast = std::get<1>(res);
            auto s2 = ast_node_to_str(ast);
            REQUIRE(s2.has_value());
            REQUIRE(s2.value() == s);
        }
    }
}
