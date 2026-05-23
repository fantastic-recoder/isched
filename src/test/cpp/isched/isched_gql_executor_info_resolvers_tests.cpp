// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_info_resolvers_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for built-in Info & Metrics GraphQL resolvers
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

TEST_CASE("Server Info & Metrics Resolvers", "[gql][resolvers][info]") {
    auto db = std::make_shared<DatabaseManager>();
    db->ensure_system_db();
    GqlExecutor proc(db);

    SECTION("Basic info queries") {
        auto res = exec_full(proc, "{ version serverInfo { version name } uptime clientCount systemState health info { version } env configprops }");
        WARN_IF_ERROR(res);
        REQUIRE_FALSE(res.data.is_null());
        REQUIRE(res.data.contains("version"));
    }
    
    SECTION("Metrics queries require auth") {
        auto res = exec_full(proc, "{ metrics { activeSubscriptions } serverMetrics { cpuUsage } tenantMetrics(tenantId: \"t1\") { cpuUsage } }");
        REQUIRE_FALSE(res.is_success());
        
        ResolverCtx admin_ctx;
        admin_ctx.roles = {"role_platform_admin"};
        admin_ctx.db = db;
        auto res2 = proc.execute("{ metrics { activeSubscriptions } serverMetrics { cpuUsage } tenantMetrics(tenantId: \"t1\") { cpuUsage } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);
    }
}
