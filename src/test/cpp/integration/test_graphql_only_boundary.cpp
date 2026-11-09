// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_only_boundary.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Verifies the backend exposes data operations only through POST /graphql
 *        (spec 013, T040): REST-style endpoints are not served.
 *
 * HTTP port: 18096  |  WebSocket port: 18097
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18096;
static constexpr int k_ws_port   = 18097;

namespace {
struct Fixture {
    std::unique_ptr<Server> server;
    Fixture() {
        const std::string data_home = "/tmp/isched-test-boundary-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        (void)::setenv("XDG_DATA_HOME", data_home.c_str(), 1);
        Server::Configuration cfg;
        cfg.port = k_http_port;
        cfg.ws_port = k_ws_port;
        cfg.allowed_origins = {
            "http://localhost:" + std::to_string(k_http_port),
            "http://127.0.0.1:" + std::to_string(k_http_port),
            "http://localhost:4200"
        };
        cfg.max_threads = 4;
        server = Server::create(cfg);
        REQUIRE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    ~Fixture() {
        if (server && server->get_status() != Server::Status::STOPPED) server->stop();
    }
};
} // namespace

TEST_CASE_METHOD(Fixture, "REST-style data endpoints are not served", "[boundary][graphql-only][T040]") {
    httplib::Client client("localhost", k_http_port);
    client.set_connection_timeout(2);
    client.set_read_timeout(5);

    for (const char* path : {"/api/users", "/api/v1/orgs", "/rest/health", "/users", "/login"}) {
        auto get = client.Get(path);
        REQUIRE(get != nullptr);
        REQUIRE(get->status >= 400);
        auto post = client.Post(path, "{}", "application/json");
        REQUIRE(post != nullptr);
        REQUIRE(post->status >= 400);
    }
}

TEST_CASE_METHOD(Fixture, "POST /graphql serves operations", "[boundary][graphql-only][T040]") {
    httplib::Client client("localhost", k_http_port);
    client.set_read_timeout(5);
    const nlohmann::json body = {{"query", "query { isched { health { status } } }"}};
    httplib::Headers headers{{"Origin", "http://localhost:" + std::to_string(k_http_port)}};
    auto res = client.Post("/graphql", headers, body.dump(), "application/json");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->get_header_value("Access-Control-Allow-Origin") == headers.begin()->second);
    REQUIRE(res->get_header_value("Access-Control-Allow-Credentials") == "true");
    auto j = nlohmann::json::parse(res->body);
    REQUIRE(j.contains("data"));
}

TEST_CASE_METHOD(Fixture, "GraphQL HTTP requests require an exact configured Origin",
                 "[boundary][origin][T053]") {
    httplib::Client client("localhost", k_http_port);
    client.set_read_timeout(5);
    const nlohmann::json body = {{"query", "query { isched { health { status } } }"}};

    for (const std::string origin : {"", "null", "http://localhost:18096/path",
                                     "http://localhost.evil:18096",
                                     "http://unlisted.example"}) {
        httplib::Headers headers;
        if (!origin.empty()) headers.emplace("Origin", origin);
        const auto rejected = client.Post("/graphql", headers, body.dump(), "application/json");
        REQUIRE(rejected != nullptr);
        REQUIRE(rejected->status == 403);
        REQUIRE(rejected->get_header_value("Access-Control-Allow-Origin").empty());
    }

    const nlohmann::json mutation = {{"query", "mutation { isched { logout } }"}};
    const auto rejected_mutation = client.Post(
        "/graphql", mutation.dump(), "application/json");
    REQUIRE(rejected_mutation != nullptr);
    REQUIRE(rejected_mutation->status == 403);

    httplib::Headers canonicalized{{"Origin", "HTTP://LOCALHOST:18096"}};
    const auto accepted = client.Post("/graphql", canonicalized, body.dump(), "application/json");
    REQUIRE(accepted != nullptr);
    REQUIRE(accepted->status == 200);

    httplib::Headers preflight{{"Origin", "http://localhost:18096"}};
    const auto allowed_preflight = client.Options("/graphql", preflight);
    REQUIRE(allowed_preflight != nullptr);
    REQUIRE(allowed_preflight->status == 204);
    REQUIRE(allowed_preflight->get_header_value("Access-Control-Allow-Headers") ==
            "Content-Type, X-CSRF-Token");

    httplib::Headers denied_preflight{{"Origin", "http://unlisted.example"}};
    const auto rejected_preflight = client.Options("/graphql", denied_preflight);
    REQUIRE(rejected_preflight != nullptr);
    REQUIRE(rejected_preflight->status == 403);
}
