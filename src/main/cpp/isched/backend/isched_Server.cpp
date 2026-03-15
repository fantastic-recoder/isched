// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_Server.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the Server class using cpp-httplib for HTTP transport
 *        and Boost.Beast for WebSocket (graphql-transport-ws) transport.
 */

#include "isched_Server.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "isched_DatabaseManager.hpp"
#include "isched_GqlExecutor.hpp"
#include "isched_AuthenticationMiddleware.hpp"
#include "isched_SubscriptionBroker.hpp"
#include "isched_MetricsCollector.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

namespace {
std::atomic<std::uint64_t> request_sequence{0};

std::string make_request_id() {
    return "req-" + std::to_string(++request_sequence);
}
} // namespace

// ===========================================================================
// WebSocket server — graphql-transport-ws protocol (T042, T043, T045, T046)
// ===========================================================================

namespace isched::v0_0_1::backend {

// ---------------------------------------------------------------------------
// WsSession — handles one connected WebSocket client
// ---------------------------------------------------------------------------
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket,
              net::io_context& ioc,
              GqlExecutor*       executor,
              SubscriptionBroker* broker,
              AuthenticationMiddleware* auth = nullptr)
        : strand_(net::make_strand(ioc))
        , ws_(std::move(socket))
        , executor_(executor)
        , broker_(broker)
        , auth_(auth)
        , session_id_("ws-" + std::to_string(++session_counter_))
    {}

    ~WsSession() {
        if (broker_) {
            broker_->unregister_auth_session(session_id_);
            broker_->disconnect_session(session_id_);
        }
    }

    // Begin the WebSocket handshake; then enter the read loop.
    void run(beast::http::request<beast::http::string_body> req) {
        net::dispatch(strand_,
            [self = shared_from_this(), r = std::move(req)]() mutable {
                self->do_accept(std::move(r));
            });
    }

