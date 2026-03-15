// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_Server.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the Server class using Boost.Beast async I/O for
 *        both HTTP (POST /graphql) and WebSocket (graphql-transport-ws) transport.
 *
 *        A single Boost.Asio io_context is shared by the HTTP acceptor and the
 *        WebSocket acceptor.  A pool of min_threads..max_threads std::threads all
 *        call io_context::run(), work-stealing completion handlers between them.
 *        Each HTTP connection is served by an async Beast HttpSession that loops
 *        over keep-alive requests without tying up a thread between requests.
 */

#include "isched_Server.hpp"

#include "isched_DatabaseManager.hpp"
#include "isched_GqlExecutor.hpp"
#include "isched_AuthenticationMiddleware.hpp"
#include "isched_SubscriptionBroker.hpp"
#include "isched_MetricsCollector.hpp"
#include "isched_UiAssetRegistry.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
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
// HttpSession — handles one HTTP connection with keep-alive support.
//
// Reads a Beast HTTP request, routes POST /graphql to the GraphQL executor,
// writes the response, then loops back for the next request (keep-alive).
// The session runs entirely on the shared io_context work-stealing pool —
// no thread is ever blocked waiting for I/O.
// ---------------------------------------------------------------------------
namespace isched::v0_0_1::backend {

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    using ExecuteFn = std::function<std::string(const std::string&,
                                                const std::string&,
                                                const std::string&,
                                                const std::string& /*remote_ip*/)>;
    using MetricFn   = std::function<void(double /*ms*/)>;

    HttpSession(tcp::socket socket, net::io_context& ioc,
                ExecuteFn execute_fn, MetricFn metric_fn)
        : strand_(net::make_strand(ioc))
        , stream_(std::move(socket))
        , execute_fn_(std::move(execute_fn))
        , metric_fn_(std::move(metric_fn))
    {}

    void run() {
        net::dispatch(strand_, [self = shared_from_this()]() { self->do_read(); });
    }

private:
    void do_read() {
        req_ = {};
        stream_.expires_after(std::chrono::seconds(30));
        beast::http::async_read(stream_, buf_, req_,
            net::bind_executor(strand_,
                [self = shared_from_this()](beast::error_code ec, std::size_t) {
                    self->on_read(ec);
                }));
    }

    void on_read(beast::error_code ec) {
        if (ec == beast::http::error::end_of_stream) {
            beast::error_code ignored;
            stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
            return;
        }
        if (ec) return;
        handle_request();
    }

