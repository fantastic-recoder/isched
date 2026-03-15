// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_Server.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Core server class for isched Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 * 
 * This file contains the main Server class that orchestrates the Universal Application Server Backend.
 * The server exposes GraphQL over HTTP and WebSocket as the active transport surface,
 * manages tenant-aware runtime state, and maintains 20ms response time targets.
 * 
 * @example Basic server usage:
 * @code{cpp}
 * #include "isched_Server.hpp"
 * 
 * auto server = isched::v0_0_1::backend::Server::create();
 * server->configure_port(8080);
 * server->configure_thread_pool(10, 100); // min 10, max 100 threads
 * server->start();
 * @endcode
 * 
 * @note All resource management uses smart pointers as required by FR-021.
 * @see FR-001, FR-002, FR-003, FR-010, FR-016 in spec.md
 */

#pragma once

#include "isched_common.hpp"
#include <atomic>
#include <mutex>
#include <boost/url.hpp>

namespace spdlog {
    class logger;
}

namespace isched::v0_0_1::backend {

/**
 * @class Server
 * @brief Main server class for Universal Application Server Backend
 * 
 * The Server class is the primary orchestrator for the isched backend system.
 * It manages GraphQL transport endpoints, coordinates tenant-aware runtime state, handles GraphQL requests,
 * and maintains the overall server lifecycle.
 * 
 * ## Key Features:
 * - GraphQL-only transport serving with specification compliance
 * - 20ms response time targets with performance monitoring
 * - Smart pointer-based resource management (no raw pointers)
 * - Built-in health and server information via GraphQL fields
 * 
 * ## Performance Requirements:
 * - Target response time: 20ms (95th percentile)
 * - Concurrent client support: thousands of connections
 * - Adaptive thread pool sizing based on load
 * 
 * ## Thread Safety:
 * All public methods are thread-safe unless explicitly documented otherwise.
 * 
 * @invariant Server instance must be created via Server::create() factory method
 * @invariant All resources managed via smart pointers (FR-021 compliance)
 */
class Server {
public:
    /**
     * @brief Server configuration structure
     * 
     * Contains all configurable parameters for server operation.
     * All parameters have sensible defaults for immediate operation.
     */
    struct Configuration {
        uint16_t port;                           ///< HTTP server port
        uint16_t ws_port;                        ///< WebSocket server port (0 = port + 1)
        String host;                             ///< Bind address
        size_t min_threads;                      ///< Minimum thread pool size
        size_t max_threads;                      ///< Maximum thread pool size
        Duration response_timeout;               ///< Target response time (ms)
        String work_directory;                   ///< Working directory for tenant data
        bool enable_introspection;               ///< Enable GraphQL introspection
        size_t max_query_complexity;             ///< Maximum GraphQL query complexity
        /// Secret key used for signing/verifying JWTs.
        /// Must be at least 32 bytes; a random default is generated at startup if empty.
        String jwt_secret_key;
        
        /**
         * @brief Default constructor with sensible defaults
         */
        Configuration() 
            : port(8080)
            , ws_port(0)
            , host("localhost")
            , min_threads(4)
            , max_threads(100)
            , response_timeout(20)
            , work_directory("./data")
            , enable_introspection(true)
            , max_query_complexity(1000)
        {}
        
        /**
         * @brief Validate configuration parameters
         * @return true if configuration is valid, false otherwise
         * @throw std::invalid_argument if critical parameters are invalid
         */
        bool validate() const;
    };

    /**
     * @brief Server status enumeration
     */
    enum class Status {
        STOPPED,        ///< Server is not running
        STARTING,       ///< Server is in startup process
        RUNNING,        ///< Server is actively serving requests
        STOPPING,       ///< Server is shutting down gracefully
        ERROR          ///< Server encountered an error state
    };

    /**
     * @brief Factory method to create server instance
     * @param config Optional configuration (uses defaults if not provided)
     * @return Unique pointer to configured server instance
     * @throw std::runtime_error if server creation fails
     * 
     * @example Creating a server with custom configuration:
     * @code{cpp}
     * Server::Configuration config;
     * config.port = 3000;
     * config.max_threads = 50;
     * auto server = Server::create(config);
     * @endcode
     */
    static UniquePtr<Server> create(const Configuration& config = Configuration{});

    /**
     * @brief Virtual destructor for proper cleanup
     * 
     * Ensures graceful shutdown of all resources including:
     * - Active connections termination
     * - Thread pool cleanup
     * - Tenant runtime state cleanup
     */
    virtual ~Server();

    void set_configuration(const Configuration & p_config);

    bool is_running() const {
        return get_status() == Server::Status::RUNNING;
    }

    boost::urls::url get_playground_url() const {
        return boost::urls::url();
    }

