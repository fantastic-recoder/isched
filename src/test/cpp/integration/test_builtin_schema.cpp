// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_builtin_schema.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for built-in GraphQL schema queries (T017 — User Story 1)
 *
 * Tests the built-in GraphQL schema fields (hello, version, uptime, echo mutation,
 * error handling, and response extension fields) over real HTTP transport.
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

class BuiltinSchemaTestFixture {
public:
    std::unique_ptr<Server> server;

    BuiltinSchemaTestFixture() {
        Server::Configuration config;
        config.port = 18081;
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        server = Server::create(config);
    }

    ~BuiltinSchemaTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    // Helper: POST a GraphQL query body, return parsed JSON response
    nlohmann::json post_graphql(const std::string& query,
                                const std::string& variables_str = "{}") {
        httplib::Client client("localhost", 18081);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);

        nlohmann::json body = {{"query", query}};
        if (variables_str != "{}") {
            body["variables"] = nlohmann::json::parse(variables_str);
        }
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        return nlohmann::json::parse(res->body);
    }
};

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Built-in query: hello", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ hello }");
    REQUIRE(resp.contains("data"));
    REQUIRE_FALSE(resp.contains("errors"));
    REQUIRE(resp["data"]["hello"] == "Hello, GraphQL!");

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Built-in query: version", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ version }");
    REQUIRE(resp["data"]["version"] == "0.0.1");

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Built-in query: uptime", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ uptime }");
    REQUIRE(resp.contains("data"));
    // uptime is an integer >= 0
    REQUIRE(resp["data"]["uptime"].is_number_integer());
    REQUIRE(resp["data"]["uptime"].get<long long>() >= 0);

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Built-in mutation: echo", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql(R"(mutation { echo(message: "hello world") })");
    REQUIRE(resp.contains("data"));
    REQUIRE(resp["data"]["echo"] == "hello world");

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Built-in query: combined hello and version", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ hello version uptime }");
    REQUIRE(resp["data"]["hello"] == "Hello, GraphQL!");
    REQUIRE(resp["data"]["version"] == "0.0.1");
    REQUIRE(resp["data"]["uptime"].is_number_integer());

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Response extensions are populated", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ hello }");
    REQUIRE(resp.contains("extensions"));
    REQUIRE(resp["extensions"].contains("requestId"));
    REQUIRE(resp["extensions"].contains("endpoint"));
    REQUIRE(resp["extensions"].contains("executionTimeMs"));
    REQUIRE(resp["extensions"]["endpoint"] == "/graphql");
    // requestId should be non-empty string
    REQUIRE(resp["extensions"]["requestId"].is_string());
    REQUIRE_FALSE(resp["extensions"]["requestId"].get<std::string>().empty());

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Error: invalid JSON body returns 400", "[integration][builtin][error][us1]") {
    REQUIRE(server->start());

    httplib::Client client("localhost", 18081);
    client.set_connection_timeout(2);
    auto res = client.Post("/graphql", "not-valid-json{{{", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Error: missing query field returns 400", "[integration][builtin][error][us1]") {
    REQUIRE(server->start());

    httplib::Client client("localhost", 18081);
    client.set_connection_timeout(2);
    nlohmann::json body = {{"variables", {}}};
    auto res = client.Post("/graphql", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "Error: unknown field produces errors array", "[integration][builtin][error][us1]") {
    REQUIRE(server->start());

    auto resp = post_graphql("{ nonExistentField }");
    // The response should have errors
    REQUIRE(resp.contains("errors"));
    REQUIRE(resp["errors"].is_array());
    REQUIRE_FALSE(resp["errors"].empty());

    server->stop();
}

TEST_CASE_METHOD(BuiltinSchemaTestFixture, "GraphQL is the only external endpoint", "[integration][builtin][us1]") {
    REQUIRE(server->start());

    httplib::Client client("localhost", 18081);
    client.set_connection_timeout(2);

    // GET /health (REST) should not exist
    auto health_res = client.Get("/health");
    REQUIRE((health_res == nullptr || health_res->status == 404));

    // GET /metrics (REST) should not exist
    auto metrics_res = client.Get("/metrics");
    REQUIRE((metrics_res == nullptr || metrics_res->status == 404));

    // GET / (UI/playground) should not serve a frontend
    auto root_res = client.Get("/");
    REQUIRE((root_res == nullptr || root_res->status == 404));

    server->stop();
}
