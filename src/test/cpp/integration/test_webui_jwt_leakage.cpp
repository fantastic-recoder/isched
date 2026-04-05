// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_webui_jwt_leakage.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md - Mozilla Public License 2.0
 * @brief Regression tests that ensure JWT values are not leaked in GraphQL errors
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-05
 */

#include <catch2/catch_test_macros.hpp>

#include <isched/backend/isched_Server.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

using json = nlohmann::json;
using namespace isched::v0_0_1::backend;

namespace {
constexpr auto kSensitiveBearer = "Bearer super-secret-jwt-token-do-not-leak";
} // namespace

TEST_CASE("GraphQL responses never echo Authorization token content", "[webui][security][jwt]") {
    auto server = Server::create();

    SECTION("Query parse/validation errors do not contain bearer value") {
        const std::string malformed_query = "query {";
        const auto response = server->execute_graphql(malformed_query, "{}", kSensitiveBearer, "127.0.0.1");

        REQUIRE(response.find("super-secret-jwt-token-do-not-leak") == std::string::npos);
    }

    SECTION("CSRF failures do not contain bearer value") {
        const std::string mutation_query = R"(
            mutation {
                completePlatformBootstrap(input: {
                    email: "admin@example.com"
                    password: "SecurePassword123!"
                }) {
                    success
                }
            }
        )";

        std::unordered_map<std::string, std::string> headers;
        headers["Origin"] = "http://localhost:8080";

        const auto response = server->execute_graphql(
            mutation_query,
            "{}",
            kSensitiveBearer,
            "127.0.0.1",
            headers);

        REQUIRE(response.find("super-secret-jwt-token-do-not-leak") == std::string::npos);

        const auto parsed = json::parse(response);
        REQUIRE(parsed.contains("errors"));
        REQUIRE(parsed["errors"].is_array());
        REQUIRE_FALSE(parsed["errors"].empty());
    }
}

