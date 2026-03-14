// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_session_revocation.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for session revocation mutations (T049-008)
 *
 * Covers: logout, revokeSession, revokeAllSessions, terminateAllSessions.
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

static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

// ---------------------------------------------------------------------------
// Context helpers
// ---------------------------------------------------------------------------
static ResolverCtx platform_admin_ctx(const std::string& tenant_id = "platform") {
    ResolverCtx ctx;
    ctx.tenant_id       = tenant_id;
    ctx.current_user_id = "admin_001";
    ctx.user_name       = "Platform Admin";
    ctx.roles           = {std::string(Role::PLATFORM_ADMIN)};
    return ctx;
}

static ResolverCtx tenant_admin_ctx(const std::string& tenant_id,
                                    const std::string& user_id = "tadmin_001") {
    ResolverCtx ctx;
    ctx.tenant_id       = tenant_id;
    ctx.current_user_id = user_id;
    ctx.user_name       = "Tenant Admin";
    ctx.roles           = {std::string(Role::TENANT_ADMIN)};
    return ctx;
}

static ResolverCtx anonymous_ctx() {
    return {};
}

// ---------------------------------------------------------------------------
// Build a DB + GqlExecutor with AuthenticationMiddleware wired
// ---------------------------------------------------------------------------
static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_executor() {
    auto db  = std::make_shared<DatabaseManager>();
    auto res = db->ensure_system_db();
    REQUIRE(res);
    auto exec = std::make_shared<GqlExecutor>(db);
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    exec->set_auth_middleware(auth);
    return {db, exec};
}

static void require_success(const ExecutionResult& result, const std::string& op = "") {
    if (!result.is_success()) {
        std::string msg = op.empty() ? "GraphQL error" : op + " failed";
        if (!result.errors.empty()) msg += ": " + result.errors[0].message;
        FAIL(msg);
    }
}

// ---------------------------------------------------------------------------
// Helpers: create org and user, then login to get token + session_id
// ---------------------------------------------------------------------------
static std::string create_org(const std::shared_ptr<GqlExecutor>& exec,
                               const std::string& suffix) {
    const std::string name = "RevOrg_" + suffix;
    auto res = exec->execute(
        R"(mutation($n:String!){createOrganization(input:{name:$n subscriptionTier:"free"
             userLimit:10 storageLimit:1073741824}){id}})",
        json{{"n", name}}.dump(),
        platform_admin_ctx());
    require_success(res, "create org");
    return res.data["createOrganization"]["id"].get<std::string>();
}

static std::string create_user(const std::shared_ptr<GqlExecutor>& exec,
                                const std::string& org_id,
                                const std::string& email,
                                const std::string& password) {
    auto res = exec->execute(
        R"(mutation($o:ID!,$e:String!,$p:String!){
             createUser(organizationId:$o input:{email:$e password:$p displayName:"Test"}){id}
           })",
        json{{"o", org_id}, {"e", email}, {"p", password}}.dump(),
        tenant_admin_ctx(org_id));
    require_success(res, "create user");
    return res.data["createUser"]["id"].get<std::string>();
}

/// Returns {token, session_id} from a login call.
static std::pair<std::string, std::string> do_login(
    const std::shared_ptr<GqlExecutor>& exec,
    const std::shared_ptr<DatabaseManager>& db,
    const std::string& org_id,
    const std::string& email,
    const std::string& password)
{
    auto res = exec->execute(
        R"(mutation($e:String!,$p:String!,$o:ID){
             login(email:$e password:$p organizationId:$o){token expiresAt}
           })",
        json{{"e", email}, {"p", password}, {"o", org_id}}.dump(),
        anonymous_ctx());
    require_success(res, "login");
    const std::string token = res.data["login"]["token"].get<std::string>();

    // Extract jti (session_id) via validate_request so we can revoke by ID
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    // Same secret used in make_executor()
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    const auto ar = auth->validate_request(
        std::unordered_map<std::string, std::string>{{"Authorization", "Bearer " + token}}, "");
    return {token, ar.session_id};
}

// ============================================================================
// Tests: logout
// ============================================================================

TEST_CASE("logout: revokes the caller's session so the token is rejected",
          "[integration][session][logout][T049-008]") {
    auto [db, exec] = make_executor();

    const std::string org_id = create_org(exec, g_run_suffix + "_logout_" + std::to_string(__LINE__));
    const std::string email = "logout_user_" + g_run_suffix + "@example.com";
    create_user(exec, org_id, email, "LogoutPass1!");

    auto [token, session_id] = do_login(exec, db, org_id, email, "LogoutPass1!");
    REQUIRE(!token.empty());
    REQUIRE(!session_id.empty());

    // Build authenticated ctx from the issued token
    auto auth_ctx = ResolverCtx{};
    auth_ctx.tenant_id       = org_id;
    auth_ctx.session_id      = session_id;
    auth_ctx.current_user_id = ""; // not needed by logout
    auth_ctx.db              = db;

    // logout
    auto logout_res = exec->execute(
        "mutation { logout }",
        "{}",
        auth_ctx);
    require_success(logout_res, "logout");
    REQUIRE(logout_res.data["logout"] == true);

    // After logout, validate_token must reject the session
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    const auto vt_res = auth->validate_token(*db, token, org_id);
    REQUIRE(!vt_res.is_authenticated);
}

TEST_CASE("logout: unauthenticated caller succeeds without side-effects",
          "[integration][session][logout][T049-008]") {
    auto [db, exec] = make_executor();

    // No session_id in context — should still return true gracefully
    auto result = exec->execute("mutation { logout }", "{}", anonymous_ctx());
    require_success(result, "logout anon");
    REQUIRE(result.data["logout"] == true);
}

