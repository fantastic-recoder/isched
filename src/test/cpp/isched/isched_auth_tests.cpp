// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_auth_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Test suite for Authentication Middleware and RBAC (T047-017)
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <memory>
#include <string>
#include <chrono>

#include "isched/backend/isched_AuthenticationMiddleware.hpp"
#include "isched/backend/isched_DatabaseManager.hpp"
#include "isched/backend/isched_GqlExecutor.hpp"
#include "isched/backend/isched_gql_error.hpp"

using namespace isched::v0_0_1::backend;
using isched::v0_0_1::gql::EErrorCodes;

TEST_CASE("Authentication Middleware Basic Functionality", "[auth]") {

}

TEST_CASE("JWT generate and validate round-trip", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    auth->configure_jwt_secret("test-secret-key-at-least-32-bytes!");

    const std::string user_id = "alice";
    const std::string tenant_id = "tenant-1";
    const std::vector<std::string> perms = {"read", "write"};

    const std::string token = auth->generate_jwt_token(user_id, tenant_id, perms, 60);
    REQUIRE_FALSE(token.empty());

    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer " + token}};
    auto result = auth->validate_request(headers, tenant_id);

    REQUIRE(result.is_authenticated);
    REQUIRE(result.user_id == user_id);
    REQUIRE(result.tenant_id == tenant_id);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("JWT validation fails with wrong secret", "[auth][jwt]") {
    auto auth1 = AuthenticationMiddleware::create();
    auto auth2 = AuthenticationMiddleware::create();
    auth1->configure_jwt_secret("secret-one-at-least-32-bytes-long!");
    auth2->configure_jwt_secret("secret-two-at-least-32-bytes-long!");

    const std::string token = auth1->generate_jwt_token("bob", "tenant-1", {"read"}, 60);
    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer " + token}};
    auto result = auth2->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("JWT validation fails on missing Authorization header", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    auth->configure_jwt_secret("test-secret-key-at-least-32-bytes!");

    std::unordered_map<std::string, std::string> headers{};
    auto result = auth->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("JWT validation fails when secret not configured", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    // No secret configured

    std::unordered_map<std::string, std::string> headers{{"Authorization", "Bearer sometoken"}};
    auto result = auth->validate_request(headers, "tenant-1");

    REQUIRE_FALSE(result.is_authenticated);
    REQUIRE_FALSE(result.error_message.empty());
}

TEST_CASE("generate_jwt_token throws when secret not configured", "[auth][jwt]") {
    auto auth = AuthenticationMiddleware::create();
    REQUIRE_THROWS_AS(auth->generate_jwt_token("user", "tenant", {"read"}, 60), std::runtime_error);
}

TEST_CASE("Session create and get", "[auth][session]") {
    auto auth = AuthenticationMiddleware::create();
    auto session = auth->create_session("user1", "tenant-1", {"read"});

    REQUIRE_FALSE(session.session_id.empty());
    REQUIRE(session.user_id == "user1");
    REQUIRE(session.tenant_id == "tenant-1");

    auto retrieved = auth->get_session(session.session_id);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->user_id == "user1");
}

TEST_CASE("Session invalidation removes session", "[auth][session]") {
    auto auth = AuthenticationMiddleware::create();
    auto session = auth->create_session("user2", "tenant-1", {});

    auth->invalidate_session(session.session_id);
    auto retrieved = auth->get_session(session.session_id);
    REQUIRE_FALSE(retrieved.has_value());
}

// =============================================================================
// T047-017: RBAC — Role constants
// =============================================================================

TEST_CASE("Role namespace constants have expected string values", "[rbac][role]") {
    REQUIRE(Role::PLATFORM_ADMIN == "role_platform_admin");
    REQUIRE(Role::TENANT_ADMIN   == "role_tenant_admin");
    REQUIRE(Role::USER           == "role_user");
    REQUIRE(Role::SERVICE        == "role_service");
}

// =============================================================================
// T047-017: RBAC — GqlExecutor gate (pure resolver logic, no DB)
// =============================================================================

namespace {
    // Minimal executor setup used across RBAC gate tests.
    // Registers one gated field ("secret") and one open field ("open").
    std::unique_ptr<GqlExecutor> make_rbac_executor() {
        auto exec = std::make_unique<GqlExecutor>(std::make_shared<DatabaseManager>());
        // Register a custom schema with the two test fields
        auto load_res = exec->load_schema(
            "type Query { secret: String open: String }"
        );
        (void)load_res; // ignore missing-resolver errors during schema load
        exec->register_resolver({}, "secret",
            [](const nlohmann::json&, const nlohmann::json&, const ResolverCtx&) -> nlohmann::json {
                return "classified";
            });
        exec->register_resolver({}, "open",
            [](const nlohmann::json&, const nlohmann::json&, const ResolverCtx&) -> nlohmann::json {
                return "public";
            });
        exec->require_roles("secret", {std::string(Role::PLATFORM_ADMIN)});
        // "open" has no role gate
        return exec;
    }
} // anonymous namespace

TEST_CASE("RBAC gate: ungated field allows anonymous caller", "[rbac][gate]") {
    auto exec = make_rbac_executor();
    // No roles in context → anonymous
    auto result = exec->execute("query { open }", "{}");
    REQUIRE(result.is_success());
    REQUIRE(result.data["open"] == "public");
}

TEST_CASE("RBAC gate: gated field is denied when caller has no roles", "[rbac][gate]") {
    auto exec = make_rbac_executor();
    auto result = exec->execute("query { secret }", "{}", ResolverCtx{});
    // The resolver must NOT have been called; a FORBIDDEN error must be present
    REQUIRE_FALSE(result.is_success());
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
    // Field value must be null (not the resolver's "classified" string)
    REQUIRE(result.data.contains("secret"));
    REQUIRE(result.data["secret"].is_null());
}

TEST_CASE("RBAC gate: gated field is denied when caller holds wrong role", "[rbac][gate]") {
    auto exec = make_rbac_executor();
    ResolverCtx ctx;
    ctx.roles = {std::string(Role::USER)};
    auto result = exec->execute("query { secret }", "{}", std::move(ctx));
    REQUIRE_FALSE(result.is_success());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
    REQUIRE(result.data["secret"].is_null());
}

TEST_CASE("RBAC gate: gated field is allowed when caller holds required role", "[rbac][gate]") {
    auto exec = make_rbac_executor();
    ResolverCtx ctx;
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};
    auto result = exec->execute("query { secret }", "{}", std::move(ctx));
    REQUIRE(result.is_success());
    REQUIRE(result.data["secret"] == "classified");
}

TEST_CASE("RBAC gate: OR logic — any matching role is sufficient", "[rbac][gate]") {
    auto exec = std::make_unique<GqlExecutor>(std::make_shared<DatabaseManager>());
    std::ignore = exec->load_schema("type Query { admin_or_tenant: String }");
    exec->register_resolver({}, "admin_or_tenant",
        [](const nlohmann::json&, const nlohmann::json&, const ResolverCtx&) -> nlohmann::json {
            return "ok";
        });
    exec->require_roles("admin_or_tenant",
                        {std::string(Role::PLATFORM_ADMIN), std::string(Role::TENANT_ADMIN)});

    SECTION("tenant_admin is allowed") {
        ResolverCtx ctx;
        ctx.roles = {std::string(Role::TENANT_ADMIN)};
        auto result = exec->execute("query { admin_or_tenant }", "{}", std::move(ctx));
        REQUIRE(result.is_success());
        REQUIRE(result.data["admin_or_tenant"] == "ok");
    }
    SECTION("platform_admin is allowed") {
        ResolverCtx ctx;
        ctx.roles = {std::string(Role::PLATFORM_ADMIN)};
        auto result = exec->execute("query { admin_or_tenant }", "{}", std::move(ctx));
        REQUIRE(result.is_success());
        REQUIRE(result.data["admin_or_tenant"] == "ok");
    }
    SECTION("service is denied") {
        ResolverCtx ctx;
        ctx.roles = {std::string(Role::SERVICE)};
        auto result = exec->execute("query { admin_or_tenant }", "{}", std::move(ctx));
        REQUIRE_FALSE(result.is_success());
        REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
    }
}

TEST_CASE("RBAC gate: roles are propagated to the resolver via context", "[rbac][gate]") {
    auto exec = std::make_unique<GqlExecutor>(std::make_shared<DatabaseManager>());
    std::ignore = exec->load_schema("type Query { whoami: String }");

    std::vector<std::string> captured_roles;
    exec->register_resolver({}, "whoami",
        [&captured_roles](const nlohmann::json&, const nlohmann::json&, const ResolverCtx& ctx) -> nlohmann::json {
            captured_roles = ctx.roles;
            return ctx.current_user_id;
        });

    ResolverCtx ctx;
    ctx.current_user_id = "alice";
    ctx.roles = {std::string(Role::TENANT_ADMIN), std::string(Role::USER)};
    auto result = exec->execute("query { whoami }", "{}", std::move(ctx));

    REQUIRE(result.is_success());
    REQUIRE(result.data["whoami"] == "alice");
    REQUIRE(captured_roles.size() == 2);
    REQUIRE(captured_roles[0] == std::string(Role::TENANT_ADMIN));
    REQUIRE(captured_roles[1] == std::string(Role::USER));
}

TEST_CASE("RBAC gate: removing role gate (empty roles) re-enables open access", "[rbac][gate]") {
    auto exec = std::make_unique<GqlExecutor>(std::make_shared<DatabaseManager>());
    std::ignore = exec->load_schema("type Query { guarded: String }");
    exec->register_resolver({}, "guarded",
        [](const nlohmann::json&, const nlohmann::json&, const ResolverCtx&) -> nlohmann::json {
            return "value";
        });
    exec->require_roles("guarded", {std::string(Role::PLATFORM_ADMIN)});

    // Confirm it is gated
    auto denied = exec->execute("query { guarded }", "{}", ResolverCtx{});
    REQUIRE_FALSE(denied.is_success());

    // Remove the gate
    exec->require_roles("guarded", {});

    // Now anonymous should succeed
    auto allowed = exec->execute("query { guarded }", "{}", ResolverCtx{});
    REQUIRE(allowed.is_success());
    REQUIRE(allowed.data["guarded"] == "value");
}

// =============================================================================
// T047-017: Platform role CRUD — DatabaseManager down to isched_system.db
// =============================================================================

namespace {
    // Helper: return a DatabaseManager with ensure_system_db() already called.
    // The system DB is stored at <DataHome>/isched/isched_system.db and is
    // idempotent, so repeated test runs are safe.
    std::shared_ptr<DatabaseManager> make_system_db_manager() {
        auto db = std::make_shared<DatabaseManager>();
        auto res = db->ensure_system_db();
        REQUIRE(res); // ensure setup succeeded
        return db;
    }

    // Unique test-role ID — long enough to never collide with production roles.
    constexpr std::string_view kTestRoleId = "role_test_rbac_unit_t047017";

    // Best-effort cleanup helper (called before and after tests that create roles).
    void try_delete_test_role(DatabaseManager& db) {
        (void)db.delete_platform_role(std::string(kTestRoleId));
    }
} // anonymous namespace

TEST_CASE("DatabaseManager::create_platform_role inserts a new custom role", "[rbac][db]") {
    auto db = make_system_db_manager();
    try_delete_test_role(*db); // clean pre-existing leftovers

    auto res = db->create_platform_role(
        std::string(kTestRoleId), "Test RBAC Role", "Created by unit test T047-017");
    REQUIRE(res);

    // Cleanup
    try_delete_test_role(*db);
}

TEST_CASE("DatabaseManager::create_platform_role returns DuplicateKey on second insert", "[rbac][db]") {
    auto db = make_system_db_manager();
    try_delete_test_role(*db);

    REQUIRE(db->create_platform_role(std::string(kTestRoleId), "Test Role", ""));
    auto dup = db->create_platform_role(std::string(kTestRoleId), "Test Role", "");
    REQUIRE_FALSE(dup);
    REQUIRE(dup.error() == DatabaseError::DuplicateKey);

    try_delete_test_role(*db);
}

TEST_CASE("DatabaseManager::delete_platform_role removes an existing custom role", "[rbac][db]") {
    auto db = make_system_db_manager();
    try_delete_test_role(*db);
    REQUIRE(db->create_platform_role(std::string(kTestRoleId), "Test Role", "delete test"));

    auto del = db->delete_platform_role(std::string(kTestRoleId));
    REQUIRE(del);

    // Second delete should return NotFound
    auto del2 = db->delete_platform_role(std::string(kTestRoleId));
    REQUIRE_FALSE(del2);
    REQUIRE(del2.error() == DatabaseError::NotFound);
}

TEST_CASE("DatabaseManager::delete_platform_role returns NotFound for unknown id", "[rbac][db]") {
    auto db = make_system_db_manager();
    // Make sure the test role does not exist
    try_delete_test_role(*db);

    auto res = db->delete_platform_role(std::string(kTestRoleId));
    REQUIRE_FALSE(res);
    REQUIRE(res.error() == DatabaseError::NotFound);
}

TEST_CASE("DatabaseManager::delete_platform_role returns AccessDenied for built-in roles", "[rbac][db]") {
    auto db = make_system_db_manager();

    SECTION("role_platform_admin is protected") {
        auto res = db->delete_platform_role("role_platform_admin");
        REQUIRE_FALSE(res);
        REQUIRE(res.error() == DatabaseError::AccessDenied);
    }
    SECTION("role_tenant_admin is protected") {
        auto res = db->delete_platform_role("role_tenant_admin");
        REQUIRE_FALSE(res);
        REQUIRE(res.error() == DatabaseError::AccessDenied);
    }
    SECTION("role_user is protected") {
        auto res = db->delete_platform_role("role_user");
        REQUIRE_FALSE(res);
        REQUIRE(res.error() == DatabaseError::AccessDenied);
    }
    SECTION("role_service is protected") {
        auto res = db->delete_platform_role("role_service");
        REQUIRE_FALSE(res);
        REQUIRE(res.error() == DatabaseError::AccessDenied);
    }
}

// =============================================================================
// T047-017: Full stack — GqlExecutor → DatabaseManager → isched_system.db
// =============================================================================

namespace {
    // Unique test role ID for E2E tests (distinct from DB-only tests above)
    constexpr std::string_view kE2eRoleId = "role_test_rbac_e2e_t047017";
} // anonymous namespace

TEST_CASE("createRole mutation: anonymous caller is denied by RBAC gate", "[rbac][e2e]") {
    auto exec = std::make_unique<GqlExecutor>(make_system_db_manager());
    // No auth context → empty roles
    auto result = exec->execute(
        R"(mutation { createRole(input: {id: "role_x", name: "X", scope: "platform"}) })",
        "{}",
        ResolverCtx{});
    REQUIRE_FALSE(result.is_success());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
}

TEST_CASE("createRole mutation: tenant_admin caller is allowed", "[rbac][e2e]") {
    auto db = make_system_db_manager();
    // Pre-clean
    (void)db->delete_platform_role(std::string(kE2eRoleId));

    auto exec = std::make_unique<GqlExecutor>(db);

    ResolverCtx ctx;
    ctx.current_user_id = "test-user";
    ctx.roles = {std::string(Role::TENANT_ADMIN)};

    auto result = exec->execute(
        R"(mutation { createRole(input: {id: ")" + std::string(kE2eRoleId) +
            R"(", name: "E2E Test Role", scope: "platform"}) })",
        "{}",
        std::move(ctx));

    REQUIRE(result.is_success());
    REQUIRE(result.data["createRole"] == true);

    // Post-clean
    (void)db->delete_platform_role(std::string(kE2eRoleId));
}

