// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_health_queries.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for GraphQL-based health and server info queries (T018 — User Story 1)
 *
 * Tests the health, serverInfo, and metrics GraphQL fields over real HTTP transport,
 * verifying that operational status is accessible exclusively through the GraphQL endpoint.
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_common.hpp>
#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;

class HealthQueryTestFixture {
public:
    std::unique_ptr<Server> server;

    HealthQueryTestFixture() {
        Server::Configuration config;
        config.port = 18082;
        config.allowed_origins = {"http://localhost:18082"};
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        server = Server::create(config);
    }

    ~HealthQueryTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    nlohmann::json post_graphql(const std::string& query) {
        httplib::Client client("localhost", 18082);
        client.set_default_headers({{"Origin", "http://localhost:18082"}});
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        nlohmann::json body = {{"query", query}};
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        return nlohmann::json::parse(res->body);
    }
};

TEST_CASE_METHOD(HealthQueryTestFixture, "Health query: overall status is UP", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { health { status } } }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["isched"]["health"]["status"] == "UP");

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "Health query: components are reported", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { health { status components } } }");
    REQUIRE(resp["data"]["isched"]["health"]["status"] == "UP");
    // components should be present (non-null)
    REQUIRE_FALSE(resp["data"]["isched"]["health"]["components"].is_null());

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "Health query: timestamp is present", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { health { status timestamp } } }");
    REQUIRE(resp["data"]["isched"]["health"]["status"] == "UP");
    // timestamp in milliseconds since epoch — should be a large positive number
    REQUIRE_FALSE(resp["data"]["isched"]["health"]["timestamp"].is_null());

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "ServerInfo query: version and activeTenants", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { serverInfo { version activeTenants } } }");
    REQUIRE(resp.contains("data"));
    REQUIRE(resp["data"]["isched"]["serverInfo"]["version"] == "0.0.1");
    REQUIRE(resp["data"]["isched"]["serverInfo"]["activeTenants"] == 1);

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "ServerInfo query: transportModes is an array", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { serverInfo { transportModes } } }");
    REQUIRE(resp.contains("data"));
    REQUIRE(resp["data"]["isched"]["serverInfo"]["transportModes"].is_array());
    REQUIRE_FALSE(resp["data"]["isched"]["serverInfo"]["transportModes"].empty());

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "Metrics query: activeConnections and requestsInInterval", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { metrics { activeConnections requestsInInterval } } }");
    REQUIRE(resp.contains("data"));
    REQUIRE(resp["data"]["isched"]["metrics"]["activeConnections"].is_number());
    REQUIRE(resp["data"]["isched"]["metrics"]["requestsInInterval"].is_number());

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "Health and serverInfo in a single query", "[integration][health][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ isched { health { status } serverInfo { version activeTenants transportModes } } }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));

    REQUIRE(resp["data"]["isched"]["health"]["status"] == "UP");
    REQUIRE(resp["data"]["isched"]["serverInfo"]["version"] == "0.0.1");
    REQUIRE(resp["data"]["isched"]["serverInfo"]["activeTenants"] == 1);
    REQUIRE(resp["data"]["isched"]["serverInfo"]["transportModes"].is_array());

    server->stop();
}

TEST_CASE_METHOD(HealthQueryTestFixture, "Health data is fresh: repeated queries return consistent status", "[integration][health][us1]") {
    REQUIRE(server->start());

    for (int i = 0; i < 3; ++i) {
        auto resp = post_graphql("{ isched { health { status } } }");
        REQUIRE(resp["data"]["isched"]["health"]["status"] == "UP");
    }

    server->stop();
}
