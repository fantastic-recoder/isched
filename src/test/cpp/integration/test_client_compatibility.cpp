// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_client_compatibility.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for HTTP + WebSocket client compatibility (T039).
 *
 * Verifies that:
 *  - HTTP GraphQL queries work while a WebSocket client is connected
 *  - Multiple simultaneous WebSocket connections are each served independently
 *
 * HTTP port: 18090  |  WebSocket port: 18091 (= port + 1)
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18090;
static constexpr int k_ws_port   = 18091;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class CompatFixture {
public:
    std::unique_ptr<Server> server;

    CompatFixture() {
        Server::Configuration cfg;
        cfg.port            = k_http_port;
        cfg.ws_port         = k_ws_port;
        cfg.max_threads     = 4;
        cfg.enable_introspection = true;
        cfg.max_query_complexity = 500;
        server = Server::create(cfg);
        REQUIRE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    ~CompatFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    nlohmann::json post_graphql(const std::string& query,
                                const nlohmann::json& variables = nullptr) {
        httplib::Client client("localhost", k_http_port);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        nlohmann::json body = {{"query", query}};
        if (!variables.is_null()) body["variables"] = variables;
        auto res = client.Post("/graphql", body.dump(), "application/json");
        REQUIRE(res != nullptr);
        return nlohmann::json::parse(res->body);
    }
};

// ---------------------------------------------------------------------------
// Minimal synchronous WebSocket client
// ---------------------------------------------------------------------------
struct WsClient {
    net::io_context                             ioc;
    tcp::resolver                               resolver{ioc};
    websocket::stream<beast::tcp_stream>        ws{ioc};
    beast::flat_buffer                          buf;

    void connect(int port) {
        auto eps = resolver.resolve("127.0.0.1", std::to_string(port));
        net::connect(ws.next_layer().socket(), eps);
        ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
        }));
        ws.handshake("127.0.0.1:" + std::to_string(port), "/graphql");
    }

    nlohmann::json recv() {
        buf.consume(buf.size());
        ws.read(buf);
        return nlohmann::json::parse(beast::buffers_to_string(buf.data()));
    }

    void send(const nlohmann::json& msg) {
        ws.write(net::buffer(msg.dump()));
    }

    void close() {
        beast::error_code ec;
        ws.close(websocket::close_code::normal, ec);
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(CompatFixture,
                 "HTTP GraphQL query works while WebSocket client is connected",
                 "[compatibility][T039]") {
    // Open a WebSocket connection and keep it open
    WsClient ws;
    ws.connect(k_ws_port);
    ws.send({{"type", "connection_init"}});
    auto ack = ws.recv();
    REQUIRE(ack["type"] == "connection_ack");

    // Issue an HTTP query concurrently
    auto result = post_graphql("{ serverInfo { host port status } }");
    REQUIRE_FALSE(result.contains("errors"));
    REQUIRE(result["data"]["serverInfo"]["status"] == "RUNNING");

    ws.close();
}

TEST_CASE_METHOD(CompatFixture,
                 "Multiple simultaneous WebSocket connections are served independently",
                 "[compatibility][T039]") {
    WsClient c1, c2;

    c1.connect(k_ws_port);
    c2.connect(k_ws_port);

    c1.send({{"type", "connection_init"}});
    c2.send({{"type", "connection_init"}});

    auto ack1 = c1.recv();
    auto ack2 = c2.recv();

    REQUIRE(ack1["type"] == "connection_ack");
    REQUIRE(ack2["type"] == "connection_ack");

    // Each client subscribes independently
    c1.send({{"type", "subscribe"}, {"id", "s1"},
             {"payload", {{"query", "subscription { healthChanged { status } }"}}}});
    c2.send({{"type", "subscribe"}, {"id", "s2"},
             {"payload", {{"query", "subscription { healthChanged { status } }"}}}});

    auto n1 = c1.recv();
    auto n2 = c2.recv();

    REQUIRE(n1["type"] == "next");
    REQUIRE(n1["id"]   == "s1");
    REQUIRE(n2["type"] == "next");
    REQUIRE(n2["id"]   == "s2");

    c1.close();
    c2.close();
}

TEST_CASE_METHOD(CompatFixture,
                 "subscribe without connection_init receives error",
                 "[compatibility][T039]") {
    WsClient c;
    c.connect(k_ws_port);

    // Do NOT send connection_init
    c.send({{"type", "subscribe"}, {"id", "bad-1"},
            {"payload", {{"query", "subscription { healthChanged { status } }"}}}});

    auto err = c.recv();
    REQUIRE(err["type"] == "connection_error");

    c.close();
}
