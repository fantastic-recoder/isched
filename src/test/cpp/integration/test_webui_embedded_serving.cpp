// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_webui_embedded_serving.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md - Mozilla Public License 2.0
 * @brief Integration tests for embedded WebUI serving semantics (T022)
 */

#include <isched/backend/isched_Server.hpp>
#include <isched/backend/isched_UiAssetRegistry.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

class EmbeddedServingFixture {
public:
    static constexpr int PORT = 19082;

    EmbeddedServingFixture() {
        workDirectory = std::filesystem::temp_directory_path() /
            ("isched-webui-embedded-serving-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(workDirectory);

        Server::Configuration cfg;
        cfg.port = PORT;
        cfg.work_directory = workDirectory.string();
        server = Server::create(cfg);
        REQUIRE(server);
        REQUIRE(server->start());
        std::this_thread::sleep_for(50ms);
    }

    ~EmbeddedServingFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
        std::error_code remove_ec;
        std::filesystem::remove_all(workDirectory, remove_ec);
    }

    [[nodiscard]] httplib::Client makeClient() const {
        httplib::Client client("localhost", PORT);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        return client;
    }

    [[nodiscard]] json postGraphql(const std::string& query,
                                   const json& variables = json::object()) const {
        auto client = makeClient();
        json body = {
            {"query", query},
            {"variables", variables}
        };
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        const auto parsed = json::parse(res->body, nullptr, false);
        REQUIRE_FALSE(parsed.is_discarded());
        return parsed;
    }

    std::unique_ptr<Server> server;
    std::filesystem::path workDirectory;
};

void requireSecurityHeaders(const httplib::Headers& headers) {
    const auto xcto = headers.find("X-Content-Type-Options");
    REQUIRE(xcto != headers.end());
    REQUIRE(xcto->second == "nosniff");

    const auto xfo = headers.find("X-Frame-Options");
    REQUIRE(xfo != headers.end());
    REQUIRE(xfo->second == "DENY");

    const auto csp = headers.find("Content-Security-Policy");
    REQUIRE(csp != headers.end());
    REQUIRE(csp->second.find("default-src 'self'") != std::string::npos);
    REQUIRE(csp->second.find("frame-ancestors 'none'") != std::string::npos);
}

