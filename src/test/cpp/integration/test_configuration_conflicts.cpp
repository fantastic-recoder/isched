// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_configuration_conflicts.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for configuration optimistic-concurrency conflicts (T028a)
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

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

static std::shared_ptr<GqlExecutor> make_executor() {
    auto db = std::make_shared<DatabaseManager>();
    auto exec = std::make_shared<GqlExecutor>(db);
    return exec;
}

TEST_CASE("applyConfiguration rejects stale expectedVersion and preserves active snapshot",
          "[integration][config][conflict][T028a]") {
    auto exec = make_executor();
    const std::string tenant = "cfg-conflict-" + g_run_suffix;

    auto create_v1 = exec->execute(
        R"(mutation($input: ApplyConfigurationInput!) {
             applyConfiguration(input: $input) { success snapshotId errors }
           })",
        json{{"input", {
            {"tenantId", tenant},
            {"schemaSdl", "type V1Type { id: String! }"},
            {"version", "1.0.0"},
            {"displayName", "v1"}
        }}}.dump(),
        anonymous_ctx());

    require_success(create_v1, "applyConfiguration v1");
    REQUIRE(create_v1.data["applyConfiguration"]["success"] == true);
    const std::string snapshot_v1 = create_v1.data["applyConfiguration"]["snapshotId"].get<std::string>();
    REQUIRE_FALSE(snapshot_v1.empty());

    auto activate_v1 = exec->execute(
        R"(mutation($id: String!) {
             activateSnapshot(id: $id) { success snapshotId errors }
           })",
        json{{"id", snapshot_v1}}.dump(),
        anonymous_ctx());
    require_success(activate_v1, "activateSnapshot v1");
    REQUIRE(activate_v1.data["activateSnapshot"]["success"] == true);

    auto history_before = exec->execute(
        R"(query($tenantId: String!) {
             configurationHistory(tenantId: $tenantId) { id version isActive }
           })",
        json{{"tenantId", tenant}}.dump(),
        anonymous_ctx());
    require_success(history_before, "configurationHistory before conflict");
    const auto before_count = history_before.data["configurationHistory"].size();
    REQUIRE(before_count == 1);

    auto stale_attempt = exec->execute(
        R"(mutation($input: ApplyConfigurationInput!) {
             applyConfiguration(input: $input) { success snapshotId errors }
           })",
        json{{"input", {
            {"tenantId", tenant},
            {"schemaSdl", "type V2Type { id: String! name: String }"},
            {"version", "2.0.0"},
            {"displayName", "v2 stale"},
            {"expectedVersion", "0.9.0"}
        }}}.dump(),
        anonymous_ctx());

    REQUIRE(stale_attempt.is_success());
    REQUIRE(stale_attempt.data["applyConfiguration"]["success"] == false);
    REQUIRE(stale_attempt.data["applyConfiguration"]["snapshotId"].is_null());
    REQUIRE(stale_attempt.data["applyConfiguration"]["errors"].is_array());
    REQUIRE_FALSE(stale_attempt.data["applyConfiguration"]["errors"].empty());

    auto history_after = exec->execute(
        R"(query($tenantId: String!) {
             configurationHistory(tenantId: $tenantId) { id version isActive }
           })",
        json{{"tenantId", tenant}}.dump(),
        anonymous_ctx());
    require_success(history_after, "configurationHistory after conflict");
    REQUIRE(history_after.data["configurationHistory"].size() == before_count);

    auto active = exec->execute(
        R"(query($tenantId: String!) {
             activeConfiguration(tenantId: $tenantId) { id version isActive }
           })",
        json{{"tenantId", tenant}}.dump(),
        anonymous_ctx());
    require_success(active, "activeConfiguration after conflict");
    REQUIRE(active.data["activeConfiguration"]["id"] == snapshot_v1);
    REQUIRE(active.data["activeConfiguration"]["version"] == "1.0.0");
    REQUIRE(active.data["activeConfiguration"]["isActive"] == true);
}

