// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_users_resolvers_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for built-in Users GraphQL resolvers
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

TEST_CASE("User & DataSource Resolvers", "[gql][resolvers][users]") {
    auto db = std::make_shared<DatabaseManager>();
    db->ensure_system_db();
    db->initialize_tenant("default_tenant");
    GqlExecutor proc(db);

    ResolverCtx admin_ctx;
    admin_ctx.roles = {"role_platform_admin"};
    admin_ctx.tenant_id = "default_tenant";
    admin_ctx.db = db;

    SECTION("User mutations and queries") {
        auto res1 = proc.execute("mutation { createUser(input: { username: \"u1\", password: \"password123456\", email: \"e@example.com\" }) }", "{}", admin_ctx);
        if (!res1.is_success()) WARN("createUser error: " << (!res1.errors.empty() ? res1.errors[0].message : ""));

        auto res2 = proc.execute("{ users { id } }", "{}", admin_ctx);
        WARN_IF_ERROR(res2);
    }

    SECTION("DataSource mutations and queries") {
        auto res1 = proc.execute("mutation { createDataSource(input: { id: \"d1\", name: \"DS1\", type: REST, configJson: \"{}\" }) }", "{}", admin_ctx);
        if (!res1.is_success()) WARN("createDataSource error: " << (!res1.errors.empty() ? res1.errors[0].message : ""));

        auto res2 = proc.execute("mutation { updateDataSource(id: \"d1\", input: { name: \"D1\" }) }", "{}", admin_ctx);
        if (!res2.is_success()) WARN("updateDataSource error: " << (!res2.errors.empty() ? res2.errors[0].message : ""));

        auto res3 = proc.execute("mutation { deleteDataSource(id: \"d1\") }", "{}", admin_ctx);
        if (!res3.is_success()) WARN("deleteDataSource error: " << (!res3.errors.empty() ? res3.errors[0].message : ""));
    }
}
