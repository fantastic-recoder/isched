// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_DatabaseManager.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief SQLite-based database management layer with connection pooling and tenant isolation
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 * 
 * This file provides the database management infrastructure for the Isched Universal
 * Application Server Backend. It implements per-tenant database isolation using SQLite
 * with connection pooling, automatic schema generation, and transaction management.
 * 
 * Key Features:
 * - Per-tenant SQLite database files for absolute data isolation
 * - Connection pooling with smart pointer resource management
 * - Automatic schema migration with backup and rollback support
 * - Transaction management with ACID compliance
 * - Performance monitoring for 20ms response time targets
 * - C++23 features and C++ Core Guidelines compliance
 * 
 * Constitutional Compliance:
 * - Performance: Optimized for multi-tenant operation with connection pooling
 * - Security: Absolute tenant data isolation via separate database files
 * - Testing: Comprehensive unit and integration test coverage
 * - Portability: Cross-platform SQLite with standard C++23
 * - C++ Core Guidelines: Smart pointer usage, RAII patterns, const-correctness
 */

#pragma once

#include "isched_common.hpp"
#include <sqlite3.h>
#include <memory>
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <variant>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <atomic>

#include "isched/shared/config/isched_config.hpp"

namespace isched::v0_0_1::backend {

/**
 * @brief Error types for database operations
 */
enum class DatabaseError {
    ConnectionFailed,
    QueryFailed,
    TransactionFailed,
    SchemaValidationFailed,
    TenantNotFound,
    MigrationFailed,
    BackupFailed,
    PoolExhausted,
    NotFound,       ///< Requested record does not exist
    DuplicateKey,   ///< INSERT failed due to UNIQUE constraint
    AccessDenied    ///< Operation refused (e.g. deleting a built-in role)
};

/**
 * @brief Database operation result type using std::variant (C++23 compatible)
 * 
 * Since std::expected may not be available in all C++23 implementations yet,
 * we use std::variant as a compatible alternative that provides similar functionality.
 */
template<typename T>
class DatabaseResult {
public:
    /**
     * @brief Construct successful result
     */
    DatabaseResult(T value) : result_(std::move(value)) {}
    
    /**
     * @brief Construct error result
     */
    DatabaseResult(DatabaseError error) : result_(error) {}
    
    /**
     * @brief Check if result contains value
     */
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(result_);
    }
    
    /**
     * @brief Check if result contains error
     */
    [[nodiscard]] bool has_error() const noexcept {
        return std::holds_alternative<DatabaseError>(result_);
    }
    
    /**
     * @brief Get value (throws if error)
     */
    [[nodiscard]] const T& value() const & {
        return std::get<T>(result_);
    }
    
    /**
     * @brief Get value (throws if error)
     */
    [[nodiscard]] T& value() & {
        return std::get<T>(result_);
    }
    
    /**
     * @brief Get value (throws if error)
     */
    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(result_));
    }
    
    /**
     * @brief Get error (throws if value)
     */
    [[nodiscard]] DatabaseError error() const {
        return std::get<DatabaseError>(result_);
    }
    
    /**
     * @brief Get value or default
     */
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) const & {
        return has_value() ? value() : static_cast<T>(std::forward<U>(default_value));
    }
    
    /**
     * @brief Explicit bool conversion
     */
    explicit operator bool() const noexcept {
        return has_value();
    }

private:
    std::variant<T, DatabaseError> result_;
};

/**
 * @brief Specialization for void type
 */
template<>
class DatabaseResult<void> {
public:
    /**
     * @brief Construct successful result
     */
    DatabaseResult() : has_error_(false), error_value_{} {}
    
    /**
     * @brief Construct error result
     */
    DatabaseResult(DatabaseError error) : has_error_(true), error_value_(error) {}
    
    /**
     * @brief Check if result is successful
     */
    [[nodiscard]] bool has_value() const noexcept {
        return !has_error_;
    }
    
    /**
     * @brief Check if result contains error
     */
    [[nodiscard]] bool has_error() const noexcept {
        return has_error_;
    }
    
    /**
     * @brief Get error (throws if successful)
     */
    [[nodiscard]] DatabaseError error() const {
        if (!has_error_) {
            throw std::logic_error("Attempting to get error from successful result");
        }
        return error_value_;
    }
    
    /**
     * @brief Explicit bool conversion
     */
    explicit operator bool() const noexcept {
        return has_value();
    }

private:
    bool has_error_;
    DatabaseError error_value_;
};

// -------------------------------------------------------------------------
// Plain-data records returned by system-DB helper methods (T047-005)
// -------------------------------------------------------------------------

