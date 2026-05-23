// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_operation_tests.cpp
 * @brief Tests for GraphQL operation execution (Sections 4 & 5)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "isched/backend/isched_GqlExecutor.hpp"
#include "isched/backend/isched_DatabaseManager.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <print>

using namespace isched::v0_0_1::backend;
using nlohmann::json;

TEST_CASE("GraphQL Operations and Fragments Execution", "[graphql][executor][operations]") {
    auto db = std::make_shared<DatabaseManager>();
    GqlExecutor executor(db);

    executor.register_resolver({"Query"}, "hello", [](const json& p, const json& a, const ResolverCtx& c) { return "world"; });
    executor.register_resolver({"Query"}, "user", [](const json& p, const json& a, const ResolverCtx& c) {
        return json{{"id", "1"}, {"name", "Alice"}, {"email", "alice@example.com"}};
    });

    ResolverCtx admin_ctx;
    admin_ctx.roles = {"role_platform_admin"};

    SECTION("Execute query with multiple operations and operationName selected") {
        std::string query = R"(
            query Op1 { hello }
            query Op2 { user { name } }
        )";
        
        auto resultOp1 = executor.execute(query, "{}", std::string_view("Op1"));
        if (!resultOp1.is_success() && !resultOp1.errors.empty()) {
            std::println("Op1 error: {}", resultOp1.errors[0].message);
        }
        REQUIRE(resultOp1.is_success());
        REQUIRE(resultOp1.data["hello"] == "world");

        auto resultOp2 = executor.execute(query, "{}", admin_ctx, std::string_view("Op2"));
        if (!resultOp2.is_success() && !resultOp2.errors.empty()) {
            std::println("Op2 error: {}", resultOp2.errors[0].message);
        }
        REQUIRE(resultOp2.is_success());
        REQUIRE(resultOp2.data["user"]["name"] == "Alice");
    }

    SECTION("Fails if operationName not provided and multiple operations exist") {
        std::string query = R"(
            query Op1 { hello }
            query Op2 { user { name } }
        )";
        auto result = executor.execute(query, "{}");
        REQUIRE(!result.is_success());
        REQUIRE(result.errors.size() > 0);
        REQUIRE_THAT(result.errors[0].message, Catch::Matchers::ContainsSubstring("Must provide operation name"));
    }

    SECTION("Fails if provided operationName does not exist") {
        std::string query = R"(
            query Op1 { hello }
        )";
        auto result = executor.execute(query, "{}", std::string_view("Op2"));
        REQUIRE(!result.is_success());
        REQUIRE(result.errors.size() > 0);
        REQUIRE_THAT(result.errors[0].message, Catch::Matchers::ContainsSubstring("Unknown operation"));
    }

    SECTION("Execute query with fragment spread") {
        std::string query = R"(
            query {
                user {
                    ...UserFragment
                }
            }
            fragment UserFragment on User {
                id
                name
            }
        )";
        auto result = executor.execute(query, "{}", admin_ctx);
        if (!result.is_success() && !result.errors.empty()) {
            std::println("Fragment spread error: {}", result.errors[0].message);
        }
        REQUIRE(result.is_success());
        REQUIRE(result.data["user"]["id"] == "1");
        REQUIRE(result.data["user"]["name"] == "Alice");
        REQUIRE(!result.data["user"].contains("email"));
    }

    SECTION("Execute query with nested fragments") {
        std::string query = R"(
            query {
                user {
                    ...Frag1
                }
            }
            fragment Frag1 on User {
                id
                ...Frag2
            }
            fragment Frag2 on User {
                name
            }
        )";
        auto result = executor.execute(query, "{}", admin_ctx);
        if (!result.is_success() && !result.errors.empty()) {
            std::println("Nested fragment error: {}", result.errors[0].message);
        }
        REQUIRE(result.is_success());
        REQUIRE(result.data["user"]["id"] == "1");
        REQUIRE(result.data["user"]["name"] == "Alice");
    }

    SECTION("Detects cycle in fragment spread") {
        std::string query = R"(
            query {
                user {
                    ...CycleFrag
                }
            }
            fragment CycleFrag on User {
                id
                ...CycleFrag
            }
        )";
        auto result = executor.execute(query, "{}", admin_ctx);
        // Cyclic fragment spreads should be caught or gracefully ignored.
        // Our implementation ignores cyclic fragments by cycle detection.
        if (!result.is_success() && !result.errors.empty()) {
            std::println("Cycle error: {}", result.errors[0].message);
        }
        REQUIRE(result.is_success());
        REQUIRE(result.data["user"]["id"] == "1");
    }

    SECTION("Execute query with inline fragment") {
        std::string query = R"(
            query {
                user {
                    ... on User {
                        email
                    }
                }
            }
        )";
        auto result = executor.execute(query, "{}", admin_ctx);
        if (!result.is_success() && !result.errors.empty()) {
            std::println("Inline fragment error: {}", result.errors[0].message);
        }
        REQUIRE(result.is_success());
        REQUIRE(result.data["user"]["email"] == "alice@example.com");
    }
}
