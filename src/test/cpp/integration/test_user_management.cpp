// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_user_management.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for User and Organization management over GraphQL (T047-018)
 *
 * Tests full CRUD for User and Organization through the GqlExecutor with realistic
 * authenticated ResolverCtx values; verifies RBAC rejects unauthorized callers.
 *
 * Note: login-mutation tests (login returns valid JWT) are deferred to when T047-016
 * is implemented (depends on T049-001 / T049-002).
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_gql_error.hpp>

using namespace isched::v0_0_1::backend;
using isched::v0_0_1::gql::EErrorCodes;
using json = nlohmann::json;

/// Per-process unique suffix so org/user names don't clash between test runs.
static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

// ---------------------------------------------------------------------------
// Helper: build an authenticated ResolverCtx for a platform admin
// ---------------------------------------------------------------------------
static ResolverCtx platform_admin_ctx(const std::string& tenant_id = "org_test") {
    ResolverCtx ctx;
    ctx.tenant_id       = tenant_id;
    ctx.current_user_id = "admin_001";
    ctx.user_name       = "Admin User";
    ctx.roles           = {std::string(Role::PLATFORM_ADMIN)};
    return ctx;
}

// ---------------------------------------------------------------------------
// Helper: build an authenticated ResolverCtx for a tenant admin
// ---------------------------------------------------------------------------
static ResolverCtx tenant_admin_ctx(const std::string& tenant_id = "org_test") {
    ResolverCtx ctx;
    ctx.tenant_id       = tenant_id;
    ctx.current_user_id = "tadmin_001";
    ctx.user_name       = "Tenant Admin";
    ctx.roles           = {std::string(Role::TENANT_ADMIN)};
    return ctx;
}

// ---------------------------------------------------------------------------
// Helper: build an anonymous (no roles) ResolverCtx
// ---------------------------------------------------------------------------
static ResolverCtx anonymous_ctx() {
    ResolverCtx ctx;
    ctx.current_user_id = "";
    ctx.roles           = {};
    return ctx;
}

// ---------------------------------------------------------------------------
// Helper: check that a response has no errors
// ---------------------------------------------------------------------------
static void require_success(const ExecutionResult& result, const std::string& op = "") {
    if (!result.is_success()) {
        std::string msg = op.empty() ? "GraphQL error" : op + " failed";
        if (!result.errors.empty()) {
            msg += ": ";
            msg += result.errors[0].message;
        }
        FAIL(msg);
    }
}

// ============================================================================
// Organization CRUD tests
// ============================================================================

// Helper: create a DB+executor with the system DB already initialized
static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_executor() {
    auto db   = std::make_shared<DatabaseManager>();
    auto res  = db->ensure_system_db();
    REQUIRE(res); // must succeed
    auto exec = std::make_shared<GqlExecutor>(db);
    return {db, exec};
}

TEST_CASE("Organization: createOrganization succeeds for platform_admin",
          "[integration][org][crud][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "AcmeCorp_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const std::string vars = json{
        {"name",             org_name},
        {"domain",           "acme.example.com"},
        {"subscriptionTier", "pro"},
        {"userLimit",        50},
        {"storageLimit",     2147483648LL}
    }.dump();

    const std::string query = R"(
        mutation($name: String!, $domain: String, $subscriptionTier: String,
                 $userLimit: Int, $storageLimit: Int) {
          createOrganization(input: {
            name: $name
            domain: $domain
            subscriptionTier: $subscriptionTier
            userLimit: $userLimit
            storageLimit: $storageLimit
          }) {
            id
            name
            domain
            subscriptionTier
            userLimit
            storageLimit
            createdAt
          }
        }
    )";

    const auto vars_obj = json{
        {"name",             org_name},
        {"domain",           "acme.example.com"},
        {"subscriptionTier", "pro"},
        {"userLimit",        50},
        {"storageLimit",     2147483648LL}
    };

    auto result = exec->execute(query, vars_obj.dump(), platform_admin_ctx());
    require_success(result, "createOrganization");

    REQUIRE(result.data.contains("createOrganization"));
    const auto& org = result.data["createOrganization"];
    REQUIRE(!org["id"].get<std::string>().empty());
    REQUIRE(org["name"] == org_name);
    REQUIRE(org["subscriptionTier"] == "pro");
    REQUIRE(org["userLimit"] == 50);
    REQUIRE(!org["createdAt"].get<std::string>().empty());
}