    bool has_playground_endpoint() const {
        return !get_playground_url().empty();
    }

    bool has_graphql_endpoint() const {
        return true;
    }

    [[nodiscard]] String get_graphql_endpoint_path() const {
        return "/graphql";
    }

    // Prevent copying and moving (RAII resource management)
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    /**
     * @brief Start the server and begin accepting requests
     * @return true if startup successful, false otherwise
     * @throw std::runtime_error if startup fails critically
     * 
     * Startup sequence:
     * 1. Initialize thread pool
    * 2. Setup tenant-aware runtime state
    * 3. Initialize GraphQL executor
    * 4. Bind GraphQL transport endpoints
    * 5. Begin accepting connections
     * 
     * @post Server status becomes RUNNING on success
     * @post All health monitoring endpoints are active
     * @post Built-in GraphQL schema is available
     */
    bool start();

    /**
     * @brief Stop the server gracefully
     * @param timeout_ms Maximum time to wait for graceful shutdown (default: 5000ms)
     * @return true if shutdown completed within timeout, false otherwise
     * 
     * Shutdown sequence:
     * 1. Stop accepting new connections
     * 2. Complete active requests (up to timeout)
     * 3. Shutdown tenant runtime state
     * 4. Stop thread pool
     * 
     * @post Server status becomes STOPPED
     * @note Active connections receive proper termination notices
     */
    bool stop(Duration timeout_ms = Duration{5000});

    /**
     * @brief Get current server status
     * @return Current status enumeration value
     * @note Thread-safe operation
     */
    Status get_status() const noexcept;

    /**
     * @brief Get current server configuration
     * @return Const reference to active configuration
     * @note Thread-safe operation (configuration is immutable after startup)
     */
    const Configuration& get_configuration() const noexcept;

    /**
     * @brief Get server performance metrics
     * @return JSON string containing current metrics
     * 
     * Metrics include:
     * - Active connection count
     * - Average response time (95th percentile)
     * - Thread pool utilization
     * - Memory usage statistics
     * - Request count and error rates
     * 
     * @example Metrics format:
     * @code{json}
     * {
     *   "active_connections": 42,
     *   "response_time_95p": 18.5,
     *   "thread_pool_utilization": 0.65,
     *   "memory_usage_mb": 256,
     *   "requests_total": 15420,
     *   "errors_total": 12,
     *   "uptime_seconds": 3600
     * }
     * @endcode
     */
    String get_metrics() const;

    /**
     * @brief Get server health status for monitoring
     * @return JSON string with health information
     * 
     * Health check includes:
     * - Server operational status
     * - Database connectivity
    * - GraphQL executor readiness
    * - Memory and resource utilization
     * 
     * @note This method is used by the built-in GraphQL health monitoring queries
     */
    String get_health() const;

    /**
     * @brief Execute a GraphQL operation against the server's active schema.
     * @param query GraphQL query string
     * @param variables_json GraphQL variables as a JSON string
     * @param authorization_header Value of the HTTP Authorization header (optional)
     * @return JSON-encoded GraphQL response payload
     */
    String execute_graphql(const String& query, const String& variables_json = "{}",
                           const String& authorization_header = "");

private:
    /**
     * @brief Private constructor (use factory method)
     * @param config Server configuration
     */
    explicit Server(const Configuration& config);

    /**
     * @brief Initialize server components
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Setup HTTP endpoints and routing
     */
    void setup_endpoints();

    /**
     * @brief Process GraphQL query
     * @param query GraphQL query string
     * @param variables_json Query variables as JSON string
     * @return JSON response string
     */
    std::string process_graphql_query(const std::string& query, const std::string& variables_json);

    /**
     * @brief Convert status enum to string
     * @param status Server status
     * @return String representation
     */
    std::string status_to_string(Status status) const;

    /**
     * @brief Update response time metric
     * @param response_time_ms Response time in milliseconds
     */
    void update_response_time_metric(double response_time_ms);

    // Member variables (all managed via smart pointers)
    Configuration m_config;                                    ///< Server configuration
    std::atomic<Status> m_status{Status::STOPPED};           ///< Current server status

    // Core components (forward declared, defined in implementation)
    class Impl;                                               ///< PIMPL idiom for implementation details
    UniquePtr<Impl> m_impl;                                   ///< Implementation details

    // Thread synchronization
    mutable std::mutex m_status_mutex;                        ///< Status access synchronization
    mutable std::mutex m_metrics_mutex;                       ///< Metrics access synchronization

    // Performance tracking
    std::atomic<uint64_t> m_request_count{0};                ///< Total request counter
    std::atomic<uint64_t> m_error_count{0};                  ///< Total error counter
    TimePoint m_start_time;                                   ///< Server start timestamp
};

} // namespace isched::v0_0_1::backend