/// A row from the @c organizations table in @c isched_system.db.
struct OrganizationRecord {
    std::string id;
    std::string name;
    std::string domain;               ///< May be empty
    std::string subscription_tier;
    int         user_limit{0};
    int         storage_limit{0};
    std::string created_at;
};

/// A row from the @c platform_admins table in @c isched_system.db.
/// @c password_hash is excluded from list results for security.
struct PlatformAdminRecord {
    std::string id;
    std::string email;
    std::string password_hash;        ///< Populated only by get_platform_admin_by_* helpers
    std::string display_name;
    bool        is_active{true};
    std::string created_at;
    std::string last_login;           ///< May be empty
};

/// A row from the @c platform_roles table in @c isched_system.db.
struct PlatformRoleRecord {
    std::string id;
    std::string name;
    std::string description;
    std::string created_at;
};

/**
 * @brief Custom deleter for SQLite database connections
 * 
 * Ensures proper cleanup of SQLite resources following RAII principles.
 * Used with std::unique_ptr to manage SQLite connection lifecycle.
 */
struct SqliteDeleter {
    void operator()(sqlite3* db) const noexcept {
        if (db) {
            sqlite3_close_v2(db);
        }
    }
};

/**
 * @brief Type alias for smart pointer managed SQLite connections
 */
using SqliteConnection = std::unique_ptr<sqlite3, SqliteDeleter>;

/**
 * @brief RAII wrapper for database transactions
 * 
 * Provides automatic transaction management with rollback on exception.
 * Follows C++ Core Guidelines for resource management.
 */
class DatabaseTransaction {
public:
    /**
     * @brief Construct transaction and begin
     * @param connection SQLite connection to use
     */
    explicit DatabaseTransaction(sqlite3* connection);
    
    /**
     * @brief Destructor - automatically rollback if not committed
     */
    ~DatabaseTransaction() noexcept;
    
    // Non-copyable, movable
    DatabaseTransaction(const DatabaseTransaction&) = delete;
    DatabaseTransaction& operator=(const DatabaseTransaction&) = delete;
    DatabaseTransaction(DatabaseTransaction&&) noexcept = default;
    DatabaseTransaction& operator=(DatabaseTransaction&&) noexcept = default;
    
    /**
     * @brief Commit the transaction
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> commit() noexcept;
    
    /**
     * @brief Rollback the transaction
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> rollback() noexcept;
    
private:
    sqlite3* connection_;
    bool committed_;
    bool rolled_back_;
};

/**
 * @brief Connection pool for managing SQLite connections per tenant
 * 
 * Implements object pool pattern with smart pointer management.
 * Thread-safe design for concurrent access from multiple tenant processes.
 */
class ConnectionPool {
public:
    /**
     * @brief RAII wrapper for pooled connections
     * 
     * Automatically returns connection to pool when destroyed.
     */
    class PooledConnection {
    public:
        explicit PooledConnection(SqliteConnection conn, ConnectionPool* pool);
        ~PooledConnection() noexcept;
        
        // Non-copyable, movable
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
        PooledConnection(PooledConnection&&) noexcept = default;
        PooledConnection& operator=(PooledConnection&&) noexcept = default;
        
        /**
         * @brief Get raw SQLite connection pointer
         * @return Non-owning pointer to SQLite connection
         */
        [[nodiscard]] sqlite3* get() const noexcept { return connection_.get(); }
        
        /**
         * @brief Check if connection is valid
         * @return True if connection is valid
         */
        [[nodiscard]] bool is_valid() const noexcept { return connection_ != nullptr; }
        
    private:
        SqliteConnection connection_;
        ConnectionPool* pool_;
    };
    
    /**
     * @brief Construct connection pool
     * @param database_path Path to SQLite database file
     * @param max_connections Maximum number of connections in pool
     */
    explicit ConnectionPool(std::string database_path, std::size_t max_connections = 10);
    
    /**
     * @brief Destructor - cleanup all connections
     */
    ~ConnectionPool() noexcept = default;
    
    // Non-copyable, non-movable (resource manager)
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    
    /**
     * @brief Acquire connection from pool
     * @param timeout_ms Maximum time to wait for connection (default: 5000ms)
     * @return Pooled connection or error
     */
    [[nodiscard]] DatabaseResult<PooledConnection> acquire(
        std::chrono::milliseconds timeout_ms = std::chrono::milliseconds{5000}
    );
    
    /**
     * @brief Get pool statistics
     * @return JSON object with pool metrics
     */
    [[nodiscard]] nlohmann::json get_statistics() const;
    
private:
    friend class PooledConnection;
    
