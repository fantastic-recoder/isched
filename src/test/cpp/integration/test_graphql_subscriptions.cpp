// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_subscriptions.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for subscription event delivery (T038).
 *
 * Verifies that:
 *  - healthChanged subscription delivers an initial health event
 *  - configurationActivated subscription delivers an event after activateSnapshot
 *
 * HTTP port: 18088  |  WebSocket port: 18089 (= port + 1)
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

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <isched/backend/isched_Server.hpp>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18088;
static constexpr int k_ws_port   = 18089;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SubscriptionTestFixture {
public:
    std::unique_ptr<Server> server;

    SubscriptionTestFixture() {
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

    ~SubscriptionTestFixture() {
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
// Synchronous WebSocket helper (same pattern as T037)
// ---------------------------------------------------------------------------
struct SyncWs2 {
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

TEST_CASE_METHOD(SubscriptionTestFixture,
                 "healthChanged subscription delivers initial health event",
                 "[subscriptions][T038]") {
    SyncWs2 c;
    c.connect(k_ws_port);

    // Init
    c.send({{"type", "connection_init"}});
    auto ack = c.recv();
    REQUIRE(ack["type"] == "connection_ack");

    // Subscribe to healthChanged
    c.send({
        {"type", "subscribe"}, {"id", "h-1"},
        {"payload", {{"query", "subscription { healthChanged { status timestamp } }"}}}
    });

    auto next = c.recv();
    REQUIRE(next["type"] == "next");
    REQUIRE(next["id"]   == "h-1");

    // The initial healthChanged response contains data
    const auto& payload = next["payload"];
    REQUIRE(payload.contains("data"));
    const auto& data = payload["data"];
    REQUIRE(data.contains("healthChanged"));
    const auto& hc = data["healthChanged"];
    REQUIRE(hc["status"].get<std::string>() == "UP");
    REQUIRE(!hc["timestamp"].get<std::string>().empty());

    c.close();
}

TEST_CASE_METHOD(SubscriptionTestFixture,
                 "configurationActivated subscription delivers initial value",
                 "[subscriptions][T038]") {
    // Apply a snapshot first via HTTP
    const std::string tenant_id = "sub-tenant-" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());

    const std::string sdl = "type Query { ping: String }";
    auto apply_result = post_graphql(
        R"(mutation { applyConfiguration(input: { tenantId: ")" + tenant_id +
        R"(" schemaSdl: "type Query { ping: String }" }) { success } })");
    REQUIRE(apply_result["data"]["applyConfiguration"]["success"] == true);

    // Get snapshot id
    auto hist = post_graphql(
        "query($t: String!) { configurationHistory(tenantId: $t) { id } }",
        {{"t", tenant_id}});
    REQUIRE(hist["data"]["configurationHistory"].size() == 1);
    const std::string snap_id = hist["data"]["configurationHistory"][0]["id"];

    // Activate the snapshot
    auto act = post_graphql(
        "mutation($id: ID!) { activateSnapshot(id: $id) { success } }",
        {{"id", snap_id}});
    REQUIRE(act["data"]["activateSnapshot"]["success"] == true);

    // Now subscribe to configurationActivated
    SyncWs2 c;
    c.connect(k_ws_port);

    c.send({{"type", "connection_init"}});
    auto ack = c.recv();
    REQUIRE(ack["type"] == "connection_ack");

    c.send({
        {"type", "subscribe"}, {"id", "ca-1"},
        {"payload", {
            {"query", "subscription($t: String!) { configurationActivated(tenantId: $t) { tenantId snapshotId } }"},
            {"variables", {{"t", tenant_id}}}
        }}
    });

    auto next = c.recv();
    REQUIRE(next["type"] == "next");
    REQUIRE(next["id"]   == "ca-1");

    // The initial configurationActivated response reflects the active snapshot
    const auto& payload = next["payload"];
    REQUIRE(payload.contains("data"));
    const auto& data = payload["data"];
    REQUIRE(data.contains("configurationActivated"));

    c.close();
}
