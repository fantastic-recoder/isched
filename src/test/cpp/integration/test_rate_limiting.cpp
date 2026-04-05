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

#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

TEST_CASE("Rate Limiting: Record and Check Failed Attempts", "[rate-limiting]") {
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
    auto auth = AuthenticationMiddleware::create();

    SECTION("Rate limit expires after window duration") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 100, 5);  // 100ms window
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        // Wait for window to expire
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }

    SECTION("Expired window resets counter on next attempt") {
        for (int i = 0; i < 5; ++i) {
            auth->record_failed_attempt("user@example.com", 100, 5);
        }
        REQUIRE(auth->is_rate_limited("user@example.com"));

        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Record a new attempt - should start fresh window
        auth->record_failed_attempt("user@example.com", 900000, 5);
        REQUIRE(!auth->is_rate_limited("user@example.com"));
    }
}

TEST_CASE("Rate Limiting: Per-Identity Isolation", "[rate-limiting]") {
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