TEST_CASE("Organization: createOrganization is denied for anonymous caller",
          "[integration][org][rbac][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string query = R"(
        mutation {
          createOrganization(input: { name: "ShouldFail" }) {
            id name
          }
        }
    )";

    auto result = exec->execute(query, "{}", anonymous_ctx());
    // RBAC gate: field set to null, errors array populated with FORBIDDEN
    REQUIRE(!result.errors.empty());
    bool has_forbidden = false;
    for (const auto& err : result.errors) {
        if (err.code == EErrorCodes::FORBIDDEN) {
            has_forbidden = true;
            break;
        }
    }
    REQUIRE(has_forbidden);
}

TEST_CASE("Organization: createOrganization is denied for tenant_admin",
          "[integration][org][rbac][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string query = R"(
        mutation {
          createOrganization(input: { name: "ShouldAlsoFail" }) {
            id name
          }
        }
    )";

    auto result = exec->execute(query, "{}", tenant_admin_ctx());
    REQUIRE(!result.errors.empty());
    bool has_forbidden = false;
    for (const auto& err : result.errors) {
        if (err.code == EErrorCodes::FORBIDDEN) {
            has_forbidden = true;
            break;
        }
    }
    REQUIRE(has_forbidden);
}

TEST_CASE("Organization: list organizations returns array for platform_admin",
          "[integration][org][crud][T047-018]") {
    auto [db, exec] = make_executor();

    auto result = exec->execute(
        "query { organizations { id name subscriptionTier } }",
        "{}",
        platform_admin_ctx());
    require_success(result, "organizations");

    REQUIRE(result.data.contains("organizations"));
    REQUIRE(result.data["organizations"].is_array());
}

TEST_CASE("Organization: updateOrganization modifies name for platform_admin",
          "[integration][org][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // First, create one
    const std::string org_name = "UpdateTargetOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto create_vars = json{
        {"name", org_name},
        {"subscriptionTier", "free"},
        {"userLimit", 5},
        {"storageLimit", 1073741824}
    };
    const std::string create_query = R"(
        mutation($name: String!, $subscriptionTier: String, $userLimit: Int, $storageLimit: Int) {
          createOrganization(input: {
            name: $name
            subscriptionTier: $subscriptionTier
            userLimit: $userLimit
            storageLimit: $storageLimit
          }) { id }
        }
    )";
    auto create_result = exec->execute(create_query, create_vars.dump(), platform_admin_ctx());
    require_success(create_result, "createOrganization for update test");
    const std::string org_id = create_result.data["createOrganization"]["id"].get<std::string>();

    // Now update
    const auto update_vars = json{{"id", org_id}, {"newName", "RenamedOrg"}};
    const std::string update_query = R"(
        mutation($id: ID!, $newName: String) {
          updateOrganization(id: $id, input: { name: $newName }) {
            id
            name
          }
        }
    )";
    auto update_result = exec->execute(update_query, update_vars.dump(), platform_admin_ctx());
    require_success(update_result, "updateOrganization");
    REQUIRE(update_result.data["updateOrganization"]["name"] == "RenamedOrg");
}

TEST_CASE("Organization: deleteOrganization removes the org for platform_admin",
          "[integration][org][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // Create first
    const std::string org_name = "DeleteTargetOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto create_vars = json{{"name", org_name}, {"subscriptionTier", "free"},
                                   {"userLimit", 5}, {"storageLimit", 1073741824}};
    const std::string create_query = R"(
        mutation($name: String!, $subscriptionTier: String, $userLimit: Int, $storageLimit: Int) {
          createOrganization(input: {
            name: $name subscriptionTier: $subscriptionTier
            userLimit: $userLimit storageLimit: $storageLimit
          }) { id }
        }
    )";
    auto cr = exec->execute(create_query, create_vars.dump(), platform_admin_ctx());
    require_success(cr, "create for delete test");
    const std::string org_id = cr.data["createOrganization"]["id"].get<std::string>();

    // Delete
    const auto del_vars = json{{"id", org_id}};
    const auto del_result = exec->execute(
        "mutation($id: ID!) { deleteOrganization(id: $id) }",
        del_vars.dump(),
        platform_admin_ctx());
    require_success(del_result, "deleteOrganization");
    REQUIRE(del_result.data["deleteOrganization"] == true);
}

