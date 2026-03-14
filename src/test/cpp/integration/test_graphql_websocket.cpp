// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_websocket.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for WebSocket connection lifecycle (T037).
 *
 * Verifies that the graphql-transport-ws protocol is correctly implemented:
 *  - connection_init → connection_ack
 *  - subscribe → initial next response
 *  - complete echo
 *  - ping → pong
 *
 * HTTP port: 18086  |  WebSocket port: 18087 (= port + 1)
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

#include <isched/backend/isched_Server.hpp>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18086;
static constexpr int k_ws_port   = 18087;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class WsLifecycleFixture {
public:
    std::unique_ptr<Server> server;

    WsLifecycleFixture() {
        Server::Configuration cfg;
        cfg.port            = k_http_port;
        cfg.ws_port         = k_ws_port;
        cfg.max_threads     = 4;
        cfg.enable_introspection = true;
        cfg.max_query_complexity = 500;
        server = Server::create(cfg);
        REQUIRE(server->start());
        // Small pause so both HTTP and WebSocket listeners are ready
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    ~WsLifecycleFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }
};

// ---------------------------------------------------------------------------
// Synchronous WebSocket helper
// ---------------------------------------------------------------------------
struct SyncWs {
    net::io_context                                     ioc;
    tcp::resolver                                       resolver{ioc};
    websocket::stream<beast::tcp_stream>                ws{ioc};
    beast::flat_buffer                                  buf;

    void connect(int port) {
        auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
        net::connect(ws.next_layer().socket(), endpoints);
        ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
        }));
        ws.handshake("127.0.0.1:" + std::to_string(port), "/graphql");
    }

    nlohmann::json send_recv(const nlohmann::json& msg) {
        ws.write(net::buffer(msg.dump()));
        buf.consume(buf.size());
        ws.read(buf);
        return nlohmann::json::parse(beast::buffers_to_string(buf.data()));
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

TEST_CASE_METHOD(WsLifecycleFixture,
                 "WebSocket upgrade succeeds and connection_ack is received",
                 "[websocket][lifecycle][T037]") {
    SyncWs c;
    c.connect(k_ws_port);

    auto ack = c.send_recv({{"type", "connection_init"}});
    REQUIRE(ack["type"] == "connection_ack");

    c.close();
}

TEST_CASE_METHOD(WsLifecycleFixture,
                 "subscribe delivers initial next response",
                 "[websocket][lifecycle][T037]") {
    SyncWs c;
    c.connect(k_ws_port);

    // Initialise
    auto ack = c.send_recv({{"type", "connection_init"}});
    REQUIRE(ack["type"] == "connection_ack");

    // Subscribe to healthChanged
    nlohmann::json sub = {
        {"type", "subscribe"},
        {"id",   "op-1"},
        {"payload", {{"query", "subscription { healthChanged { status timestamp } }"}}}
    };
    c.send(sub);

    auto next = c.recv();
    REQUIRE(next["type"] == "next");
    REQUIRE(next["id"]   == "op-1");
    // 'data' is top-level; payload wraps it according to graphql-transport-ws
    REQUIRE(next.contains("payload"));

    c.close();
}

TEST_CASE_METHOD(WsLifecycleFixture,
                 "complete message unsubscribes and server echoes complete",
                 "[websocket][lifecycle][T037]") {
    SyncWs c;
    c.connect(k_ws_port);

    auto ack = c.send_recv({{"type", "connection_init"}});
    REQUIRE(ack["type"] == "connection_ack");

    // subscribe
    c.send({{"type", "subscribe"}, {"id", "op-2"},
            {"payload", {{"query", "subscription { healthChanged { status } }"}}}});
    auto next = c.recv();
    REQUIRE(next["type"] == "next");

    // complete
    auto complete = c.send_recv({{"type", "complete"}, {"id", "op-2"}});
    REQUIRE(complete["type"] == "complete");
    REQUIRE(complete["id"]   == "op-2");

    c.close();
}

TEST_CASE_METHOD(WsLifecycleFixture,
                 "ping returns pong",
                 "[websocket][lifecycle][T037]") {
    SyncWs c;
    c.connect(k_ws_port);

    auto ack = c.send_recv({{"type", "connection_init"}});
    REQUIRE(ack["type"] == "connection_ack");

    auto pong = c.send_recv({{"type", "ping"}});
    REQUIRE(pong["type"] == "pong");

    c.close();
}
