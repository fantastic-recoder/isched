// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_rest_datasource_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for RestDataSource and CryptoUtils (T048-008).
 *
 * Tests spin up a local httplib::Server on an ephemeral port, then exercise
 * RestDataSource::fetch in the same process.  No network traffic leaves the machine.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

// Library headers
#include "isched/backend/isched_CryptoUtils.hpp"
#include "isched/backend/isched_RestDataSource.hpp"
#include "httplib.h"

using namespace isched::v0_0_1::backend;

// ── Helpers ───────────────────────────────────────────────────────────────────

/// RAII wrapper that starts a local HTTP server on a background thread and
/// stops it when destroyed.
struct LocalServer {
    httplib::Server         srv;
    std::thread             thread;
    int                     port{0};
    std::atomic<bool>       ready{false};

    explicit LocalServer(std::function<void(httplib::Server&)> setup) {
        setup(srv);
        port = srv.bind_to_any_port("127.0.0.1");
        REQUIRE(port > 0);
        thread = std::thread([this] {
            ready.store(true);
            srv.listen_after_bind();
        });
        // Poll until the server is actually accepting connections
        for (int i = 0; i < 50; ++i) {
            if (srv.is_running()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        REQUIRE(srv.is_running());
    }

    ~LocalServer() {
        srv.stop();
        if (thread.joinable()) thread.join();
    }

    std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

// ── CryptoUtils tests ─────────────────────────────────────────────────────────

TEST_CASE("CryptoUtils encrypt/decrypt round-trip", "[crypto][T048-001a]") {
    const std::string master  = "super-secret-master-key-for-testing";
    const std::string tenant  = "tenant-acme";
    const std::string plain   = "sk-1234567890abcdef";

    const std::string ciphertext = encrypt_secret(plain, tenant, master);
    REQUIRE(!ciphertext.empty());
    // Ciphertext should differ from plaintext
    REQUIRE(ciphertext != plain);

    const std::string decrypted = decrypt_secret(ciphertext, tenant, master);
    REQUIRE(decrypted == plain);
}

TEST_CASE("CryptoUtils different tenants produce different ciphertexts", "[crypto][T048-001a]") {
    const std::string master = "same-master-key";
    const std::string plain  = "api-key-value";

    const auto ct1 = encrypt_secret(plain, "tenant-a", master);
    const auto ct2 = encrypt_secret(plain, "tenant-b", master);
    // Separate HKDF-derived keys → different ciphertexts
    REQUIRE(ct1 != ct2);
    // Cross-tenant decryption must fail (wrong derived key → GCM auth failure)
    REQUIRE_THROWS(decrypt_secret(ct1, "tenant-b", master));
    REQUIRE_THROWS(decrypt_secret(ct2, "tenant-a", master));
}

TEST_CASE("CryptoUtils tampered ciphertext is rejected", "[crypto][T048-001a]") {
    const std::string master = "test-master";
    const std::string tenant = "tenantX";
    auto ct = encrypt_secret("secret", tenant, master);
    // Flip a byte in the middle of the base64 blob
    if (!ct.empty()) ct[ct.size() / 2] ^= 0xFF;
    REQUIRE_THROWS(decrypt_secret(ct, tenant, master));
}

TEST_CASE("CryptoUtils empty master_secret throws", "[crypto][T048-001a]") {
    REQUIRE_THROWS_AS(encrypt_secret("v", "t", ""), std::runtime_error);
    REQUIRE_THROWS_AS(decrypt_secret("dGVzdA==", "t", ""), std::runtime_error);
}

TEST_CASE("CryptoUtils encrypt empty plaintext is valid", "[crypto][T048-001a]") {
    const std::string master = "key";
    const std::string tenant = "t1";
    const auto ct = encrypt_secret("", tenant, master);
    REQUIRE(!ct.empty());
    const auto pt = decrypt_secret(ct, tenant, master);
    REQUIRE(pt.empty());
}

// ── RestDataSource tests ──────────────────────────────────────────────────────

TEST_CASE("RestDataSource fetch returns JSON body on 200", "[rest_datasource][T048-005]") {
    LocalServer local([](httplib::Server& s) {
        s.Get("/data", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"value":42})", "application/json");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 3000;

    const auto result = RestDataSource::fetch(cfg, "/data");
    REQUIRE(result.is_object());
    REQUIRE(result.value("value", 0) == 42);
}

TEST_CASE("RestDataSource fetch returns HttpError on 404", "[rest_datasource][T048-005][T048-006]") {
    LocalServer local([](httplib::Server& s) {
        s.Get("/missing", [](const httplib::Request&, httplib::Response& res) {
            res.status = 404;
            res.set_content("Not Found", "text/plain");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 3000;

    const auto result = RestDataSource::fetch(cfg, "/missing");
    REQUIRE(result.is_object());
    REQUIRE(result.value("statusCode", 0) == 404);
    REQUIRE(result.contains("message"));
    REQUIRE(result.contains("url"));
}

TEST_CASE("RestDataSource bearer_passthrough forwards Authorization header", "[rest_datasource][T048-005]") {
    std::string captured_auth;
    LocalServer local([&](httplib::Server& s) {
        s.Get("/secure", [&](const httplib::Request& req, httplib::Response& res) {
            captured_auth = req.get_header_value("Authorization");
            res.set_content(R"({"ok":true})", "application/json");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "bearer_passthrough";
    cfg.timeout_ms = 3000;

    const auto _capture_result = RestDataSource::fetch(cfg, "/secure", "GET", {}, "test-token-xyz");
    std::ignore = _capture_result;
    REQUIRE(captured_auth == "Bearer test-token-xyz");
}

TEST_CASE("RestDataSource api_key sets configured header", "[rest_datasource][T048-005]") {
    std::string captured_key;
    LocalServer local([&](httplib::Server& s) {
        s.Get("/apidata", [&](const httplib::Request& req, httplib::Response& res) {
            captured_key = req.get_header_value("X-API-Key");
            res.set_content(R"({"ok":true})", "application/json");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url       = local.base_url();
    cfg.auth_kind      = "api_key";
    cfg.api_key_header = "X-API-Key";
    cfg.api_key_value  = "my-secret-api-key";
    cfg.timeout_ms     = 3000;

    const auto _api_result = RestDataSource::fetch(cfg, "/apidata");
    std::ignore = _api_result;
    REQUIRE(captured_key == "my-secret-api-key");
}

TEST_CASE("RestDataSource POST sends JSON body", "[rest_datasource][T048-005]") {
    std::string captured_body;
    LocalServer local([&](httplib::Server& s) {
        s.Post("/echo", [&](const httplib::Request& req, httplib::Response& res) {
            captured_body = req.body;
            res.set_content(req.body, "application/json");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 3000;

    nlohmann::json body = {{"key", "value"}, {"num", 7}};
    const auto _post_result = RestDataSource::fetch(cfg, "/echo", "POST", body);
    std::ignore = _post_result;
    REQUIRE(!captured_body.empty());
    const auto parsed = nlohmann::json::parse(captured_body);
    REQUIRE(parsed.value("num", 0) == 7);
}

TEST_CASE("RestDataSource returns HttpError on connection refused", "[rest_datasource][T048-005][T048-006]") {
    // Use a port that nothing is listening on
    DataSourceConfig cfg;
    cfg.base_url   = "http://127.0.0.1:19999";
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 500; // Short timeout to fail fast

    const auto result = RestDataSource::fetch(cfg, "/");
    REQUIRE(result.is_object());
    // Either statusCode=0 (connection error) or still has the HttpError fields
    REQUIRE(result.contains("statusCode"));
    REQUIRE(result.contains("message"));
}

TEST_CASE("RestDataSource returns HttpError on 500 upstream error", "[rest_datasource][T048-005][T048-006]") {
    LocalServer local([](httplib::Server& s) {
        s.Get("/fail", [](const httplib::Request&, httplib::Response& res) {
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 3000;

    const auto result = RestDataSource::fetch(cfg, "/fail");
    REQUIRE(result.value("statusCode", 0) == 500);
}

TEST_CASE("RestDataSource non-JSON 200 response is wrapped", "[rest_datasource][T048-005]") {
    LocalServer local([](httplib::Server& s) {
        s.Get("/text", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("hello world", "text/plain");
        });
    });

    DataSourceConfig cfg;
    cfg.base_url   = local.base_url();
    cfg.auth_kind  = "none";
    cfg.timeout_ms = 3000;

    const auto result = RestDataSource::fetch(cfg, "/text");
    REQUIRE(result.is_object());
    // Non-JSON body should be wrapped under "data"
    REQUIRE(result.contains("data"));
}

TEST_CASE("RestDataSource empty base_url returns HttpError", "[rest_datasource][T048-005][T048-006]") {
    DataSourceConfig cfg;
    cfg.base_url  = "";
    cfg.auth_kind = "none";

    const auto result = RestDataSource::fetch(cfg, "/path");
    REQUIRE(result.value("statusCode", -1) == 0);
}
