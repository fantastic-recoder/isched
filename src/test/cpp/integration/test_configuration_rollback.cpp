// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_configuration_rollback.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for configuration rollback (T028 — User Story 2)
 *
 * Verifies that rollbackConfiguration restores the previous active snapshot and
 * handles edge cases such as no prior snapshot gracefully.
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;

/// Unique suffix per process run to isolate test tenants in the shared SQLite file
static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

class RollbackTestFixture {
public:
    std::unique_ptr<Server> server;

    RollbackTestFixture() {
        Server::Configuration config;
        config.port = 18085;
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        server = Server::create(config);
    }

    ~RollbackTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    nlohmann::json post_graphql(const std::string& query) {
        httplib::Client client("localhost", 18085);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        nlohmann::json body = {{"query", query}};
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        return nlohmann::json::parse(res->body);
    }

    std::string apply_snapshot(const std::string& tenant_id,
                               const std::string& schema_sdl,
                               const std::string& version = "1.0.0") {
        auto resp = post_graphql(
            "mutation { applyConfiguration(input: {"
            "tenantId: \"" + tenant_id + "\" "
            "schemaSdl: \"" + schema_sdl + "\" "
            "version: \"" + version + "\""
            "}) { success snapshotId } }"
        );
        REQUIRE(resp["data"]["applyConfiguration"]["success"] == true);
        return resp["data"]["applyConfiguration"]["snapshotId"].get<std::string>();
    }

    void activate(const std::string& snap_id) {
        auto r = post_graphql(
            "mutation { activateSnapshot(id: \"" + snap_id + "\") { success } }"
        );
        REQUIRE(r["data"]["activateSnapshot"]["success"] == true);
    }

    std::string active_id(const std::string& tenant_id) {
        auto resp = post_graphql(
            "{ activeConfiguration(tenantId: \"" + tenant_id + "\") { id } }"
        );
        if (resp["data"]["activeConfiguration"].is_null()) return "";
        return resp["data"]["activeConfiguration"]["id"].get<std::string>();
    }
};

TEST_CASE_METHOD(RollbackTestFixture,
    "rollbackConfiguration: single snapshot with no prior gives an error",
    "[integration][rollback][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "rback1-" + g_run_suffix;
    const std::string id_v1 = apply_snapshot(tenant, "type A { x: String }", "1.0.0");
    activate(id_v1);

    // Try to roll back — only one snapshot, no prior available
    auto resp = post_graphql(
        "mutation { rollbackConfiguration(tenantId: \"" + tenant + "\") {"
        " success snapshotId errors } }"
    );
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["rollbackConfiguration"]["success"] == false);
    REQUIRE_FALSE(resp["data"]["rollbackConfiguration"]["errors"].empty());

    server->stop();
}

TEST_CASE_METHOD(RollbackTestFixture,
    "rollbackConfiguration: rolls back to previous snapshot",
    "[integration][rollback][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "rback2-" + g_run_suffix;
    const std::string id_v1 = apply_snapshot(tenant, "type A { x: String }", "1.0.0");
    const std::string id_v2 = apply_snapshot(tenant, "type B { y: Int }",    "2.0.0");

    activate(id_v1);
    REQUIRE(active_id(tenant) == id_v1);

    activate(id_v2);
    REQUIRE(active_id(tenant) == id_v2);

    // Roll back — should return to v1
    auto resp = post_graphql(
        "mutation { rollbackConfiguration(tenantId: \"" + tenant + "\") {"
        " success snapshotId errors } }"
    );
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["rollbackConfiguration"]["success"] == true);

    // v1 should be active again
    REQUIRE(active_id(tenant) == id_v1);

    server->stop();
}

TEST_CASE_METHOD(RollbackTestFixture,
    "rollbackConfiguration: unknown tenant gives an error",
    "[integration][rollback][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "no-such-tenant-" + g_run_suffix;
    auto resp = post_graphql(
        "mutation { rollbackConfiguration(tenantId: \"" + tenant + "\") {"
        " success errors } }"
    );
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["rollbackConfiguration"]["success"] == false);

    server->stop();
}

TEST_CASE_METHOD(RollbackTestFixture,
    "rollbackConfiguration: three snapshots rolls back correctly",
    "[integration][rollback][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "rback3-" + g_run_suffix;
    const std::string id_v1 = apply_snapshot(tenant, "type V1 { a: String }", "1.0.0");
    const std::string id_v2 = apply_snapshot(tenant, "type V2 { b: Int }",    "2.0.0");
    const std::string id_v3 = apply_snapshot(tenant, "type V3 { c: Float }",  "3.0.0");

    activate(id_v1);
    activate(id_v2);
    activate(id_v3);
    REQUIRE(active_id(tenant) == id_v3);

    // Roll back — must land on something other than v3
    auto r = post_graphql(
        "mutation { rollbackConfiguration(tenantId: \"" + tenant + "\") { success snapshotId } }"
    );
    REQUIRE(r["data"]["rollbackConfiguration"]["success"] == true);
    REQUIRE(active_id(tenant) != id_v3);

    server->stop();
}