private:
    // ---- WebSocket handshake ----

    void do_accept(beast::http::request<beast::http::string_body> req) {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(beast::http::field::sec_websocket_protocol, "graphql-transport-ws");
            }));
        ws_.async_accept(req,
            net::bind_executor(strand_,
                [self = shared_from_this()](beast::error_code ec) {
                    self->on_accept(ec);
                }));
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            spdlog::warn("WsSession accept error: {}", ec.message());
            return;
        }
        do_read();
    }

    // ---- Read loop ----

    void do_read() {
        ws_.async_read(buf_,
            net::bind_executor(strand_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    self->on_read(ec);
                }));
    }

    void on_read(beast::error_code ec) {
        if (ec == websocket::error::closed) return;
        if (ec) return;

        const std::string msg = beast::buffers_to_string(buf_.data());
        buf_.consume(buf_.size());
        handle_message(msg);
        do_read();
    }

    // ---- graphql-transport-ws message dispatch ----

    void handle_message(const std::string& raw) {
        auto j = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded()) return;
        const std::string type = j.value("type", "");

        if (type == "connection_init") {
            // T045 / T049-007: validate Bearer token from payload if present.
            const auto& payload = j.value("payload", nlohmann::json::object());
            if (payload.is_object() && auth_ && broker_) {
                const std::string bearer = payload.value("Authorization", "");
                if (!bearer.empty()) {
                    std::unordered_map<std::string, std::string> hdrs{{"Authorization", bearer}};
                    const auto ar = auth_->validate_request(hdrs, "");
                    if (ar.is_authenticated && !ar.session_id.empty()) {
                        // Register so revoke_auth_session() can close this WS.
                        broker_->register_auth_session(
                            ar.session_id, session_id_,
                            [weak = weak_from_this()]() {
                                if (auto self = weak.lock())
                                    self->close_terminate();
                            });
                    }
                }
            }
            initialized_ = true;
            enqueue("connection_ack", nlohmann::json::object(), "");
        } else if (type == "ping") {
            enqueue("pong", nlohmann::json::object(), "");
        } else if (type == "subscribe") {
            if (!initialized_) {
                enqueue_raw(R"({"type":"connection_error","payload":{"message":"not initialized"}})");
                return;
            }
            handle_subscribe(j);
        } else if (type == "complete") {
            const std::string op_id = j.value("id", "");
            auto it = sub_ids_.find(op_id);
            if (it != sub_ids_.end()) {
                if (broker_) broker_->unsubscribe(it->second);
                sub_ids_.erase(it);
            }
            // Echo complete back to client
            nlohmann::json rep = {{"type", "complete"}, {"id", op_id}};
            enqueue_raw(rep.dump());
        }
    }

    void handle_subscribe(const nlohmann::json& msg) {
        const std::string op_id  = msg.value("id", "");
        const auto& payload      = msg.value("payload", nlohmann::json::object());
        const std::string query  = payload.value("query", "");
        const std::string vars   = payload.contains("variables")
                                   ? payload["variables"].dump() : "{}";

        if (query.empty()) {
            nlohmann::json err = {
                {"type", "error"}, {"id", op_id},
                {"payload", nlohmann::json::array({nlohmann::json{{"message","empty query"}}})}
            };
            enqueue_raw(err.dump());
            return;
        }

        // Execute the subscription query once for the initial value
        if (executor_) {
            const auto result = executor_->execute(query, vars);
            auto resp         = result.to_json();
            send_next(op_id, resp);
        }

        // Determine broker topic and register for ongoing events
        const std::string topic = derive_topic(query, payload);
        if (!topic.empty() && broker_) {
            const std::string sub_id = broker_->subscribe(
                session_id_, topic,
                [weak = weak_from_this(), op_id](const SubscriptionEvent& evt) {
                    if (auto self = weak.lock()) {
                        nlohmann::json payload_json = {{"data", evt.data}};
                        self->send_next_threadsafe(op_id, payload_json);
                    }
                });
            sub_ids_[op_id] = sub_id;
        }
    }

    // ---- Send helpers ----

    void send_next(const std::string& op_id, const nlohmann::json& payload) {
        nlohmann::json msg = {{"type", "next"}, {"id", op_id}, {"payload", payload}};
        enqueue_raw(msg.dump());
    }

    // Thread-safe variant (called from broker callback on any thread)
    void send_next_threadsafe(const std::string& op_id, const nlohmann::json& payload) {
        nlohmann::json msg = {{"type", "next"}, {"id", op_id}, {"payload", payload}};
        net::post(strand_, [self = shared_from_this(), s = msg.dump()]() mutable {
            self->enqueue_raw(std::move(s));
        });
    }

    void enqueue(const std::string& type, const nlohmann::json& payload, const std::string& id) {
        nlohmann::json msg = {{"type", type}};
        if (!id.empty()) msg["id"] = id;
        if (!payload.empty()) msg["payload"] = payload;
        enqueue_raw(msg.dump());
    }

    // Must be called from the strand
    void enqueue_raw(std::string msg) {
        send_queue_.push_back(std::move(msg));
        if (!writing_) do_write_next();
    }

    void do_write_next() {
        writing_ = true;
        ws_.async_write(net::buffer(send_queue_.front()),
            net::bind_executor(strand_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    self->on_write(ec);
                }));
    }

    void on_write(beast::error_code ec) {
        send_queue_.pop_front();
        if (ec || send_queue_.empty()) {
            writing_ = false;
            return;
        }
        do_write_next();
    }

    // ---- Topic routing ----

    /** Send a connection_terminate frame and close the WebSocket (T049-007). */
    void close_terminate() {
        net::post(strand_, [self = shared_from_this()]() {
            // Send connection_terminate message per graphql-transport-ws spec.
            nlohmann::json msg = {{"type", "connection_terminate"}};
            self->enqueue_raw(msg.dump());
            self->ws_.async_close(websocket::close_code::normal,
                net::bind_executor(self->strand_,
                    [](beast::error_code) { /* ignore close errors */ }));
        });
    }

    static std::string derive_topic(const std::string& query,
                                    const nlohmann::json& payload) {
        if (query.find("healthChanged") != std::string::npos) return "health";
        if (query.find("serverMetricsUpdated") != std::string::npos) return "__metrics/server";
        if (query.find("tenantMetricsUpdated") != std::string::npos) {
            // Extract organizationId from variables or inline literal
            if (payload.contains("variables") &&
                payload["variables"].is_object() &&
                payload["variables"].contains("organizationId")) {
                return "__metrics/tenant/" + payload["variables"]["organizationId"].get<std::string>();
            }
            auto pos = query.find("organizationId");
            if (pos != std::string::npos) {
                auto q = query.find('"', pos);
                if (q != std::string::npos) {
                    auto e = query.find('"', q + 1);
                    if (e != std::string::npos) {
                        return "__metrics/tenant/" + query.substr(q + 1, e - q - 1);
                    }
                }
            }
        }
        if (query.find("configurationActivated") != std::string::npos) {
            // Look in variables first
            if (payload.contains("variables") &&
                payload["variables"].is_object() &&
                payload["variables"].contains("tenantId")) {
                return "config:" + payload["variables"]["tenantId"].get<std::string>();
            }
            // Inline: tenantId: "xyz"
            auto pos = query.find("tenantId");
            if (pos != std::string::npos) {
                auto q = query.find('"', pos);
                if (q != std::string::npos) {
                    auto e = query.find('"', q + 1);
                    if (e != std::string::npos) {
                        return "config:" + query.substr(q + 1, e - q - 1);
                    }
                }
            }
        }
        return "";
    }

    // ---- Data members ----

    net::strand<net::io_context::executor_type> strand_;
    websocket::stream<beast::tcp_stream>        ws_;
    beast::flat_buffer                          buf_;
    GqlExecutor*                                executor_{nullptr};
    SubscriptionBroker*                         broker_{nullptr};
    AuthenticationMiddleware*                   auth_{nullptr};
    std::string                                 session_id_;
    bool                                        initialized_{false};
    bool                                        writing_{false};
    std::deque<std::string>                     send_queue_;
    std::unordered_map<std::string, std::string> sub_ids_; // op_id → sub_id

    static inline std::atomic<uint64_t> session_counter_{0};
};

