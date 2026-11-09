// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_subscription_auth.cpp
 * @brief Regression tests for cookie-authenticated WebSocket subscriptions.
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_Server.hpp>
#include <isched/backend/isched_AuthenticationMiddleware.hpp>
#include "../isched/isched_graphql_test_helpers.hpp"

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

static constexpr int k_http_port = 18094;
static constexpr int k_ws_port   = 18095;
static constexpr std::string_view k_jwt_secret =
    "test-websocket-session-secret-at-least-32-bytes!";
static constexpr std::string_view k_protected_subscription =
    "subscription { isched { serverMetricsUpdated { activeConnections } } }";

namespace {
struct Fixture {
    std::unique_ptr<Server> server;
    std::string suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

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
        cfg.jwt_secret_key = std::string(k_jwt_secret);
        server = Server::create(cfg);
        REQUIRE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    ~Fixture() {
        if (server && server->get_status() != Server::Status::STOPPED) server->stop();
    }

    isched::test::AuthPayload authenticate() {
        isched::test::GraphQLTestClient client("localhost", k_http_port);
        return client.bootstrap_or_login_platform_admin(
            "subauth-" + suffix + "@example.com", "SubAuthPass!12345", "Sub Auth");
    }

    std::string expired_token() const {
        std::unique_ptr<AuthenticationMiddleware> auth{AuthenticationMiddleware::create()};
        auth->configure_jwt_secret(std::string(k_jwt_secret));
        return auth->generate_jwt_token(
            "expired-user", "platform", {"platform_admin"}, -1,
            "expired-ws-session", "Expired Session", {"platform_admin"});
    }
};

struct SyncWs {
    net::io_context ioc;
    tcp::resolver resolver{ioc};
    websocket::stream<beast::tcp_stream> ws{ioc};
    beast::flat_buffer buf;
    int port{k_ws_port};
    std::string origin = "http://localhost:" + std::to_string(k_http_port);
    std::string cookie;
    bool include_origin{true};

    void connect() {
        auto eps = resolver.resolve("127.0.0.1", std::to_string(port));
        ws.next_layer().expires_after(std::chrono::seconds(10));
        beast::get_lowest_layer(ws).connect(eps);
        ws.set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
            req.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
            if (include_origin) req.set(beast::http::field::origin, origin);
            if (!cookie.empty()) req.set(beast::http::field::cookie, cookie);
        }));
        ws.handshake("127.0.0.1:" + std::to_string(port), "/graphql");
    }

    void send(const json& message) {
        ws.next_layer().expires_after(std::chrono::seconds(10));
        ws.write(net::buffer(message.dump()));
    }

    beast::error_code read_one() {
        buf.consume(buf.size());
        ws.next_layer().expires_after(std::chrono::seconds(10));
        beast::error_code ec;
        ws.read(buf, ec);
        return ec;
    }

    json read_json() {
        REQUIRE_FALSE(read_one());
        return json::parse(beast::buffers_to_string(buf.data()));
    }
};

void require_unauthorized_close(SyncWs& client) {
    bool acknowledged = false;
    beast::error_code ec;
    for (int i = 0; i < 4; ++i) {
        ec = client.read_one();
        if (ec) break;
        const auto message = json::parse(
            beast::buffers_to_string(client.buf.data()), nullptr, false);
        if (!message.is_discarded() &&
            message.value("type", "") == "connection_ack") {
            acknowledged = true;
        }
    }
    REQUIRE_FALSE(acknowledged);
    REQUIRE(ec == websocket::error::closed);
    REQUIRE(static_cast<int>(client.ws.reason().code) == 4401);
}
} // namespace