// ============================================================================
// User CRUD tests
// ============================================================================

TEST_CASE("User: createUser succeeds for tenant_admin",
          "[integration][user][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // Create an org first so the tenant DB exists
    const std::string org_name = "UserTestOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto org_vars = json{{"name", org_name}, {"subscriptionTier", "free"},
                                {"userLimit", 10}, {"storageLimit", 1073741824}};
    auto org_res = exec->execute(
        R"(mutation($name:String!,$subscriptionTier:String,$userLimit:Int,$storageLimit:Int){
             createOrganization(input:{name:$name subscriptionTier:$subscriptionTier
                                      userLimit:$userLimit storageLimit:$storageLimit}){id}
           })",
        org_vars.dump(), platform_admin_ctx());
    require_success(org_res, "create org for user test");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    // Create user as tenant admin of that org
    const auto user_vars = json{
        {"orgId", org_id},
        {"email", "alice@example.com"},
        {"password", "S3cret!Pass"},
        {"displayName", "Alice Example"}
    };
    const std::string create_user_query = R"(
        mutation($orgId: ID!, $email: String!, $password: String!, $displayName: String) {
          createUser(
            organizationId: $orgId
            input: { email: $email password: $password displayName: $displayName }
          ) {
            id
            email
            displayName
            roles
            isActive
            createdAt
          }
        }
    )";

    auto result = exec->execute(create_user_query, user_vars.dump(),
                                tenant_admin_ctx(org_id));
    require_success(result, "createUser");

    const auto& user = result.data["createUser"];
    REQUIRE(!user["id"].get<std::string>().empty());
    REQUIRE(user["email"] == "alice@example.com");
    REQUIRE(user["displayName"] == "Alice Example");
    REQUIRE(user["isActive"] == true);
    REQUIRE(!user["createdAt"].get<std::string>().empty());
}

TEST_CASE("User: createUser is denied for anonymous caller",
          "[integration][user][rbac][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string query = R"(
        mutation {
          createUser(
            organizationId: "org_test"
            input: { email: "x@test.com" password: "pass123" }
          ) { id }
        }
    )";

    auto result = exec->execute(query, "{}", anonymous_ctx());
    REQUIRE(!result.errors.empty());
    bool has_forbidden = false;
    for (const auto& err : result.errors) {
        if (err.code == EErrorCodes::FORBIDDEN) {
            has_forbidden = true;
            break;
        }
    }
    REQUIRE(has_forbidden);
}

TEST_CASE("User: createUser is denied for USER role",
          "[integration][user][rbac][T047-018]") {
    auto [db, exec] = make_executor();

    ResolverCtx ctx;
    ctx.tenant_id       = "org_test";
    ctx.current_user_id = "regular_user";
    ctx.roles           = {std::string(Role::USER)};

    const std::string query = R"(
        mutation {
          createUser(
            organizationId: "org_test"
            input: { email: "x@test.com" password: "pass123" }
          ) { id }
        }
    )";

    auto result = exec->execute(query, "{}", ctx);
    REQUIRE(!result.errors.empty());
    bool has_forbidden = false;
    for (const auto& err : result.errors) {
        if (err.code == EErrorCodes::FORBIDDEN) {
            has_forbidden = true;
            break;
        }
    }
    REQUIRE(has_forbidden);
}