    /**
     * @brief Return connection to pool (called by PooledConnection destructor)
     * @param connection Connection to return
     */
    void return_connection(SqliteConnection connection) noexcept;
    
    /**
     * @brief Create new SQLite connection
     * @return New connection or error
     */
    [[nodiscard]] DatabaseResult<SqliteConnection> create_connection();
    
    const std::string database_path_;
    const std::size_t max_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_cv_;            ///< Notified when a connection is returned
    std::queue<SqliteConnection> available_connections_;
    std::size_t active_connections_;
    std::size_t total_created_;
    std::size_t total_requests_;
    std::size_t pool_hits_;
};

/**
 * @brief Schema migration manager for safe database evolution
 * 
 * Handles automatic schema migration with backup and rollback support.
 * Ensures data safety during schema changes.
 */
class SchemaMigrator {
public:
    /**
     * @brief Migration script definition
     */
    struct Migration {
        std::string version;          ///< Target schema version
        std::string description;      ///< Human-readable description
        std::string up_sql;          ///< SQL to apply migration
        std::string down_sql;        ///< SQL to rollback migration (optional)
        bool is_safe;               ///< True if migration is non-destructive
    };
    
    /**
     * @brief Construct schema migrator
     * @param connection Database connection to use
     */
    explicit SchemaMigrator(sqlite3* connection);
    
    /**
     * @brief Get current schema version
     * @return Current version or error
     */
    [[nodiscard]] DatabaseResult<std::string> get_current_version() const;
    
    /**
     * @brief Apply migration to target version
     * @param target_version Target schema version
     * @param backup_path Path for database backup (auto-generated if empty)
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> migrate_to_version(
        const std::string& target_version,
        const std::string& backup_path = ""
    );
    
    /**
     * @brief Rollback to previous version
     * @param backup_path Path to backup database
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> rollback_from_backup(
        const std::string& backup_path
    );
    
    /**
     * @brief Register migration script
     * @param migration Migration definition
     */
    void register_migration(Migration migration);
    
    /**
     * @brief Get available migrations
     * @return List of registered migrations
     */
    [[nodiscard]] const std::vector<Migration>& get_migrations() const noexcept {
        return migrations_;
    }
    
private:
    /**
     * @brief Create database backup
     * @param backup_path Path for backup file
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> create_backup(const std::string& backup_path) const;
    
    /**
     * @brief Validate data integrity
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> validate_integrity() const;
    
    /**
     * @brief Initialize migration tracking table
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> initialize_migration_table();
    
    sqlite3* connection_;
    std::vector<Migration> migrations_;
};

/**
 * @brief Main database manager for tenant-isolated database operations
 * 
 * Provides high-level database operations with automatic tenant isolation,
 * connection pooling, and performance monitoring.
 */
class DatabaseManager {

public:
    /**
     * @brief Database configuration
     */
    struct Config {
        std::string base_path{getDataHome()+"/isched/tenants"};  ///< Base directory for tenant databases
        std::size_t connection_pool_size{10};              ///< Connections per tenant pool
        std::chrono::milliseconds query_timeout{20};       ///< Query timeout for 20ms target
        bool enable_wal_mode{true};                        ///< Enable WAL mode for concurrency
        bool enable_foreign_keys{true};                    ///< Enable foreign key constraints
        std::size_t cache_size_kb{8192};                   ///< SQLite cache size in KB
    };
    
    /**
     * @brief Query execution result
     */
    struct QueryResult {
        std::vector<std::vector<std::string>> rows;  ///< Result rows
        std::vector<std::string> columns;            ///< Column names
        std::size_t affected_rows{0};               ///< Number of affected rows
        std::chrono::microseconds execution_time;   ///< Query execution time
    };
    
    /**
     * @brief Construct database manager
     * @param config Database configuration
     */
    explicit DatabaseManager(Config config);

    // Convenience default constructor delegating to default-config
    DatabaseManager() : DatabaseManager(Config{}) {}
    
    /**
     * @brief Destructor - cleanup all resources
     */
    ~DatabaseManager() noexcept = default;
    
    // Non-copyable, non-movable (resource manager)
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;
    
    /**
     * @brief Initialize tenant database
     * @param tenant_id Unique tenant identifier
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> initialize_tenant(const std::string& tenant_id);
    
    /**
     * @brief Execute SQL query for tenant
     * @param tenant_id Tenant identifier
     * @param sql SQL query to execute
     * @param parameters Query parameters (optional)
     * @return Query result or error
     */
    [[nodiscard]] DatabaseResult<QueryResult> execute_query(
        const std::string& tenant_id,
        const std::string& sql,
        const std::vector<std::string>& parameters = {}
    );
    
