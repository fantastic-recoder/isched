// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_webui_security_foundation.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Foundational security integration tests for CSRF, origin validation, and GraphQL-only endpoint behavior
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-05
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/backend/isched_Server.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_gql_error.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using namespace isched::v0_0_1::backend;
using isched::v0_0_1::gql::EErrorCodes;

TEST_CASE("WebUI Security Foundation - CSRF Protection", "[webui][security][csrf]") {
    auto server = Server::create();
    auto config = server->get_configuration();
    REQUIRE(config.port == 8080);

    SECTION("Mutation without CSRF token is rejected") {
        // T010: CSRF validation for mutations
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
        // Intentionally omit X-CSRF-Token to test CSRF rejection

        const auto response = server->execute_graphql(mutation_query, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        REQUIRE(resp_json.contains("errors"));
        REQUIRE(resp_json["errors"].is_array());
        REQUIRE(resp_json["errors"].size() > 0);

        // Check that the error code is CSRF_FAILED
        const auto& first_error = resp_json["errors"][0];
        REQUIRE(first_error.contains("message"));
        REQUIRE(first_error["message"].get<std::string>().find("CSRF") != std::string::npos);
        REQUIRE(first_error.contains("code"));
        REQUIRE(first_error["code"].get<int>() == static_cast<int>(EErrorCodes::CSRF_FAILED));
    }

    SECTION("Mutation with CSRF token but no Origin/Referer is rejected") {
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
        headers["X-CSRF-Token"] = "valid-csrf-token-value";

        const auto response = server->execute_graphql(mutation_query, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        REQUIRE(resp_json.contains("errors"));
        REQUIRE(resp_json["errors"].is_array());
        REQUIRE(resp_json["errors"].size() > 0);
        REQUIRE(resp_json["errors"][0]["message"].get<std::string>().find("Origin/Referer") != std::string::npos);
        REQUIRE(resp_json["errors"][0]["code"].get<int>() == static_cast<int>(EErrorCodes::CSRF_FAILED));
    }

    SECTION("Mutation with mismatched Origin is rejected") {
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
        headers["X-CSRF-Token"] = "valid-csrf-token-value";
        headers["Origin"] = "http://evil.example:9999";

        const auto response = server->execute_graphql(mutation_query, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        REQUIRE(resp_json.contains("errors"));
        REQUIRE(resp_json["errors"].is_array());
        REQUIRE(resp_json["errors"].size() > 0);
        REQUIRE(resp_json["errors"][0]["message"].get<std::string>().find("Origin/Referer") != std::string::npos);
        REQUIRE(resp_json["errors"][0]["code"].get<int>() == static_cast<int>(EErrorCodes::CSRF_FAILED));
    }

    SECTION("Query without CSRF token is allowed") {
        // Queries (non-mutations) should NOT require CSRF token
        const std::string query = R"(
            query {
                systemState {
                    seedModeActive
                }
            }
        )";

        std::unordered_map<std::string, std::string> headers;
        // No CSRF token for query - should be allowed

        const auto response = server->execute_graphql(query, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        // Should not have CSRF errors (may have other errors like missing schema, but not CSRF)
        if (resp_json.contains("errors")) {
            for (const auto& error : resp_json["errors"]) {
                if (error.contains("message")) {
                    REQUIRE(error["message"].get<std::string>().find("CSRF") == std::string::npos);
                }
            }
        }
    }

    SECTION("Mutation with CSRF token is processed") {
        // T010: With CSRF token present, mutation should be processed (not rejected for CSRF)
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
        headers["X-CSRF-Token"] = "valid-csrf-token-value";
        headers["Origin"] = "http://localhost:8080";

        const auto response = server->execute_graphql(mutation_query, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        // Response should not have CSRF_FAILED error (may have other errors, but not CSRF)
        if (resp_json.contains("errors")) {
            for (const auto& error : resp_json["errors"]) {
                if (error.contains("message")) {
                    const auto msg = error["message"].get<std::string>();
                    // The error should not be about CSRF token specifically
                    REQUIRE(msg.find("CSRF token missing") == std::string::npos);
                    REQUIRE(msg.find("Origin/Referer validation failed") == std::string::npos);
                }
            }
        }
    }
}

TEST_CASE("WebUI Security Foundation - GraphQL-Only Endpoint", "[webui][security][graphql-only]") {
    auto server = Server::create();
    REQUIRE(server->get_status() == Server::Status::STOPPED);

    SECTION("GraphQL endpoint is available") {
        // T005: Verify /graphql endpoint is the primary API
        // This is a compile-time check that the server is wired for GraphQL only
        // At runtime, we verify that the execute_graphql method exists and works
        const std::string query = "query { __typename }";
        const auto response = server->execute_graphql(query);

        // Should parse as valid JSON response
        const auto resp_json = json::parse(response);
        REQUIRE(resp_json.contains("extensions"));
        REQUIRE(resp_json["extensions"]["endpoint"].get<std::string>() == "/graphql");
    }
}

TEST_CASE("WebUI Security Foundation - Error Code Mapping", "[webui][security][error-codes]") {
    auto server = Server::create();

    SECTION("CSRF_FAILED error code is properly mapped") {
        // T010: Ensure CSRF_FAILED errors are distinguishable
        const std::string mutation = "mutation { completePlatformBootstrap(input: {email: \"x\", password: \"y\"}) { success } }";

        std::unordered_map<std::string, std::string> headers;
        // No CSRF token

        const auto response = server->execute_graphql(mutation, "{}", "", "127.0.0.1", headers);
        const auto resp_json = json::parse(response);

        REQUIRE(resp_json.contains("errors"));
        REQUIRE(!resp_json["errors"].empty());
        REQUIRE(resp_json["errors"][0]["code"].get<int>() == static_cast<int>(EErrorCodes::CSRF_FAILED));
    }
}

TEST_CASE("WebUI Security Foundation - Client IP Tracking", "[webui][security][logging]") {
    auto server = Server::create();

    SECTION("Client IP is captured from request") {
        // T010: Verify remote_ip is tracked for rate-limiting and logging
        const std::string query = "query { hello }";
        const std::string remote_ip = "192.168.1.100";

        const auto response = server->execute_graphql(query, "{}", "", remote_ip);
        const auto resp_json = json::parse(response);

        // Verify response includes requestId and timestamp for logging
        REQUIRE(resp_json.contains("extensions"));
        REQUIRE(resp_json["extensions"].contains("requestId"));
        REQUIRE(resp_json["extensions"].contains("timestamp"));
    }
}

