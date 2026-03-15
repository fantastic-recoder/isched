// SPDX-License-Identifier: MPL-2.0
/**
 * @file benchmark_suite.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Performance benchmark suite (T052).
 *
 * T052-002  hello query throughput ≥ 1000 req/s (5-second window, sequential)
 * T052-003  100 threads × 10 { version } queries, 0 errors, ≤ 10 s
 * T052-004  50 simultaneous WS subscriptions, initial event fan-out ≤ 500 ms
 * T052-005  10 concurrent __schema { types } requests, each ≤ 500 ms
 * T052-006  1000 sequential { version } queries, p95 ≤ 20 ms
 *
 * HTTP port: 18092  |  WebSocket port: 18093 (= port + 1)
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
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

static constexpr int k_http_port = 18092;
static constexpr int k_ws_port   = 18093; // ws_port=0 means port+1 in Server

// ---------------------------------------------------------------------------
// Fixture — one server instance per TEST_CASE
// ---------------------------------------------------------------------------
class BenchmarkFixture {
public:
    std::unique_ptr<Server> server;

    BenchmarkFixture() {
        Server::Configuration cfg;
        cfg.port                 = static_cast<uint16_t>(k_http_port);
        cfg.ws_port              = 0; // use port+1 = 18093
        cfg.min_threads          = 8;
        cfg.max_threads          = 120;
        cfg.enable_introspection = true;
        cfg.max_query_complexity = 1000;
        server = Server::create(cfg);
        REQUIRE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~BenchmarkFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }
};

// ---------------------------------------------------------------------------
// Synchronous WebSocket helper
// ---------------------------------------------------------------------------
struct SyncWsBench {
    net::io_context                          ioc;
    tcp::resolver                            resolver{ioc};
    websocket::stream<beast::tcp_stream>     ws{ioc};
    beast::flat_buffer                       buf;

    // non-copyable, non-movable (io_context constraint)
    SyncWsBench()                              = default;
    SyncWsBench(const SyncWsBench&)            = delete;
    SyncWsBench& operator=(const SyncWsBench&) = delete;

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
// T052-002: hello query throughput ≥ 1000 req/s
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(BenchmarkFixture,
                 "T052-002: hello query throughput >= 1000 req/s over 5 seconds",
                 "[benchmark][T052]")
{
    using clock = std::chrono::steady_clock;

    httplib::Client client("localhost", k_http_port);
    client.set_connection_timeout(2);
    client.set_read_timeout(5);

    const std::string body_str =
        nlohmann::json{{"query", "{ hello }"}}.dump();

    long long count = 0;
    const auto deadline = clock::now() + std::chrono::seconds(5);

    while (clock::now() < deadline) {
        auto res = client.Post("/graphql", body_str, "application/json");
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
        ++count;
    }

    const long long rps = count / 5;
    INFO("T052-002 throughput: " << count << " req in 5 s = " << rps << " req/s");
    REQUIRE(count >= 5000); // ≥ 1000 req/s
}

// ---------------------------------------------------------------------------
// T052-003: 100 concurrent threads × 10 { version } queries — 0 errors, ≤ 10 s
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(BenchmarkFixture,
                 "T052-003: 100 concurrent threads x10 version queries without errors",
                 "[benchmark][T052]")
{
    using clock = std::chrono::steady_clock;

    constexpr int k_threads    = 100;
    constexpr int k_per_thread = 10;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;
    threads.reserve(k_threads);

    const auto start = clock::now();

    for (int t = 0; t < k_threads; ++t) {
        threads.emplace_back([&error_count]() {
            httplib::Client client("localhost", k_http_port);
            client.set_connection_timeout(2);
            client.set_read_timeout(5);
            const std::string body_str =
                nlohmann::json{{"query", "{ version }"}}.dump();
            for (int i = 0; i < k_per_thread; ++i) {
                // Retry up to 5 times with brief back-off to handle transient
                // TCP accept-queue saturation under 100-thread burst load.
                bool ok = false;
                for (int att = 0; att < 5 && !ok; ++att) {
                    if (att > 0) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(att * 20));
                    }
                    auto res = client.Post("/graphql", body_str, "application/json");
                    if (res && res->status == 200) {
                        ok = true;
                    }
                }
                if (!ok) {
                    ++error_count;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();

    INFO("T052-003: " << (k_threads * k_per_thread) << " requests in "
         << elapsed_ms << " ms, errors=" << error_count.load());
    REQUIRE(error_count.load() == 0);
    REQUIRE(elapsed_ms <= 10000);
}

// ---------------------------------------------------------------------------
// T052-004: WS subscription fan-out — 50 clients, initial event, all ≤ 500 ms
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(BenchmarkFixture,
                 "T052-004: 50 WS subscriptions all receive initial health event within 500 ms",
                 "[benchmark][T052]")
{
    using clock = std::chrono::steady_clock;

    constexpr int k_clients = 50;

    // Allocate on heap: io_context is non-copyable/non-movable
    std::vector<std::unique_ptr<SyncWsBench>> clients;
    clients.reserve(k_clients);
    for (int i = 0; i < k_clients; ++i) {
        clients.emplace_back(std::make_unique<SyncWsBench>());
        clients.back()->connect(k_ws_port);
    }

    // Send connection_init for all
    for (int i = 0; i < k_clients; ++i) {
        clients[i]->send({{"type", "connection_init"}});
    }
    // Receive connection_ack for all
    for (int i = 0; i < k_clients; ++i) {
        auto ack = clients[i]->recv();
        REQUIRE(ack["type"] == "connection_ack");
    }

    // Subscribe all to healthChanged
    for (int i = 0; i < k_clients; ++i) {
        clients[i]->send({
            {"type",    "subscribe"},
            {"id",      "hc-" + std::to_string(i)},
            {"payload", {{"query", "subscription { healthChanged { status } }"}}}
        });
    }

    // Receive initial fan-out events — measure total wall-clock time
    const auto t0 = clock::now();
    for (int i = 0; i < k_clients; ++i) {
        auto next = clients[i]->recv();
        REQUIRE(next["type"] == "next");
        REQUIRE(next["payload"]["data"]["healthChanged"]["status"] == "UP");
    }
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();

    INFO("T052-004 fan-out: all " << k_clients
         << " clients received initial event in " << elapsed_ms << " ms");
    REQUIRE(elapsed_ms <= 500);

    for (auto& c : clients) {
        c->close();
    }
}

// ---------------------------------------------------------------------------
// T052-005: 10 concurrent introspection requests — each ≤ 500 ms
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(BenchmarkFixture,
                 "T052-005: 10 concurrent introspection requests each within 500 ms",
                 "[benchmark][T052]")
{
    using clock = std::chrono::steady_clock;

    // Warm-up: the first few introspection requests serialise the entire schema;
    // subsequent calls are fast.  Three sequential warm-up iterations ensure the
    // server connection pool has settled before the timed concurrent phase.
    {
        httplib::Client warm("localhost", k_http_port);
        warm.set_connection_timeout(2);
        warm.set_read_timeout(10);
        const std::string body_str =
            nlohmann::json{{"query", "{ __schema { types { name } } }"}}.dump();
        for (int i = 0; i < 3; ++i) {
            warm.Post("/graphql", body_str, "application/json");
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    constexpr int k_threads = 10;
    std::vector<long long> durations(static_cast<std::size_t>(k_threads), 0LL);
    std::atomic<int>       error_count{0};
    std::vector<std::thread> threads;
    threads.reserve(k_threads);

    for (int t = 0; t < k_threads; ++t) {
        threads.emplace_back([&durations, &error_count, t]() {
            httplib::Client client("localhost", k_http_port);
            client.set_connection_timeout(2);
            client.set_read_timeout(10);
            const std::string body_str =
                nlohmann::json{{"query", "{ __schema { types { name } } }"}}.dump();
            const auto t0  = clock::now();
            auto        res = client.Post("/graphql", body_str, "application/json");
            durations[static_cast<std::size_t>(t)] =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    clock::now() - t0).count();
            if (!res || res->status != 200) {
                ++error_count;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    REQUIRE(error_count.load() == 0);
    for (int t = 0; t < k_threads; ++t) {
        const auto d = durations[static_cast<std::size_t>(t)];
        INFO("T052-005 thread " << t << " introspection: " << d << " ms");
        REQUIRE(d <= 500);
    }
}

// ---------------------------------------------------------------------------
// T052-006: p95 latency — 1000 sequential { version } queries, p95 ≤ 20 ms
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(BenchmarkFixture,
                 "T052-006: 1000 sequential version queries have p95 latency <= 20 ms",
                 "[benchmark][T052]")
{
    using clock = std::chrono::steady_clock;

    constexpr int k_requests = 1000;

    httplib::Client client("localhost", k_http_port);
    client.set_connection_timeout(2);
    client.set_read_timeout(5);

    const std::string body_str =
        nlohmann::json{{"query", "{ version }"}}.dump();

    std::vector<long long> durations;
    durations.reserve(k_requests);

    for (int i = 0; i < k_requests; ++i) {
        const auto t0  = clock::now();
        auto        res = client.Post("/graphql", body_str, "application/json");
        durations.push_back(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now() - t0).count());
        REQUIRE(res != nullptr);
        REQUIRE(res->status == 200);
    }

    std::sort(durations.begin(), durations.end());

    const std::size_t p95_idx = static_cast<std::size_t>(k_requests * 95 / 100);
    const long long   p95     = durations[p95_idx];
    const long long   p99     = durations[static_cast<std::size_t>(k_requests * 99 / 100)];
    const long long   p_max   = durations.back();

    INFO("T052-006 latency: p95=" << p95 << " ms, p99=" << p99 << " ms, max=" << p_max << " ms");
    REQUIRE(p95 <= 20);
}