    /**
     * @brief Execute SQL query within transaction
     * @param tenant_id Tenant identifier
     * @param transaction_fn Function to execute within transaction
     * @return Transaction result or error
     */
    [[nodiscard]] DatabaseResult<nlohmann::json> execute_transaction(
        const std::string& tenant_id,
        std::function<DatabaseResult<nlohmann::json>(sqlite3*)> transaction_fn
    );
    
    /**
     * @brief Generate database schema from data model
     * @param tenant_id Tenant identifier
     * @param data_model JSON data model definition
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> generate_schema(
        const std::string& tenant_id,
        const nlohmann::json& data_model
    );
    
    /**
     * @brief Get tenant database statistics
     * @param tenant_id Tenant identifier
     * @return Database statistics or error
     */
    [[nodiscard]] DatabaseResult<nlohmann::json> get_tenant_stats(
        const std::string& tenant_id
    ) const;
    
    /**
     * @brief Get global database manager statistics
     * @return Global statistics
     */
    [[nodiscard]] nlohmann::json get_global_stats() const;

    // -------------------------------------------------------------------------
    // Configuration snapshot persistence (T029)
    // -------------------------------------------------------------------------

    /**
     * @brief Initialise the config-snapshot store (creates table if absent).
     *
     * Uses the reserved "__config__" tenant slot.  Safe to call multiple times.
     */
    [[nodiscard]] DatabaseResult<void> initialize_config_store();

    /**
     * @brief Persist a new snapshot row.
     *
     * The snapshot is stored with is_active = false.  To activate it, call
     * activate_config_snapshot() afterwards.
     *
     * @param snapshot Snapshot to persist; its id field must be non-empty.
     * @return Success/failure.
     */
    [[nodiscard]] DatabaseResult<void> save_config_snapshot(
        const ConfigurationSnapshot& snapshot);

    /**
     * @brief Atomically activate a snapshot.
     *
     * Deactivates all other snapshots for the same tenant, then sets
     * is_active = 1 and activated_at for the given id — all in one SQLite
     * transaction.
     *
     * @param snapshot_id ID of the snapshot to activate.
     * @return Success/failure.
     */
    [[nodiscard]] DatabaseResult<void> activate_config_snapshot(
        const std::string& snapshot_id);

    /**
     * @brief Return the active snapshot for a tenant, or nullopt.
     */
    [[nodiscard]] DatabaseResult<std::optional<ConfigurationSnapshot>>
    get_active_config_snapshot(const std::string& tenant_id) const;

    /**
     * @brief Return all snapshots for a tenant (newest first).
     */
    [[nodiscard]] DatabaseResult<std::vector<ConfigurationSnapshot>>
    list_config_snapshots(const std::string& tenant_id) const;

    /**
     * @brief Return a single snapshot by its id, or nullopt if not found.
     */
    [[nodiscard]] DatabaseResult<std::optional<ConfigurationSnapshot>>
    get_config_snapshot(const std::string& snapshot_id) const;

    // -------------------------------------------------------------------------
    // System database (T047-000)
    // -------------------------------------------------------------------------

    /**
     * @brief Open (or create) the platform-level @c isched_system.db.
     *
     * Creates three tables on first call (idempotent via @c CREATE TABLE IF NOT
     * EXISTS) and seeds the four built-in @c platform_roles rows.  Must be
     * called once at server startup before any RBAC or organisation operation.
     *
     * Path: @c <DataHome>/isched/isched_system.db
     */
    [[nodiscard]] DatabaseResult<void> ensure_system_db();

    /**
     * @brief Insert a custom platform-scope role into @c platform_roles.
     *
     * The four built-in roles (role_platform_admin … role_service) cannot be
     * replaced because the table uses @c INSERT OR IGNORE semantics.
     *
     * @param id          Unique role identifier (e.g. "role_billing_admin").
     * @param name        Human-readable name.
     * @param description Optional description.
     * @return @c DatabaseResult<void>; @c DatabaseError::DuplicateKey if @p id
     *         already exists.
     */
    [[nodiscard]] DatabaseResult<void> create_platform_role(
        const std::string& id,
        const std::string& name,
        const std::string& description);

    /**
     * @brief Remove a custom platform-scope role from @c platform_roles.
     *
     * Built-in roles (those seeded by @c ensure_system_db) are protected: if
     * the caller attempts to delete one of them, the method returns
     * @c DatabaseError::AccessDenied.
     *
     * @param id  Role identifier to remove.
     * @return @c DatabaseResult<void>; @c DatabaseError::NotFound if @p id does
     *         not exist; @c DatabaseError::AccessDenied for built-in roles.
     */
    [[nodiscard]] DatabaseResult<void> delete_platform_role(const std::string& id);

