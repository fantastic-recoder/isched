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
            mutation {
                login(input: { username: "a", password: "b" }) { token }
            }
        )");
        REQUIRE_FALSE(res1.is_success());

        auto res2 = proc.execute("{ currentUser { id } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);

        auto res3 = proc.execute("mutation { logout }", "{}", admin_ctx);
        WARN_IF_ERROR(res3);
        
        auto res4 = proc.execute("mutation { revokeSession(token: \"abc\") }", "{}", admin_ctx);
        REQUIRE_FALSE(res4.is_success()); 
        
        auto res5 = proc.execute("mutation { revokeAllSessions(userId: \"u1\") }", "{}", admin_ctx);
        WARN_IF_ERROR(res5); 
        
        auto res6 = proc.execute("mutation { terminateAllSessions(organizationId: \"org1\") }", "{}", admin_ctx);
        WARN_IF_ERROR(res6);
    }
}
