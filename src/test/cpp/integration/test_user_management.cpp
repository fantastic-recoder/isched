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
#include <filesystem>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include <isched/backend/isched_gql_error.hpp>

#include "../isched/isched_graphql_test_helpers.hpp"

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
    ResolverCtx ctx = isched::test::platform_admin_ctx(tenant_id);
    ctx.user_name = "Admin User";
    return ctx;
}

// ---------------------------------------------------------------------------
// Helper: build an authenticated ResolverCtx for a tenant admin
// ---------------------------------------------------------------------------
static ResolverCtx tenant_admin_ctx(const std::string& tenant_id = "org_test") {
    ResolverCtx ctx = isched::test::tenant_admin_ctx(tenant_id);
    ctx.user_name = "Tenant Admin";
    return ctx;
}

// ---------------------------------------------------------------------------
// Helper: build an anonymous (no roles) ResolverCtx
// ---------------------------------------------------------------------------
static ResolverCtx anonymous_ctx() {
    return isched::test::anonymous_ctx();
}

// ---------------------------------------------------------------------------
// Helper: check that a response has no errors
// ---------------------------------------------------------------------------
static void require_success(const ExecutionResult& result, const std::string& op = "") {
    const std::string operation_name = op.empty() ? "GraphQL operation" : op;
    try {
        isched::test::require_success(result, operation_name);
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

// ============================================================================
// Organization CRUD tests
// ============================================================================

// Helper: create a DB+executor with an isolated temporary system DB
static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_executor() {
    // Use a unique temp directory per call to avoid cross-test contamination
    const auto tmp = std::filesystem::temp_directory_path() /
        ("isched_test_" + g_run_suffix + "_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tmp / "tenants");

    DatabaseManager::Config cfg;
    cfg.base_path      = (tmp / "tenants").string();
    cfg.system_db_path = (tmp / "isched_system.db").string();

    auto db  = std::make_shared<DatabaseManager>(cfg);
    auto res = db->ensure_system_db();
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

// Helper: create an org and return its id + initial revision (0)
static std::string create_test_org(
    std::shared_ptr<GqlExecutor> exec,
    const std::string& name)
{
    const auto vars = json{{"name", name}};
    const std::string q = R"(
        mutation($name: String!) {
          createOrganization(input: { name: $name }) { id }
        }
    )";
    auto r = exec->execute(q, vars.dump(), platform_admin_ctx());
    REQUIRE(r.is_success());
    return r.data["createOrganization"]["id"].get<std::string>();
}

// ============================================================================
// WebUI contract: organizations returns a connection (nodes + pageInfo)
// ============================================================================

TEST_CASE("Organization: organizations query returns connection object for platform_admin",
          "[integration][org][webui][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string q = R"(
        query($page: PageInput!, $sort: [SortInput!]) {
          organizations(page: $page, sort: $sort) {
            nodes { id name status revision updatedAt }
            pageInfo { number size totalElements totalPages }
          }
        }
    )";
    const auto vars = json{
        {"page", json{{"number", 1}, {"size", 10}}},
        {"sort", json::array({json{{"field", "name"}, {"direction", "ASC"}}})}
    };
    auto result = exec->execute(q, vars.dump(), platform_admin_ctx());
    require_success(result, "organizations connection");

    REQUIRE(result.data.contains("organizations"));
    const auto& conn = result.data["organizations"];
    REQUIRE(conn.contains("nodes"));
    REQUIRE(conn["nodes"].is_array());
    REQUIRE(conn.contains("pageInfo"));
    REQUIRE(conn["pageInfo"]["number"].is_number());
    REQUIRE(conn["pageInfo"]["size"].is_number());
    REQUIRE(conn["pageInfo"]["totalElements"].is_number());
    REQUIRE(conn["pageInfo"]["totalPages"].is_number());
}

TEST_CASE("Organization: createOrganization returns status, revision and updatedAt",
          "[integration][org][webui][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "WebUICreateOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const auto vars = json{{"name", org_name}};
    const std::string q = R"(
        mutation($name: String!) {
          createOrganization(input: { name: $name }) {
            id name status revision updatedAt
          }
        }
    )";
    auto result = exec->execute(q, vars.dump(), platform_admin_ctx());
    require_success(result, "createOrganization webui fields");

    const auto& org = result.data["createOrganization"];
    REQUIRE(!org["id"].get<std::string>().empty());
    REQUIRE(org["name"] == org_name);
    REQUIRE(org["status"] == "ACTIVE");
    REQUIRE(org["revision"] == 0);
    REQUIRE(!org["updatedAt"].get<std::string>().empty());
}

TEST_CASE("Organization: organizations nodes include status, revision, updatedAt",
          "[integration][org][webui][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "WebUIFieldsOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    std::ignore = create_test_org(exec, org_name);

    const std::string q = R"(
        query($page: PageInput!) {
          organizations(page: $page) {
            nodes { id name status revision updatedAt }
            pageInfo { totalElements }
          }
        }
    )";
    const auto vars = json{{"page", json{{"number", 1}, {"size", 50}}}};
    auto result = exec->execute(q, vars.dump(), platform_admin_ctx());
    require_success(result, "organizations nodes fields");

    const auto& nodes = result.data["organizations"]["nodes"];
    REQUIRE(!nodes.empty());
    bool found = false;
    for (const auto& n : nodes) {
        if (n["name"] == org_name) {
            found = true;
            REQUIRE(n.contains("status"));
            REQUIRE(n.contains("revision"));
            REQUIRE(n.contains("updatedAt"));
            REQUIRE(n["status"] == "ACTIVE");
            REQUIRE(n["revision"] == 0);
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("Organization: updateOrganization modifies name for platform_admin",
          "[integration][org][crud][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "UpdateTargetOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const std::string org_id = create_test_org(exec, org_name);

    // Update with correct expectedRevision=0
    const auto update_vars = json{{"id", org_id}, {"newName", "RenamedOrg"}, {"rev", 0}};
    const std::string update_query = R"(
        mutation($id: ID!, $newName: String, $rev: Int!) {
          updateOrganization(id: $id, input: { name: $newName }, expectedRevision: $rev) {
            id name status revision updatedAt
          }
        }
    )";
    auto update_result = exec->execute(update_query, update_vars.dump(), platform_admin_ctx());
    require_success(update_result, "updateOrganization");
    REQUIRE(update_result.data["updateOrganization"]["name"] == "RenamedOrg");
    REQUIRE(update_result.data["updateOrganization"]["revision"] == 1);
}

TEST_CASE("Organization: updateOrganization with wrong expectedRevision returns CONFLICT",
          "[integration][org][webui][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "ConflictOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const std::string org_id = create_test_org(exec, org_name);

    // Submit with wrong revision (99 instead of 0)
    const auto vars = json{{"id", org_id}, {"newName", "ShouldConflict"}, {"rev", 99}};
    const std::string q = R"(
        mutation($id: ID!, $newName: String, $rev: Int!) {
          updateOrganization(id: $id, input: { name: $newName }, expectedRevision: $rev) {
            id name
          }
        }
    )";
    auto result = exec->execute(q, vars.dump(), platform_admin_ctx());
    REQUIRE(!result.errors.empty());
    bool has_conflict = false;
    for (const auto& err : result.errors) {
        if (err.code == EErrorCodes::CONFLICT) {
            has_conflict = true;
            break;
        }
    }
    REQUIRE(has_conflict);
}

TEST_CASE("Organization: updateOrganization can change status to SUSPENDED",
          "[integration][org][webui][T047-018]") {
    auto [db, exec] = make_executor();

    const std::string org_name = "SuspendOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    const std::string org_id = create_test_org(exec, org_name);

    const auto vars = json{{"id", org_id}, {"rev", 0}};
    const std::string q = R"(
        mutation($id: ID!, $rev: Int!) {
          updateOrganization(id: $id, input: { status: "SUSPENDED" }, expectedRevision: $rev) {
            id status revision
          }
        }
    )";
    auto result = exec->execute(q, vars.dump(), platform_admin_ctx());
    require_success(result, "updateOrganization status");
    REQUIRE(result.data["updateOrganization"]["status"] == "SUSPENDED");
    REQUIRE(result.data["updateOrganization"]["revision"] == 1);
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
        {"password", "S3cret!Pass1"},
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
        {"password", "P@ssw0rd!123"}, {"displayName", "Bob"}
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
        {"password", "S3cure!Pass1"}, {"displayName", "Carol"}
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
// Login mutation tests (T047-016)
// ============================================================================

/// Create an executor that has AuthenticationMiddleware wired so the `login`
/// mutation can create and validate JWT sessions.
static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_executor_with_auth() {
    auto [db, exec] = make_executor();
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-login-jwt-secret-at-least-32-bytes!");
    exec->set_auth_middleware(auth);
    return {db, exec};
}

TEST_CASE("Login: valid tenant-user credentials return token and expiresAt",
          "[integration][login][T047-016]") {
    auto [db, exec] = make_executor_with_auth();

    // --- create org ---
    const std::string org_name = "LoginOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    auto org_res = exec->execute(
        R"(mutation($n:String!){createOrganization(input:{name:$n subscriptionTier:"free"
             userLimit:10 storageLimit:1073741824}){id}})",
        json{{"n", org_name}}.dump(),
        platform_admin_ctx());
    require_success(org_res, "create org for login test");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    // --- create user ---
    const std::string email    = "bob_" + g_run_suffix + "@example.com";
    const std::string password = "Bob$ecret9911";
    auto cu_res = exec->execute(
        R"(mutation($o:ID!,$e:String!,$p:String!){
             createUser(organizationId:$o input:{email:$e password:$p displayName:"Bob"}){id}
           })",
        json{{"o", org_id}, {"e", email}, {"p", password}}.dump(),
        tenant_admin_ctx(org_id));
    require_success(cu_res, "create user for login test");

    // --- call login mutation ---
    const auto login_vars = json{
        {"email",          email},
        {"password",       password},
        {"organizationId", org_id}
    };
    auto result = exec->execute(
        R"(mutation($email:String!,$password:String!,$organizationId:ID){
             login(email:$email password:$password organizationId:$organizationId){
               token
               expiresAt
             }
           })",
        login_vars.dump(),
        anonymous_ctx());

    require_success(result, "login");
    const auto& payload = result.data["login"];
    REQUIRE(payload.contains("token"));
    REQUIRE(payload.contains("expiresAt"));
    REQUIRE(!payload["token"].get<std::string>().empty());
    REQUIRE(!payload["expiresAt"].get<std::string>().empty());
}

