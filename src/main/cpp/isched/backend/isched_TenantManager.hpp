// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_TenantManager.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Tenant management system for multi-tenant process pool with database isolation
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * This file contains the TenantManager class responsible for managing multiple tenants
 * in the Universal Application Server Backend. Each tenant operates in isolation with
 * dedicated database connections and process resources.
 * 
 * @example Basic tenant management usage:
 * @code{cpp}
 * #include "isched_TenantManager.hpp"
 * 
 * auto manager = isched::v0_0_1::backend::TenantManager::create();
 * manager->configure_pool_size(10, 50); // min 10, max 50 processes
 * manager->start();
 * 
 * auto tenant_id = manager->create_tenant("example-org", tenant_config);
 * auto session = manager->get_tenant_session(tenant_id);
 * @endcode
 * 
 * @note All resource management uses smart pointers as required by FR-021.
 * @see FR-005, FR-006, FR-007 in spec.md for multi-tenant requirements
 */

#pragma once

#include "isched_common.hpp"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace isched::v0_0_1::backend {

/**
 * @brief Multi-tenant process pool manager with database isolation
 * 
 * The TenantManager class provides isolated execution environments for multiple
 * tenants, ensuring data isolation, resource management, and load balancing
 * across tenant processes.
 * 
 * Key Features:
 * - Process pool with adaptive sizing
 * - Per-tenant SQLite database isolation
 * - Load balancing algorithm for tenant assignment
 * - Smart pointer-based resource management
 * - Thread-safe operations with atomic counters
 * 
 * @note This class follows the PIMPL pattern for implementation hiding
 */
class TenantManager {
public:
    // Type aliases for smart pointer usage
    using UniquePtr = std::unique_ptr<TenantManager>;
    using SharedPtr = std::shared_ptr<TenantManager>;
    using String = std::string;
    using TenantId = std::string;
    using ProcessId = uint32_t;
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;

    /**
     * @brief Configuration for tenant pool management
     */
    struct PoolConfiguration {
        size_t min_processes;               ///< Minimum processes to maintain
        size_t max_processes;               ///< Maximum processes allowed
        Duration process_idle_timeout;      ///< Time before idle process cleanup
        size_t max_tenants_per_process;     ///< Maximum tenants per process
        String database_root_path;          ///< Root directory for tenant databases
        bool enable_process_recycling;      ///< Enable process recycling for memory management
        Duration health_check_interval;     ///< Process health check frequency
        
        /**
         * @brief Default constructor with production defaults
         */
        PoolConfiguration() 
            : min_processes(4)
            , max_processes(50)
            , process_idle_timeout(300000)  // 5 minutes
            , max_tenants_per_process(10)
            , database_root_path("./data/tenants")
            , enable_process_recycling(true)
            , health_check_interval(30000)  // 30 seconds
        {}
        
        /**
         * @brief Validate pool configuration parameters
         * @return true if configuration is valid
         * @throw std::invalid_argument if critical parameters are invalid
         */
        bool validate() const;
    };

    /**
     * @brief Tenant configuration and metadata
     */
    struct TenantConfiguration {
        TenantId tenant_id;                 ///< Unique tenant identifier
        String organization_name;           ///< Organization display name
        String domain;                      ///< Optional organization domain
        size_t user_limit;                  ///< Maximum users for this tenant
        size_t storage_limit_mb;            ///< Storage limit in megabytes
        bool enable_introspection;          ///< Enable GraphQL introspection
        size_t max_query_complexity;        ///< Maximum GraphQL query complexity
        Duration request_timeout;           ///< Request timeout for this tenant
        
        /**
         * @brief Default constructor
         */
        TenantConfiguration() 
            : user_limit(100)
            , storage_limit_mb(1024)  // 1GB
            , enable_introspection(true)
            , max_query_complexity(1000)
            , request_timeout(20000)  // 20 seconds
        {}
    };

    /**
     * @brief Tenant process status enumeration
     */
    enum class ProcessStatus {
        STARTING,       ///< Process is starting up
        READY,          ///< Process is ready to handle requests
        BUSY,           ///< Process is handling requests
        IDLE,           ///< Process is idle
        STOPPING,       ///< Process is shutting down
        ERROR           ///< Process encountered an error
    };

    /**
     * @brief Tenant session information
     */
    struct TenantSession {
        TenantId tenant_id;                 ///< Associated tenant ID
        ProcessId process_id;               ///< Assigned process ID
        String database_path;               ///< Path to tenant database
        std::shared_ptr<void> database_connection; ///< Database connection (type-erased)
        std::atomic<uint64_t> request_count{0}; ///< Requests handled by this session
        TimePoint last_activity;            ///< Last request timestamp
        ProcessStatus status;               ///< Current process status
        
