// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_webui_bootstrap_contract.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief GraphQL contract tests for bootstrap status/completion operations.
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

#include "../isched/isched_graphql_test_helpers.hpp"

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

namespace {
std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>> make_executor_with_auth() {
    auto db = std::make_shared<DatabaseManager>();
    REQUIRE(db->ensure_system_db());

    const auto admins = db->list_platform_admins();
    REQUIRE(admins);
    for (const auto& admin : admins.value()) {
        REQUIRE(db->delete_platform_admin(admin.id));
    }

    auto exec = std::make_shared<GqlExecutor>(db);
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-bootstrap-contract-jwt-secret-at-least-32-bytes!");
    exec->set_auth_middleware(auth);
    return {db, exec};
}
} // namespace

TEST_CASE("BootstrapStatus reports bootstrap availability on clean system", "[integration][contract][bootstrap][T019]") {
    auto [db, exec] = make_executor_with_auth();
    (void)db;

    const auto status = exec->execute(
        R"(query { platformBootstrapStatus { isBootstrapAllowed bootstrapState } })",
        "{}",
        isched::test::anonymous_ctx());

    REQUIRE(status.is_success());
    REQUIRE(status.data.contains("platformBootstrapStatus"));
    REQUIRE(status.data["platformBootstrapStatus"]["isBootstrapAllowed"].get<bool>());
    REQUIRE(status.data["platformBootstrapStatus"]["bootstrapState"].get<std::string>() == "PendingInitialization");
}

TEST_CASE("CompleteBootstrap initializes platform and flips status", "[integration][contract][bootstrap][T019]") {
    auto [db, exec] = make_executor_with_auth();

    const std::string email = "contract_bootstrap_" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count()) + "@example.com";

    const auto complete = exec->execute(
        R"(mutation($input: BootstrapPlatformAdminInput!) {
             completePlatformBootstrap(input: $input) {
               success
               bootstrapState
               requiresRedirectToLogin
             }
           })",
        json{{"input", {
            {"email", email},
            {"password", "BootstrapContract123!"},
            {"displayName", "Contract Admin"}
        }}}.dump(),
        isched::test::anonymous_ctx());

    REQUIRE(complete.is_success());
    REQUIRE(complete.data["completePlatformBootstrap"]["success"].get<bool>());
    REQUIRE(complete.data["completePlatformBootstrap"]["bootstrapState"].get<std::string>() == "Initialized");

    const auto admins = db->list_platform_admins();
    REQUIRE(admins);
    REQUIRE(admins.value().size() == 1);

    const auto status_after = exec->execute(
        R"(query { platformBootstrapStatus { isBootstrapAllowed bootstrapState } })",
        "{}",
        isched::test::anonymous_ctx());

    REQUIRE(status_after.is_success());
    REQUIRE_FALSE(status_after.data["platformBootstrapStatus"]["isBootstrapAllowed"].get<bool>());
    REQUIRE(status_after.data["platformBootstrapStatus"]["bootstrapState"].get<std::string>() == "Initialized");
}

