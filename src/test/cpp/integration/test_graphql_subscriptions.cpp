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
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <optional>
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
#include "../isched/isched_graphql_test_helpers.hpp"

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

using namespace isched::v0_0_1::backend;

static constexpr int k_http_port = 18088;
static constexpr int k_ws_port   = 18089;

static std::string make_unique_suffix() {
    return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
           "-" + std::to_string(std::rand());
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class SubscriptionTestFixture {
public:
    std::unique_ptr<Server> server;
    std::unique_ptr<isched::test::GraphQLTestClient> auth_client;
    std::optional<isched::test::AuthPayload> auth;
    std::string run_suffix;

    SubscriptionTestFixture() {
        run_suffix = make_unique_suffix();
        const std::string data_home = "/tmp/isched-test-subscriptions-" + run_suffix;
        (void)::setenv("XDG_DATA_HOME", data_home.c_str(), 1);

        Server::Configuration cfg;
        cfg.port            = k_http_port;
        cfg.ws_port         = k_ws_port;
        cfg.allowed_origins = {
            "http://localhost:" + std::to_string(k_http_port),
            "http://127.0.0.1:" + std::to_string(k_http_port),
            "http://localhost:4200"
        };
        cfg.max_threads     = 4;
        cfg.enable_introspection = true;
        cfg.max_query_complexity = 500;
        server = Server::create(cfg);
        REQUIRE(server->start());
        auth_client = std::make_unique<isched::test::GraphQLTestClient>("localhost", k_http_port);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    ~SubscriptionTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }

    const isched::test::AuthPayload& ensure_auth() {
        if (!auth.has_value()) {
            const std::string email = "subscriptions-admin-" + run_suffix + "@example.com";
            auth = auth_client->bootstrap_or_login_platform_admin(
                email,
                "SubscriptionsAdminPass!123",
                "Subscriptions Admin");
        }
        return auth.value();
    }

    nlohmann::json post_graphql(const std::string& query,
                                const nlohmann::json& variables = nullptr) {
        httplib::Client client("localhost", k_http_port);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);

        nlohmann::json body = {{"query", query}};
        if (!variables.is_null()) {
            body["variables"] = variables;
        }

        const auto first_non_ws = query.find_first_not_of(" \t\n\r");
        const bool is_mutation =
            first_non_ws != std::string::npos && query.compare(first_non_ws, 8, "mutation") == 0;

        const std::string origin = "http://localhost:" + std::to_string(k_http_port);
        httplib::Headers headers{{"Origin", origin}};
        if (is_mutation) {
            const auto& current_auth = ensure_auth();
            headers.emplace("X-CSRF-Token", current_auth.csrf_token);
            headers.emplace("Cookie", "isched_session=" + current_auth.token);
            headers.emplace("Referer", origin + "/graphql");
        }
        auto res = client.Post("/graphql", headers, body.dump(), "application/json");

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
    std::string origin = "http://localhost:" + std::to_string(k_http_port);

    void connect(int port) {
        auto eps = resolver.resolve("127.0.0.1", std::to_string(port));
        // Timeout on TCP connect so the test fails fast if the port
        // is unreachable instead of blocking until CTest kills us.
        ws.next_layer().expires_after(std::chrono::seconds(10));
        beast::get_lowest_layer(ws).connect(eps);
        ws.set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
            req.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
            req.set(beast::http::field::origin, origin);
        }));
        ws.handshake("127.0.0.1:" + std::to_string(port), "/graphql");
    }

    nlohmann::json recv() {
        buf.consume(buf.size());
        // Apply a 30-second timeout so the test fails fast instead of
        // blocking until the CTest timeout (180 s) is reached.
        ws.next_layer().expires_after(std::chrono::seconds(30));
        ws.read(buf);
        return nlohmann::json::parse(beast::buffers_to_string(buf.data()));
    }

    void send(const nlohmann::json& msg) {
        ws.next_layer().expires_after(std::chrono::seconds(10));
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
        {"payload", {{"query", "subscription { isched { healthChanged { status timestamp } } }"}}}
    });

    auto next = c.recv();
    REQUIRE(next["type"] == "next");
    REQUIRE(next["id"]   == "h-1");

    // The initial healthChanged response contains data
    const auto& payload = next["payload"];
    REQUIRE(payload.contains("data"));
    const auto& data = payload["data"];
    REQUIRE(data.contains("isched"));
    REQUIRE(data["isched"].contains("healthChanged"));
    const auto& hc = data["isched"]["healthChanged"];
    REQUIRE(hc["status"].get<std::string>() == "UP");
    REQUIRE(!hc["timestamp"].get<std::string>().empty());

    c.send({{"type", "complete"}, {"id", "h-1"}});
    auto complete = c.recv();
    REQUIRE(complete["type"] == "complete");
    REQUIRE(complete["id"] == "h-1");

    c.close();
}