TEST_CASE("createRole mutation: platform_admin caller is allowed", "[rbac][e2e]") {
    auto db = make_system_db_manager();
    (void)db->delete_platform_role(std::string(kE2eRoleId));

    auto exec = std::make_unique<GqlExecutor>(db);

    ResolverCtx ctx;
    ctx.current_user_id = "admin-user";
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};

    auto result = exec->execute(
        R"(mutation { createRole(input: {id: ")" + std::string(kE2eRoleId) +
            R"(", name: "E2E Test Role", scope: "platform"}) })",
        "{}",
        std::move(ctx));

    REQUIRE(result.is_success());
    REQUIRE(result.data["createRole"] == true);

    // Post-clean
    (void)db->delete_platform_role(std::string(kE2eRoleId));
}

TEST_CASE("createRole mutation: duplicate id returns error response", "[rbac][e2e]") {
    auto db = make_system_db_manager();
    (void)db->delete_platform_role(std::string(kE2eRoleId));
    REQUIRE(db->create_platform_role(std::string(kE2eRoleId), "Pre-existing", ""));

    auto exec = std::make_unique<GqlExecutor>(db);

    ResolverCtx ctx;
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};

    auto result = exec->execute(
        R"(mutation { createRole(input: {id: ")" + std::string(kE2eRoleId) +
            R"(", name: "Attempt Dup", scope: "platform"}) })",
        "{}",
        std::move(ctx));

    // Resolver throws → executor captures it as an error, does not crash
    REQUIRE_FALSE(result.is_success());
    REQUIRE_FALSE(result.errors.empty());

    (void)db->delete_platform_role(std::string(kE2eRoleId));
}