TEST_CASE("User: updateUser modifies displayName for tenant_admin",
          "[integration][user][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // Create org + user
    const std::string org_name = "UpdateUserOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto org_vars = json{{"name", org_name}, {"subscriptionTier", "free"},
                                {"userLimit", 10}, {"storageLimit", 1073741824}};
    auto org_res = exec->execute(
        R"(mutation($name:String!,$subscriptionTier:String,$userLimit:Int,$storageLimit:Int){
             createOrganization(input:{name:$name subscriptionTier:$subscriptionTier
                                      userLimit:$userLimit storageLimit:$storageLimit}){id}
           })",
        org_vars.dump(), platform_admin_ctx());
    require_success(org_res, "create org");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    const auto cu_vars = json{
        {"orgId", org_id}, {"email", "bob@example.com"},
        {"password", "P@ssw0rd!"}, {"displayName", "Bob"}
    };
    auto cu_res = exec->execute(
        R"(mutation($orgId:ID!,$email:String!,$password:String!,$displayName:String){
             createUser(organizationId:$orgId
               input:{email:$email password:$password displayName:$displayName}){id}
           })",
        cu_vars.dump(), tenant_admin_ctx(org_id));
    require_success(cu_res, "createUser for update test");
    const std::string user_id = cu_res.data["createUser"]["id"].get<std::string>();

    // Update displayName
    const auto upd_vars = json{{"orgId", org_id}, {"id", user_id}, {"displayName", "Bobby"}};
    auto upd_res = exec->execute(
        R"(mutation($orgId:ID!,$id:ID!,$displayName:String){
             updateUser(organizationId:$orgId id:$id input:{displayName:$displayName}){
               id displayName
             }
           })",
        upd_vars.dump(), tenant_admin_ctx(org_id));
    require_success(upd_res, "updateUser");
    REQUIRE(upd_res.data["updateUser"]["displayName"] == "Bobby");
}

TEST_CASE("User: deleteUser removes user for tenant_admin",
          "[integration][user][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // Create org + user
    const std::string org_name = "DeleteUserOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto org_vars = json{{"name", org_name}, {"subscriptionTier", "free"},
                                {"userLimit", 10}, {"storageLimit", 1073741824}};
    auto org_res = exec->execute(
        R"(mutation($name:String!,$subscriptionTier:String,$userLimit:Int,$storageLimit:Int){
             createOrganization(input:{name:$name subscriptionTier:$subscriptionTier
                                      userLimit:$userLimit storageLimit:$storageLimit}){id}
           })",
        org_vars.dump(), platform_admin_ctx());
    require_success(org_res, "create org");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    const auto cu_vars = json{
        {"orgId", org_id}, {"email", "carol@example.com"},
        {"password", "S3cure!Pass"}, {"displayName", "Carol"}
    };
    auto cu_res = exec->execute(
        R"(mutation($orgId:ID!,$email:String!,$password:String!,$displayName:String){
             createUser(organizationId:$orgId
               input:{email:$email password:$password displayName:$displayName}){id}
           })",
        cu_vars.dump(), tenant_admin_ctx(org_id));
    require_success(cu_res, "createUser for delete test");
    const std::string user_id = cu_res.data["createUser"]["id"].get<std::string>();

    // Delete
    const auto del_vars = json{{"orgId", org_id}, {"id", user_id}};
    auto del_res = exec->execute(
        R"(mutation($orgId:ID!,$id:ID!){
             deleteUser(organizationId:$orgId id:$id)
           })",
        del_vars.dump(), tenant_admin_ctx(org_id));
    require_success(del_res, "deleteUser");
    REQUIRE(del_res.data["deleteUser"] == true);
}

TEST_CASE("User: list users returns array for tenant_admin",
          "[integration][user][crud][T047-018]") {
    auto [db, exec] = make_executor();

    // Create org
    const std::string org_name = "ListUserOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto org_vars = json{{"name", org_name}, {"subscriptionTier", "free"},
                                {"userLimit", 10}, {"storageLimit", 1073741824}};
    auto org_res = exec->execute(
        R"(mutation($name:String!,$subscriptionTier:String,$userLimit:Int,$storageLimit:Int){
             createOrganization(input:{name:$name subscriptionTier:$subscriptionTier
                                      userLimit:$userLimit storageLimit:$storageLimit}){id}
           })",
        org_vars.dump(), platform_admin_ctx());
    require_success(org_res, "create org for list test");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    auto result = exec->execute(
        "query { users { id email isActive } }",
        "{}",
        tenant_admin_ctx(org_id));
    require_success(result, "users");
    REQUIRE(result.data["users"].is_array());
}

// ============================================================================
// Login placeholder (depends on T047-016 / T049-001 / T049-002)
// ============================================================================

// TODO(T047-016): Add login mutation integration tests once sessions table
// (T049-001) and AuthenticationMiddleware::create_session() (T049-002) are
// implemented. Tests should cover:
//   • login with valid credentials returns { token, expiresAt }
//   • login with wrong password returns error (not FORBIDDEN, but UNAUTHORIZED)
//   • token can be decoded and contains correct user_id, roles, tenant_id
