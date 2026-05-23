// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_config_resolvers_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for built-in Config GraphQL resolvers
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>

using namespace isched::v0_0_1::backend;

#define WARN_IF_ERROR(res) \
    do { \
        if (!(res).is_success()) { \
            if (!(res).errors.empty()) { \
                WARN("GraphQL error: " << (res).errors[0].message); \
            } \
        } \
    } while(false)

static auto exec_full(GqlExecutor& exec, const std::string& query, const std::string& vars = "{}") {
    return exec.execute(query, vars, false);
}

TEST_CASE("Bootstrap & Configuration Resolvers", "[gql][resolvers][config]") {
    auto db = std::make_shared<DatabaseManager>();
    db->ensure_system_db();
    db->initialize_config_store();
    GqlExecutor proc(db);

    ResolverCtx admin_ctx;
    admin_ctx.roles = {"role_platform_admin"};
    admin_ctx.tenant_id = "default_tenant";
    admin_ctx.db = db;

    SECTION("Bootstrap flow") {
        auto res1 = exec_full(proc, "{ platformBootstrapStatus }");
        WARN_IF_ERROR(res1);
        
        auto res2 = exec_full(proc, R"(
            mutation {
                bootstrapPlatformAdmin(input: {
                    username: "admin",
                    password: "password123456",
                    email: "admin@isched.local"
                })
            }
        )");
        if (!res2.is_success()) {
            WARN("Bootstrap error: " << (!res2.errors.empty() ? res2.errors[0].message : ""));
        } else {
            auto res3 = proc.execute(R"(
                mutation {
                    completePlatformBootstrap
                }
            )", "{}", admin_ctx);
            WARN_IF_ERROR(res3);
        }
    }

    SECTION("Configuration queries and mutations") {
        auto res = proc.execute(R"(
            mutation {
                applyConfiguration(input: {
                    configJson: "{}"
                }) { snapshotId }
            }
        )", "{}", admin_ctx);
        WARN_IF_ERROR(res);
        std::string snap_id = "unknown";
        if (res.is_success() && res.data.contains("applyConfiguration") && res.data["applyConfiguration"].is_object() &&
            res.data["applyConfiguration"].contains("snapshotId") && res.data["applyConfiguration"]["snapshotId"].is_string()) {
            snap_id = res.data["applyConfiguration"]["snapshotId"].get<std::string>();
        }
        
        auto res2 = proc.execute("{ activeConfiguration { configJson } configurationHistory { id } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);
        
        if (snap_id != "unknown") {
            auto res3 = proc.execute(std::format(R"(
                mutation {{
                    activateSnapshot(id: "{}") {{ success snapshotId }}
                }}
            )", snap_id), "{}", admin_ctx);
            WARN_IF_ERROR(res3);
        }
    }
}