    void handle_request() {
        const auto started_at = std::chrono::steady_clock::now();

        // Capture remote IP once — used for rate-limiting and logging.
        std::string remote_ip;
        beast::error_code ec_addr;
        const auto remote_ep = stream_.socket().remote_endpoint(ec_addr);
        if (!ec_addr) remote_ip = remote_ep.address().to_string();

        beast::http::response<beast::http::string_body> res{
            beast::http::status::ok, req_.version()};
        res.set(beast::http::field::server, "isched/1.0");
        res.set(beast::http::field::content_type, "application/json");
        res.set(beast::http::field::access_control_allow_origin, "*");
        res.keep_alive(req_.keep_alive());

        const std::string_view target = req_.target();

        if (req_.method() == beast::http::verb::options) {
            // CORS preflight
            res.result(beast::http::status::no_content);
            res.set(beast::http::field::access_control_allow_methods, "POST, OPTIONS");
            res.set(beast::http::field::access_control_allow_headers,
                    "Content-Type, Authorization");
            res.body() = "";
        } else if (req_.method() == beast::http::verb::get &&
                   (target == "/isched" || target == "/isched/" ||
                    target.starts_with("/isched/"))) {
            // ── Admin UI static asset handler ────────────────────────────────
            // Security headers applied to all /isched responses.
            res.set("X-Content-Type-Options", "nosniff");
            res.set("X-Frame-Options", "DENY");

            const auto& registry = UiAssetRegistry::instance();
            if (!registry.has_index_html()) {
                // Build artefacts missing — Angular project not yet compiled.
                res.result(beast::http::status::service_unavailable);
                res.set(beast::http::field::content_type, "text/plain; charset=utf-8");
                res.body() = "Admin UI assets unavailable";
            } else {
                // Strip the /isched prefix; map empty remainder to /index.html
                std::string asset_path;
                if (target == "/isched" || target == "/isched/") {
                    asset_path = "/index.html";
                } else {
                    // /isched/... → strip first 7 chars ("/isched")
                    asset_path = std::string{target.substr(7)};
                    if (asset_path.empty()) asset_path = "/index.html";
                }

                const auto entry = registry.find(asset_path);
                if (entry) {
                    // ETag cache check
                    const std::string_view if_none_match =
                        req_[beast::http::field::if_none_match];
                    const std::string quoted_etag =
                        '"' + std::string{entry->etag} + '"';
                    if (!if_none_match.empty() && if_none_match == quoted_etag) {
                        // 304 Not Modified
                        beast::http::response<beast::http::empty_body> not_modified{
                            beast::http::status::not_modified, req_.version()};
                        not_modified.set(beast::http::field::server, "isched/1.0");
                        not_modified.set("ETag", quoted_etag);
                        not_modified.set("X-Content-Type-Options", "nosniff");
                        not_modified.set("X-Frame-Options", "DENY");
                        not_modified.keep_alive(req_.keep_alive());
                        not_modified.prepare_payload();
                        auto sp = std::make_shared<
                            beast::http::response<beast::http::empty_body>>(
                            std::move(not_modified));
                        beast::http::async_write(stream_, *sp,
                            net::bind_executor(strand_,
                                [self = shared_from_this(), sp, close = !sp->keep_alive()]
                                (beast::error_code ec2, std::size_t) {
                                    self->on_write(ec2, close);
                                }));
                        return;
                    }
                    // Serve asset
                    res.set(beast::http::field::content_type, std::string{entry->mime_type});
                    res.set(beast::http::field::etag, quoted_etag);
                    res.body().assign(
                        reinterpret_cast<const char*>(entry->data.data()),
                        entry->data.size());
                } else {
                    // Unknown path — check if it looks like a file (has extension)
                    const bool looks_like_file =
                        asset_path.find('.') != std::string::npos;
                    if (looks_like_file) {
                        // Missing static file → 404 JSON
                        res.result(beast::http::status::not_found);
                        res.body() = R"({"errors":[{"message":"asset not found"}]})"
                        "";
                    } else {
                        // Push-state fallback: serve index.html for Angular routes
                        const auto idx = registry.find("/index.html");
                        res.set(beast::http::field::content_type,
                                "text/html; charset=utf-8");
                        res.body().assign(
                            reinterpret_cast<const char*>(idx->data.data()),
                            idx->data.size());
                    }
                }
            }
        } else if (req_.method() == beast::http::verb::post &&
                   req_.target() == "/graphql") {
            auto body = nlohmann::json::parse(req_.body(), nullptr, /*exceptions=*/false);
            if (body.is_discarded()) {
                res.result(beast::http::status::bad_request);
                res.body() = R"({"errors":[{"message":"Request body must be valid JSON"}]})";
            } else {
                std::string query;
                std::string variables_json = "{}";
                if (body.contains("query") && body["query"].is_string())
                    query = body["query"].get<std::string>();
                if (body.contains("variables") && !body["variables"].is_null())
                    variables_json = body["variables"].dump();
                if (query.empty()) {
                    res.result(beast::http::status::bad_request);
                    res.body() = R"({"errors":[{"message":"Missing or empty 'query' field"}]})";
                } else {
                    const std::string auth_hdr =
                        std::string(req_[beast::http::field::authorization]);
                    res.body() = execute_fn_(query, variables_json, auth_hdr, remote_ip);
                }
            }
        } else {
            res.result(beast::http::status::not_found);
            res.body() = R"({"errors":[{"message":"Not found. Use POST /graphql"}]})";
        }

        const double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started_at).count();
        metric_fn_(elapsed_ms);

        res.prepare_payload();
        do_write(std::move(res));
    }

    void do_write(beast::http::response<beast::http::string_body> res) {
        const bool close = !res.keep_alive();
        // Keep the response alive for the async write via shared_ptr
        auto sp = std::make_shared<beast::http::response<beast::http::string_body>>(
            std::move(res));
        beast::http::async_write(stream_, *sp,
            net::bind_executor(strand_,
                [self = shared_from_this(), sp, close]
                (beast::error_code ec, std::size_t) {
                    self->on_write(ec, close);
                }));
    }

    void on_write(beast::error_code ec, bool close) {
        if (ec || close) {
            beast::error_code ignored;
            stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
            return;
        }
        do_read(); // keep-alive: read the next request on this connection
    }

    net::strand<net::io_context::executor_type>        strand_;
    beast::tcp_stream                                  stream_;
    beast::flat_buffer                                 buf_;
    beast::http::request<beast::http::string_body>     req_;
    ExecuteFn                                          execute_fn_;
    MetricFn                                           metric_fn_;
};

