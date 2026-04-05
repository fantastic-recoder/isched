// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_rate_limiting.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for rate limiting enforcement (Q2 - RATE_LIMITED gap)
 *
 * Tests RATE_LIMITED error responses for authentication mutations when an identity
 * exceeds the failed-attempt threshold within the rate-limiting window.
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>

#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

#include "../isched/isched_graphql_test_helpers.hpp"

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

namespace {
struct ExecutorWithAuth {
    std::shared_ptr<GqlExecutor> exec;
    std::shared_ptr<AuthenticationMiddleware> auth;
};

class ScopedEnvVar {
public:
    ScopedEnvVar(const std::string& key, const std::string& value) : key_(key) {
        const char* existing = std::getenv(key.c_str());
        if (existing != nullptr) {
            had_previous_ = true;
            previous_value_ = existing;
        }

        if (value.empty()) {
            unsetenv(key_.c_str());
        } else {
            setenv(key_.c_str(), value.c_str(), 1);
        }
    }

    ~ScopedEnvVar() {
        if (had_previous_) {
            setenv(key_.c_str(), previous_value_.c_str(), 1);
        } else {
            unsetenv(key_.c_str());
        }
    }

private:
    std::string key_;
    bool had_previous_{false};
    std::string previous_value_;
};

ExecutorWithAuth make_executor_with_auth(const std::shared_ptr<DatabaseManager>& db) {
    auto exec = std::make_shared<GqlExecutor>(db);
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-rate-limit-jwt-secret-at-least-32-bytes");
    exec->set_auth_middleware(auth);
    return ExecutorWithAuth{exec, auth};
}

void seed_platform_admin(DatabaseManager& db, const std::string& email, const std::string& password) {
    REQUIRE(db.ensure_system_db());

    const auto existing = db.list_platform_admins();
    REQUIRE(existing);
    for (const auto& admin : existing.value()) {
        REQUIRE(db.delete_platform_admin(admin.id));
    }

    const auto create_result = db.create_platform_admin(
        "rate_limit_test_admin",
        email,
        hash_password(password),
        "Rate Limit Test Admin");
    REQUIRE(create_result);
}

ExecutionResult login_attempt(
    const std::shared_ptr<GqlExecutor>& exec,
    const std::string& email,
    const std::string& password)
{
    return exec->execute(
        R"(mutation($email: String!, $password: String!) {
             login(email: $email, password: $password) { token expiresAt }
           })",
        json{{"email", email}, {"password", password}}.dump(),
        isched::test::anonymous_ctx());
}
} // namespace

TEST_CASE("Rate Limiting: Record and Check Failed Attempts", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    auto auth = AuthenticationMiddleware::create();

    SECTION("First failed attempt does not trigger rate limit") {
        auth->record_failed_attempt("user@example.com", 900000, 5);
        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }

    SECTION("Attempts below max_attempts do not trigger rate limit") {
        for (int i = 0; i < 4; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
            REQUIRE(!auth->is_rate_limited("user@example.com"));
        }
    }

    SECTION("Reaching max_attempts triggers rate limit") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));
    }

    SECTION("Exceeding max_attempts keeps rate limit active") {
        for (int i = 0; i < 7; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));
    }
}

TEST_CASE("Rate Limiting: Reset Failed Attempts", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    auto auth = AuthenticationMiddleware::create();

    SECTION("Reset removes rate limit") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        auth->reset_failed_attempts("user@example.com");
        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }

    SECTION("After reset, counter restarts from scratch") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
        }
        auth->reset_failed_attempts("user@example.com");

        auth->record_failed_attempt("user@example.com", 900000, 5);
        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }
}

TEST_CASE("Rate Limiting: Get Reset Milliseconds", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    auto auth = AuthenticationMiddleware::create();

    SECTION("Non-rate-limited identity returns 0") {
        REQUIRE(auth->get_rate_limit_reset_ms("user@example.com") == 0);
    }

    SECTION("Rate-limited identity returns positive milliseconds") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 5);
        }
        int reset_ms = auth->get_rate_limit_reset_ms("user@example.com");
        REQUIRE(reset_ms > 0);
        REQUIRE(reset_ms <= 900000);  // Within window
    }
}

TEST_CASE("Rate Limiting: Window Expiration", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "100"};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "5"};
    auto auth = AuthenticationMiddleware::create();

    SECTION("Rate limit expires after window duration") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 100, 5);  // 100ms window
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        // Wait for window to expire
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (auth->is_rate_limited("user@example.com") && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }

    SECTION("Expired window resets counter on next attempt") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 100, 5);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (auth->is_rate_limited("user@example.com") && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Record a new attempt with the same short window - it should start fresh.
        auth->record_failed_attempt("user@example.com", 100, 5);
        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }
}

TEST_CASE("Rate Limiting: Per-Identity Isolation", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    auto auth = AuthenticationMiddleware::create();

    SECTION("Rate limiting one identity does not affect others") {
        // Rate limit user1
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user1@example.com", 900000, 5);
        }

        // user2 should not be affected
        auth->record_failed_attempt("user2@example.com", 900000, 5);

        REQUIRE(auth->is_rate_limited("user1@example.com"));
        REQUIRE(!auth->is_rate_limited("user2@example.com"));
    }
}

TEST_CASE("Rate Limiting: Different Max Attempts", "[rate-limiting]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    auto auth = AuthenticationMiddleware::create();

    SECTION("Max attempts of 3") {
        for (int i = 0; i < 3; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 3);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));
    }

    SECTION("Max attempts of 10") {
        for (int i = 0; i < 10; ++i) {
            auth->record_failed_attempt("user@example.com", 900000, 10);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        // But 9 attempts should not trigger it
        auto auth2 = AuthenticationMiddleware::create();
        for (int i = 0; i < 9; ++i) {
            auth2->record_failed_attempt("user2@example.com", 900000, 10);
        }
        REQUIRE(!auth2->is_rate_limited("user2@example.com"));
    }
}

TEST_CASE("Rate Limiting: login resolver returns deterministic RATE_LIMITED envelope",
          "[rate-limiting][integration][graphql]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "5"};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "rate-limit-envelope@example.com", "CorrectHorseBatteryStaple123");
    auto exec = make_executor_with_auth(db).exec;

    for (int i = 0; i < 4; ++i) {
        const auto attempt = login_attempt(exec, "rate-limit-envelope@example.com", "wrong-password");
        REQUIRE_FALSE(attempt.is_success());
        REQUIRE_FALSE(isched::test::has_error_code(attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));
    }

    const auto lockout_attempt = login_attempt(exec, "rate-limit-envelope@example.com", "wrong-password");
    REQUIRE_FALSE(lockout_attempt.is_success());
    REQUIRE(isched::test::has_error_code(lockout_attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));

    const auto lockout_json = lockout_attempt.to_json();
    REQUIRE(lockout_json.contains("errors"));
    REQUIRE(lockout_json["errors"][0]["extensions"]["code"] == "RATE_LIMITED");
    REQUIRE(lockout_json["errors"][0]["extensions"].contains("retryAfterMs"));
    REQUIRE(lockout_json["errors"][0]["message"] ==
            "RATE_LIMITED: Too many authentication attempts. retryAfterMs="
            + std::to_string(lockout_json["errors"][0]["extensions"]["retryAfterMs"].get<int>()));

}


