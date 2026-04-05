// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_configuration_snapshots.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for configuration snapshot creation (T026 — User Story 2)
 *
 * Tests that the applyConfiguration mutation persists a snapshot and that the
 * configurationHistory and activeConfiguration queries return correct data.
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>
#include "../isched/isched_graphql_test_helpers.hpp"

using namespace isched::v0_0_1::backend;

/// Per-process unique suffix so tenant names don't clash between test runs.
static const std::string g_run_suffix = std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count());

class SnapshotTestFixture {
public:
    std::unique_ptr<Server> server;
    std::unique_ptr<isched::test::GraphQLTestClient> client;
    std::optional<isched::test::AuthPayload> auth;

    SnapshotTestFixture() {
        const std::string data_home = "/tmp/isched-test-snapshots-" + g_run_suffix;
        (void)::setenv("XDG_DATA_HOME", data_home.c_str(), 1);

        Server::Configuration config;
        config.port = 18083;
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        server = Server::create(config);
        client = std::make_unique<isched::test::GraphQLTestClient>("localhost", 18083);
    }

    ~SnapshotTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    const isched::test::AuthPayload& ensure_auth() {
        if (!auth.has_value()) {
            const std::string email = "config-admin-" + g_run_suffix + "@example.com";
            auth = client->bootstrap_or_login_platform_admin(email, "ConfigAdminPass!123", "Config Admin");
        }
        return auth.value();
    }

    nlohmann::json post_graphql(const std::string& query,
                                const nlohmann::json& variables = nullptr) {
        (void)variables;
        const auto first_non_ws = query.find_first_not_of(" \t\n\r");
        const bool is_mutation =
            first_non_ws != std::string::npos && query.compare(first_non_ws, 8, "mutation") == 0;
        return is_mutation ? client->post_mutation(query, ensure_auth()) : client->post_query(query);
    }
};

TEST_CASE_METHOD(SnapshotTestFixture,
    "applyConfiguration: creates a new snapshot and returns snapshotId",
    "[integration][config][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "alpha-" + g_run_suffix;
    const std::string mutation =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\" "
        "schemaSdl: \"type Widget { id: String! name: String }\" "
        "displayName: \"Alpha Config v1\" "
        "version: \"1.0.0\""
        "}) { success snapshotId errors } }";

    auto resp = post_graphql(mutation);
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));

    auto& result = resp["data"]["applyConfiguration"];
    REQUIRE(result["success"] == true);
    REQUIRE(result["snapshotId"].is_string());
    REQUIRE_FALSE(result["snapshotId"].get<std::string>().empty());
    REQUIRE(result["errors"].empty());

    server->stop();
}

TEST_CASE_METHOD(SnapshotTestFixture,
    "applyConfiguration with missing schemaSdl returns error",
    "[integration][config][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "alpha-err-" + g_run_suffix;
    const std::string mutation =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\""
        "}) { success errors } }";

    // Our resolver validates the missing schemaSdl argument
    auto resp = post_graphql(mutation);
    REQUIRE(resp.contains("data"));
    bool has_error = resp.contains("errors") ||
        (resp["data"].contains("applyConfiguration") &&
         resp["data"]["applyConfiguration"]["success"] == false);
    REQUIRE(has_error);

    server->stop();
}

TEST_CASE_METHOD(SnapshotTestFixture,
    "configurationHistory returns empty array for unknown tenant",
    "[integration][config][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "unknown-" + g_run_suffix;
    auto resp = post_graphql(
        "{ configurationHistory(tenantId: \"" + tenant + "\") {"
        "  id tenantId version isActive createdAt"
        "} }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["configurationHistory"].is_array());
    REQUIRE(resp["data"]["configurationHistory"].empty());

    server->stop();
}

TEST_CASE_METHOD(SnapshotTestFixture,
    "configurationHistory returns created snapshots for a tenant",
    "[integration][config][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "beta-" + g_run_suffix;
    const std::string mut1 =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\" "
        "schemaSdl: \"type Foo { id: String! }\" version: \"1.0.0\""
        "}) { success snapshotId } }";
    const std::string mut2 =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\" "
        "schemaSdl: \"type Bar { name: String }\" version: \"2.0.0\""
        "}) { success snapshotId } }";

    auto r1 = post_graphql(mut1);
    REQUIRE(r1["data"]["applyConfiguration"]["success"] == true);
    auto r2 = post_graphql(mut2);
    REQUIRE(r2["data"]["applyConfiguration"]["success"] == true);

    auto resp = post_graphql(
        "{ configurationHistory(tenantId: \"" + tenant + "\") {"
        "  id version isActive createdAt"
        "} }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    const auto& history = resp["data"]["configurationHistory"];
    REQUIRE(history.is_array());
    REQUIRE(history.size() >= 2);

    server->stop();
}

TEST_CASE_METHOD(SnapshotTestFixture,
    "activeConfiguration returns null before any activation",
    "[integration][config][us2]")
{
    REQUIRE(server->start());

    const std::string tenant = "gamma-" + g_run_suffix;
    const std::string create_mut =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\" "
        "schemaSdl: \"type Thing { id: String! }\" version: \"1.0.0\""
        "}) { success snapshotId } }";
    auto r = post_graphql(create_mut);
    REQUIRE(r["data"]["applyConfiguration"]["success"] == true);

    // Query for active configuration — should be null (not activated yet)
    auto resp = post_graphql(
        "{ activeConfiguration(tenantId: \"" + tenant + "\") {"
        "  id version isActive"
        "} }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["activeConfiguration"].is_null());

    server->stop();
}

TEST_CASE_METHOD(SnapshotTestFixture,
    "applyConfiguration with invalid SDL returns an error — not stored (T-GQL-023)",
    "[integration][config][us2][gql-023]")
{
    REQUIRE(server->start());

    const std::string tenant = "inv-sdl-" + g_run_suffix;
    // Deliberately broken SDL: unclosed brace
    const std::string mutation =
        "mutation { applyConfiguration(input: {"
        "tenantId: \"" + tenant + "\" "
        "schemaSdl: \"type Broken { id: String!\" "   // missing closing }
        "version: \"1.0.0\""
        "}) { success snapshotId errors } }";

    auto resp = post_graphql(mutation);
    REQUIRE(resp.contains("data"));
    auto& result = resp["data"]["applyConfiguration"];
    REQUIRE(result["success"] == false);
    REQUIRE_FALSE(result["errors"].empty());

    // Verify no snapshot was stored (history must be empty for this tenant)
    auto hist = post_graphql(
        "{ configurationHistory(tenantId: \"" + tenant + "\") { id } }");
    REQUIRE(hist["data"]["configurationHistory"].is_array());
    REQUIRE(hist["data"]["configurationHistory"].empty());

    server->stop();
}