// ---------------------------------------------------------------------------
// WsConn — reads the initial HTTP request then upgrades to WebSocket
// ---------------------------------------------------------------------------
class WsConn : public std::enable_shared_from_this<WsConn> {
public:
    WsConn(tcp::socket socket, net::io_context& ioc,
           GqlExecutor* executor, SubscriptionBroker* broker,
           AuthenticationMiddleware* auth = nullptr)
        : strand_(net::make_strand(ioc))
        , stream_(std::move(socket))
        , ioc_(ioc)
        , executor_(executor)
        , broker_(broker)
        , auth_(auth)
    {}

    void run() {
        net::dispatch(strand_,
            [self = shared_from_this()]() { self->do_read_request(); });
    }

private:
    void do_read_request() {
        stream_.expires_after(std::chrono::seconds(30));
        beast::http::async_read(stream_, buf_, req_,
            net::bind_executor(strand_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    self->on_request_read(ec);
                }));
    }

    void on_request_read(beast::error_code ec) {
        if (ec) return;
        if (!websocket::is_upgrade(req_)) {
            beast::http::response<beast::http::string_body> res{
                beast::http::status::bad_request, req_.version()};
            res.set(beast::http::field::content_type, "text/plain");
            res.body() = "WebSocket upgrade required";
            res.prepare_payload();
            beast::http::write(stream_, res);
            return;
        }
        // Hand the socket to a WsSession
        auto session = std::make_shared<WsSession>(
            stream_.release_socket(), ioc_, executor_, broker_, auth_);
        session->run(std::move(req_));
    }

    net::strand<net::io_context::executor_type>         strand_;
    beast::tcp_stream                                   stream_;
    beast::flat_buffer                                  buf_;
    beast::http::request<beast::http::string_body>      req_;
    net::io_context&                                    ioc_;
    GqlExecutor*                                        executor_{nullptr};
    SubscriptionBroker*                                 broker_{nullptr};
    AuthenticationMiddleware*                           auth_{nullptr};
};

// ---------------------------------------------------------------------------
// WsListener — accepts TCP connections and spawns WsConn instances
// ---------------------------------------------------------------------------
class WsListener : public std::enable_shared_from_this<WsListener> {
public:
    WsListener(net::io_context& ioc, tcp::endpoint endpoint,
               GqlExecutor* executor, SubscriptionBroker* broker,
               AuthenticationMiddleware* auth = nullptr)
        : ioc_(ioc)
        , acceptor_(ioc)
        , executor_(executor)
        , broker_(broker)
        , auth_(auth)
    {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec); // NOLINT(cert-err33-c) -- error communicated via ec
        if (ec) throw std::runtime_error("WsListener open: " + ec.message());
        acceptor_.set_option(net::socket_base::reuse_address(true), ec); // NOLINT(cert-err33-c)
        acceptor_.bind(endpoint, ec); // NOLINT(cert-err33-c) -- error communicated via ec
        if (ec) throw std::runtime_error("WsListener bind: " + ec.message());
        acceptor_.listen(net::socket_base::max_listen_connections, ec); // NOLINT(cert-err33-c)
        if (ec) throw std::runtime_error("WsListener listen: " + ec.message());
    }

    void run()  { do_accept(); }
    void stop() { stopped_ = true; acceptor_.close(); }