std::string extractFirstScriptPath(const std::string& html) {
    const std::string needle = "src=\"";
    auto src_pos = html.find(needle);
    if (src_pos == std::string::npos) {
        return {};
    }
    src_pos += needle.size();
    const auto end_pos = html.find('"', src_pos);
    if (end_pos == std::string::npos) {
        return {};
    }
    return html.substr(src_pos, end_pos - src_pos);
}

} // namespace

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: serves static WebUI assets",
                 "[integration][webui][embedded-serving][US1][T022]") {
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("Embedded UI assets are unavailable; rebuild UI embed target to run this suite.");
        return;
    }

    auto client = makeClient();

    auto index = client.Get("/isched");
    REQUIRE(index != nullptr);
    REQUIRE(index->status == 200);
    REQUIRE(index->get_header_value("Content-Type").find("text/html") != std::string::npos);
    REQUIRE(index->body.find("<app-root") != std::string::npos);
    requireSecurityHeaders(index->headers);

    // Probe one emitted bundle from index.html to verify asset serving path.
    const std::string script_path = extractFirstScriptPath(index->body);
    REQUIRE_FALSE(script_path.empty());
    auto script = client.Get(("/isched/" + script_path).c_str());
    REQUIRE(script != nullptr);
    REQUIRE(script->status == 200);
    REQUIRE(script->get_header_value("Content-Type").find("javascript") != std::string::npos);
    requireSecurityHeaders(script->headers);

    auto missing = client.Get("/isched/does-not-exist.1234");
    REQUIRE(missing != nullptr);
    REQUIRE(missing->status == 404);
    const auto missing_json = json::parse(missing->body, nullptr, false);
    REQUIRE_FALSE(missing_json.is_discarded());
    REQUIRE(missing_json.contains("errors"));
}

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: redirects root and GET /graphql to canonical /isched",
                 "[integration][webui][embedded-serving][routing]") {
    auto client = makeClient();

    for (const auto* route : {"/", "/graphql"}) {
        auto res = client.Get(route);
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 302);
        REQUIRE(res->get_header_value("Location") == "/isched");
    }
}

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: keeps SPA fallback for /isched routes",
                 "[integration][webui][embedded-serving][US1][T022]") {
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("Embedded UI assets are unavailable; rebuild UI embed target to run this suite.");
        return;
    }

    auto client = makeClient();
    const std::array<const char*, 3> routes = {
        "/isched/",
        "/isched/bootstrap",
        "/isched/admin/users"
    };

    for (const auto* route : routes) {
        auto res = client.Get(route);
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        REQUIRE(res->get_header_value("Content-Type").find("text/html") != std::string::npos);
        REQUIRE(res->body.find("<app-root") != std::string::npos);
        requireSecurityHeaders(res->headers);
    }
}

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: distinguishes missing static assets from SPA fallback routes",
                 "[integration][webui][embedded-serving][T068]") {
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("Embedded UI assets are unavailable; rebuild UI embed target to run this suite.");
        return;
    }

    auto client = makeClient();

    auto spa_route = client.Get("/isched/admin/users");
    REQUIRE(spa_route != nullptr);
    REQUIRE(spa_route->status == 200);
    REQUIRE(spa_route->get_header_value("Content-Type").find("text/html") != std::string::npos);
    REQUIRE(spa_route->body.find("<app-root") != std::string::npos);
    requireSecurityHeaders(spa_route->headers);

    auto missing_asset = client.Get("/isched/admin/users.missing.js");
    REQUIRE(missing_asset != nullptr);
    REQUIRE(missing_asset->status == 404);
    REQUIRE(missing_asset->get_header_value("Content-Type").find("application/json") != std::string::npos);
    requireSecurityHeaders(missing_asset->headers);

    const auto payload = json::parse(missing_asset->body, nullptr, false);
    REQUIRE_FALSE(payload.is_discarded());
    REQUIRE(payload.contains("errors"));
    REQUIRE(payload["errors"].is_array());
    REQUIRE_FALSE(payload["errors"].empty());
    REQUIRE(payload["errors"][0]["message"] == "asset not found");
    REQUIRE(payload["errors"][0]["extensions"]["code"] == "NOT_FOUND");
    REQUIRE(payload["errors"][0]["extensions"]["path"] == "/admin/users.missing.js");
}

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: emits hardening headers and supports If-None-Match 304",
                 "[integration][webui][embedded-serving][T069]") {
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("Embedded UI assets are unavailable; rebuild UI embed target to run this suite.");
        return;
    }

    auto client = makeClient();
    auto first = client.Get("/isched");
    REQUIRE(first != nullptr);
    REQUIRE(first->status == 200);
    requireSecurityHeaders(first->headers);

    const auto etag = first->get_header_value("ETag");
    REQUIRE_FALSE(etag.empty());

    httplib::Headers list_match_headers{{"If-None-Match", std::string{"\"stale\", "} + etag}};
    auto not_modified = client.Get("/isched", list_match_headers);
    REQUIRE(not_modified != nullptr);
    REQUIRE(not_modified->status == 304);
    REQUIRE(not_modified->body.empty());
    requireSecurityHeaders(not_modified->headers);
    REQUIRE(not_modified->get_header_value("ETag") == etag);
}

TEST_CASE_METHOD(EmbeddedServingFixture,
                 "Embedded serving: bootstrap route availability is controlled by bootstrap status",
                 "[integration][webui][embedded-serving][US1][T022]") {
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("Embedded UI assets are unavailable; rebuild UI embed target to run this suite.");
        return;
    }

    auto bootstrap_status_before = postGraphql("query { systemState { seedModeActive } }");
    REQUIRE(bootstrap_status_before.contains("data"));
    REQUIRE(bootstrap_status_before["data"]["systemState"]["seedModeActive"].get<bool>());

    // Route is always served by SPA, while GraphQL status determines whether bootstrap actions are valid.
    auto client = makeClient();
    auto bootstrap_route_before = client.Get("/isched/bootstrap");
    REQUIRE(bootstrap_route_before != nullptr);
    REQUIRE(bootstrap_route_before->status == 200);

    const auto unique_suffix = std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    const auto bootstrap_mutation = postGraphql(
        R"(mutation($input: BootstrapPlatformAdminInput!) {
               bootstrapPlatformAdmin(input: $input) { token expiresAt }
           })",
        json{{"input", {
            {"email", "embedded_" + unique_suffix + "@example.com"},
            {"password", "EmbeddedPass!123"},
            {"displayName", "Embedded Bootstrap"}
        }}}
    );

    REQUIRE(bootstrap_mutation.contains("data"));
    REQUIRE(bootstrap_mutation["data"].contains("bootstrapPlatformAdmin"));
    REQUIRE_FALSE(bootstrap_mutation["data"]["bootstrapPlatformAdmin"]["token"].get<std::string>().empty());

    auto bootstrap_status_after = postGraphql("query { systemState { seedModeActive } }");
    REQUIRE_FALSE(bootstrap_status_after["data"]["systemState"]["seedModeActive"].get<bool>());

    // The frontend route still falls back to SPA entry, but bootstrap is no longer available via API state.
    auto bootstrap_route_after = client.Get("/isched/bootstrap");
    REQUIRE(bootstrap_route_after != nullptr);
    REQUIRE(bootstrap_route_after->status == 200);
    REQUIRE(bootstrap_route_after->body.find("<app-root") != std::string::npos);
}



