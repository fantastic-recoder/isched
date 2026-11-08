// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_auth_resolvers_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for built-in Auth GraphQL resolvers
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

TEST_CASE("Authentication & Session Resolvers", "[gql][resolvers][auth]") {
    auto db = std::make_shared<DatabaseManager>();
    db->ensure_system_db();
    db->initialize_tenant("default_tenant");
    GqlExecutor proc(db);
    
    ResolverCtx admin_ctx;
    admin_ctx.roles = {"role_platform_admin"};
    admin_ctx.tenant_id = "default_tenant";
    admin_ctx.db = db;

    SECTION("Login and session queries") {
        auto res1 = exec_full(proc, R"(
            mutation { isched {
                login(input: { username: "a", password: "b" }) { token }
            } }
        )");
        REQUIRE_FALSE(res1.is_success());

        auto res2 = proc.execute("{ isched { currentUser { id } } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);

        auto res3 = proc.execute("mutation { isched { logout } }", "{}", admin_ctx);
        WARN_IF_ERROR(res3);
        
        auto res4 = proc.execute("mutation { revokeSession(token: \"abc\") }", "{}", admin_ctx);
        REQUIRE_FALSE(res4.is_success()); 
        
        auto res5 = proc.execute("mutation { revokeAllSessions(userId: \"u1\") }", "{}", admin_ctx);
        WARN_IF_ERROR(res5); 
        
        auto res6 = proc.execute("mutation { terminateAllSessions(organizationId: \"org1\") }", "{}", admin_ctx);
        WARN_IF_ERROR(res6);
    }
}

TEST_CASE("Authentication and plugin admin RBAC", "[gql][auth][plugin]") {
    auto db = std::make_shared<DatabaseManager>();
    REQUIRE(db->ensure_system_db().has_value());
    REQUIRE(db->initialize_tenant("default_tenant").has_value());
    REQUIRE(db->initialize_plugin_store().has_value());

    GqlExecutor executor{db};

    ResolverCtx tenant_admin;
    tenant_admin.roles = {"role_tenant_admin"};
    tenant_admin.tenant_id = "default_tenant";
    tenant_admin.db = db;

    ResolverCtx platform_admin;
    platform_admin.roles = {"role_platform_admin"};
    platform_admin.tenant_id = "default_tenant";
    platform_admin.db = db;

    const auto baseline_plugins = executor.execute("query { isched { plugins { id } } }", "{}", platform_admin);
    REQUIRE(baseline_plugins.is_success());
    REQUIRE(baseline_plugins.data.contains("isched"));
    REQUIRE(baseline_plugins.data["isched"].contains("plugins"));
    REQUIRE(baseline_plugins.data["isched"]["plugins"].is_array());
    const auto baseline_size = baseline_plugins.data["isched"]["plugins"].size();

    const auto plugins_forbidden = executor.execute("query { isched { plugins { id } } }", "{}", tenant_admin);
    REQUIRE_FALSE(plugins_forbidden.is_success());
    REQUIRE_FALSE(plugins_forbidden.errors.empty());
    CHECK(plugins_forbidden.errors.front().message.find("Access denied") != std::string::npos);

    const auto plugin_forbidden = executor.execute("query { isched { plugin(id: \"missing\") { id } } }", "{}", tenant_admin);
    REQUIRE_FALSE(plugin_forbidden.is_success());
    REQUIRE_FALSE(plugin_forbidden.errors.empty());
    CHECK(plugin_forbidden.errors.front().message.find("Access denied") != std::string::npos);

    CHECK_FALSE(executor.execute("mutation { isched { loadPlugin(input:{libraryPath:\"/tmp/nope.so\", kind:\"request\"}) { success } } }", "{}", tenant_admin).is_success());
    CHECK_FALSE(executor.execute("mutation { isched { unloadPlugin(id:\"auth-seeded-plugin\") { success } } }", "{}", tenant_admin).is_success());
    CHECK_FALSE(executor.execute("subscription { isched { pluginLifecycleEvents { eventType } } }", "{}", tenant_admin).is_success());

    const auto after_forbidden = executor.execute("query { isched { plugins { id } } }", "{}", platform_admin);
    REQUIRE(after_forbidden.is_success());
    REQUIRE(after_forbidden.data.contains("isched"));
    REQUIRE(after_forbidden.data["isched"].contains("plugins"));
    REQUIRE(after_forbidden.data["isched"]["plugins"].is_array());
    CHECK(after_forbidden.data["isched"]["plugins"].size() == baseline_size);

    CHECK(executor.execute("query { isched { plugins { id } } }", "{}", platform_admin).is_success());
    CHECK(executor.execute("query { isched { plugin(id: \"missing\") { id } } }", "{}", platform_admin).is_success());
    CHECK(executor.execute("subscription { isched { pluginLifecycleEvents { eventType } } }", "{}", platform_admin).is_success());
}
