// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_schema_activation.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for schema update after activateSnapshot mutation (T027 — User Story 2)
 *
 * Verifies that activating a snapshot marks it as active in the database and that
 * the activeConfiguration query reflects the change.
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;

class SchemaActivationFixture {
public:
    std::unique_ptr<Server> server;

    SchemaActivationFixture() {
        Server::Configuration config;
        config.port = 18084;
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        server = Server::create(config);
    }

    ~SchemaActivationFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    nlohmann::json post_graphql(const std::string& query,
                                const nlohmann::json& variables = nullptr) {
        httplib::Client client("localhost", 18084);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        nlohmann::json body = {{"query", query}};
        if (!variables.is_null()) {
            body["variables"] = variables;
        }
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        return nlohmann::json::parse(res->body);
    }

    /// Helper: create a snapshot and return its ID
    std::string apply_snapshot(const std::string& tenant_id,
                               const std::string& schema_sdl,
                               const std::string& version = "1.0.0") {
        std::string mut = "mutation { applyConfiguration(input: {"
                          "tenantId: \"" + tenant_id + "\" "
                          "schemaSdl: \"" + schema_sdl + "\" "
                          "version: \"" + version + "\""
                          "}) { success snapshotId errors } }";
        auto resp = post_graphql(mut);
        REQUIRE(resp["data"]["applyConfiguration"]["success"] == true);
        return resp["data"]["applyConfiguration"]["snapshotId"].get<std::string>();
    }
};

TEST_CASE_METHOD(SchemaActivationFixture,
    "activateSnapshot: marks snapshot as active",
    "[integration][activation][us2]")
{
    REQUIRE(server->start());

    const std::string snap_id = apply_snapshot(
        "tenant-act1",
        "type Product { id: String! price: Float }",
        "2.0.0"
    );

    // Activate it
    const std::string activate_mut = "mutation { activateSnapshot(id: \"" + snap_id + "\") {"
                                     " success snapshotId errors } }";
    auto act_resp = post_graphql(activate_mut);
    REQUIRE(act_resp.contains("data"));
    REQUIRE_FALSE(act_resp.contains("errors"));
    REQUIRE(act_resp["data"]["activateSnapshot"]["success"] == true);
    REQUIRE(act_resp["data"]["activateSnapshot"]["snapshotId"] == snap_id);

    server->stop();
}

TEST_CASE_METHOD(SchemaActivationFixture,
    "activeConfiguration: returns activated snapshot",
    "[integration][activation][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "tenant-act2";
    const std::string snap_id = apply_snapshot(
        tenant,
        "type Order { id: String! amount: Float }",
        "3.0.0"
    );

    // Activate
    post_graphql("mutation { activateSnapshot(id: \"" + snap_id + "\") { success } }");

    // Query active configuration
    auto resp = post_graphql(
        "{ activeConfiguration(tenantId: \"" + tenant + "\") {"
        "  id version isActive createdAt"
        "} }"
    );
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));

    const auto& active = resp["data"]["activeConfiguration"];
    REQUIRE_FALSE(active.is_null());
    REQUIRE(active["id"] == snap_id);
    REQUIRE(active["version"] == "3.0.0");
    REQUIRE(active["isActive"] == true);
    REQUIRE(active["createdAt"].is_string());

    server->stop();
}

TEST_CASE_METHOD(SchemaActivationFixture,
    "activateSnapshot: only one snapshot per tenant is active",
    "[integration][activation][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "tenant-act3";
    const std::string id_v1 = apply_snapshot(tenant, "type V1 { x: String }", "1.0.0");
    const std::string id_v2 = apply_snapshot(tenant, "type V2 { y: Int }",    "2.0.0");

    // Activate v1 first
    post_graphql("mutation { activateSnapshot(id: \"" + id_v1 + "\") { success } }");

    // Then activate v2
    post_graphql("mutation { activateSnapshot(id: \"" + id_v2 + "\") { success } }");

    // Only v2 should be active
    auto resp = post_graphql(
        "{ activeConfiguration(tenantId: \"" + tenant + "\") { id } }"
    );
    REQUIRE_FALSE(resp["data"]["activeConfiguration"].is_null());
    REQUIRE(resp["data"]["activeConfiguration"]["id"] == id_v2);

    // History should show v2 active, v1 inactive
    auto hist = post_graphql(
        "{ configurationHistory(tenantId: \"" + tenant + "\") { id isActive } }"
    );
    int active_count = 0;
    for (const auto& entry : hist["data"]["configurationHistory"]) {
        if (entry["isActive"] == true) ++active_count;
    }
    REQUIRE(active_count == 1);

    server->stop();
}

TEST_CASE_METHOD(SchemaActivationFixture,
    "activateSnapshot: non-existent id returns error",
    "[integration][activation][us2]")
{
    REQUIRE(server->start());

    auto resp = post_graphql(
        "mutation { activateSnapshot(id: \"nonexistent-snap\") { success errors } }"
    );
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["activateSnapshot"]["success"] == false);
    REQUIRE_FALSE(resp["data"]["activateSnapshot"]["errors"].empty());

    server->stop();
}