    /// Fetch a single role by @p id; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<PlatformRoleRecord> get_platform_role(const std::string& id);

    /// Return all rows from @c platform_roles (built-in + custom).
    [[nodiscard]] DatabaseResult<std::vector<PlatformRoleRecord>> list_platform_roles();

    // -------------------------------------------------------------------------
    // Organization CRUD (T047-005)
    // -------------------------------------------------------------------------

    /// Insert a new organization record.
    /// Returns @c DuplicateKey if @p id already exists.
    [[nodiscard]] DatabaseResult<void> create_organization(
        const std::string& id,
        const std::string& name,
        const std::string& domain,
        const std::string& subscription_tier,
        int user_limit,
        int storage_limit);

    /// Fetch a single organization by @p id; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<OrganizationRecord> get_organization(const std::string& id);

    /// Return all organization records.
    [[nodiscard]] DatabaseResult<std::vector<OrganizationRecord>> list_organizations();

    /// Partially update an organization.  Only non-null optionals are written.
    /// Returns @c NotFound if @p id does not exist.
    [[nodiscard]] DatabaseResult<void> update_organization(
        const std::string& id,
        std::optional<std::string> name,
        std::optional<std::string> domain,
        std::optional<std::string> subscription_tier,
        std::optional<int>         user_limit,
        std::optional<int>         storage_limit);

    /// Delete an organization by @p id; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<void> delete_organization(const std::string& id);

    // -------------------------------------------------------------------------
    // Platform-admin CRUD (T047-005)
    // -------------------------------------------------------------------------

    /// Insert a new platform admin.
    /// Returns @c DuplicateKey if @p id or @p email already exists.
    [[nodiscard]] DatabaseResult<void> create_platform_admin(
        const std::string& id,
        const std::string& email,
        const std::string& password_hash,
        const std::string& display_name);

    /// Fetch a platform admin by @p id; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<PlatformAdminRecord> get_platform_admin_by_id(
        const std::string& id);

    /// Fetch a platform admin by @p email; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<PlatformAdminRecord> get_platform_admin_by_email(
        const std::string& email);

    /// Return all platform admin records (password_hash excluded).
    [[nodiscard]] DatabaseResult<std::vector<PlatformAdminRecord>> list_platform_admins();

    /// Partially update a platform admin; only non-null optionals are written.
    /// Returns @c NotFound if @p id does not exist.
    [[nodiscard]] DatabaseResult<void> update_platform_admin(
        const std::string& id,
        std::optional<std::string> display_name,
        std::optional<bool>        is_active);

    /// Delete a platform admin by @p id; returns @c NotFound if absent.
    [[nodiscard]] DatabaseResult<void> delete_platform_admin(const std::string& id);

private:
    /**
     * @brief Get connection pool for tenant
     * @param tenant_id Tenant identifier
     * @return Connection pool or error
     */
    [[nodiscard]] DatabaseResult<std::shared_ptr<ConnectionPool>> get_tenant_pool(
        const std::string& tenant_id
    );
    
    /**
     * @brief Get tenant database path
     * @param tenant_id Tenant identifier
     * @return Database file path
     */
    [[nodiscard]] std::string get_tenant_db_path(const std::string& tenant_id) const;
    
    /**
     * @brief Configure SQLite connection
     * @param connection SQLite connection to configure
     * @return Success/failure result
     */
    [[nodiscard]] DatabaseResult<void> configure_connection(sqlite3* connection) const;
    
    Config config_;
    mutable std::mutex pools_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> tenant_pools_;
    std::unique_ptr<SchemaMigrator> migrator_;

    // Config store — dedicated SQLite file for configuration snapshots
    mutable std::mutex config_db_mutex_;
    SqliteConnection config_db_;   ///< Opened lazily by initialize_config_store()
    bool config_store_initialized_{false};

    [[nodiscard]] DatabaseResult<sqlite3*> get_config_db() const;

    // System database — isched_system.db for platform-level entities
    mutable std::mutex system_db_mutex_;
    SqliteConnection system_db_;   ///< Opened/created by ensure_system_db()
    bool system_db_initialized_{false};

    // Performance monitoring
    mutable std::atomic<std::size_t> total_queries_{0};
    mutable std::atomic<std::size_t> total_transactions_{0};
    mutable std::atomic<std::size_t> failed_queries_{0};
    mutable std::atomic<std::chrono::microseconds::rep> total_query_time_{0};
};

} // namespace isched::v0_0_1::backend