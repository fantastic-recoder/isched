/**
 * @file isched_server.hpp
 * @brief Core server class for isched Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 * 
 * This file contains the main Server class that orchestrates the Universal Application Server Backend.
 * The server provides GraphQL endpoints, manages multi-tenant architecture, coordinates CLI processes,
 * and maintains 20ms response time targets.
 * 
 * @example Basic server usage:
 * @code{cpp}
 * #include "isched_server.hpp"
 * 
 * auto server = isched::v0_0_1::backend::Server::create();
 * server->configure_port(8080);
 * server->configure_thread_pool(10, 100); // min 10, max 100 threads
 * server->start();
 * @endcode
 * 
 * @note All resource management uses smart pointers as required by FR-021.
 * @see FR-001, FR-002, FR-009, FR-010, FR-011 in spec.md
 */

#pragma once

#include "isched_common.hpp"
#include <atomic>
#include <mutex>

// Forward declarations for external dependencies
namespace restbed {
    class Service;
    class Session;
    class Resource;
}

namespace spdlog {
    class logger;
}

// Forward declarations for external dependencies
namespace restbed {
    class Service;
    class Resource;
    class Session;
}

namespace spdlog {
    class logger;
}

namespace isched::v0_0_1::backend {

/**
 * @class Server
 * @brief Main server class for Universal Application Server Backend
 * 
 * The Server class is the primary orchestrator for the isched backend system.
 * It manages HTTP endpoints, coordinates tenant processes, handles GraphQL requests,
 * and maintains the overall server lifecycle.
 * 
 * ## Key Features:
 * - Multi-tenant process management with adaptive thread pools
 * - GraphQL endpoint serving with specification compliance
 * - 20ms response time targets with performance monitoring
 * - Smart pointer-based resource management (no raw pointers)
 * - Built-in health monitoring endpoints
 * - CLI process coordination via IPC
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
        String host;                             ///< Bind address
        size_t min_threads;                      ///< Minimum thread pool size
        size_t max_threads;                      ///< Maximum thread pool size
        Duration response_timeout;               ///< Target response time (ms)
        String work_directory;                   ///< Working directory for tenant data
        bool enable_introspection;               ///< Enable GraphQL introspection
        size_t max_query_complexity;             ///< Maximum GraphQL query complexity
        
        /**
         * @brief Default constructor with sensible defaults
         */
        Configuration() 
            : port(8080)
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
     * - Tenant process coordination
     * - IPC resource cleanup
     */
    virtual ~Server();

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
     * 2. Setup tenant manager
     * 3. Initialize GraphQL executor
     * 4. Start CLI coordinator
     * 5. Bind HTTP endpoints
     * 6. Begin accepting connections
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
     * 3. Shutdown tenant processes
     * 4. Cleanup IPC resources
     * 5. Stop thread pool
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
     * - CLI process status
     * - Memory and resource utilization
     * 
     * @note This method is used by the built-in GraphQL health monitoring queries
     */
    String get_health() const;

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
     * @brief Setup GraphQL endpoint handler
     * @param resource Restbed resource to configure
     */
    void setup_graphql_endpoint(SharedPtr<restbed::Resource> resource);

    /**
     * @brief Setup health monitoring endpoints
     * @param resource Restbed resource to configure
     */
    void setup_health_endpoints(SharedPtr<restbed::Resource> resource);

    /**
     * @brief Handle GraphQL request processing
     * @param session Restbed session for request/response
     */
    void handle_graphql_request(const SharedPtr<restbed::Session>& session);

    /**
     * @brief Handle health check requests
     * @param session Restbed session for request/response
     */
    void handle_health_request(const SharedPtr<restbed::Session>& session);

    /**
     * @brief Send JSON response helper method
     * @param session Restbed session
     * @param response JSON response string
     * @param status_code HTTP status code (default 200)
     */
    void send_json_response(const SharedPtr<restbed::Session>& session, const std::string& response, int status_code = 200);

    /**
     * @brief Send error response helper method
     * @param session Restbed session
     * @param error_message Error message
     * @param status_code HTTP status code (default 400)
     */
    void send_error_response(const SharedPtr<restbed::Session>& session, const std::string& error_message, int status_code = 400);

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
    SharedPtr<restbed::Service> m_http_service;               ///< HTTP service instance
    SharedPtr<spdlog::logger> m_logger;                      ///< Logging instance
    
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