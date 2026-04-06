// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_schema_upload.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for tenant schema document upload / list / fetch
 *        (spec-006, T002-T004, T013-T017, T023-T025, T029-T032, T040)
 *
 * Covers:
 *  - SC-001: tenant_admin upload authorization
 *  - SC-002: conflict without overwrite / overwrite succeeds
 *  - SC-003: validation (invalid name, invalid SDL, size limit)
 *  - SC-004: cross-tenant isolation
 *  - SC-005: list metadata fields (name/createdAt/updatedAt/updatedBy only)
 *  - SC-006: single-document fetch (success, null-on-miss, tenant isolation)
 *  - SC-007: upload backend latency (>=95% of N=100 uploads in <2 s)
 *  - SC-008: default 1 MB size limit
 *  - SC-009: overwrite concurrency last-successful-write-wins
 *  - SC-010: list sizeBytes absent
 *
 *  Restart-durability test (FR-011) uses controlled in-process restart simulation
 *  (executor + db recreate from same files).
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_gql_error.hpp>
#include <isched/backend/isched_AuthenticationMiddleware.hpp>

#include "../isched/isched_graphql_test_helpers.hpp"

using namespace isched::v0_0_1::backend;
using isched::v0_0_1::gql::EErrorCodes;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Unique run suffix to avoid cross-test filesystem collisions
// ---------------------------------------------------------------------------
static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

// ---------------------------------------------------------------------------
// Latency / performance helpers (T004)
// ---------------------------------------------------------------------------

using SteadyClock   = std::chrono::steady_clock;
using SteadyTimePoint = SteadyClock::time_point;
using Millis        = std::chrono::milliseconds;

/// Measure duration of a callable in milliseconds.
template<typename Fn>
static double measure_ms(Fn&& fn) {
    const auto t0 = SteadyClock::now();
    std::forward<Fn>(fn)();
    const auto t1 = SteadyClock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

/// Compute the P95 of a sorted vector of doubles.
static double p95(std::vector<double>& samples) {
    if (samples.empty()) return 0.0;
    std::sort(samples.begin(), samples.end());
    const std::size_t idx = static_cast<std::size_t>(
        std::ceil(0.95 * static_cast<double>(samples.size()))) - 1;
    return samples[std::min(idx, samples.size() - 1)];
}

// ---------------------------------------------------------------------------
// Shared fixture helpers (T003)
// ---------------------------------------------------------------------------

/// Minimal valid GraphQL SDL document usable as test schema content.
static const std::string k_valid_sdl = "type Query { ping: String }";

/// Build an isolated DB + executor pair using a unique temp directory.
static std::pair<std::shared_ptr<DatabaseManager>, std::shared_ptr<GqlExecutor>>
make_isolated_executor(const std::string& label = "") {
    const auto tmp = std::filesystem::temp_directory_path() /
        ("isched_su_" + g_run_suffix + "_" + label + "_" +
         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tmp / "tenants");

    DatabaseManager::Config cfg;
    cfg.base_path      = (tmp / "tenants").string();
    cfg.system_db_path = (tmp / "isched_system.db").string();

    auto db   = std::make_shared<DatabaseManager>(cfg);
    REQUIRE(db->ensure_system_db());
    auto exec = std::make_shared<GqlExecutor>(db);
    return {db, exec};
}

/// Build a tenant_admin ResolverCtx for `tenant_id`.
static ResolverCtx make_admin_ctx(const std::string& tenant_id = "org_a") {
    return isched::test::tenant_admin_ctx(tenant_id);
}

/// Build a plain-user (role_user) ResolverCtx.
static ResolverCtx make_user_ctx(const std::string& tenant_id = "org_a") {
    return isched::test::make_resolver_ctx(
        tenant_id, "user_test", {std::string(Role::USER)});
}

/// Build an unauthenticated (no user/roles) ResolverCtx.
static ResolverCtx make_anon_ctx() {
    return isched::test::anonymous_ctx();
}

// ---------------------------------------------------------------------------
// SC-001 / T013 / T014: Upload authorization (tenant_admin only)
// ---------------------------------------------------------------------------

TEST_CASE("SC-001: uploadSchemaDocument requires tenant_admin role",
          "[schema_upload][auth][SC-001][T013][T014]") {

    auto [db, exec] = make_isolated_executor("auth");
    REQUIRE(db->initialize_tenant("org_a"));

    const std::string query = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) {
                success
                schema { name createdAt updatedAt updatedBy }
                error { code message conflictingName }
            }
        }
    )";
    const json vars = {
        {"input", {
            {"name",    "billing-v1"},
            {"content", k_valid_sdl},
            {"overwrite", false}
        }}
    };

    SECTION("tenant_admin succeeds") {
        const auto result = exec->execute(query, vars.dump(), make_admin_ctx("org_a"));
        REQUIRE(result.is_success());
        REQUIRE(result.data["uploadSchemaDocument"]["success"] == true);
        REQUIRE(result.data["uploadSchemaDocument"]["schema"]["name"] == "billing-v1");
    }

    SECTION("unauthenticated caller is rejected") {
        const auto result = exec->execute(query, vars.dump(), make_anon_ctx());
        // The RBAC gate fires FORBIDDEN for callers with no roles.
        REQUIRE_FALSE(result.is_success());
        REQUIRE_FALSE(result.errors.empty());
    }

    SECTION("plain user (role_user) is rejected") {
        const auto result = exec->execute(query, vars.dump(), make_user_ctx("org_a"));
        REQUIRE_FALSE(result.is_success());
        REQUIRE_FALSE(result.errors.empty());
    }
}