// ---------------------------------------------------------------------------
// HttpListener — accepts TCP connections and spawns HttpSessions.
// ---------------------------------------------------------------------------
class HttpListener : public std::enable_shared_from_this<HttpListener> {
public:
    HttpListener(net::io_context& ioc, tcp::endpoint endpoint,
                 HttpSession::ExecuteFn execute_fn,
                 HttpSession::MetricFn  metric_fn)
        : ioc_(ioc)
        , acceptor_(ioc)
        , execute_fn_(std::move(execute_fn))
        , metric_fn_(std::move(metric_fn))
    {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) throw std::runtime_error("HttpListener open: " + ec.message());
        acceptor_.set_option(net::socket_base::reuse_address(true), ec); // NOLINT(cert-err33-c)
        acceptor_.bind(endpoint, ec);
        if (ec) throw std::runtime_error("HttpListener bind: " + ec.message());
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) throw std::runtime_error("HttpListener listen: " + ec.message());
    }

    void run()  { do_accept(); }
    void stop() { stopped_ = true; acceptor_.close(); }
    bool is_open() const { return acceptor_.is_open(); }

private:
    void do_accept() {
        acceptor_.async_accept(net::make_strand(ioc_),
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
                self->on_accept(ec, std::move(socket));
            });
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<HttpSession>(
                std::move(socket), ioc_, execute_fn_, metric_fn_);
            session->run();
        } else if (!stopped_) {
            spdlog::warn("HttpListener accept error: {}", ec.message());
        }
        if (!stopped_) do_accept();
    }

    net::io_context&          ioc_;
    tcp::acceptor             acceptor_;
    bool                      stopped_{false};
    HttpSession::ExecuteFn    execute_fn_;
    HttpSession::MetricFn     metric_fn_;
};

