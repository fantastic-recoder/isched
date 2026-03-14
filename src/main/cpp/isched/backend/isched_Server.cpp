// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_Server.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the Server class using cpp-httplib for HTTP transport.
 *
 * GraphQL POST requests arrive at /graphql, are parsed and dispatched to
 * GqlExecutor::execute(), and the JSON response is returned to the client.
 *
 * @note All implementations follow C++ Core Guidelines with smart pointer usage.
 */

#include "isched_Server.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "isched_DatabaseManager.hpp"
#include "isched_GqlExecutor.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {
std::atomic<std::uint64_t> request_sequence{0};

std::string make_request_id() {
    return "req-" + std::to_string(++request_sequence);
}
} // namespace

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
        gql_executor = GqlExecutor::create(database);

        http_server = std::make_unique<httplib::Server>();
    }

    ~Impl() {
        if (http_server) {
            http_server->stop();
        }
        if (http_thread.joinable()) {
            http_thread.join();
        }
    }

    Configuration config;
    TimePoint start_time;
    std::shared_ptr<DatabaseManager> database;
    std::unique_ptr<GqlExecutor> gql_executor;
    std::unique_ptr<httplib::Server> http_server;
    std::thread http_thread;
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<double> avg_response_time{0.0};
};

// ---------------------------------------------------------------------------
// Configuration validation
// ---------------------------------------------------------------------------

bool Server::Configuration::validate() const {
    if (port > 65535) {
        throw std::invalid_argument("Port must be between 1 and 65535");
    }
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

            const std::string response_body = execute_graphql(query, variables_json);
            res.set_content(response_body, "application/json");

            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at);
            update_response_time_metric(static_cast<double>(elapsed_ms.count()));
            m_impl->active_connections.fetch_sub(1);
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

        m_status.store(Status::RUNNING);
        m_impl->start_time = std::chrono::steady_clock::now();
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

String Server::execute_graphql(const String& query, const String& variables_json) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto request_id = make_request_id();

    m_request_count.fetch_add(1);
    m_impl->total_requests.fetch_add(1);

    ExecutionResult result;
    if (!m_impl->gql_executor) {
        result.errors.push_back(gql::Error{
            .code = gql::EErrorCodes::UNKNOWN_ERROR,
            .message = "GraphQL executor is not initialized"
        });
    } else {
        result = m_impl->gql_executor->execute(query);
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

    auto response = result.to_json();
    response["extensions"]["requestId"] = request_id;
    response["extensions"]["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    response["extensions"]["executionTimeMs"] = elapsed_ms.count();
    response["extensions"]["endpoint"] = get_graphql_endpoint_path();

    return response.dump();
}

} // namespace isched::v0_0_1::backend

