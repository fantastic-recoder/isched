// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_bootstrap_platform_admin.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration coverage for one-time platform-admin bootstrap (T047-018a)
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

static ResolverCtx anonymous_ctx() {
    ResolverCtx ctx;
    ctx.current_user_id = "";
    ctx.roles = {};
    return ctx;
}

static void require_success(const ExecutionResult& result, const std::string& op) {
    if (!result.is_success()) {
        std::string msg = op + " failed";
        if (!result.errors.empty()) {
            msg += ": ";
            msg += result.errors.front().message;
        }
        FAIL(msg);
    }
}

static void reset_platform_admins(DatabaseManager& db) {
    auto list_res = db.list_platform_admins();
    REQUIRE(list_res);
    for (const auto& admin : list_res.value()) {
        auto del_res = db.delete_platform_admin(admin.id);
        REQUIRE(del_res);
    }
}

static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_executor_with_auth() {
    auto db = std::make_shared<DatabaseManager>();
    REQUIRE(db->ensure_system_db());
    reset_platform_admins(*db);

    auto exec = std::make_shared<GqlExecutor>(db);
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-bootstrap-jwt-secret-at-least-32-bytes!");
    exec->set_auth_middleware(auth);
    return {db, exec};
}

TEST_CASE("bootstrapPlatformAdmin succeeds exactly once and persists first platform admin",
          "[integration][auth][bootstrap][T047-018a]") {
    auto [db, exec] = make_executor_with_auth();

    const std::string email = "bootstrap_" + g_run_suffix + "@example.com";
    const std::string password = "Boot$trap123";

    const auto first_vars = json{{"input", {
        {"email", email},
        {"password", password},
        {"displayName", "Bootstrap Admin"}
    }}};

    auto first = exec->execute(
        R"(mutation($input: BootstrapPlatformAdminInput!) {
             bootstrapPlatformAdmin(input: $input) { token expiresAt }
           })",
        first_vars.dump(),
        anonymous_ctx());

    require_success(first, "bootstrapPlatformAdmin first call");
    REQUIRE(first.data.contains("bootstrapPlatformAdmin"));
    REQUIRE(!first.data["bootstrapPlatformAdmin"]["token"].get<std::string>().empty());
    REQUIRE(!first.data["bootstrapPlatformAdmin"]["expiresAt"].get<std::string>().empty());

    auto admins_after_first = db->list_platform_admins();
    REQUIRE(admins_after_first);
    REQUIRE(admins_after_first.value().size() == 1);
    REQUIRE(admins_after_first.value().front().email == email);

    // Realistic follow-up flow: the persisted platform admin can immediately log in.
    auto login = exec->execute(
        R"(mutation($email: String!, $password: String!) {
             login(email: $email, password: $password) { token expiresAt }
           })",
        json{{"email", email}, {"password", password}}.dump(),
        anonymous_ctx());

    require_success(login, "platform login after bootstrap");
    REQUIRE(!login.data["login"]["token"].get<std::string>().empty());
}

TEST_CASE("bootstrapPlatformAdmin rejects subsequent unauthenticated attempts without creating extra admins",
          "[integration][auth][bootstrap][T047-018a]") {
    auto [db, exec] = make_executor_with_auth();

    const std::string first_email = "bootstrap_first_" + g_run_suffix + "@example.com";
    const std::string second_email = "bootstrap_second_" + g_run_suffix + "@example.com";

    auto first = exec->execute(
        R"(mutation($input: BootstrapPlatformAdminInput!) {
             bootstrapPlatformAdmin(input: $input) { token expiresAt }
           })",
        json{{"input", {{"email", first_email}, {"password", "FirstPass!123"}, {"displayName", "First"}}}}.dump(),
        anonymous_ctx());
    require_success(first, "bootstrapPlatformAdmin initial provisioning");

    auto second = exec->execute(
        R"(mutation($input: BootstrapPlatformAdminInput!) {
             bootstrapPlatformAdmin(input: $input) { token expiresAt }
           })",
        json{{"input", {{"email", second_email}, {"password", "SecondPass!123"}, {"displayName", "Second"}}}}.dump(),
        anonymous_ctx());

    REQUIRE_FALSE(second.is_success());
    REQUIRE_FALSE(second.errors.empty());

    auto admins_after_second = db->list_platform_admins();
    REQUIRE(admins_after_second);
    REQUIRE(admins_after_second.value().size() == 1);
    REQUIRE(admins_after_second.value().front().email == first_email);
}

