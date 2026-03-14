// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_auth_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Test suite for Authentication Middleware
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * Tests for the Universal Application Server Backend authentication system,
 * following TDD approach as required by Constitutional compliance.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <memory>
#include <string>
#include <chrono>

#include "isched/backend/isched_AuthenticationMiddleware.hpp"

using namespace isched::v0_0_1::backend;

TEST_CASE("Authentication Middleware Basic Functionality", "[auth]") {

}

TEST_CASE("JWT generate and validate round-trip", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    auth->configure_jwt_secret("test-secret-key-at-least-32-bytes!");

    const std::string user_id = "alice";
    const std::string tenant_id = "tenant-1";
    const std::vector<std::string> perms = {"read", "write"};

    const std::string token = auth->generate_jwt_token(user_id, tenant_id, perms, 60);
    REQUIRE_FALSE(token.empty());

    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer " + token}};
    auto result = auth->validate_request(headers, tenant_id);

    REQUIRE(result.is_authenticated);
    REQUIRE(result.user_id == user_id);
    REQUIRE(result.tenant_id == tenant_id);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("JWT validation fails with wrong secret", "[auth][jwt]") {
    auto auth1 = AuthenticationMiddleware::create();
    auto auth2 = AuthenticationMiddleware::create();
    auth1->configure_jwt_secret("secret-one-at-least-32-bytes-long!");
    auth2->configure_jwt_secret("secret-two-at-least-32-bytes-long!");

    const std::string token = auth1->generate_jwt_token("bob", "tenant-1", {"read"}, 60);
    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer " + token}};
    auto result = auth2->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("JWT validation fails on missing Authorization header", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    auth->configure_jwt_secret("test-secret-key-at-least-32-bytes!");

    std::unordered_map<std::string, std::string> headers{};
    auto result = auth->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("JWT validation fails when secret not configured", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    // No secret configured

    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer sometoken"}};
    auto result = auth->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("generate_jwt_token throws when secret not configured", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    REQUIRE_THROWS_AS(auth->generate_jwt_token("user", "tenant", {"read"}, 60), std::runtime_error);
}

TEST_CASE("Session create and get", "[auth][session]") {
    auto auth = AuthenticationMiddleware::create();
    auto session = auth->create_session("user1", "tenant-1", {"read"});

    REQUIRE_FALSE(session.session_id.empty());
    REQUIRE(session.user_id == "user1");
    REQUIRE(session.tenant_id == "tenant-1");

    auto retrieved = auth->get_session(session.session_id);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->user_id == "user1");
}

TEST_CASE("Session invalidation removes session", "[auth][session]") {
    auto auth = AuthenticationMiddleware::create();
    auto session = auth->create_session("user2", "tenant-1", {});

    auth->invalidate_session(session.session_id);
    auto retrieved = auth->get_session(session.session_id);
    REQUIRE_FALSE(retrieved.has_value());
}