TEST_CASE_METHOD(SubscriptionTestFixture,
                 "configurationActivated subscription delivers initial value",
                 "[subscriptions][T038]") {
    // Apply a snapshot first via HTTP
    const std::string tenant_id = "sub-tenant-" + run_suffix;

    const std::string sdl = "type Query { ping: String }";
    auto apply_result = post_graphql(
        R"(mutation { isched { applyConfiguration(input: { tenantId: ")" + tenant_id +
        R"(" schemaSdl: "type Query { ping: String }" }) { success } } })");
    REQUIRE(apply_result["data"]["isched"]["applyConfiguration"]["success"] == true);

    // Get snapshot id
    auto hist = post_graphql(
        "query($t: String!) { isched { configurationHistory(tenantId: $t) { id } } }",
        {{"t", tenant_id}});
    REQUIRE(hist["data"]["isched"]["configurationHistory"].size() == 1);
    const std::string snap_id = hist["data"]["isched"]["configurationHistory"][0]["id"];

    // Activate the snapshot
    auto act = post_graphql(
        "mutation($id: ID!) { isched { activateSnapshot(id: $id) { success } } }",
        {{"id", snap_id}});
    REQUIRE(act["data"]["isched"]["activateSnapshot"]["success"] == true);

    // Now subscribe to configurationActivated
    SyncWs2 c;
    c.connect(k_ws_port);

    c.send({{"type", "connection_init"}});
    auto ack = c.recv();
    REQUIRE(ack["type"] == "connection_ack");

    c.send({
        {"type", "subscribe"}, {"id", "ca-1"},
        {"payload", {
            {"query", "subscription($t: String!) { isched { configurationActivated(tenantId: $t) { tenantId snapshotId } } }"},
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
    REQUIRE(data.contains("isched"));
    REQUIRE(data["isched"].contains("configurationActivated"));

    c.send({{"type", "complete"}, {"id", "ca-1"}});
    auto complete = c.recv();
    REQUIRE(complete["type"] == "complete");
    REQUIRE(complete["id"] == "ca-1");

    c.close();
}

// ---------------------------------------------------------------------------
// Spec 013 / T030: subscription delivery through a proxied connection
//
// A minimal TCP relay stands in for the Angular dev-server proxy (`ws: true`):
// the WebSocket upgrade is made against the relay port and forwarded to the
// backend HTTP port, which accepts /graphql upgrades.
// ---------------------------------------------------------------------------
namespace {
class TcpRelay {
public:
    explicit TcpRelay(int listen_port, int target_port)
        : acceptor_(ioc_, tcp::endpoint(tcp::v4(), static_cast<unsigned short>(listen_port))),
          target_port_(target_port) {
        thread_ = std::thread([this] { accept_loop(); });
    }
    ~TcpRelay() {
        stopped_ = true;
        beast::error_code ec;
        {   // Unblock the blocking accept() with a throw-away connection.
            tcp::socket wake(ioc_);
            wake.connect(acceptor_.local_endpoint(), ec);
        }
        if (thread_.joinable()) thread_.join();
        acceptor_.close(ec);
        for (auto& s : sockets_) {
            s->shutdown(tcp::socket::shutdown_both, ec);
            s->close(ec);
        }
        for (auto& t : pumps_) if (t.joinable()) t.join();
    }

private:
    void accept_loop() {
        while (!stopped_) {
            beast::error_code ec;
            auto client = std::make_shared<tcp::socket>(ioc_);
            acceptor_.accept(*client, ec);
            if (ec || stopped_) return;
            auto upstream = std::make_shared<tcp::socket>(ioc_);
            tcp::resolver resolver(ioc_);
            upstream->connect(*resolver.resolve("127.0.0.1", std::to_string(target_port_)), ec);
            if (ec) return;
            sockets_.push_back(client);
            sockets_.push_back(upstream);
            pumps_.emplace_back([client, upstream] { pump(*client, *upstream); });
            pumps_.emplace_back([client, upstream] { pump(*upstream, *client); });
        }
    }
    static void pump(tcp::socket& from, tcp::socket& to) {
        std::array<char, 4096> data{};
        beast::error_code ec;
        for (;;) {
            const auto n = from.read_some(net::buffer(data), ec);
            if (ec) break;
            net::write(to, net::buffer(data.data(), n), ec);
            if (ec) break;
        }
        to.shutdown(tcp::socket::shutdown_both, ec);
        from.shutdown(tcp::socket::shutdown_both, ec);
    }

    net::io_context ioc_;
    tcp::acceptor   acceptor_;
    int             target_port_;
    std::atomic<bool> stopped_{false};
    std::thread     thread_;
    std::vector<std::thread> pumps_;
    std::vector<std::shared_ptr<tcp::socket>> sockets_;
};
}  // namespace

TEST_CASE_METHOD(SubscriptionTestFixture,
                 "healthChanged subscription delivers through a proxied connection",
                 "[subscriptions][proxy][T030]") {
    constexpr int k_proxy_port = 18098;
    TcpRelay relay(k_proxy_port, k_http_port);

    SyncWs2 c;
    c.connect(k_proxy_port);

    c.send({{"type", "connection_init"}, {"payload", nlohmann::json::object()}});
    REQUIRE(c.recv()["type"] == "connection_ack");

    c.send({
        {"type", "subscribe"}, {"id", "px-1"},
        {"payload", {{"query", "subscription { isched { healthChanged { status } } }"}}}
    });
    auto next = c.recv();
    REQUIRE(next["type"] == "next");
    REQUIRE(next["id"] == "px-1");
    REQUIRE(next["payload"]["data"]["isched"]["healthChanged"]["status"] == "UP");

    c.close();
}