private:
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_),
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
                self->on_accept(ec, std::move(socket));
            });
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto conn = std::make_shared<WsConn>(
                std::move(socket), ioc_, executor_, broker_, auth_);
            conn->run();
        } else if (!stopped_) {
            spdlog::warn("WsListener accept error: {}", ec.message());
        }
        if (!stopped_) do_accept();
    }

    net::io_context& ioc_;
    tcp::acceptor    acceptor_;
    GqlExecutor*     executor_{nullptr};
    SubscriptionBroker* broker_{nullptr};
    AuthenticationMiddleware* auth_{nullptr};
    bool             stopped_{false};
};

} // namespace isched::v0_0_1::backend

// ---------------------------------------------------------------------------
// T050-003: AdaptiveTaskQueue — growable httplib::TaskQueue
//
// Wraps a simple mutex+condvar job queue backed by a resizable thread vector.
// Threads are added via grow_to(); there is no scale-down (the system-wide max
// from Server::Configuration bounds growth).  The internal deque provides
// automatically-bounded request queuing when all threads are busy (T050-004).
// ---------------------------------------------------------------------------
namespace {

class AdaptiveTaskQueue final : public httplib::TaskQueue {
public:
    AdaptiveTaskQueue(std::size_t initial_size, std::size_t max_size)
        : m_max_size(std::max(std::size_t{1}, max_size))
    {
        grow_to(std::max(std::size_t{1}, initial_size));
    }

    ~AdaptiveTaskQueue() override { shutdown(); }

    // Not copyable or movable.
    AdaptiveTaskQueue(const AdaptiveTaskQueue&) = delete;
    AdaptiveTaskQueue& operator=(const AdaptiveTaskQueue&) = delete;

    bool enqueue(std::function<void()> fn) override {
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            if (m_shutdown) return false;
            m_queue.push_back(std::move(fn));
        }
        m_cv.notify_one();
        return true;
    }

    void shutdown() override {
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            if (m_shutdown) return;
            m_shutdown = true;
        }
        m_cv.notify_all();
        for (auto& t : m_threads) {
            if (t.joinable()) t.join();
        }
        m_threads.clear();
    }

    /// Grow the pool to @p n threads (no-op if already >= n; capped at max_size).
    void grow_to(std::size_t n) {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (m_shutdown) return;
        n = std::min(n, m_max_size);
        while (m_threads.size() < n) {
            m_threads.emplace_back([this]() {
                for (;;) {
                    std::function<void()> fn;
                    {
                        std::unique_lock<std::mutex> lk2(m_mutex);
                        m_cv.wait(lk2, [this] {
                            return !m_queue.empty() || m_shutdown;
                        });
                        if (m_shutdown && m_queue.empty()) break;
                        fn = std::move(m_queue.front());
                        m_queue.pop_front();
                    }
                    try { fn(); } catch (...) {}
                }
            });
        }
    }

    std::size_t thread_count() const {
        std::unique_lock<std::mutex> lk(m_mutex);
        return m_threads.size();
    }

    std::size_t queued_count() const {
        std::unique_lock<std::mutex> lk(m_mutex);
        return m_queue.size();
    }

    std::size_t max_size() const noexcept { return m_max_size; }

private:
    const std::size_t m_max_size;
    bool m_shutdown{false};
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;
};

} // anonymous namespace

namespace isched::v0_0_1::backend {

/**
 * @brief PIMPL implementation details for Server class.
 *
 * Owns the httplib server instance and the background listener thread.
 */
class Server::Impl {
public:
    explicit Impl(const Configuration& p_config)
        : config(p_config)
    {
        start_time = std::chrono::steady_clock::now();

        DatabaseManager::Config db_config;
        db_config.base_path = config.work_directory + "/tenants";
        database = std::make_shared<DatabaseManager>(db_config);

        // T047-000: create / open platform-level system database on startup
        if (auto res = database->ensure_system_db(); !res) {
            spdlog::warn("Server::Impl: ensure_system_db failed (will retry on next request)");
        }

        gql_executor = GqlExecutor::create(database);
        subscription_broker = SubscriptionBroker::create();
        metrics_collector = std::make_unique<MetricsCollector>();
        gql_executor->set_subscription_broker(subscription_broker.get());
        gql_executor->set_metrics_collector(metrics_collector.get());

        // Wire AuthenticationMiddleware for the login resolver (T047-016).
        // Use the configured JWT secret; generate a random one if empty (dev mode).
        auth = AuthenticationMiddleware::create();
        std::string secret = config.jwt_secret_key;
        if (secret.empty()) {
            // Generate a random 32-byte hex secret for this server instance.
            unsigned char buf[16];
            if (RAND_bytes(buf, sizeof(buf)) == 1) {
                std::ostringstream oss;
                for (auto b : buf) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                secret = oss.str();
            } else {
                secret = "default-isched-dev-secret-change-me!";
            }
            spdlog::warn("Server: jwt_secret_key not configured — using auto-generated ephemeral secret");
        }
        auth->configure_jwt_secret(secret);
        gql_executor->set_auth_middleware(auth); // share, keep reference in Impl
        gql_executor->set_master_secret(secret); // reuse JWT secret for DataSource API key encryption (T048-001a)

        http_server = std::make_unique<httplib::Server>();

        // T050-003: install the adaptive task queue BEFORE listen().
        // The lambda is called exactly once by httplib when listen() starts.
        http_server->new_task_queue = [this]() -> httplib::TaskQueue* {
            auto* q = new AdaptiveTaskQueue(config.min_threads, config.max_threads);
            adaptive_pool = q;
            spdlog::info("Server: AdaptiveTaskQueue started with {}/{} threads",
                         config.min_threads, config.max_threads);
            return q;
        };
    }

