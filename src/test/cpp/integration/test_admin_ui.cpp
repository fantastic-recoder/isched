// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_admin_ui.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for the /isched static asset server (T-UI-F-001)
 *
 * Verifies:
 * - GET /isched and /isched/ serve the index.html (200, text/html, <app-root> sentinel)
 * - GET /isched/<js-bundle> serves JavaScript (200, application/javascript)
 * - GET /isched/nonexistent.xyz returns 404 JSON
 * - GET /isched/a/deep/route serves index.html (push-state fallback)
 * - ETag / 304 / stale-ETag behaviour
 * - X-Content-Type-Options and X-Frame-Options security headers
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>
#include <isched/backend/isched_UiAssetRegistry.hpp>

using namespace isched::v0_0_1::backend;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Test fixture: start a server on a free port
// ---------------------------------------------------------------------------
class AdminUiFixture {
public:
    static constexpr int PORT = 19081;

    std::unique_ptr<Server> server;

    AdminUiFixture() {
        Server::Configuration cfg;
        cfg.port = PORT;
        // min_threads defaults to 4; keep at least that many
        server = Server::create(cfg);
        REQUIRE(server->start());
        std::this_thread::sleep_for(50ms); // brief pause for listener to bind
    }

    ~AdminUiFixture() {
        if (server && server->get_status() != Server::Status::STOPPED)
            server->stop();
    }

    httplib::Client make_client() {
        httplib::Client c("localhost", PORT);
        c.set_connection_timeout(2);
        c.set_read_timeout(5);
        return c;
    }
};

// ----------------------------------------------------------------------------
// Helper: check security headers are present
// ----------------------------------------------------------------------------
static void require_security_headers(const httplib::Headers& headers) {
    auto it_xcto = headers.find("X-Content-Type-Options");
    REQUIRE(it_xcto != headers.end());
    REQUIRE(it_xcto->second == "nosniff");

    auto it_xfo = headers.find("X-Frame-Options");
    REQUIRE(it_xfo != headers.end());
    REQUIRE(it_xfo->second == "DENY");
}

// ============================================================================
// Tests
// ============================================================================

TEST_CASE_METHOD(AdminUiFixture, "GET /isched serves index.html",
                 "[integration][admin-ui][T-UI-F-001]") {
    // Only meaningful when assets are embedded; skip gracefully if not.
    if (!UiAssetRegistry::instance().has_index_html()) {
        WARN("isched_ui_assets.hpp not regenerated yet — UI asset tests skipped");
        return;
    }

    auto client = make_client();
    auto res = client.Get("/isched");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    // Content-Type must be text/html
    const std::string ct = res->get_header_value("Content-Type");
    REQUIRE(ct.find("text/html") != std::string::npos);
    // Body must contain the Angular root element
    REQUIRE(res->body.find("<app-root>") != std::string::npos);
    require_security_headers(res->headers);
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched/ (trailing slash) serves index.html",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    auto client = make_client();
    auto res = client.Get("/isched/");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->get_header_value("Content-Type").find("text/html") != std::string::npos);
    REQUIRE(res->body.find("<app-root>") != std::string::npos);
    require_security_headers(res->headers);
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched/<js-bundle> serves JavaScript",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    // Find the first .js asset to use as a probe
    // We look for main-*.js by scanning the registry index
    // A simpler approach: the index.html references <script src="main-XXXXXX.js">
    // We'll fetch index.html and extract a JS filename from it.
    auto client = make_client();
    auto index_res = client.Get("/isched");
    REQUIRE(index_res != nullptr);
    REQUIRE(index_res->status == 200);

    // Find the main JS bundle name in the HTML source.
    const std::string& html = index_res->body;
    const std::string search = "src=\"";
    auto pos = html.find(search);
    REQUIRE(pos != std::string::npos);
    pos += search.size();
    auto end = html.find('"', pos);
    REQUIRE(end != std::string::npos);
    const std::string js_name = html.substr(pos, end - pos);
    REQUIRE_FALSE(js_name.empty());

    // Fetch the JS bundle
    const std::string js_path = "/isched/" + js_name;
    auto js_res = client.Get(js_path.c_str());
    REQUIRE(js_res != nullptr);
    CHECK(js_res->status == 200);
    const std::string js_ct = js_res->get_header_value("Content-Type");
    CHECK(js_ct.find("javascript") != std::string::npos);
    require_security_headers(js_res->headers);
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched/nonexistent.xyz returns 404 JSON",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    auto client = make_client();
    auto res = client.Get("/isched/definitely-does-not-exist.xyz");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 404);
    auto body = nlohmann::json::parse(res->body, nullptr, false);
    REQUIRE_FALSE(body.is_discarded());
    REQUIRE(body.contains("errors"));
    REQUIRE_FALSE(body["errors"].empty());
    REQUIRE(body["errors"][0]["message"] == "asset not found");
    require_security_headers(res->headers);
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched/deep/nested/route falls back to index.html",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    auto client = make_client();
    auto res = client.Get("/isched/a/deep/nested/route");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->get_header_value("Content-Type").find("text/html") != std::string::npos);
    REQUIRE(res->body.find("<app-root>") != std::string::npos);
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched with valid ETag returns 304",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    auto client = make_client();

    // First request — capture ETag
    auto res1 = client.Get("/isched");
    REQUIRE(res1 != nullptr);
    REQUIRE(res1->status == 200);
    const std::string etag = res1->get_header_value("ETag");
    REQUIRE_FALSE(etag.empty());

    // Second request with matching ETag
    httplib::Headers hdrs = {{"If-None-Match", etag}};
    auto res2 = client.Get("/isched", hdrs);
    REQUIRE(res2 != nullptr);
    REQUIRE(res2->status == 304);
    REQUIRE(res2->body.empty());
}

TEST_CASE_METHOD(AdminUiFixture, "GET /isched with stale ETag returns 200",
                 "[integration][admin-ui][T-UI-F-001]") {
    if (!UiAssetRegistry::instance().has_index_html()) return;

    auto client = make_client();
    httplib::Headers hdrs = {{"If-None-Match", "\"stale-etag-value\""}};
    auto res = client.Get("/isched", hdrs);
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(res->body.find("<app-root>") != std::string::npos);
}
