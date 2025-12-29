/**
 * @file isched_built_in_schema.hpp
 * @brief Built-in GraphQL schema for health monitoring and server introspection
 * @author Isched Development Team
 * @date 2025-11-02
 * @version 1.0.0
 */

#pragma once

#include "isched_common.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>

// Forward declarations to avoid circular dependencies
namespace isched::v0_0_1::backend {
    class GqlExecutor;
    class DatabaseManager;
}

namespace isched::v0_0_1::backend {

/**
 * @brief Built-in schema provider for health monitoring and server introspection
 * 
 * Provides a comprehensive GraphQL schema following Spring Boot Actuator patterns
 * for health monitoring, metrics collection, and server configuration introspection.
 * All resolvers use smart pointers for constitutional compliance.
 */
class BuiltInSchema {
public:
    /**
     * @brief Health status enumeration
     */
    enum class HealthStatus {
        UP,
        DOWN,
        OUT_OF_SERVICE,
        UNKNOWN
    };

    /**
     * @brief Server metrics structure
     */
    struct ServerMetrics {
        std::chrono::milliseconds uptime;
        std::size_t active_connections;
        std::size_t total_requests;
        std::size_t failed_requests;
        double average_response_time;
        std::size_t memory_usage_bytes;
        std::size_t thread_count;
        std::chrono::system_clock::time_point start_time;
    };

    /**
     * @brief Configuration section structure
     */
    struct ConfigurationInfo {
        std::string server_version;
        std::string build_timestamp;
        std::string environment;
        nlohmann::json server_config;
        nlohmann::json database_config;
        std::vector<std::string> enabled_features;
    };

    /**
     * @brief Tenant information structure
     */
    struct TenantInfo {
        std::string tenant_id;
        HealthStatus status;
        std::size_t active_connections;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point last_access;
        std::size_t database_size_bytes;
    };

    /**
     * @brief Constructor
     * @param database Database manager for retrieving tenant information
     */
    explicit BuiltInSchema(std::shared_ptr<DatabaseManager> database);

    /**
     * @brief Destructor
     */
    ~BuiltInSchema() noexcept = default;

    bool is_valid() {
        return false;
    }

    static std::unique_ptr<BuiltInSchema> create(std::shared_ptr<DatabaseManager> database);

    // Non-copyable, movable
    BuiltInSchema(const BuiltInSchema&) = delete;
    BuiltInSchema& operator=(const BuiltInSchema&) = delete;
    BuiltInSchema(BuiltInSchema&&) = default;
    BuiltInSchema& operator=(BuiltInSchema&&) = default;

    /**
     * @brief Register all built-in resolvers with the GraphQL executor
     * @param executor GraphQL executor to register resolvers with
     */
    void register_resolvers(GqlExecutor& executor);

    /**
     * @brief Get complete schema definition as JSON
     * @return GraphQL schema definition in JSON format
     */
    [[nodiscard]] nlohmann::json get_schema_definition() const ;

    /**
     * @brief Get server health status
     * @return Current health status with detailed information
     */
    [[nodiscard]] nlohmann::json get_health_status() const ;

    /**
     * @brief Get comprehensive server metrics
     * @return Current server metrics and performance data
     */
    [[nodiscard]] ServerMetrics get_server_metrics() const;

    /**
     * @brief Get server configuration information
     * @return Server configuration and environment details
     */
    [[nodiscard]] ConfigurationInfo get_configuration_info() const;

    /**
     * @brief Get all tenant information
     * @return List of all tenants with their status and metrics
     */
    [[nodiscard]] std::vector<TenantInfo> get_tenant_info() const;

    /**
     * @brief Get detailed application info (Spring Boot actuator style)
     * @return Application information including version, build details, etc.
     */
    [[nodiscard]] nlohmann::json get_app_info() const;

    /**
     * @brief Get environment properties
     * @return Environment variables and system properties
     */
    [[nodiscard]] nlohmann::json get_environment_info() const;

    /**
     * @brief Get JVM-style metrics (adapted for C++ runtime)
     * @return Memory usage, thread information, and runtime metrics
     */
    [[nodiscard]] nlohmann::json get_runtime_metrics() const;

private:
    std::shared_ptr<DatabaseManager> database_;           ///< Database manager for tenant queries
    std::chrono::system_clock::time_point start_time_;    ///< Server start time
    mutable std::atomic<std::size_t> request_counter_;    ///< Total request counter
    mutable std::atomic<std::size_t> error_counter_;      ///< Error counter
    mutable std::atomic<std::size_t> active_connections_; ///< Active connection counter

    /**
     * @brief Create health resolver
     * @return JSON resolver function for health endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_health_resolver() const;

    /**
     * @brief Create info resolver
     * @return JSON resolver function for info endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_info_resolver() const;

    /**
     * @brief Create metrics resolver
     * @return JSON resolver function for metrics endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_metrics_resolver() const;

    /**
     * @brief Create environment resolver
     * @return JSON resolver function for environment endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_env_resolver() const;

    /**
     * @brief Create tenants resolver
     * @return JSON resolver function for tenants endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_tenants_resolver() const;

    /**
     * @brief Create configuration resolver
     * @return JSON resolver function for configuration endpoint
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_config_resolver() const;

    /**
     * @brief Create enhanced schema introspection resolver
     * @return JSON resolver function for enhanced schema introspection
     */
    [[nodiscard]] std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
    create_schema_resolver() const;

    /**
     * @brief Helper to convert HealthStatus to string
     * @param status Health status enum value
     * @return String representation of health status
     */
    [[nodiscard]] static std::string health_status_to_string(HealthStatus status) noexcept;

    /**
     * @brief Helper to get system memory usage
     * @return Memory usage in bytes
     */
    [[nodiscard]] static std::size_t get_memory_usage() noexcept;

    /**
     * @brief Helper to get current thread count
     * @return Number of active threads
     */
    [[nodiscard]] static std::size_t get_thread_count() noexcept;
};

} // namespace isched::v0_0_1::backend