TEST_CASE_METHOD(Fixture, "valid browser session cookie authenticates both WS listener routes",
                 "[subscriptions][auth][T007][T008][T054]") {
    const auto auth = authenticate();
    for (const int port : {k_http_port, k_ws_port}) {
        SyncWs client;
        client.port = port;
        client.cookie = "isched_session=" + auth.token;
        client.connect();
        client.send({{"type", "connection_init"}});
        REQUIRE(client.read_json().value("type", "") == "connection_ack");

        client.send({{"type", "subscribe"},
                     {"id", "cookie-auth"},
                     {"payload", {{"query", k_protected_subscription}}}});
        const auto result = client.read_json();
        REQUIRE(result.value("type", "") == "next");
        REQUIRE(result["payload"].contains("data"));
        REQUIRE_FALSE(result["payload"].contains("errors"));
        client.ws.close(websocket::close_code::normal);
    }
}

TEST_CASE_METHOD(Fixture, "invalid, empty, or duplicate session cookie closes with 4401",
                 "[subscriptions][auth][T008][T054]") {
    const std::vector<std::string> cookies{
             "isched_session=not-a-valid-session",
             "isched_session=",
             "isched_session",
             "isched_session=" + expired_token(),
             "isched_session=first; isched_session=second"};
    for (const int port : {k_http_port, k_ws_port}) {
        for (const auto& cookie : cookies) {
            SyncWs client;
            client.port = port;
            client.cookie = cookie;
            client.connect();
            client.send({{"type", "connection_init"}});
            require_unauthorized_close(client);
        }
    }
}

TEST_CASE_METHOD(Fixture, "connection_init credentials are ignored and protected work is rejected",
                 "[subscriptions][auth][T008][T063]") {
    const auto auth = authenticate();
    SyncWs client;
    client.connect();
    client.send({{"type", "connection_init"},
                 {"payload", {{"Authorization", "Bearer " + auth.token}}}});
    REQUIRE(client.read_json().value("type", "") == "connection_ack");

    client.send({{"type", "subscribe"},
                 {"id", "anonymous-protected"},
                 {"payload", {{"query", k_protected_subscription}}}});
    const auto result = client.read_json();
    REQUIRE(result.value("type", "") == "next");
    REQUIRE(result["payload"].contains("errors"));
    const auto complete = client.read_json();
    REQUIRE(complete.value("type", "") == "complete");
}

TEST_CASE_METHOD(Fixture, "missing cookie remains anonymous for public subscriptions",
                 "[subscriptions][auth][T008]") {
    SyncWs client;
    client.connect();
    client.send({{"type", "connection_init"}});
    REQUIRE(client.read_json().value("type", "") == "connection_ack");

    client.send({{"type", "subscribe"},
                 {"id", "public"},
                 {"payload", {{"query", "subscription { isched { healthChanged { status } } }"}}}});
    REQUIRE(client.read_json().value("type", "") == "next");
}

TEST_CASE_METHOD(Fixture, "revoked cookie session closes the live WebSocket with 4401",
                 "[subscriptions][auth][T054]") {
    const auto auth = authenticate();
    SyncWs client;
    client.cookie = "isched_session=" + auth.token;
    client.connect();
    client.send({{"type", "connection_init"}});
    REQUIRE(client.read_json().value("type", "") == "connection_ack");

    isched::test::GraphQLTestClient http_client("localhost", k_http_port);
    const auto logout = http_client.post_mutation("mutation { isched { logout } }", auth);
    REQUIRE_FALSE(isched::test::GraphQLTestClient::has_errors(logout));
    REQUIRE(client.read_one() == websocket::error::closed);
    REQUIRE(static_cast<int>(client.ws.reason().code) == 4401);
}

TEST_CASE_METHOD(Fixture, "WebSocket handshake rejects absent, malformed, and unlisted Origins",
                 "[subscriptions][origin][T054]") {
    for (const int port : {k_http_port, k_ws_port}) {
        for (const std::string origin : {"", "null", "http://localhost:18095/path",
                                         "http://localhost.evil:18094",
                                         "http://unlisted.example"}) {
            SyncWs client;
            client.port = port;
            client.origin = origin;
            REQUIRE_THROWS(client.connect());
        }

        SyncWs missing_origin;
        missing_origin.port = port;
        missing_origin.include_origin = false;
        REQUIRE_THROWS(missing_origin.connect());
    }
}
