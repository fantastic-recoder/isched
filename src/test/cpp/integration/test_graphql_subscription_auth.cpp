// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_subscription_auth.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Regression tests for WebSocket `connection_init` authentication (spec 013, T008).
 *
 * - valid in-memory bearer token in connection_init.payload → connection_ack
 * - presented-but-invalid token → close code 4401, no connection_ack
 * - no credential → anonymous ack (public subscriptions remain available)
 *
 * HTTP port: 18094  |  WebSocket port: 18095
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_Server.hpp>
#include "../isched/isched_graphql_test_helpers.hpp"

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18094;
static constexpr int k_ws_port   = 18095;

namespace {
struct Fixture {
    std::unique_ptr<Server> server;
    std::string suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    Fixture() {
        const std::string data_home = "/tmp/isched-test-subauth-" + suffix;
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

struct SyncWs {
    net::io_context ioc;
    tcp::resolver resolver{ioc};
    websocket::stream<beast::tcp_stream> ws{ioc};
    beast::flat_buffer buf;
    std::string origin = "http://localhost:" + std::to_string(k_http_port);

    void connect() {
        auto eps = resolver.resolve("127.0.0.1", std::to_string(k_ws_port));
        ws.next_layer().expires_after(std::chrono::seconds(10));
        beast::get_lowest_layer(ws).connect(eps);
        ws.set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
            req.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
            req.set(beast::http::field::origin, origin);
        }));
        ws.handshake("127.0.0.1:" + std::to_string(k_ws_port), "/graphql");
    }
    void send(const nlohmann::json& m) {
        ws.next_layer().expires_after(std::chrono::seconds(10));
        ws.write(net::buffer(m.dump()));
    }
    /// Reads one frame; returns error_code (success => buf holds JSON).
    beast::error_code read_one() {
        buf.consume(buf.size());
        ws.next_layer().expires_after(std::chrono::seconds(10));
        beast::error_code ec;
        ws.read(buf, ec);
        return ec;
    }
};
} // namespace

TEST_CASE_METHOD(Fixture, "connection_init with valid bearer token is acknowledged",
                 "[subscriptions][auth][T008]") {
    isched::test::GraphQLTestClient client("localhost", k_http_port);
    const auto auth = client.bootstrap_or_login_platform_admin(
        "subauth-" + suffix + "@example.com", "SubAuthPass!12345", "Sub Auth");

    SyncWs c;
    c.connect();
    c.send({{"type", "connection_init"},
            {"payload", {{"Authorization", "Bearer " + auth.token}}}});
    REQUIRE_FALSE(c.read_one());
    auto msg = nlohmann::json::parse(beast::buffers_to_string(c.buf.data()));
    REQUIRE(msg["type"] == "connection_ack");
}

TEST_CASE_METHOD(Fixture, "connection_init with invalid bearer token closes with 4401",
                 "[subscriptions][auth][T008]") {
    SyncWs c;
    c.connect();
    c.send({{"type", "connection_init"},
            {"payload", {{"Authorization", "Bearer not-a-valid-token"}}}});

    // Must never receive connection_ack; stream ends with close code 4401.
    bool acked = false;
    beast::error_code ec;
    for (int i = 0; i < 4; ++i) {
        ec = c.read_one();
        if (ec) break;
        auto msg = nlohmann::json::parse(beast::buffers_to_string(c.buf.data()), nullptr, false);
        if (!msg.is_discarded() && msg.value("type", "") == "connection_ack") acked = true;
    }
    REQUIRE_FALSE(acked);
    REQUIRE(ec == websocket::error::closed);
    REQUIRE(static_cast<int>(c.ws.reason().code) == 4401);
}

TEST_CASE_METHOD(Fixture, "connection_init without credentials stays anonymous (public subscriptions)",
                 "[subscriptions][auth][T008]") {
    SyncWs c;
    c.connect();
    c.send({{"type", "connection_init"}});
    REQUIRE_FALSE(c.read_one());
    auto msg = nlohmann::json::parse(beast::buffers_to_string(c.buf.data()));
    REQUIRE(msg["type"] == "connection_ack");
}

TEST_CASE_METHOD(Fixture, "WebSocket handshake rejects missing, malformed, and unlisted Origins",
                 "[subscriptions][origin][T054]") {
    for (const std::string origin : {"", "null", "http://localhost:18095/path",
                                     "http://localhost.evil:18094",
                                     "http://unlisted.example"}) {
        SyncWs c;
        c.origin = origin;
        REQUIRE_THROWS(c.connect());
    }
}