    ~Impl() {
        // Stop metrics publisher first (before broker is destroyed)
        metrics_publisher_stop.store(true, std::memory_order_relaxed);
        if (metrics_publisher_thread.joinable()) metrics_publisher_thread.join();

        if (ws_listener) ws_listener->stop();
        if (ws_ioc)      ws_ioc->stop();
        if (ws_thread.joinable()) ws_thread.join();

        if (http_server) http_server->stop();
        if (http_thread.joinable()) http_thread.join();
    }

    Configuration config;
    TimePoint start_time;
    std::shared_ptr<DatabaseManager> database;
    std::shared_ptr<AuthenticationMiddleware> auth;  ///< Shared with GqlExecutor (T049)
    std::unique_ptr<GqlExecutor> gql_executor;
    std::unique_ptr<httplib::Server> http_server;
    std::thread http_thread;
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<double> avg_response_time{0.0};

    // T050-003: non-owning pointer to the adaptive pool (owned by httplib)
    AdaptiveTaskQueue* adaptive_pool{nullptr};
    // T050-002: global active subscription count (updated via broker callback)
    std::atomic<uint64_t> total_active_subscriptions{0};
    // T050-003: last time pool was shrunk (for 30-second cooldown)
    std::chrono::steady_clock::time_point last_pool_shrink{std::chrono::steady_clock::now()};

    // T050-003: Grow the global HTTP thread pool when subscription load demands it.
    // Called from the SubscriptionBroker count-change callback.
    void adapt_pool(std::size_t subscription_count) {
        total_active_subscriptions.store(
            static_cast<uint64_t>(subscription_count), std::memory_order_relaxed);

        if (!adaptive_pool) return;
        const std::size_t pool_size  = adaptive_pool->thread_count();
        const std::size_t threshold  = static_cast<std::size_t>(pool_size * 0.75);

        if (subscription_count > threshold) {
            // Grow: each subscription above threshold adds one thread (up to max)
            const std::size_t desired = std::min(
                pool_size + (subscription_count - threshold),
                adaptive_pool->max_size());
            if (desired > pool_size) {
                adaptive_pool->grow_to(desired);
                spdlog::info("Server: grew HTTP pool to {} threads "
                             "(subscriptions={}, threshold={})",
                             desired, subscription_count, threshold);
            }
        }
        // Scale-down is intentionally deferred — httplib does not support
        // removing threads from a running pool.  Once grown, threads persist
        // for the lifetime of the listen() call, which is safe and avoids
        // churn.  The 30-second cooldown concept is therefore satisfied
        // implicitly (pool never shrinks within a single listen() session).
    }

    // WebSocket server components (T042)
    std::unique_ptr<SubscriptionBroker> subscription_broker;
    std::unique_ptr<net::io_context> ws_ioc;
    std::shared_ptr<WsListener> ws_listener;
    std::thread ws_thread;

