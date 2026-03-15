// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_TenantManager.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief In-process multi-tenant manager with per-tenant SQLite database isolation
 * @author isched Development Team
 * @version 2.0.0
 * @date 2026-03-13
 *
 * Manages multiple tenants within a single server process. Each tenant has:
 * - A dedicated SQLite database (via DatabaseManager)
 * - A quota-limited configuration (user_limit, storage_limit_mb, etc.)
 * - Tenant-scoped session handles returned to resolvers
 *
 * @example Basic usage:
 * @code{cpp}
 * #include "isched_TenantManager.hpp"
 *
 * auto manager = isched::v0_0_1::backend::TenantManager::create();
 * manager->start();
 * manager->create_tenant("example-org", config);
 * auto session = manager->get_tenant_session("example-org");
 * @endcode
 *
 * @note All resource ownership uses smart pointers (FR-019).
 * @see FR-011, FR-014 in spec.md
 */

#pragma once

#include "isched_common.hpp"
#include "isched_DatabaseManager.hpp"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace isched::v0_0_1::backend {

/**
 * @brief In-process multi-tenant manager
 *
 * Replaces the former process-pool design. Tenant isolation is now purely
 * logical: each tenant's data lives in its own SQLite file managed by the
 * shared DatabaseManager, and resource quotas are enforced per-request.
 *
 * @note Follows the PIMPL pattern; non-copyable, non-movable.
 */
class TenantManager {
public:
    using UniquePtr = std::unique_ptr<TenantManager>;
    using SharedPtr = std::shared_ptr<TenantManager>;
    using String    = std::string;
    using TenantId  = std::string;
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration  = std::chrono::milliseconds;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Top-level configuration for the tenant manager.
     *
     * Replaces the previous PoolConfiguration – no process-management knobs
     * remain; only storage location and tenant cap are needed.
     */
    struct Configuration {
        String      database_root_path; ///< Root dir for per-tenant SQLite files
        std::size_t max_tenants;        ///< Hard cap on concurrent tenants

        Configuration()
            : database_root_path("./data/tenants")
            , max_tenants(1000)
        {}

        /** Return true when parameters are valid (throws on hard failures). */
        bool validate() const;
    };

    /**
     * @brief Per-tenant quota and metadata.
     */
    struct TenantConfiguration {
        TenantId tenant_id;                 ///< Unique tenant identifier (set by create_tenant)
        String   organization_name;         ///< Human-readable org name
        String   domain;                    ///< Optional org domain
        std::size_t user_limit{100};        ///< Max users for this tenant
        std::size_t storage_limit_mb{1024}; ///< Storage quota in MiB
        bool enable_introspection{true};    ///< Allow GraphQL introspection
        std::size_t max_query_complexity{1000}; ///< GraphQL query complexity cap
        Duration request_timeout{20000};    ///< Per-request timeout
        // T050-001: advisory thread pool configuration stored per-tenant
        std::size_t min_threads{4};         ///< Advisory minimum HTTP worker threads
        std::size_t max_threads{16};        ///< Advisory maximum HTTP worker threads

        TenantConfiguration() = default;
    };

    // -------------------------------------------------------------------------
    // Session handle
    // -------------------------------------------------------------------------

    /**
     * @brief Tenant session handle returned to resolvers.
     *
     * Holds a typed reference to the shared DatabaseManager so resolvers can
     * execute tenant-scoped queries without knowing the underlying path.
     */
    struct TenantSession {
        TenantId tenant_id;                          ///< Associated tenant
        std::shared_ptr<DatabaseManager> database;   ///< Shared database manager
        std::atomic<uint64_t> request_count{0};      ///< Requests served by this session
        TimePoint last_activity;                     ///< Timestamp of last use

        TenantSession(TenantId id, std::shared_ptr<DatabaseManager> db)
            : tenant_id(std::move(id))
            , database(std::move(db))
            , last_activity(std::chrono::steady_clock::now())
        {}

        // Non-copyable because of the atomic
        TenantSession(const TenantSession&) = delete;
        TenantSession& operator=(const TenantSession&) = delete;
    };

    // -------------------------------------------------------------------------
    // Status
    // -------------------------------------------------------------------------

    enum class Status {
        STOPPED,  ///< Not running
        RUNNING,  ///< Actively serving tenants
        ERROR     ///< Encountered a fatal error
    };

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Factory: create a TenantManager with the given configuration.
     * @throw std::runtime_error on invalid config
     */
    static UniquePtr create(const Configuration& config = Configuration{});

    ~TenantManager();

    TenantManager(const TenantManager&)            = delete;
    TenantManager& operator=(const TenantManager&) = delete;
    TenantManager(TenantManager&&)                 = delete;
    TenantManager& operator=(TenantManager&&)      = delete;

    /** Start the tenant manager (idempotent if already running). */
    bool start();

    /** Stop the tenant manager and release all resources. */
    bool stop(Duration timeout_ms = Duration{10000});

    // -------------------------------------------------------------------------
    // Queries / operations
    // -------------------------------------------------------------------------

    /** Thread-safe status query. */
    Status get_status() const noexcept;

    /**
     * @brief Register a new tenant and initialise its database.
     * @throw std::invalid_argument if tenant_id already exists
     * @throw std::runtime_error on database initialisation failure
     */
    bool create_tenant(const TenantId& tenant_id, const TenantConfiguration& config);

    /**
     * @brief Deregister a tenant and release its in-memory state.
     * @note The SQLite file is NOT deleted (data is preserved for audit).
     */
    bool remove_tenant(const TenantId& tenant_id);

    /**
     * @brief Obtain a session for the given tenant.
     *
     * Sessions are cached per tenant; calling this multiple times returns the
     * same shared_ptr. The caller should call release_tenant_session() when
     * the request is complete so that activity timestamps stay current.
     *
     * @throw std::runtime_error if tenant not registered
     */
    std::shared_ptr<TenantSession> get_tenant_session(const TenantId& tenant_id);

    /** Mark a session as idle (updates last_activity). */
    void release_tenant_session(std::shared_ptr<TenantSession> session);

    /** Return a snapshot of currently registered tenant IDs. */
    std::vector<TenantId> get_tenant_list() const;

    /**
     * @brief Return the stored configuration for a tenant.
     * @throw std::runtime_error if tenant not found
     */
    TenantConfiguration get_tenant_configuration(const TenantId& tenant_id) const;

    /** Return the configuration this manager was created with. */
    const Configuration& get_configuration() const noexcept;

    /** Return a JSON string with current runtime metrics. */
    String get_metrics() const;

    /** Return a JSON string with the current health status. */
    String get_health() const;

    // T050-002: global subscription count tracking
    /** Increment the global active-subscription counter (call on subscribe). */
    void on_subscription_start() noexcept;

    /** Decrement the global active-subscription counter (call on disconnect). */
    void on_subscription_end() noexcept;

    /** Return the current total count of active subscriptions across all tenants. */
    uint64_t get_active_subscription_count() const noexcept;

private:
    explicit TenantManager(const Configuration& config);

    bool initialize();

    Configuration m_config;
    std::atomic<Status> m_status{Status::STOPPED};

    class Implementation;
    std::unique_ptr<Implementation> m_impl;

    mutable std::mutex m_status_mutex;
    mutable std::mutex m_tenants_mutex;

    std::shared_ptr<DatabaseManager> m_database_manager;

    std::atomic<uint64_t> m_total_requests{0};
    std::atomic<uint64_t> m_active_tenants{0};
    std::atomic<uint64_t> m_total_active_subscription_count{0}; ///< T050-002
    TimePoint m_start_time{};
};

} // namespace isched::v0_0_1::backend
