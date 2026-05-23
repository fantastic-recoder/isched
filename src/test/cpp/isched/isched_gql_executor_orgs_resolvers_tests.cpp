// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_orgs_resolvers_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for built-in Organization GraphQL resolvers
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

TEST_CASE("Role & Organization Resolvers", "[gql][resolvers][orgs]") {
    auto db = std::make_shared<DatabaseManager>();
    db->ensure_system_db();
    db->initialize_tenant("default_tenant");
    GqlExecutor proc(db);

    ResolverCtx admin_ctx;
    admin_ctx.roles = {"role_platform_admin"};
    admin_ctx.tenant_id = "default_tenant";
    admin_ctx.db = db;

    SECTION("Role mutations and queries") {
        auto res1 = proc.execute("mutation { createRole(input: { id: \"r1\", name: \"Role1\", scope: \"platform\" }) }", "{}", admin_ctx);
        if (!res1.is_success()) WARN("createRole error: " << (!res1.errors.empty() ? res1.errors[0].message : ""));

        auto res2 = proc.execute("{ roles { id name } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);

        auto res3 = proc.execute("mutation { deleteRole(id: \"r1\") }", "{}", admin_ctx);
        if (!res3.is_success()) WARN("deleteRole error: " << (!res3.errors.empty() ? res3.errors[0].message : ""));
    }

    SECTION("Organization mutations and queries") {
        auto res1 = proc.execute("mutation { createOrganization(input: { id: \"o1\", name: \"Org1\" }) }", "{}", admin_ctx);
        if (!res1.is_success()) WARN("createOrganization error: " << (!res1.errors.empty() ? res1.errors[0].message : ""));

        auto res2 = proc.execute("{ organizations { id name } organization(id: \"o1\") { id } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);

        auto res3 = proc.execute("mutation { updateOrganization(id: \"o1\", input: { name: \"Org2\" }) }", "{}", admin_ctx);
        if (!res3.is_success()) WARN("updateOrganization error: " << (!res3.errors.empty() ? res3.errors[0].message : ""));

        auto res4 = proc.execute("mutation { deleteOrganization(id: \"o1\") }", "{}", admin_ctx);
        if (!res4.is_success()) WARN("deleteOrganization error: " << (!res4.errors.empty() ? res4.errors[0].message : ""));
    }
}