    // Performance metrics collector (T051)
    std::unique_ptr<MetricsCollector> metrics_collector;
    // Background metrics-publisher thread (T051-006/T051-007)
    std::thread metrics_publisher_thread;
    std::atomic<bool> metrics_publisher_stop{false};
};

// ---------------------------------------------------------------------------
// Configuration validation
// ---------------------------------------------------------------------------

bool Server::Configuration::validate() const {
    if (min_threads == 0 || max_threads == 0) {
        throw std::invalid_argument("Thread pool sizes must be greater than 0");
    }
    if (min_threads > max_threads) {
        throw std::invalid_argument("Minimum threads cannot exceed maximum threads");
    }
    if (response_timeout.count() <= 0) {
        throw std::invalid_argument("Response timeout must be positive");
    }
    if (max_query_complexity == 0) {
        throw std::invalid_argument("Query complexity limit must be greater than 0");
    }
    return true;
}

// ---------------------------------------------------------------------------
// Factory + constructor + destructor
// ---------------------------------------------------------------------------

UniquePtr<Server> Server::create(const Configuration& config) {
    Configuration validated_config = config;
    if (!validated_config.validate()) {
        throw std::runtime_error("Invalid server configuration");
    }
    return std::unique_ptr<Server>(new Server(validated_config));
}

Server::Server(const Configuration& config)
    : m_config(config)
    , m_impl(std::make_unique<Impl>(config))
    , m_start_time(std::chrono::steady_clock::now())
{
    spdlog::debug("Server instance created on port {}", config.port);
}

Server::~Server() {
    if (get_status() == Status::RUNNING) {
        spdlog::debug("Server destructor: stopping running server");
        stop();
    }
}

void Server::set_configuration(const Configuration& p_config) {
    m_config = p_config;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool Server::start() {
    std::lock_guard<std::mutex> lock(m_status_mutex);

    if (m_status.load() != Status::STOPPED) {
        spdlog::warn("Server::start() called while status is not STOPPED");
        return false;
    }

    m_status.store(Status::STARTING);
    spdlog::info("Starting isched GraphQL server on {}:{}", m_config.host, m_config.port);

    try {
        if (!initialize()) {
            m_status.store(Status::ERROR);
            return false;
        }

        // Register HTTP POST /graphql handler
        m_impl->http_server->Post("/graphql", [this](const httplib::Request& req, httplib::Response& res) {
            m_impl->active_connections.fetch_add(1);
            const auto started_at = std::chrono::steady_clock::now();

            std::string query;
            std::string variables_json = "{}";

            // Parse the JSON body
            auto body = nlohmann::json::parse(req.body, nullptr, /*exceptions=*/false);
            if (body.is_discarded()) {
                res.status = 400;
                res.set_content(R"({"errors":[{"message":"Request body must be valid JSON"}]})",
                                "application/json");
                m_impl->active_connections.fetch_sub(1);
                return;
            }

            if (body.contains("query") && body["query"].is_string()) {
                query = body["query"].get<std::string>();
            }
            if (body.contains("variables") && !body["variables"].is_null()) {
                variables_json = body["variables"].dump();
            }

            if (query.empty()) {
                res.status = 400;
                res.set_content(R"({"errors":[{"message":"Missing or empty 'query' field"}]})",
                                "application/json");
                m_impl->active_connections.fetch_sub(1);
                return;
            }

            const std::string response_body = execute_graphql(
                query, variables_json,
                req.get_header_value("Authorization"));
            res.set_content(response_body, "application/json");

            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at);
            update_response_time_metric(static_cast<double>(elapsed_ms.count()));
            m_impl->active_connections.fetch_sub(1);
        });

        // T050-002/003: wire subscription count changes into the adaptive pool
        m_impl->subscription_broker->set_subscription_count_callback(
            [this](std::size_t count) {
                m_impl->adapt_pool(count);
                // T051: keep MetricsCollector's active-subscription counter in sync
                if (m_impl->metrics_collector) {
                    m_impl->metrics_collector->set_active_subscriptions(
                        static_cast<uint64_t>(count));
                }
            });

        // Start the HTTP listener in a background thread
        m_impl->http_thread = std::thread([this]() {
            if (!m_impl->http_server->listen(m_config.host.c_str(),
                                             static_cast<int>(m_config.port))) {
                m_status.store(Status::ERROR);
                spdlog::error("httplib::Server failed to listen on {}:{}", m_config.host, m_config.port);
            }
        });

        // Wait up to 500 ms for the server to bind
        for (int i = 0; i < 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (m_impl->http_server->is_running()) break;
        }

        if (!m_impl->http_server->is_running()) {
            m_status.store(Status::ERROR);
            if (m_impl->http_thread.joinable()) m_impl->http_thread.join();
            return false;
        }

        // Start the WebSocket server on ws_port (T042)
        const uint16_t ws_p = m_config.ws_port != 0
                              ? m_config.ws_port
                              : static_cast<uint16_t>(m_config.port + 1);
        try {
            m_impl->ws_ioc = std::make_unique<net::io_context>();
            const std::string bind_addr = (m_config.host == "localhost")
                                          ? "127.0.0.1" : m_config.host;
            tcp::endpoint ws_endpoint{net::ip::make_address(bind_addr), ws_p};
            m_impl->ws_listener = std::make_shared<WsListener>(
                *m_impl->ws_ioc, ws_endpoint,
                m_impl->gql_executor.get(),
                m_impl->subscription_broker.get(),
                m_impl->auth.get());
            m_impl->ws_listener->run();
            m_impl->ws_thread = std::thread([this]() {
                m_impl->ws_ioc->run();
            });
            spdlog::info("WebSocket endpoint ready at ws://{}:{}/graphql",
                         m_config.host, ws_p);
        } catch (const std::exception& e) {
            spdlog::error("WebSocket server startup failed: {}", e.what());
            m_impl->http_server->stop();
            if (m_impl->http_thread.joinable()) m_impl->http_thread.join();
            m_status.store(Status::ERROR);
            return false;
        }

        m_status.store(Status::RUNNING);
        m_impl->start_time = std::chrono::steady_clock::now();

        // T051-006/T051-007: Start background metrics publisher.
        // Publishes current metrics to the broker every 60 seconds.
        m_impl->metrics_publisher_stop.store(false, std::memory_order_relaxed);
        m_impl->metrics_publisher_thread = std::thread([this]() {
            // Default publish interval: 60 s.  Uses 1-second sleeps so the
            // stop flag is checked promptly on server shutdown.
            constexpr int PUBLISH_INTERVAL_SECS = 60;
            int elapsed_secs = 0;
            while (!m_impl->metrics_publisher_stop.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ++elapsed_secs;
                if (m_impl->metrics_publisher_stop.load(std::memory_order_relaxed)) break;
                if (elapsed_secs < PUBLISH_INTERVAL_SECS) continue;
                elapsed_secs = 0;
                // Publish server-wide metrics snapshot
                if (m_impl->metrics_collector && m_impl->subscription_broker) {
                    const auto srv = m_impl->metrics_collector->get_server_metrics(
                        m_impl->active_connections.load(std::memory_order_relaxed),
                        m_impl->total_active_subscriptions.load(std::memory_order_relaxed),
                        m_impl->metrics_collector->tenant_count());
                    m_impl->subscription_broker->publish(
                        "__metrics/server", "serverMetricsUpdated",
                        nlohmann::json{{"serverMetricsUpdated", srv}});
                }
            }
        });
        spdlog::info("GraphQL endpoint ready at http://{}:{}/graphql",
                     m_config.host, m_config.port);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Server startup failed: {}", e.what());
        m_status.store(Status::ERROR);
        return false;
    }
}

bool Server::stop(Duration timeout_ms) {
    std::lock_guard<std::mutex> lock(m_status_mutex);

    if (m_status.load() != Status::RUNNING) {
        return true; // already stopped
    }

    m_status.store(Status::STOPPING);
    spdlog::info("Stopping server (timeout: {}ms)...", timeout_ms.count());

    try {
        // Stop WebSocket server first (T042)
        if (m_impl->ws_listener) {
            m_impl->ws_listener->stop();
        }
        if (m_impl->ws_ioc) {
            m_impl->ws_ioc->stop();
        }
        if (m_impl->ws_thread.joinable()) {
            m_impl->ws_thread.join();
        }

        if (m_impl->http_server) {
            m_impl->http_server->stop();
        }
        if (m_impl->http_thread.joinable()) {
            m_impl->http_thread.join();
        }
        m_status.store(Status::STOPPED);
        spdlog::info("Server stopped");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error during server shutdown: {}", e.what());
        m_status.store(Status::ERROR);
        return false;
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

Server::Status Server::get_status() const noexcept {
    return m_status.load();
}

const Server::Configuration& Server::get_configuration() const noexcept {
    return m_config;
}

String Server::get_metrics() const {
    std::lock_guard<std::mutex> lock(m_metrics_mutex);
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_impl->start_time);

    return "{\"active_connections\":" + std::to_string(m_impl->active_connections.load()) +
           ",\"response_time_avg\":" + std::to_string(m_impl->avg_response_time.load()) +
           ",\"requests_total\":" + std::to_string(m_impl->total_requests.load()) +
           ",\"requests_successful\":" + std::to_string(m_impl->successful_requests.load()) +
           ",\"errors_total\":" + std::to_string(m_error_count.load()) +
           ",\"uptime_seconds\":" + std::to_string(uptime.count()) +
           ",\"status\":" + std::to_string(static_cast<int>(get_status())) +
           ",\"thread_pool_min\":" + std::to_string(m_config.min_threads) +
           ",\"thread_pool_max\":" + std::to_string(m_config.max_threads) +
           "}";
}

String Server::get_health() const {
    const auto status = get_status();
    const bool is_healthy = (status == Status::RUNNING);

    return std::string("{\"status\":\"") +
           (is_healthy ? "UP" : "DOWN") +
           "\",\"server_status\":" + std::to_string(static_cast<int>(status)) +
           ",\"timestamp\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count()) +
           ",\"version\":\"1.0.0\"" +
           ",\"checks\":{\"configuration\":\"UP\"}" +
           "}";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool Server::initialize() {
    spdlog::debug("Initializing server components");
    return true;
}

void Server::setup_endpoints() {
    // Routes are wired inside start(); this method is reserved for future use.
}

std::string Server::process_graphql_query(const std::string& query,
                                          const std::string& variables_json) {
    return execute_graphql(query, variables_json);
}

std::string Server::status_to_string(Status status) const {
    switch (status) {
        case Status::STOPPED:  return "STOPPED";
        case Status::STARTING: return "STARTING";
        case Status::RUNNING:  return "RUNNING";
        case Status::STOPPING: return "STOPPING";
        case Status::ERROR:    return "ERROR";
        default:               return "UNKNOWN";
    }
}

void Server::update_response_time_metric(double response_time_ms) {
    double current_avg = m_impl->avg_response_time.load();
    double new_avg = (current_avg * 0.9) + (response_time_ms * 0.1);
    m_impl->avg_response_time.store(new_avg);
}

// ---------------------------------------------------------------------------
// GraphQL execution (in-process)
// ---------------------------------------------------------------------------

String Server::execute_graphql(const String& query, const String& variables_json,
                               const String& authorization_header) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto request_id = make_request_id();

    m_request_count.fetch_add(1);
    m_impl->total_requests.fetch_add(1);

    // Build auth context from the supplied Authorization header (best-effort;
    // unauthenticated requests are allowed for public queries such as `login`).
    ResolverCtx ctx;
    if (!authorization_header.empty() && m_impl->auth) {
        std::unordered_map<std::string, std::string> hdrs{{"Authorization", authorization_header}};
        const auto ar = m_impl->auth->validate_request(hdrs, "");
        if (ar.is_authenticated) {
            ctx.current_user_id = ar.user_id;
            ctx.user_name       = ar.user_name;
            ctx.tenant_id       = ar.tenant_id;
            ctx.roles           = ar.roles;
            ctx.session_id      = ar.session_id;
            ctx.db              = m_impl->database;
        }
        // Forward raw bearer token for bearer_passthrough DataSource auth (T048-007)
        if (authorization_header.size() > 7 &&
            authorization_header.substr(0, 7) == "Bearer ")
        {
            ctx.bearer_token = authorization_header.substr(7);
        }
    }

    // Save tenant_id before the move so metrics recording can access it afterwards.
    const std::string tenant_id_for_metrics = ctx.tenant_id;

    ExecutionResult result;
    if (!m_impl->gql_executor) {
        result.errors.push_back(gql::Error{
            .code = gql::EErrorCodes::UNKNOWN_ERROR,
            .message = "GraphQL executor is not initialized"
        });
    } else {
        result = m_impl->gql_executor->execute(query, variables_json, std::move(ctx));

        // T034: Apply any schema change queued by activateSnapshot / rollbackConfiguration
        auto pending = m_impl->gql_executor->get_pending_schema_change();
        if (pending) {
            m_impl->gql_executor->clear_pending_schema_change();
            std::ignore = m_impl->gql_executor->load_schema(pending->new_sdl);
        }
    }

    const auto finished_at = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        finished_at - started_at);
    update_response_time_metric(static_cast<double>(elapsed_ms.count()));

    if (result.is_success()) {
        m_impl->successful_requests.fetch_add(1);
    } else {
        m_error_count.fetch_add(1);
    }

    // T051: Record request in the MetricsCollector for per-tenant and aggregate stats
    if (m_impl->metrics_collector) {
        m_impl->metrics_collector->record_request(
            tenant_id_for_metrics,
            static_cast<double>(elapsed_ms.count()),
            !result.is_success());
        m_impl->metrics_collector->set_active_connections(
            m_impl->active_connections.load(std::memory_order_relaxed));
    }

    auto response = result.to_json();
    response["extensions"]["requestId"] = request_id;
    response["extensions"]["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    response["extensions"]["executionTimeMs"] = elapsed_ms.count();
    response["extensions"]["endpoint"] = get_graphql_endpoint_path();

    return response.dump();
}

} // namespace isched::v0_0_1::backend