/**
 * @brief PIMPL — owns application-level components and the async I/O pool.
 *
 * The io_context and its worker threads are created in Server::start() and
 * torn down in Server::stop().  HttpListener and WsListener share the same
 * io_context so all I/O (HTTP + WebSocket) benefits from the same thread pool.
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
        gql_executor->set_auth_middleware(auth);
        gql_executor->set_master_secret(secret);
    }

    ~Impl() {
        // Stop metrics publisher first (before broker is destroyed)
        metrics_publisher_stop.store(true, std::memory_order_relaxed);
        if (metrics_publisher_thread.joinable()) metrics_publisher_thread.join();

        // Stop acceptors so no new connections are initiated
        if (http_listener) http_listener->stop();
        if (ws_listener)   ws_listener->stop();

        // Release the work guard and force-stop the io_context so threads exit
        work_guard.reset();
        if (ioc) ioc->stop();

        std::lock_guard<std::mutex> lk(io_threads_mutex);
        for (auto& t : io_threads) {
            if (t.joinable()) t.join();
        }
        io_threads.clear();
    }

    Configuration config;
    TimePoint start_time;
    std::shared_ptr<DatabaseManager> database;
    std::shared_ptr<AuthenticationMiddleware> auth;  ///< Shared with GqlExecutor (T049)
    std::unique_ptr<GqlExecutor> gql_executor;
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<double> avg_response_time{0.0};

    // T050-002: global active subscription count (updated via broker callback)
    std::atomic<uint64_t> total_active_subscriptions{0};

    // Shared async I/O infrastructure — created in Server::start()
    std::unique_ptr<net::io_context> ioc;
    std::optional<net::executor_work_guard<net::io_context::executor_type>> work_guard;
    std::vector<std::thread> io_threads;
    mutable std::mutex io_threads_mutex;
    std::shared_ptr<HttpListener> http_listener;

    // WebSocket server (T042) — shares the same io_context
    std::unique_ptr<SubscriptionBroker> subscription_broker;
    std::shared_ptr<WsListener> ws_listener;

    // Performance metrics collector (T051)
    std::unique_ptr<MetricsCollector> metrics_collector;
    // Background metrics-publisher thread (T051-006/T051-007)
    std::thread metrics_publisher_thread;
    std::atomic<bool> metrics_publisher_stop{false};

    /// Grow the io_context thread pool when subscription load demands it.
    void adapt_pool(std::size_t subscription_count) {
        total_active_subscriptions.store(
            static_cast<uint64_t>(subscription_count), std::memory_order_relaxed);

        if (!ioc) return;
        std::lock_guard<std::mutex> lk(io_threads_mutex);
        const std::size_t pool_size = io_threads.size();
        const std::size_t threshold = static_cast<std::size_t>(pool_size * 0.75);
        if (subscription_count > threshold) {
            const std::size_t desired = std::min(
                pool_size + (subscription_count - threshold),
                config.max_threads);
            if (desired > pool_size && !ioc->stopped()) {
                for (std::size_t i = pool_size; i < desired; ++i) {
                    io_threads.emplace_back([this]() { ioc->run(); });
                }
                spdlog::info("Server: grew io pool to {} threads "
                             "(subscriptions={}, threshold={})",
                             desired, subscription_count, threshold);
            }
        }
    }
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

        // T050-002/003: wire subscription count changes into the io pool
        m_impl->subscription_broker->set_subscription_count_callback(
            [this](std::size_t count) {
                m_impl->adapt_pool(count);
                // T051: keep MetricsCollector's active-subscription counter in sync
                if (m_impl->metrics_collector) {
                    m_impl->metrics_collector->set_active_subscriptions(
                        static_cast<uint64_t>(count));
                }
            });

        // ── Create shared io_context + work guard ────────────────────────────
        m_impl->ioc = std::make_unique<net::io_context>();
        m_impl->work_guard.emplace(net::make_work_guard(*m_impl->ioc));

        const std::string bind_addr = (m_config.host == "localhost")
                                      ? "127.0.0.1" : m_config.host;

        // ── HTTP listener (Beast async) ───────────────────────────────────────
        try {
            tcp::endpoint http_ep{net::ip::make_address(bind_addr),
                                  m_config.port};
            m_impl->http_listener = std::make_shared<HttpListener>(
                *m_impl->ioc, http_ep,
                [this](const std::string& q, const std::string& v,
                       const std::string& a, const std::string& ip) {
                    return execute_graphql(q, v, a, ip);
                },
                [this](double ms) { update_response_time_metric(ms); });
            m_impl->http_listener->run();
            spdlog::info("GraphQL endpoint ready at http://{}:{}/graphql",
                         m_config.host, m_config.port);
        } catch (const std::exception& e) {
            spdlog::error("HTTP listener startup failed: {}", e.what());
            m_impl->work_guard.reset();
            m_status.store(Status::ERROR);
            return false;
        }

        // ── WebSocket listener (T042) — shares the same io_context ───────────
        const uint16_t ws_p = m_config.ws_port != 0
                              ? m_config.ws_port
                              : static_cast<uint16_t>(m_config.port + 1);
        try {
            tcp::endpoint ws_ep{net::ip::make_address(bind_addr), ws_p};
            m_impl->ws_listener = std::make_shared<WsListener>(
                *m_impl->ioc, ws_ep,
                m_impl->gql_executor.get(),
                m_impl->subscription_broker.get(),
                m_impl->auth.get());
            m_impl->ws_listener->run();
            spdlog::info("WebSocket endpoint ready at ws://{}:{}/graphql",
                         m_config.host, ws_p);
        } catch (const std::exception& e) {
            spdlog::error("WebSocket listener startup failed: {}", e.what());
            m_impl->http_listener->stop();
            m_impl->work_guard.reset();
            m_impl->ioc->stop();
            m_status.store(Status::ERROR);
            return false;
        }

        // ── Launch io_context worker threads ─────────────────────────────────
        {
            std::lock_guard<std::mutex> lk(m_impl->io_threads_mutex);
            for (std::size_t i = 0; i < m_config.min_threads; ++i) {
                m_impl->io_threads.emplace_back([this]() {
                    m_impl->ioc->run();
                });
            }
        }
        spdlog::info("Server: io pool started with {} threads (max {})",
                     m_config.min_threads, m_config.max_threads);

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
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Server startup failed: {}", e.what());
        m_status.store(Status::ERROR);
        return false;
    }
}

void Server::set_shutdown_callback(std::function<void()> callback) {
    if (m_impl && m_impl->gql_executor) {
        m_impl->gql_executor->set_shutdown_callback(std::move(callback));
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
        // Stop acceptors so no new connections are accepted
        if (m_impl->http_listener) m_impl->http_listener->stop();
        if (m_impl->ws_listener)   m_impl->ws_listener->stop();

        // Release the work guard, then stop the io_context to drain all threads
        m_impl->work_guard.reset();
        if (m_impl->ioc) m_impl->ioc->stop();

        {
            std::lock_guard<std::mutex> lk(m_impl->io_threads_mutex);
            for (auto& t : m_impl->io_threads) {
                if (t.joinable()) t.join();
            }
            m_impl->io_threads.clear();
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
                               const String& authorization_header,
                               const String& remote_ip) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto request_id = make_request_id();

    m_request_count.fetch_add(1);
    m_impl->total_requests.fetch_add(1);

    // Build auth context from the supplied Authorization header (best-effort;
    // unauthenticated requests are allowed for public queries such as `login`).
    ResolverCtx ctx;
    ctx.remote_ip = remote_ip;
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