TEST_CASE("deleteRole mutation: anonymous caller is denied by RBAC gate", "[rbac][e2e]") {
    auto exec = std::make_unique<GqlExecutor>(make_system_db_manager());
    auto result = exec->execute(
        R"(mutation { deleteRole(id: "role_test_ghost") })",
        "{}",
        ResolverCtx{});
    REQUIRE_FALSE(result.is_success());
    REQUIRE(result.errors[0].code == EErrorCodes::FORBIDDEN);
}

TEST_CASE("deleteRole mutation: platform_admin can delete a custom role", "[rbac][e2e]") {
    auto db = make_system_db_manager();
    (void)db->delete_platform_role(std::string(kE2eRoleId));
    REQUIRE(db->create_platform_role(std::string(kE2eRoleId), "To Be Deleted", ""));

    auto exec = std::make_unique<GqlExecutor>(db);

    ResolverCtx ctx;
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};

    auto result = exec->execute(
        R"(mutation { deleteRole(id: ")" + std::string(kE2eRoleId) + R"(") })",
        "{}",
        std::move(ctx));

    REQUIRE(result.is_success());
    REQUIRE(result.data["deleteRole"] == true);
}

TEST_CASE("deleteRole mutation: deleting a built-in role returns error response", "[rbac][e2e]") {
    auto exec = std::make_unique<GqlExecutor>(make_system_db_manager());

    ResolverCtx ctx;
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};

    auto result = exec->execute(
        R"(mutation { deleteRole(id: "role_platform_admin") })",
        "{}",
        std::move(ctx));

    // Resolver throws "cannot be deleted" → executor captures as error
    REQUIRE_FALSE(result.is_success());
    REQUIRE_FALSE(result.errors.empty());
}

TEST_CASE("deleteRole mutation: deleting non-existent role returns error response", "[rbac][e2e]") {
    auto db = make_system_db_manager();
    // Ensure the role does not exist
    (void)db->delete_platform_role(std::string(kE2eRoleId));

    auto exec = std::make_unique<GqlExecutor>(db);

    ResolverCtx ctx;
    ctx.roles = {std::string(Role::PLATFORM_ADMIN)};

    auto result = exec->execute(
        R"(mutation { deleteRole(id: ")" + std::string(kE2eRoleId) + R"(") })",
        "{}",
        std::move(ctx));

    REQUIRE_FALSE(result.is_success());
    REQUIRE_FALSE(result.errors.empty());
}

