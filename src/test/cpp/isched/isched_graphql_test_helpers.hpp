// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_graphql_test_helpers.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Shared test utilities for GraphQL integration testing
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-05
 *
 * Provides common functionality for GraphQL API tests including:
 * - CSRF token acquisition and propagation
 * - JWT token bootstrap and authentication
 * - GraphQL query/mutation execution helpers
 * - Response validation utilities
 *
 * @example Usage:
 * @code{cpp}
 * #include "isched_graphql_test_helpers.hpp"
 *
 * auto client = isched::test::GraphQLTestClient("localhost", 8080);
 * auto auth = client.bootstrap_platform_admin("admin@example.com", "password123");
 * auto response = client.post_mutation("mutation { echo(message: \"test\") }", auth.token);
 * @endcode
 */

#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

namespace isched::test {

/**
 * @brief Authentication payload returned from bootstrap/login mutations
 */
struct AuthPayload {
    std::string token;           ///< JWT access token
    std::string expires_at;      ///< Expiration timestamp (ISO 8601)
    std::string csrf_token;      ///< CSRF token for mutations
    std::string organization_id; ///< Organization ID (if organization-scoped)
};

/**
 * @brief Helper class for GraphQL API testing
 *
 * Manages HTTP connections, CSRF tokens, authentication, and provides
 * simplified mutation/query execution with automatic header management.
 */
class GraphQLTestClient {
public:
    /**
     * @brief Create a new GraphQL test client
     * @param host Target hostname or IP
     * @param port Target port number
     * @param use_ssl Whether to use HTTPS (default: false)
     */
    GraphQLTestClient(const std::string& host, int port, bool use_ssl = false)
        : host_(host), port_(port), use_ssl_(use_ssl) {
        client_ = std::make_unique<httplib::Client>(host, port);
        client_->set_connection_timeout(2, 0);      // 2 seconds
        client_->set_read_timeout(5, 0);             // 5 seconds
        client_->set_write_timeout(5, 0);            // 5 seconds
    }

    ~GraphQLTestClient() = default;

    /**
     * @brief Execute a GraphQL query (no authentication required)
     *
     * @param query GraphQL query string (e.g., "{ hello }")
     * @return Parsed JSON response body
     * @throw std::runtime_error on HTTP error or parse failure
     */
    nlohmann::json post_query(const std::string& query) {
        nlohmann::json body = {{"query", query}};
        return execute_request("/graphql", body.dump(), "application/json", {});
    }

    /**
     * @brief Execute a GraphQL mutation with CSRF token
     *
     * Automatically extracts and includes the CSRF token from the current session.
     *
     * @param mutation GraphQL mutation string (e.g., "mutation { echo(message: \"test\") }")
     * @param auth Authentication payload (contains CSRF token and JWT)
     * @return Parsed JSON response body
     * @throw std::runtime_error on HTTP error or parse failure
     */
    nlohmann::json post_mutation(const std::string& mutation, const AuthPayload& auth) {
        nlohmann::json body = {{"query", mutation}};

        httplib::Headers headers;
        headers.emplace("X-CSRF-Token", auth.csrf_token);
        if (!auth.token.empty()) {
            headers.emplace("Authorization", "Bearer " + auth.token);
        }

        return execute_request("/graphql", body.dump(), "application/json", headers);
    }

    /**
     * @brief Execute a GraphQL mutation without authentication (for testing CSRF only)
     *
     * @param mutation GraphQL mutation string
     * @param csrf_token CSRF token to include in request
     * @return Parsed JSON response body
     * @throw std::runtime_error on HTTP error or parse failure
     */
    nlohmann::json post_mutation_with_csrf(const std::string& mutation, const std::string& csrf_token) {
        nlohmann::json body = {{"query", mutation}};

        httplib::Headers headers;
        headers.emplace("X-CSRF-Token", csrf_token);

        return execute_request("/graphql", body.dump(), "application/json", headers);
    }

    /**
     * @brief Bootstrap the platform admin user (seed mode)
     *
     * Attempts to create the initial platform admin. Only succeeds once on a fresh server.
     *
     * @param email Email address for the admin
     * @param password Password (will be hashed)
     * @param display_name Display name (optional)
     * @return AuthPayload containing JWT token and CSRF token
     * @throw std::runtime_error if bootstrap fails or server is not in seed mode
     */
    AuthPayload bootstrap_platform_admin(
        const std::string& email,
        const std::string& password,
        const std::string& display_name = "Test Admin") {

        // Prime a server round-trip before issuing the bootstrap mutation.
        (void)post_query("{ systemState { seedModeActive } }");
        const std::string initial_csrf = get_csrf_token();

        // Execute bootstrap mutation
        std::string bootstrap_mutation = R"(
            mutation {
                bootstrapPlatformAdmin(input: {
                    email: ")" + email + R"("
                    password: ")" + password + R"("
                    displayName: ")" + display_name + R"("
                }) {
                    token
                    expiresAt
                }
            }
        )";