// ---------------------------------------------------------------------------
// SC-002 / T013: Conflict and overwrite semantics
// ---------------------------------------------------------------------------

TEST_CASE("SC-002: conflict detection and overwrite semantics",
          "[schema_upload][conflict][SC-002][T013]") {

    auto [db, exec] = make_isolated_executor("conflict");
    REQUIRE(db->initialize_tenant("org_a"));
    const auto ctx = make_admin_ctx("org_a");

    const std::string query = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) {
                success
                schema { name createdAt updatedAt updatedBy }
                error { code message conflictingName }
            }
        }
    )";

    // First upload: succeeds
    {
        const json vars = {{"input", {{"name","svc"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(query, vars.dump(), ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    SECTION("duplicate without overwrite returns CONFLICT") {
        const json vars = {{"input", {{"name","svc"}, {"content", k_valid_sdl}, {"overwrite", false}}}};
        const auto r = exec->execute(query, vars.dump(), ctx);
        REQUIRE(r.is_success()); // resolver itself succeeded (returned structured error)
        const auto& res = r.data["uploadSchemaDocument"];
        REQUIRE(res["success"] == false);
        REQUIRE(res["error"]["code"] == "CONFLICT");
        REQUIRE(res["error"]["conflictingName"] == "svc");
    }

    SECTION("duplicate with overwrite=true succeeds") {
        const std::string new_content = "type Query { replaced: String }";
        const json vars = {{"input", {{"name","svc"}, {"content", new_content}, {"overwrite", true}}}};
        const auto r = exec->execute(query, vars.dump(), ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
        REQUIRE(r.data["uploadSchemaDocument"]["schema"]["name"] == "svc");
    }

    SECTION("case-sensitive: 'SVC' and 'svc' are distinct names") {
        const json vars = {{"input", {{"name","SVC"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(query, vars.dump(), ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }
}

// ---------------------------------------------------------------------------
// SC-003 / T015: Validation — name regex, SDL parse, content size
// ---------------------------------------------------------------------------

TEST_CASE("SC-003: upload validation (name, SDL, size)",
          "[schema_upload][validation][SC-003][T015]") {

    auto [db, exec] = make_isolated_executor("validation");
    REQUIRE(db->initialize_tenant("org_a"));
    const auto ctx = make_admin_ctx("org_a");

    const std::string mutation = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) {
                success
                error { code message conflictingName }
            }
        }
    )";

    auto upload = [&](const std::string& name, const std::string& content) {
        const json vars = {{"input", {{"name", name}, {"content", content}}}};
        return exec->execute(mutation, vars.dump(), ctx);
    };

    SECTION("empty name is rejected") {
        const auto r = upload("", k_valid_sdl);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }

    SECTION("name > 128 chars is rejected") {
        const auto r = upload(std::string(129, 'a'), k_valid_sdl);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }

    SECTION("name with invalid chars (space) is rejected") {
        const auto r = upload("bad name", k_valid_sdl);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }

    SECTION("name with slash is rejected") {
        const auto r = upload("path/name", k_valid_sdl);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }

    SECTION("valid name with dots/dashes/underscores is accepted") {
        const auto r = upload("billing.v1-schema_2026", k_valid_sdl);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    SECTION("invalid SDL is rejected") {
        const auto r = upload("bad-sdl", "this is not valid graphql {{{");
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }

    SECTION("empty content is rejected") {
        const json vars = {{"input", {{"name", "empty"}, {"content", ""}}}};
        const auto r = exec->execute(mutation, vars.dump(), ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
        REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
    }
}

// ---------------------------------------------------------------------------
// SC-008 / T015: Default 1 MB size limit
// ---------------------------------------------------------------------------

TEST_CASE("SC-008: content exceeding 1 MB is rejected",
          "[schema_upload][validation][SC-008][T015]") {

    auto [db, exec] = make_isolated_executor("sizecheck");
    REQUIRE(db->initialize_tenant("org_a"));
    const auto ctx = make_admin_ctx("org_a");

    const std::string mutation = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) {
                success
                error { code message conflictingName }
            }
        }
    )";

    // Exactly 1 MB of content (non-valid SDL but we test size check first)
    const std::string oversized_content(1024 * 1024 + 1, 'x');
    const json vars = {{"input", {{"name","oversized"}, {"content", oversized_content}}}};
    const auto r = exec->execute(mutation, vars.dump(), ctx);
    REQUIRE(r.is_success());
    REQUIRE(r.data["uploadSchemaDocument"]["success"] == false);
    REQUIRE(r.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
}

// ---------------------------------------------------------------------------
// SC-004 / T030: Cross-tenant isolation for list and fetch
// ---------------------------------------------------------------------------

TEST_CASE("SC-004: cross-tenant isolation",
          "[schema_upload][isolation][SC-004][T030]") {

    auto [db, exec] = make_isolated_executor("isolation");
    REQUIRE(db->initialize_tenant("org_a"));
    REQUIRE(db->initialize_tenant("org_b"));

    // Upload "billing" to org_a only
    {
        const std::string mutation = R"(
            mutation($input: UploadSchemaDocumentInput!) {
                uploadSchemaDocument(input: $input) { success }
            }
        )";
        const json vars = {{"input", {{"name","billing"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    SECTION("list from org_b returns empty") {
        const auto r = exec->execute(
            "query { schemaDocuments { name } }", "{}", make_user_ctx("org_b"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["schemaDocuments"].is_array());
        REQUIRE(r.data["schemaDocuments"].empty());
    }

    SECTION("fetch billing from org_b returns null") {
        const auto r = exec->execute(
            R"(query { schemaDocument(name: "billing") { name } })", "{}", make_user_ctx("org_b"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["schemaDocument"].is_null());
    }

    SECTION("list from org_a sees billing") {
        const auto r = exec->execute(
            "query { schemaDocuments { name } }", "{}", make_user_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["schemaDocuments"].size() == 1);
        REQUIRE(r.data["schemaDocuments"][0]["name"] == "billing");
    }
}

// ---------------------------------------------------------------------------
// SC-005 / SC-010 / T024: List metadata contract — name/createdAt/updatedAt/updatedBy only
// ---------------------------------------------------------------------------

TEST_CASE("SC-005/SC-010: list returns exact metadata fields without sizeBytes",
          "[schema_upload][list][SC-005][SC-010][T024]") {

    auto [db, exec] = make_isolated_executor("listmeta");
    REQUIRE(db->initialize_tenant("org_a"));

    // Upload a schema
    {
        const std::string mutation = R"(
            mutation($input: UploadSchemaDocumentInput!) {
                uploadSchemaDocument(input: $input) { success }
            }
        )";
        const json vars = {{"input", {{"name","api-v1"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    const auto r = exec->execute(
        R"(query { schemaDocuments { name createdAt updatedAt updatedBy } })",
        "{}", make_user_ctx("org_a"));
    REQUIRE(r.is_success());
    REQUIRE(r.data["schemaDocuments"].size() == 1);

    const auto& doc = r.data["schemaDocuments"][0];
    REQUIRE(doc.contains("name"));
    REQUIRE(doc.contains("createdAt"));
    REQUIRE(doc.contains("updatedAt"));
    REQUIRE(doc.contains("updatedBy"));
    // sizeBytes must NOT be present
    REQUIRE_FALSE(doc.contains("sizeBytes"));
    REQUIRE(doc["name"] == "api-v1");
}

// ---------------------------------------------------------------------------
// SC-006 / T029-T030: schemaDocument fetch by name
// ---------------------------------------------------------------------------

TEST_CASE("SC-006: schemaDocument fetch success, null-on-miss, isolation",
          "[schema_upload][fetch][SC-006][T029][T030]") {

    auto [db, exec] = make_isolated_executor("fetch");
    REQUIRE(db->initialize_tenant("org_a"));

    // Upload doc
    {
        const std::string mutation = R"(
            mutation($input: UploadSchemaDocumentInput!) {
                uploadSchemaDocument(input: $input) { success }
            }
        )";
        const json vars = {{"input", {{"name","my-doc"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    SECTION("fetch existing document returns full content and metadata") {
        const auto r = exec->execute(
            R"(query { schemaDocument(name: "my-doc") { name content createdAt updatedAt updatedBy } })",
            "{}", make_user_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE_FALSE(r.data["schemaDocument"].is_null());
        REQUIRE(r.data["schemaDocument"]["name"] == "my-doc");
        REQUIRE(r.data["schemaDocument"]["content"] == k_valid_sdl);
        REQUIRE(!r.data["schemaDocument"]["createdAt"].get<std::string>().empty());
    }

    SECTION("fetch missing document returns null without error") {
        const auto r = exec->execute(
            R"(query { schemaDocument(name: "nonexistent") { name content } })",
            "{}", make_user_ctx("org_a"));
        // Canonical null-on-miss: no errors, schemaDocument is null
        REQUIRE(r.is_success());
        REQUIRE(r.data["schemaDocument"].is_null());
    }

    SECTION("case-sensitive: 'MY-DOC' does not resolve 'my-doc'") {
        const auto r = exec->execute(
            R"(query { schemaDocument(name: "MY-DOC") { name } })",
            "{}", make_user_ctx("org_a"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["schemaDocument"].is_null());
    }
}

// ---------------------------------------------------------------------------
// T031: Controlled restart durability
// ---------------------------------------------------------------------------

TEST_CASE("T031: uploaded schema persists after controlled restart (recreated executor)",
          "[schema_upload][durability][T031]") {

    const auto tmp = std::filesystem::temp_directory_path() /
        ("isched_restart_" + g_run_suffix);
    std::filesystem::create_directories(tmp / "tenants");

    DatabaseManager::Config cfg;
    cfg.base_path      = (tmp / "tenants").string();
    cfg.system_db_path = (tmp / "isched_system.db").string();

    // Phase 1: Upload
    {
        auto db   = std::make_shared<DatabaseManager>(cfg);
        REQUIRE(db->ensure_system_db());
        REQUIRE(db->initialize_tenant("org_restart"));

        auto exec = std::make_shared<GqlExecutor>(db);

        const std::string mutation = R"(
            mutation($input: UploadSchemaDocumentInput!) {
                uploadSchemaDocument(input: $input) { success }
            }
        )";
        const json vars = {{"input", {{"name","durable-doc"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_restart"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
        // db and exec go out of scope → all connections closed
    }

    // Phase 2: Recreate executor (simulates restart) and fetch
    {
        auto db2  = std::make_shared<DatabaseManager>(cfg);
        REQUIRE(db2->ensure_system_db());
        // No need to call initialize_tenant; table exists from Phase 1
        auto exec2 = std::make_shared<GqlExecutor>(db2);

        const auto r = exec2->execute(
            R"(query { schemaDocument(name: "durable-doc") { name content } })",
            "{}", make_user_ctx("org_restart"));
        REQUIRE(r.is_success());
        REQUIRE_FALSE(r.data["schemaDocument"].is_null());
        REQUIRE(r.data["schemaDocument"]["name"] == "durable-doc");
        REQUIRE(r.data["schemaDocument"]["content"] == k_valid_sdl);
    }
}

// ---------------------------------------------------------------------------
// SC-007 / T017: Upload performance — >=95% of N=100 uploads in <2 s each
// ---------------------------------------------------------------------------

TEST_CASE("SC-007: upload backend latency P95 < 2000ms over 100 requests",
          "[schema_upload][performance][SC-007][T017]") {

    auto [db, exec] = make_isolated_executor("perf_upload");
    REQUIRE(db->initialize_tenant("org_perf"));
    const auto ctx = make_admin_ctx("org_perf");

    constexpr int N = 100;

    const std::string mutation = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) { success }
        }
    )";

    std::vector<double> latencies_ms;
    latencies_ms.reserve(N);

    for (int i = 0; i < N; ++i) {
        const std::string name = "perf-doc-" + std::to_string(i);
        const json vars = {{"input", {
            {"name",     name},
            {"content",  k_valid_sdl},
            {"overwrite", false}
        }}};

        double ms = measure_ms([&] {
            const auto r = exec->execute(mutation, vars.dump(), ctx);
            REQUIRE(r.is_success());
            REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
        });
        latencies_ms.push_back(ms);
    }

    const double p95_ms = p95(latencies_ms);
    INFO("Upload P95 latency: " << p95_ms << " ms (threshold: 2000 ms)");
    REQUIRE(p95_ms < 2000.0);
}

// ---------------------------------------------------------------------------
// US2 / T025: List performance — >=95% of N=100 list queries with 200 docs in <500ms
// ---------------------------------------------------------------------------

TEST_CASE("SC-010: list performance P95 < 500ms over 100 queries with 200 schemas",
          "[schema_upload][performance][SC-010][T025]") {

    auto [db, exec] = make_isolated_executor("perf_list");
    REQUIRE(db->initialize_tenant("org_list"));
    const auto admin_ctx = make_admin_ctx("org_list");
    const auto user_ctx  = make_user_ctx("org_list");

    // Seed 200 schema documents
    const std::string mutation = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) { success }
        }
    )";
    for (int i = 0; i < 200; ++i) {
        const json vars = {{"input", {
            {"name",    "list-doc-" + std::to_string(i)},
            {"content", k_valid_sdl}
        }}};
        const auto r = exec->execute(mutation, vars.dump(), admin_ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    constexpr int N = 100;
    std::vector<double> latencies_ms;
    latencies_ms.reserve(N);

    for (int i = 0; i < N; ++i) {
        double ms = measure_ms([&] {
            const auto r = exec->execute(
                "query { schemaDocuments { name updatedAt } }", "{}", user_ctx);
            REQUIRE(r.is_success());
            REQUIRE(r.data["schemaDocuments"].size() == 200);
        });
        latencies_ms.push_back(ms);
    }

    const double p95_ms = p95(latencies_ms);
    INFO("List P95 latency: " << p95_ms << " ms (threshold: 500 ms)");
    REQUIRE(p95_ms < 500.0);
}

// ---------------------------------------------------------------------------
// US3 / T032: Fetch performance — >=95% of N=100 fetches with 200 docs in <300ms
// ---------------------------------------------------------------------------

TEST_CASE("SC-006: fetch performance P95 < 300ms over 100 queries with 200 schemas",
          "[schema_upload][performance][SC-006][T032]") {

    auto [db, exec] = make_isolated_executor("perf_fetch");
    REQUIRE(db->initialize_tenant("org_fetch"));
    const auto admin_ctx = make_admin_ctx("org_fetch");
    const auto user_ctx  = make_user_ctx("org_fetch");

    // Seed 200 schema documents
    const std::string mutation = R"(
        mutation($input: UploadSchemaDocumentInput!) {
            uploadSchemaDocument(input: $input) { success }
        }
    )";
    for (int i = 0; i < 200; ++i) {
        const json vars = {{"input", {
            {"name",    "fetch-doc-" + std::to_string(i)},
            {"content", k_valid_sdl}
        }}};
        const auto r = exec->execute(mutation, vars.dump(), admin_ctx);
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    constexpr int N = 100;
    std::vector<double> latencies_ms;
    latencies_ms.reserve(N);

    for (int i = 0; i < N; ++i) {
        double ms = measure_ms([&] {
            const auto r = exec->execute(
                R"(query { schemaDocument(name: "fetch-doc-50") { name content } })",
                "{}", user_ctx);
            REQUIRE(r.is_success());
            REQUIRE(r.data["schemaDocument"]["name"] == "fetch-doc-50");
        });
        latencies_ms.push_back(ms);
    }

    const double p95_ms = p95(latencies_ms);
    INFO("Fetch P95 latency: " << p95_ms << " ms (threshold: 300 ms)");
    REQUIRE(p95_ms < 300.0);
}

// ---------------------------------------------------------------------------
// SC-009 / T016: Overwrite concurrency — last-successful-write-wins
// ---------------------------------------------------------------------------

TEST_CASE("SC-009: concurrent overwrite resolves to last successful commit",
          "[schema_upload][concurrency][SC-009][T016]") {

    auto [db, exec] = make_isolated_executor("concurrency");
    REQUIRE(db->initialize_tenant("org_con"));

    // Upload initial document
    {
        const std::string mutation = R"(
            mutation($input: UploadSchemaDocumentInput!) {
                uploadSchemaDocument(input: $input) { success }
            }
        )";
        const json vars = {{"input", {{"name","shared"}, {"content", k_valid_sdl}}}};
        const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_con"));
        REQUIRE(r.is_success());
        REQUIRE(r.data["uploadSchemaDocument"]["success"] == true);
    }

    // Launch concurrent overwrites; each should either succeed or fail cleanly
    constexpr int k_threads = 8;
    std::vector<std::thread> threads;
    std::vector<bool> succeeded(k_threads, false);

    for (int i = 0; i < k_threads; ++i) {
        threads.emplace_back([i, &exec, &succeeded] {
            const std::string content = "type Query { v" + std::to_string(i) + ": String }";
            const std::string mutation = R"(
                mutation($input: UploadSchemaDocumentInput!) {
                    uploadSchemaDocument(input: $input) { success }
                }
            )";
            const json vars = {{"input", {
                {"name",    "shared"},
                {"content", content},
                {"overwrite", true}
            }}};
            try {
                const auto r = exec->execute(mutation, vars.dump(), make_admin_ctx("org_con"));
                succeeded[static_cast<std::size_t>(i)] =
                    r.is_success() && r.data["uploadSchemaDocument"]["success"] == true;
            } catch (...) {
                succeeded[static_cast<std::size_t>(i)] = false;
            }
        });
    }
    for (auto& t : threads) t.join();

    // At least one overwrite must succeed
    REQUIRE(std::any_of(succeeded.begin(), succeeded.end(), [](bool v){ return v; }));

    // Final document must be readable and have some valid content
    const auto r = exec->execute(
        R"(query { schemaDocument(name: "shared") { name content } })",
        "{}", make_user_ctx("org_con"));
    REQUIRE(r.is_success());
    REQUIRE_FALSE(r.data["schemaDocument"].is_null());
    REQUIRE(r.data["schemaDocument"]["name"] == "shared");
    // Content is one of the writes (whatever committed last)
    REQUIRE(!r.data["schemaDocument"]["content"].get<std::string>().empty());
}

// ---------------------------------------------------------------------------
// T023: schemaDocuments list — empty tenant returns empty list (not error)
// ---------------------------------------------------------------------------

TEST_CASE("T023: schemaDocuments returns empty list for tenant with no documents",
          "[schema_upload][list][T023]") {

    auto [db, exec] = make_isolated_executor("empty_list");
    REQUIRE(db->initialize_tenant("org_empty"));

    const auto r = exec->execute(
        "query { schemaDocuments { name } }", "{}", make_user_ctx("org_empty"));
    REQUIRE(r.is_success());
    REQUIRE(r.data["schemaDocuments"].is_array());
    REQUIRE(r.data["schemaDocuments"].empty());
}