TEST_CASE("Login: wrong password returns error, not FORBIDDEN",
          "[integration][login][T047-016]") {
    auto [db, exec] = make_executor_with_auth();

    const std::string org_name = "LoginOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    auto org_res = exec->execute(
        R"(mutation($n:String!){createOrganization(input:{name:$n subscriptionTier:"free"
             userLimit:10 storageLimit:1073741824}){id}})",
        json{{"n", org_name}}.dump(),
        platform_admin_ctx());
    require_success(org_res, "create org (wrong-pw test)");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    const std::string email = "carol_" + g_run_suffix + "@example.com";
    auto cu_res = exec->execute(
        R"(mutation($o:ID!,$e:String!,$p:String!){
             createUser(organizationId:$o input:{email:$e password:$p displayName:"Carol"}){id}
           })",
        json{{"o", org_id}, {"e", email}, {"p", "CorrectPass1!"}}.dump(),
        tenant_admin_ctx(org_id));
    require_success(cu_res, "create user (wrong-pw test)");

    // Attempt login with wrong password
    auto result = exec->execute(
        R"(mutation($e:String!,$p:String!,$o:ID){
             login(email:$e password:$p organizationId:$o){token expiresAt}
           })",
        json{{"e", email}, {"p", "WrongPassword!"}, {"o", org_id}}.dump(),
        anonymous_ctx());

    // Must fail — never succeed
    REQUIRE(!result.is_success());
    // Must NOT be an RBAC/FORBIDDEN error — password errors map to UNKNOWN_ERROR
    REQUIRE(!result.errors.empty());
    REQUIRE(result.errors[0].code != EErrorCodes::FORBIDDEN);
}

TEST_CASE("Login: unknown email returns error",
          "[integration][login][T047-016]") {
    auto [db, exec] = make_executor_with_auth();

    const std::string org_name = "LoginOrg_" + g_run_suffix + "_" + std::to_string(__LINE__);
    auto org_res = exec->execute(
        R"(mutation($n:String!){createOrganization(input:{name:$n subscriptionTier:"free"
             userLimit:10 storageLimit:1073741824}){id}})",
        json{{"n", org_name}}.dump(),
        platform_admin_ctx());
    require_success(org_res, "create org (unknown-email test)");
    const std::string org_id = org_res.data["createOrganization"]["id"].get<std::string>();

    auto result = exec->execute(
        R"(mutation($e:String!,$p:String!,$o:ID){
             login(email:$e password:$p organizationId:$o){token expiresAt}
           })",
        json{{"e", "nobody@nowhere.com"}, {"p", "any"}, {"o", org_id}}.dump(),
        anonymous_ctx());

    REQUIRE(!result.is_success());
    REQUIRE(!result.errors.empty());
}