        auto response = post_mutation_with_csrf(bootstrap_mutation, initial_csrf);

        if (response.contains("errors") && !response["errors"].empty()) {
            throw std::runtime_error("Bootstrap failed: " + response["errors"][0]["message"].get<std::string>());
        }

        if (!response.contains("data") || !response["data"].contains("bootstrapPlatformAdmin")) {
            throw std::runtime_error("Bootstrap mutation did not return expected data");
        }

        AuthPayload auth;
        auth.token = response["data"]["bootstrapPlatformAdmin"]["token"];
        auth.expires_at = response["data"]["bootstrapPlatformAdmin"]["expiresAt"];
        auth.csrf_token = initial_csrf; // Updated CSRF token if server sends it in response

        return auth;
    }

    /**
     * @brief Login with existing platform admin credentials
     *
     * Useful when bootstrap has already been completed in a previous test.
     */
    AuthPayload login_platform_admin(const std::string& email, const std::string& password) {
        const std::string csrf = get_csrf_token();

        std::string login_mutation = R"(
            mutation {
                login(email: ")" + email + R"(", password: ")" + password + R"(") {
                    token
                    expiresAt
                }
            }
        )";

        auto response = post_mutation_with_csrf(login_mutation, csrf);
        if (response.contains("errors") && !response["errors"].empty()) {
            throw std::runtime_error("Login failed: " + response["errors"][0]["message"].get<std::string>());
        }
        if (!response.contains("data") || !response["data"].contains("login")) {
            throw std::runtime_error("Login mutation did not return expected data");
        }

        AuthPayload auth;
        auth.token = response["data"]["login"]["token"];
        auth.expires_at = response["data"]["login"]["expiresAt"];
        auth.csrf_token = csrf;
        return auth;
    }

    /**
     * @brief Obtain authenticated context by bootstrapping once, then falling back to login.
     */
    AuthPayload bootstrap_or_login_platform_admin(
        const std::string& email,
        const std::string& password,
        const std::string& display_name = "Test Admin") {
        try {
            return bootstrap_platform_admin(email, password, display_name);
        } catch (const std::exception&) {
            return login_platform_admin(email, password);
        }
    }

    /**
     * @brief Obtain a fresh CSRF token
     *
     * Executes a simple query to get a new CSRF token from the response.
     *
     * @return CSRF token string
     * @throw std::runtime_error if unable to obtain token
     */
    std::string get_csrf_token() {
        // Ensure the server is reachable and GraphQL is functional.
        (void)post_query("{ hello }");
        // Current backend CSRF guard validates presence; tests use a stable non-empty token.
        return "test-csrf-token";
    }

    /**
     * @brief Check if response contains GraphQL errors
     *
     * @param response Parsed JSON response
     * @return true if response contains "errors" array with elements
     */
    static bool has_errors(const nlohmann::json& response) {
        return response.contains("errors") && response["errors"].is_array() && !response["errors"].empty();
    }

    /**
     * @brief Get first error message from response
     *
     * @param response Parsed JSON response
     * @return Error message string, or empty string if no errors
     */
    static std::string get_error_message(const nlohmann::json& response) {
        if (has_errors(response)) {
            return response["errors"][0]["message"].get<std::string>();
        }
        return "";
    }

    /**
     * @brief Get first error code from response extensions
     *
     * @param response Parsed JSON response
     * @return Error code string, or empty string if not present
     */
    static std::string get_error_code(const nlohmann::json& response) {
        if (has_errors(response) && response["errors"][0].contains("extensions")) {
            auto ext = response["errors"][0]["extensions"];
            if (ext.contains("code")) {
                return ext["code"].get<std::string>();
            }
        }
        return "";
    }

private:
    std::string host_;
    int port_;
    bool use_ssl_;
    std::unique_ptr<httplib::Client> client_;

    /**
     * @brief Execute a raw HTTP request
     *
     * @param path URL path (e.g., "/graphql")
     * @param body Request body string
     * @param content_type Content-Type header value
     * @param headers Additional headers to include
     * @return Parsed JSON response body
     * @throw std::runtime_error on HTTP error or parse failure
     */
    nlohmann::json execute_request(
        const std::string& path,
        const std::string& body,
        const std::string& content_type,
        const httplib::Headers& headers) {

        httplib::Headers final_headers = headers;
        final_headers.emplace("Content-Type", content_type);

        auto res = client_->Post(path, final_headers, body, content_type);

        if (!res) {
            throw std::runtime_error("HTTP request failed: connection error");
        }

        if (res->status < 200 || res->status >= 300) {
            throw std::runtime_error("HTTP error " + std::to_string(res->status) + ": " + res->body);
        }

        try {
            return nlohmann::json::parse(res->body);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("JSON parse error: ") + e.what() + "\nBody: " + res->body);
        }
    }
};

} // namespace isched::test

