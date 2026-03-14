// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_srv_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 integration tests for the HTTP GraphQL transport.
 *
 * Starts the Server in-process on a test port and fires real HTTP POST
 * /graphql requests via cpp-httplib to validate end-to-end behaviour.
 */

#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include <nlohmann/json.hpp>

#include "isched/backend/isched_LogEnvLoader.hpp"
#include "isched/backend/isched_Server.hpp"

using namespace isched::v0_0_1::backend;
using nlohmann::json;

/// Port used exclusively by this test suite to avoid conflicts.
static constexpr int k_test_port = 18482;

struct SrvFixture {
    std::unique_ptr<Server> server;

    SrvFixture() {
        Server::Configuration cfg;
        cfg.port        = k_test_port;
        cfg.host        = "127.0.0.1";
        cfg.min_threads = 2;
        cfg.max_threads = 4;
        server = Server::create(cfg);
        REQUIRE(server->start());
    }

    ~SrvFixture() {
        if (server && server->get_status() == Server::Status::RUNNING) {
            server->stop();
        }
    }
};

TEST_CASE_METHOD(SrvFixture, "HTTP GraphQL transport: hello query", "[srv][graphql][http]") {
    httplib::Client cli("127.0.0.1", k_test_port);
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(5));

    std::string body = R"({"query":"{ hello }"})";
    auto res = cli.Post("/graphql", body, "application/json");

    REQUIRE(res);
    REQUIRE(res->status == 200);

    auto j = json::parse(res->body);
    REQUIRE(j.contains("data"));
    REQUIRE(j["data"]["hello"] == "Hello, GraphQL!");
}

TEST_CASE_METHOD(SrvFixture, "HTTP GraphQL transport: version query", "[srv][graphql][http]") {
    httplib::Client cli("127.0.0.1", k_test_port);
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(5));

    std::string body = R"({"query":"{ version }"})";
    auto res = cli.Post("/graphql", body, "application/json");

    REQUIRE(res);
    REQUIRE(res->status == 200);

    auto j = json::parse(res->body);
    REQUIRE(j.contains("data"));
    REQUIRE(j["data"]["version"] == "0.0.1");
}

TEST_CASE_METHOD(SrvFixture, "HTTP GraphQL transport: missing query field returns 400", "[srv][graphql][http]") {
    httplib::Client cli("127.0.0.1", k_test_port);
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(5));

    auto res = cli.Post("/graphql", R"({"notAQuery":"x"})", "application/json");

    REQUIRE(res);
    REQUIRE(res->status == 400);
}

TEST_CASE_METHOD(SrvFixture, "HTTP GraphQL transport: invalid JSON body returns 400", "[srv][graphql][http]") {
    httplib::Client cli("127.0.0.1", k_test_port);
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(5));

    auto res = cli.Post("/graphql", "not json at all", "application/json");

    REQUIRE(res);
    REQUIRE(res->status == 400);
}

TEST_CASE_METHOD(SrvFixture, "HTTP GraphQL transport: extensions block present", "[srv][graphql][http]") {
    httplib::Client cli("127.0.0.1", k_test_port);
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(5));

    auto res = cli.Post("/graphql", R"({"query":"{ hello }"})", "application/json");

    REQUIRE(res);
    auto j = json::parse(res->body);
    REQUIRE(j.contains("extensions"));
    REQUIRE(j["extensions"].contains("requestId"));
    REQUIRE(j["extensions"].contains("executionTimeMs"));
    REQUIRE(j["extensions"]["endpoint"] == "/graphql");
}