// ============================================================================
// Tests: revokeSession
// ============================================================================

TEST_CASE("revokeSession: tenant_admin can revoke a specific session",
          "[integration][session][revokeSession][T049-008]") {
    auto [db, exec] = make_executor();

    const std::string org_id = create_org(exec, g_run_suffix + "_revS_" + std::to_string(__LINE__));
    const std::string email = "rev_sess_" + g_run_suffix + "@example.com";
    create_user(exec, org_id, email, "RevSess1!");

    auto [token, session_id] = do_login(exec, db, org_id, email, "RevSess1!");
    REQUIRE(!session_id.empty());

    // tenant_admin revokes the session
    auto ctx = tenant_admin_ctx(org_id);
    ctx.db   = db;
    auto result = exec->execute(
        R"(mutation($s:ID!){ revokeSession(sessionId:$s) })",
        json{{"s", session_id}}.dump(),
        ctx);
    require_success(result, "revokeSession");
    REQUIRE(result.data["revokeSession"] == true);

    // Token must now be rejected
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    const auto vt = auth->validate_token(*db, token, org_id);
    REQUIRE(!vt.is_authenticated);
}

TEST_CASE("revokeSession: anonymous caller is rejected with FORBIDDEN",
          "[integration][session][revokeSession][T049-008]") {
    auto [db, exec] = make_executor();

    auto ctx = anonymous_ctx();
    ctx.db   = db;
    auto result = exec->execute(
        R"(mutation{ revokeSession(sessionId:"fake-id") })",
        "{}",
        ctx);

    REQUIRE(!result.is_success());
    REQUIRE(!result.errors.empty());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
}

// ============================================================================
// Tests: revokeAllSessions
// ============================================================================

TEST_CASE("revokeAllSessions: tenant_admin can revoke all sessions for a user",
          "[integration][session][revokeAllSessions][T049-008]") {
    auto [db, exec] = make_executor();

    const std::string org_id = create_org(exec, g_run_suffix + "_revAll_" + std::to_string(__LINE__));
    const std::string email  = "rev_all_" + g_run_suffix + "@example.com";
    const std::string user_id = create_user(exec, org_id, email, "RevAll1!");

    // Login twice to create two sessions
    auto [tok1, sess1] = do_login(exec, db, org_id, email, "RevAll1!");
    auto [tok2, sess2] = do_login(exec, db, org_id, email, "RevAll1!");
    REQUIRE(!sess1.empty());
    REQUIRE(!sess2.empty());

    // Revoke all sessions for this user
    auto ctx = tenant_admin_ctx(org_id);
    ctx.db   = db;
    auto result = exec->execute(
        R"(mutation($u:ID!){ revokeAllSessions(userId:$u) })",
        json{{"u", user_id}}.dump(),
        ctx);
    require_success(result, "revokeAllSessions");
    REQUIRE(result.data["revokeAllSessions"] == true);

    // Both tokens must now be rejected
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    REQUIRE(!auth->validate_token(*db, tok1, org_id).is_authenticated);
    REQUIRE(!auth->validate_token(*db, tok2, org_id).is_authenticated);
}

TEST_CASE("revokeAllSessions: regular user is rejected with FORBIDDEN",
          "[integration][session][revokeAllSessions][T049-008]") {
    auto [db, exec] = make_executor();

    ResolverCtx ctx;
    ctx.roles = {std::string(Role::USER)};
    ctx.db    = db;
    auto result = exec->execute(
        R"(mutation{ revokeAllSessions(userId:"any") })",
        "{}",
        ctx);

    REQUIRE(!result.is_success());
    REQUIRE(!result.errors.empty());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
}

// ============================================================================
// Tests: terminateAllSessions
// ============================================================================

TEST_CASE("terminateAllSessions: platform_admin revokes all tenant sessions",
          "[integration][session][terminateAllSessions][T049-008]") {
    auto [db, exec] = make_executor();

    const std::string org_id = create_org(exec, g_run_suffix + "_termAll_" + std::to_string(__LINE__));
    const std::string email  = "term_" + g_run_suffix + "@example.com";
    create_user(exec, org_id, email, "Term1Pass!");

    auto [tok, sess] = do_login(exec, db, org_id, email, "Term1Pass!");
    REQUIRE(!sess.empty());

    // platform_admin terminates all sessions for the org
    auto ctx = platform_admin_ctx(org_id);
    ctx.db   = db;
    auto result = exec->execute(
        R"(mutation($o:ID!){ terminateAllSessions(organizationId:$o) })",
        json{{"o", org_id}}.dump(),
        ctx);
    require_success(result, "terminateAllSessions");
    REQUIRE(result.data["terminateAllSessions"] == true);

    // The tenant user token must now be rejected
    auto auth = std::shared_ptr<AuthenticationMiddleware>(AuthenticationMiddleware::create());
    auth->configure_jwt_secret("test-revoke-jwt-secret-at-least-32-bytes!!");
    REQUIRE(!auth->validate_token(*db, tok, org_id).is_authenticated);
}

TEST_CASE("terminateAllSessions: tenant_admin is rejected with FORBIDDEN",
          "[integration][session][terminateAllSessions][T049-008]") {
    auto [db, exec] = make_executor();

    auto ctx = tenant_admin_ctx("some_org");
    ctx.db   = db;
    auto result = exec->execute(
        R"(mutation{ terminateAllSessions(organizationId:"any") })",
        "{}",
        ctx);

    REQUIRE(!result.is_success());
    REQUIRE(!result.errors.empty());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
}