        TenantSession(const TenantId& id, ProcessId pid) 
            : tenant_id(id), process_id(pid), status(ProcessStatus::STARTING)
            , last_activity(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Manager status enumeration
     */
    enum class Status {
        STOPPED,        ///< Manager is not running
        STARTING,       ///< Manager is initializing
        RUNNING,        ///< Manager is actively managing tenants
        STOPPING,       ///< Manager is shutting down
        ERROR          ///< Manager encountered an error
    };

    /**
     * @brief Factory method to create TenantManager instance
     * @param config Pool configuration
     * @return Unique pointer to TenantManager instance
     * @throw std::runtime_error if configuration is invalid
     */
    static UniquePtr create(const PoolConfiguration& config = PoolConfiguration{});

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~TenantManager();

    // Non-copyable, non-moveable (contains atomics and mutexes)
    TenantManager(const TenantManager&) = delete;
    TenantManager& operator=(const TenantManager&) = delete;
    TenantManager(TenantManager&&) = delete;
    TenantManager& operator=(TenantManager&&) = delete;

    /**
     * @brief Start the tenant management system
     * @return true if startup successful
     */
    bool start();

    /**
     * @brief Stop the tenant management system
     * @param timeout_ms Maximum time to wait for graceful shutdown
     * @return true if shutdown successful
     */
    bool stop(Duration timeout_ms = Duration(10000));

    /**
     * @brief Get current manager status
     * @return Current status
     * @note Thread-safe operation
     */
    Status get_status() const noexcept;

    /**
     * @brief Create a new tenant
     * @param tenant_id Unique tenant identifier
     * @param config Tenant configuration
     * @return true if tenant created successfully
     * @throw std::invalid_argument if tenant_id already exists
     * @throw std::runtime_error if tenant creation fails
     */
    bool create_tenant(const TenantId& tenant_id, const TenantConfiguration& config);

    /**
     * @brief Remove a tenant and clean up resources
     * @param tenant_id Tenant to remove
     * @return true if tenant removed successfully
     */
    bool remove_tenant(const TenantId& tenant_id);

    /**
     * @brief Get or create a tenant session
     * @param tenant_id Tenant identifier
     * @return Shared pointer to tenant session
     * @throw std::runtime_error if tenant not found or session creation fails
     */
    std::shared_ptr<TenantSession> get_tenant_session(const TenantId& tenant_id);

    /**
     * @brief Release a tenant session when request is complete
     * @param session Session to release
     */
    void release_tenant_session(std::shared_ptr<TenantSession> session);

    /**
     * @brief Get tenant list
     * @return Vector of active tenant IDs
     * @note Thread-safe operation
     */
    std::vector<TenantId> get_tenant_list() const;

    /**
     * @brief Get tenant configuration
     * @param tenant_id Tenant identifier
     * @return Tenant configuration if found
     * @throw std::runtime_error if tenant not found
     */
    TenantConfiguration get_tenant_configuration(const TenantId& tenant_id) const;

    /**
     * @brief Get pool configuration
     * @return Current pool configuration
     * @note Thread-safe operation (configuration is immutable after startup)
     */
    const PoolConfiguration& get_pool_configuration() const noexcept;

    /**
     * @brief Get tenant management metrics
     * @return JSON string containing current metrics
     * 
     * Metrics include:
     * - Active tenant count
     * - Process pool utilization
     * - Average response time per tenant
     * - Memory usage per tenant
     * - Database connection pool status
     * 
     * @example Metrics format:
     * @code{json}
     * {
     *   "active_tenants": 15,
     *   "process_pool": {
     *     "active_processes": 12,
     *     "idle_processes": 3,
     *     "utilization": 0.80
     *   },
     *   "tenants": {
     *     "tenant_1": {
     *       "request_count": 1542,
     *       "avg_response_time": 18.5,
     *       "memory_usage_mb": 45
     *     }
     *   }
     * }
     * @endcode
     */
    String get_metrics() const;

    /**
     * @brief Get tenant health status
     * @return JSON string with health information
     * 
     * Health check includes:
     * - Tenant process status
     * - Database connectivity per tenant
     * - Resource utilization
     * - Error rates
     */
    String get_health() const;

private:
    /**
     * @brief Private constructor (use factory method)
     * @param config Pool configuration
     */
    explicit TenantManager(const PoolConfiguration& config);

    /**
     * @brief Initialize the tenant manager
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Create database directory for tenant
     * @param tenant_id Tenant identifier
     * @return Database directory path
     */
    String create_tenant_database_directory(const TenantId& tenant_id);

    /**
     * @brief Start a new tenant process
     * @param tenant_id Tenant identifier
     * @return Process ID if successful, 0 if failed
     */
    ProcessId start_tenant_process(const TenantId& tenant_id);

    /**
     * @brief Stop a tenant process
     * @param process_id Process to stop
     * @return true if stopped successfully
     */
    bool stop_tenant_process(ProcessId process_id);

    /**
     * @brief Load balance tenant assignment to processes
     * @param tenant_id Tenant identifier
     * @return Best available process ID
     */
    ProcessId assign_tenant_to_process(const TenantId& tenant_id);

    /**
     * @brief Cleanup idle processes
     */
    void cleanup_idle_processes();

    /**
     * @brief Health check for all processes
     */
    void health_check_processes();

    /**
     * @brief Background maintenance thread function
     */
    void maintenance_thread_function();

    // Member variables (all managed via smart pointers)
    PoolConfiguration m_config;                                ///< Pool configuration
    std::atomic<Status> m_status{Status::STOPPED};            ///< Current status
    
    // Core components (forward declared, defined in implementation)
    class Implementation;                                       ///< PIMPL idiom
    std::unique_ptr<Implementation> m_impl;                    ///< Implementation details

    // Thread synchronization
    mutable std::mutex m_status_mutex;                         ///< Status access synchronization
    mutable std::mutex m_tenants_mutex;                        ///< Tenant data synchronization
    mutable std::mutex m_processes_mutex;                      ///< Process pool synchronization

    // Performance tracking
    std::atomic<uint64_t> m_total_requests{0};                ///< Total requests across all tenants
    std::atomic<uint64_t> m_active_tenants{0};                ///< Currently active tenant count
    TimePoint m_start_time;                                    ///< Manager start timestamp
};

} // namespace isched::v0_0_1::backend