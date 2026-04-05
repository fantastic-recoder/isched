// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_auth_lockout_policy.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration coverage for temporary authentication lockout policy.
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

#include "../isched/isched_graphql_test_helpers.hpp"

using namespace isched::v0_0_1::backend;

namespace {
class ScopedEnvVar {
public:
    ScopedEnvVar(const std::string& key, const std::string& value) : key_(key) {
        const char* existing = std::getenv(key.c_str());
        if (existing != nullptr) {
            had_previous_ = true;
            previous_value_ = existing;
        }
        setenv(key_.c_str(), value.c_str(), 1);
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

struct ExecutorWithAuth {
    std::shared_ptr<GqlExecutor> exec;
    std::shared_ptr<AuthenticationMiddleware> auth;
};

ExecutorWithAuth make_executor_with_auth(const std::shared_ptr<DatabaseManager>& db) {
    auto exec = std::make_shared<GqlExecutor>(db);
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-lockout-jwt-secret-at-least-32-bytes!");
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
        "lockout_test_admin",
        email,
        hash_password(password),
        "Lockout Test Admin");
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
        nlohmann::json{{"email", email}, {"password", password}}.dump(),
        isched::test::anonymous_ctx());
}

void require_lockout_on_nth_failed_attempt(const std::shared_ptr<GqlExecutor>& exec,
                                           const std::string& email,
                                           int nth_attempt) {
    REQUIRE(nth_attempt >= 1);
    for (int i = 1; i < nth_attempt; ++i) {
        const auto attempt = login_attempt(exec, email, "wrong-password");
        REQUIRE_FALSE(attempt.is_success());
        REQUIRE_FALSE(isched::test::has_error_code(attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));
    }

    const auto lockout_attempt = login_attempt(exec, email, "wrong-password");
    REQUIRE_FALSE(lockout_attempt.is_success());
    REQUIRE(isched::test::has_error_code(lockout_attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));
}
} // namespace

TEST_CASE("login applies temporary lockout after five failed attempts with deterministic RATE_LIMITED metadata",
          "[integration][auth][lockout][T018a]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "5"};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "lockout@example.com", "CorrectHorseBatteryStaple123");
    auto exec = make_executor_with_auth(db).exec;

    for (int i = 0; i < 4; ++i) {
        const auto attempt = login_attempt(exec, "lockout@example.com", "wrong-password");
        REQUIRE_FALSE(attempt.is_success());
        REQUIRE_FALSE(isched::test::has_error_code(attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));
    }

    const auto lockout_attempt = login_attempt(exec, "lockout@example.com", "wrong-password");
    REQUIRE_FALSE(lockout_attempt.is_success());
    REQUIRE(isched::test::has_error_code(lockout_attempt, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));

    const auto lockout_json = lockout_attempt.to_json();
    REQUIRE(lockout_json.contains("errors"));
    REQUIRE(lockout_json["errors"][0]["extensions"]["code"] == "RATE_LIMITED");
    REQUIRE(lockout_json["errors"][0]["extensions"].contains("retryAfterMs"));

    const auto blocked_valid_login = login_attempt(exec, "lockout@example.com", "CorrectHorseBatteryStaple123");
    REQUIRE_FALSE(blocked_valid_login.is_success());
    REQUIRE(isched::test::has_error_code(blocked_valid_login, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));
}

TEST_CASE("lockout auto-unlocks once the lock window elapses", "[integration][auth][lockout][T018a]") {
    ScopedEnvVar lockout_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar lockout_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "5"};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "unlock@example.com", "CorrectHorseBatteryStaple123");
    auto exec = make_executor_with_auth(db).exec;

    for (int i = 0; i < 5; ++i) {
        (void)login_attempt(exec, "unlock@example.com", "wrong-password");
    }

    const auto during_lockout = login_attempt(exec, "unlock@example.com", "CorrectHorseBatteryStaple123");
    REQUIRE_FALSE(during_lockout.is_success());
    REQUIRE(isched::test::has_error_code(during_lockout, isched::v0_0_1::gql::EErrorCodes::RATE_LIMITED));

    std::this_thread::sleep_for(std::chrono::milliseconds(5500));

    const auto after_unlock = login_attempt(exec, "unlock@example.com", "CorrectHorseBatteryStaple123");
    REQUIRE(after_unlock.is_success());
    REQUIRE(after_unlock.data.contains("login"));
    REQUIRE_FALSE(after_unlock.data["login"]["token"].get<std::string>().empty());
}

TEST_CASE("lockout config chain applies bootstrap/auth override before feature and global defaults",
          "[integration][auth][lockout][T071]") {
    ScopedEnvVar global_window{"ISCHED_RATE_LIMIT_WINDOW_MS", "5000"};
    ScopedEnvVar global_attempts{"ISCHED_RATE_LIMIT_MAX_ATTEMPTS", "2"};
    ScopedEnvVar feature_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar feature_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "4"};
    ScopedEnvVar override_window{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar override_attempts{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_MAX_ATTEMPTS", "6"};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "precedence-override@example.com", "CorrectHorseBatteryStaple123");
    auto bundle = make_executor_with_auth(db);

    const auto cfg = bundle.auth->get_rate_limit_config();
    REQUIRE(cfg.window_ms == 5000);
    REQUIRE(cfg.max_attempts == 6);
    REQUIRE(cfg.window_source == "ISCHED_BOOTSTRAP_AUTH_LOCKOUT_WINDOW_MS");
    REQUIRE(cfg.max_attempts_source == "ISCHED_BOOTSTRAP_AUTH_LOCKOUT_MAX_ATTEMPTS");

    require_lockout_on_nth_failed_attempt(bundle.exec, "precedence-override@example.com", 6);
}

TEST_CASE("lockout config chain uses feature defaults when bootstrap/auth-specific overrides are absent",
          "[integration][auth][lockout][T071]") {
    ScopedEnvVar global_window{"ISCHED_RATE_LIMIT_WINDOW_MS", "5000"};
    ScopedEnvVar global_attempts{"ISCHED_RATE_LIMIT_MAX_ATTEMPTS", "2"};
    ScopedEnvVar feature_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", "5000"};
    ScopedEnvVar feature_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", "4"};
    ScopedEnvVar override_window{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar override_attempts{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "precedence-feature@example.com", "CorrectHorseBatteryStaple123");
    auto bundle = make_executor_with_auth(db);

    const auto cfg = bundle.auth->get_rate_limit_config();
    REQUIRE(cfg.window_ms == 5000);
    REQUIRE(cfg.max_attempts == 4);
    REQUIRE(cfg.window_source == "ISCHED_AUTH_LOCKOUT_WINDOW_MS");
    REQUIRE(cfg.max_attempts_source == "ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS");

    require_lockout_on_nth_failed_attempt(bundle.exec, "precedence-feature@example.com", 4);
}

TEST_CASE("lockout config chain falls back to global defaults when no feature-level values are set",
          "[integration][auth][lockout][T071]") {
    ScopedEnvVar global_window{"ISCHED_RATE_LIMIT_WINDOW_MS", "5000"};
    ScopedEnvVar global_attempts{"ISCHED_RATE_LIMIT_MAX_ATTEMPTS", "3"};
    ScopedEnvVar feature_window{"ISCHED_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar feature_attempts{"ISCHED_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};
    ScopedEnvVar override_window{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_WINDOW_MS", ""};
    ScopedEnvVar override_attempts{"ISCHED_BOOTSTRAP_AUTH_LOCKOUT_MAX_ATTEMPTS", ""};

    auto db = std::make_shared<DatabaseManager>();
    seed_platform_admin(*db, "precedence-global@example.com", "CorrectHorseBatteryStaple123");
    auto bundle = make_executor_with_auth(db);

    const auto cfg = bundle.auth->get_rate_limit_config();
    REQUIRE(cfg.window_ms == 5000);
    REQUIRE(cfg.max_attempts == 3);
    REQUIRE(cfg.window_source == "ISCHED_RATE_LIMIT_WINDOW_MS");
    REQUIRE(cfg.max_attempts_source == "ISCHED_RATE_LIMIT_MAX_ATTEMPTS");

    require_lockout_on_nth_failed_attempt(bundle.exec, "precedence-global@example.com", 3);
}


