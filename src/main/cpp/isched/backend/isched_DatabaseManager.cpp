// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_DatabaseManager.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of SQLite-based database management layer
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#include "isched_DatabaseManager.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <condition_variable>

namespace isched::v0_0_1::backend {

// ============================================================================
// DatabaseTransaction Implementation
// ============================================================================

DatabaseTransaction::DatabaseTransaction(sqlite3* connection)
    : connection_(connection), committed_(false), rolled_back_(false) {
    if (!connection_) {
        throw std::invalid_argument("Connection cannot be null");
    }
    
    int result = sqlite3_exec(connection_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to begin transaction: " + std::string(sqlite3_errmsg(connection_)));
    }
}

DatabaseTransaction::~DatabaseTransaction() noexcept {
    if (!committed_ && !rolled_back_) {
        // Automatically rollback on destruction if not committed
        sqlite3_exec(connection_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

DatabaseResult<void> DatabaseTransaction::commit() noexcept {
    if (committed_) {
        return DatabaseError::TransactionFailed;
    }
    
    if (rolled_back_) {
        return DatabaseError::TransactionFailed;
    }
    
    int result = sqlite3_exec(connection_, "COMMIT;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::TransactionFailed;
    }
    
    committed_ = true;
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseTransaction::rollback() noexcept {
    if (committed_) {
        return DatabaseError::TransactionFailed;
    }
    
    if (rolled_back_) {
        return DatabaseResult<void>{};  // Already rolled back
    }
    
    int result = sqlite3_exec(connection_, "ROLLBACK;", nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::TransactionFailed;
    }
    
    rolled_back_ = true;
    return DatabaseResult<void>{};
}

// ============================================================================
// ConnectionPool::PooledConnection Implementation  
// ============================================================================

ConnectionPool::PooledConnection::PooledConnection(SqliteConnection conn, ConnectionPool* pool)
    : connection_(std::move(conn)), pool_(pool) {
}

ConnectionPool::PooledConnection::~PooledConnection() noexcept {
    if (connection_ && pool_) {
        pool_->return_connection(std::move(connection_));
    }
}

// ============================================================================
// ConnectionPool Implementation
// ============================================================================

ConnectionPool::ConnectionPool(std::string database_path, std::size_t max_connections)
    : database_path_(std::move(database_path))
    , max_connections_(max_connections)
    , active_connections_(0)
    , total_created_(0)
    , total_requests_(0)
    , pool_hits_(0) {
    
    spdlog::info("Created connection pool for database: {} with max connections: {}", 
                 database_path_, max_connections_);
}

DatabaseResult<ConnectionPool::PooledConnection> ConnectionPool::acquire(
    std::chrono::milliseconds timeout_ms) {
    
    total_requests_++;
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    auto deadline = std::chrono::steady_clock::now() + timeout_ms;
    
    // Wait until a connection is available or we can create a new one
    bool ok = pool_cv_.wait_until(lock, deadline, [this]() {
        return !available_connections_.empty() || active_connections_ < max_connections_;
    });
    
    if (!ok) {
        return DatabaseError::PoolExhausted;
    }
    
    SqliteConnection connection;
    
    if (!available_connections_.empty()) {
        // Reuse existing connection
        connection = std::move(available_connections_.front());
        available_connections_.pop();
        pool_hits_++;
    } else {
        // Create new connection
        auto result = create_connection();
        if (!result) {
            return result.error();
        }
        connection = std::move(result.value());
        total_created_++;
    }
    
    active_connections_++;
    
    return PooledConnection{std::move(connection), this};
}

void ConnectionPool::return_connection(SqliteConnection connection) noexcept {
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        if (active_connections_ > 0) {
            active_connections_--;
        }
        
        if (available_connections_.size() < max_connections_) {
            available_connections_.push(std::move(connection));
        }
        // If pool is full, connection is destroyed on scope exit
    }
    pool_cv_.notify_one();
}

DatabaseResult<SqliteConnection> ConnectionPool::create_connection() {
    sqlite3* raw_connection = nullptr;
    
    int result = sqlite3_open_v2(
        database_path_.c_str(),
        &raw_connection,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    
    if (result != SQLITE_OK) {
        if (raw_connection) {
            sqlite3_close_v2(raw_connection);
        }
        return DatabaseError::ConnectionFailed;
    }
    
    auto connection = SqliteConnection{raw_connection};
    
    // Configure connection for optimal performance
    const char* pragma_statements[] = {
        "PRAGMA journal_mode=WAL;",         // Write-Ahead Logging for better concurrency
        "PRAGMA synchronous=NORMAL;",       // Good balance of safety and speed
        "PRAGMA cache_size=8000;",          // 8MB cache
        "PRAGMA foreign_keys=ON;",          // Enable foreign key constraints
        "PRAGMA temp_store=MEMORY;"         // Store temporary tables in memory
    };
    
    for (const char* pragma : pragma_statements) {
        result = sqlite3_exec(connection.get(), pragma, nullptr, nullptr, nullptr);
        if (result != SQLITE_OK) {
            spdlog::warn("Failed to set pragma: {} - {}", pragma, sqlite3_errmsg(connection.get()));
            // Continue anyway - these are optimizations, not critical
        }
    }
    
    return connection;
}

nlohmann::json ConnectionPool::get_statistics() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    return nlohmann::json{
        {"database_path", database_path_},
        {"max_connections", max_connections_},
        {"active_connections", active_connections_},
        {"available_connections", available_connections_.size()},
        {"total_created", total_created_},
        {"total_requests", total_requests_},
        {"pool_hits", pool_hits_},
        {"hit_rate", total_requests_ > 0 ? static_cast<double>(pool_hits_) / total_requests_ : 0.0}
    };
}

// ============================================================================
// SchemaMigrator Implementation
// ============================================================================

SchemaMigrator::SchemaMigrator(sqlite3* connection) : connection_(connection) {
    if (!connection_) {
        throw std::invalid_argument("Connection cannot be null");
    }
    
    auto result = initialize_migration_table();
    if (!result) {
        throw std::runtime_error("Failed to initialize migration table");
    }
}

DatabaseResult<std::string> SchemaMigrator::get_current_version() const {
    const char* sql = "SELECT value FROM _isched_metadata WHERE key = 'schema_version' LIMIT 1;";
    
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(connection_, sql, -1, &stmt, nullptr);
    
    if (result != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    
    // RAII for statement cleanup
    auto stmt_cleanup = [](sqlite3_stmt* stmt) { if (stmt) sqlite3_finalize(stmt); };
    std::unique_ptr<sqlite3_stmt, decltype(stmt_cleanup)> stmt_guard(stmt, stmt_cleanup);
    
    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const char* version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        return std::string{version ? version : "0.0.0"};
    } else if (result == SQLITE_DONE) {
        return std::string{"0.0.0"};  // No version found, assume initial state
    } else {
        return DatabaseError::QueryFailed;
    }
}

DatabaseResult<void> SchemaMigrator::migrate_to_version(
    const std::string& target_version,
    const std::string& backup_path) {
    
    auto current_version_result = get_current_version();
    if (!current_version_result) {
        return current_version_result.error();
    }
    
    std::string current_version = current_version_result.value();
    
    if (current_version == target_version) {
        return DatabaseResult<void>{};  // Already at target version
    }
    
    // Find migration to apply
    auto migration_it = std::find_if(migrations_.begin(), migrations_.end(),
        [&target_version](const Migration& m) { return m.version == target_version; });
    
    if (migration_it == migrations_.end()) {
        return DatabaseError::MigrationFailed;
    }
    
    const Migration& migration = *migration_it;
    
    // Create backup
    std::string actual_backup_path = backup_path;
    if (actual_backup_path.empty()) {
        actual_backup_path = "/tmp/isched_backup_" + std::to_string(std::time(nullptr)) + ".sqlite3";
    }
    
    auto backup_result = create_backup(actual_backup_path);
    if (!backup_result) {
        return backup_result.error();
    }
    
    // Apply migration in transaction
    DatabaseTransaction transaction(connection_);
    
    int result = sqlite3_exec(connection_, migration.up_sql.c_str(), nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        // Migration failed, transaction will auto-rollback
        return DatabaseError::MigrationFailed;
    }
    
    // Update schema version
    const char* update_sql = "INSERT OR REPLACE INTO _isched_metadata (key, value, updated_at) "
                           "VALUES ('schema_version', ?, datetime('now'));";
    
    sqlite3_stmt* stmt = nullptr;
    result = sqlite3_prepare_v2(connection_, update_sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::MigrationFailed;
    }
    
    auto stmt_cleanup = [](sqlite3_stmt* stmt) { if (stmt) sqlite3_finalize(stmt); };
    std::unique_ptr<sqlite3_stmt, decltype(stmt_cleanup)> stmt_guard(stmt, stmt_cleanup);
    
    sqlite3_bind_text(stmt, 1, target_version.c_str(), -1, SQLITE_STATIC);
    result = sqlite3_step(stmt);
    
    if (result != SQLITE_DONE) {
        return DatabaseError::MigrationFailed;
    }
    
    // Validate data integrity
    auto validation_result = validate_integrity();
    if (!validation_result) {
        return validation_result.error();
    }
    
    // Commit transaction
    auto commit_result = transaction.commit();
    if (!commit_result) {
        return commit_result.error();
    }
    
    spdlog::info("Successfully migrated from version {} to {}", current_version, target_version);
    return DatabaseResult<void>{};
}

DatabaseResult<void> SchemaMigrator::rollback_from_backup(const std::string& backup_path) {
    if (!std::filesystem::exists(backup_path)) {
        return DatabaseError::BackupFailed;
    }
    
    // Close current connection and restore from backup
    // This is a simplified implementation - in production you'd want more sophisticated handling
    try {
        std::filesystem::copy_file(backup_path, 
                                 std::string("current_database.sqlite3"),  // This should be the actual DB path
                                 std::filesystem::copy_options::overwrite_existing);
        return DatabaseResult<void>{};
    } catch (const std::filesystem::filesystem_error&) {
        return DatabaseError::BackupFailed;
    }
}

void SchemaMigrator::register_migration(Migration migration) {
    migrations_.push_back(std::move(migration));
    
    // Sort migrations by version for proper ordering
    std::sort(migrations_.begin(), migrations_.end(),
        [](const Migration& a, const Migration& b) {
            return a.version < b.version;
        });
}

DatabaseResult<void> SchemaMigrator::create_backup(const std::string& backup_path) const {
    sqlite3* backup_db = nullptr;
    
    int result = sqlite3_open(backup_path.c_str(), &backup_db);
    if (result != SQLITE_OK) {
        if (backup_db) {
            sqlite3_close(backup_db);
        }
        return DatabaseError::BackupFailed;
    }
    
    sqlite3_backup* backup_handle = sqlite3_backup_init(backup_db, "main", connection_, "main");
    if (!backup_handle) {
        sqlite3_close(backup_db);
        return DatabaseError::BackupFailed;
    }
    
    result = sqlite3_backup_step(backup_handle, -1);  // Copy entire database
    sqlite3_backup_finish(backup_handle);
    sqlite3_close(backup_db);
    
    if (result != SQLITE_DONE) {
        return DatabaseError::BackupFailed;
    }
    
    return DatabaseResult<void>{};
}

DatabaseResult<void> SchemaMigrator::validate_integrity() const {
    const char* sql = "PRAGMA integrity_check;";
    
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(connection_, sql, -1, &stmt, nullptr);
    
    if (result != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    
    auto stmt_cleanup = [](sqlite3_stmt* stmt) { if (stmt) sqlite3_finalize(stmt); };
    std::unique_ptr<sqlite3_stmt, decltype(stmt_cleanup)> stmt_guard(stmt, stmt_cleanup);
    
    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const char* check_result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (check_result && std::string{check_result} == "ok") {
            return DatabaseResult<void>{};
        }
    }
    
    return DatabaseError::SchemaValidationFailed;
}

DatabaseResult<void> SchemaMigrator::initialize_migration_table() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS _isched_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        INSERT OR IGNORE INTO _isched_metadata (key, value) 
        VALUES ('schema_version', '0.0.0');
    )";
    
    int result = sqlite3_exec(connection_, sql, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::SchemaValidationFailed;
    }
    
    return DatabaseResult<void>{};
}

// ============================================================================
// DatabaseManager Implementation
// ============================================================================

DatabaseManager::DatabaseManager(Config config) 
    : config_(std::move(config)) {
    
    // Ensure base directory exists
    if (!std::filesystem::exists(config_.base_path)) {
        std::filesystem::create_directories(config_.base_path);
    }
    
    spdlog::info("Database manager initialized with base path: {}", config_.base_path);
}

DatabaseResult<void> DatabaseManager::initialize_tenant(const std::string& tenant_id) {
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) {
        return pool_result.error();
    }
    
    auto pool = pool_result.value();
    auto connection_result = pool->acquire();
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    // First create the metadata table if it doesn't exist
    const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS _isched_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    int result = sqlite3_exec(connection.get(), create_table_sql, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    
    // Insert tenant metadata using prepared statement for the tenant_id parameter
    // Execute the fixed metadata inserts first
    const char* fixed_inserts = R"(
        INSERT OR REPLACE INTO _isched_metadata (key, value, updated_at) 
        VALUES 
            ('schema_version', '1.0.0', datetime('now')),
            ('created_at', datetime('now'), datetime('now'));
    )";
    
    result = sqlite3_exec(connection.get(), fixed_inserts, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    
    // Now insert tenant_id with prepared statement
    const char* tenant_insert = "INSERT OR REPLACE INTO _isched_metadata (key, value, updated_at) VALUES ('tenant_id', ?, datetime('now'));";
    
    sqlite3_stmt* stmt = nullptr;
    result = sqlite3_prepare_v2(connection.get(), tenant_insert, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    
    auto stmt_cleanup = [](sqlite3_stmt* stmt) { if (stmt) sqlite3_finalize(stmt); };
    std::unique_ptr<sqlite3_stmt, decltype(stmt_cleanup)> stmt_guard(stmt, stmt_cleanup);
    
    sqlite3_bind_text(stmt, 1, tenant_id.c_str(), -1, SQLITE_STATIC);
    result = sqlite3_step(stmt);
    
    if (result != SQLITE_DONE) {
        return DatabaseError::QueryFailed;
    }
    
    spdlog::info("Initialized database for tenant: {}", tenant_id);

    // Ensure the users table exists (T047-010)
    auto users_res = ensure_users_table(tenant_id);
    if (!users_res) {
        spdlog::warn("initialize_tenant: ensure_users_table failed for tenant '{}'", tenant_id);
    }

    // Ensure the sessions table exists (T049-001)
    auto sessions_res = ensure_sessions_table(tenant_id);
    if (!sessions_res) {
        spdlog::warn("initialize_tenant: ensure_sessions_table failed for tenant '{}'", tenant_id);
    }

    // Ensure the data_sources table exists (T048)
    auto ds_res = ensure_data_sources_table(tenant_id);
    if (!ds_res) {
        spdlog::warn("initialize_tenant: ensure_data_sources_table failed for tenant '{}'", tenant_id);
    }

    return DatabaseResult<void>{};
}

DatabaseResult<DatabaseManager::QueryResult> DatabaseManager::execute_query(
    const std::string& tenant_id,
    const std::string& sql,
    const std::vector<std::string>& parameters) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    total_queries_++;
    
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) {
        failed_queries_++;
        return pool_result.error();
    }
    
    auto pool = pool_result.value();
    auto connection_result = pool->acquire(config_.query_timeout);
    if (!connection_result) {
        failed_queries_++;
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(connection.get(), sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        failed_queries_++;
        return DatabaseError::QueryFailed;
    }
    
    auto stmt_cleanup = [](sqlite3_stmt* stmt) { if (stmt) sqlite3_finalize(stmt); };
    std::unique_ptr<sqlite3_stmt, decltype(stmt_cleanup)> stmt_guard(stmt, stmt_cleanup);
    
    // Bind parameters
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), parameters[i].c_str(), -1, SQLITE_STATIC);
    }
    
    QueryResult query_result;
    int column_count = sqlite3_column_count(stmt);
    
    // Get column names
    for (int i = 0; i < column_count; ++i) {
        query_result.columns.emplace_back(sqlite3_column_name(stmt, i));
    }
    
    // Execute and fetch results
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < column_count; ++i) {
            const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.emplace_back(value ? value : "");
        }
        query_result.rows.push_back(std::move(row));
    }
    
    if (result != SQLITE_DONE) {
        failed_queries_++;
        return DatabaseError::QueryFailed;
    }
    
    query_result.affected_rows = sqlite3_changes(connection.get());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    query_result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    
    total_query_time_.fetch_add(query_result.execution_time.count());
    
    return query_result;
}

DatabaseResult<nlohmann::json> DatabaseManager::execute_transaction(
    const std::string& tenant_id,
    std::function<DatabaseResult<nlohmann::json>(sqlite3*)> transaction_fn) {
    
    total_transactions_++;
    
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) {
        return pool_result.error();
    }
    
    auto pool = pool_result.value();
    auto connection_result = pool->acquire(config_.query_timeout);
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    DatabaseTransaction transaction(connection.get());
    
    auto result = transaction_fn(connection.get());
    if (!result) {
        // Transaction will auto-rollback
        return result.error();
    }
    
    auto commit_result = transaction.commit();
    if (!commit_result) {
        return commit_result.error();
    }
    
    return result;
}

DatabaseResult<void> DatabaseManager::generate_schema(
    const std::string& tenant_id,
    [[maybe_unused]] const nlohmann::json& data_model) {
    
    // This is a simplified schema generation - in a full implementation,
    // you'd parse the data_model JSON and generate appropriate CREATE TABLE statements
    
    std::string create_table_sql = "CREATE TABLE IF NOT EXISTS example_table (id INTEGER PRIMARY KEY);";
    
    auto result = execute_query(tenant_id, create_table_sql);
    if (!result) {
        return result.error();
    }
    
    return DatabaseResult<void>{};
}

DatabaseResult<nlohmann::json> DatabaseManager::get_tenant_stats(
    const std::string& tenant_id) const {
    
    auto pool_result = const_cast<DatabaseManager*>(this)->get_tenant_pool(tenant_id);
    if (!pool_result) {
        return pool_result.error();
    }
    
    auto pool = pool_result.value();
    auto stats = pool->get_statistics();
    
    return stats;
}

nlohmann::json DatabaseManager::get_global_stats() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    nlohmann::json stats{
        {"total_tenants", tenant_pools_.size()},
        {"total_queries", total_queries_.load()},
        {"total_transactions", total_transactions_.load()},
        {"failed_queries", failed_queries_.load()},
        {"average_query_time_us", 
         total_queries_.load() > 0 ? 
         static_cast<double>(total_query_time_.load()) / total_queries_.load() : 0.0}
    };
    
    return stats;
}

DatabaseResult<std::shared_ptr<ConnectionPool>> DatabaseManager::get_tenant_pool(
    const std::string& tenant_id) {
    
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = tenant_pools_.find(tenant_id);
    if (it != tenant_pools_.end()) {
        return it->second;
    }
    
    // Create new pool for tenant
    std::string db_path = get_tenant_db_path(tenant_id);
    
    // Ensure tenant directory exists
    std::filesystem::path db_dir = std::filesystem::path(db_path).parent_path();
    if (!std::filesystem::exists(db_dir)) {
        std::filesystem::create_directories(db_dir);
    }
    
    auto pool = std::make_shared<ConnectionPool>(db_path, config_.connection_pool_size);
    
    tenant_pools_[tenant_id] = pool;
    return pool;
}

std::string DatabaseManager::get_tenant_db_path(const std::string& tenant_id) const {
    return config_.base_path + "/" + tenant_id + "/data.sqlite3";
}

DatabaseResult<void> DatabaseManager::configure_connection([[maybe_unused]] sqlite3* connection) const {
    // This would apply tenant-specific connection configuration
    // For now, basic configuration is applied in ConnectionPool::create_connection
    return DatabaseResult<void>{};
}

// ============================================================================
// Configuration snapshot persistence (T029, T032, T033)
// ============================================================================

namespace {
// Time-point → seconds-since-epoch (int64)
int64_t tp_to_epoch(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point epoch_to_tp(int64_t epoch_s) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{epoch_s}};
}
} // anonymous namespace

DatabaseResult<sqlite3*> DatabaseManager::get_config_db() const {
    // Caller must hold config_db_mutex_
    if (!config_store_initialized_ || !config_db_) {
        return DatabaseResult<sqlite3*>{DatabaseError::ConnectionFailed};
    }
    return DatabaseResult<sqlite3*>{config_db_.get()};
}

DatabaseResult<void> DatabaseManager::initialize_config_store() {
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    if (config_store_initialized_) return DatabaseResult<void>{};

    // Open (or create) a dedicated SQLite file for config snapshots
    std::filesystem::path dir{config_.base_path};
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    const std::string db_path = config_.base_path + "/config_snapshots.sqlite3";
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &raw,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        if (raw) sqlite3_close_v2(raw);
        return DatabaseResult<void>{DatabaseError::ConnectionFailed};
    }
    config_db_ = SqliteConnection{raw};

    const char* ddl =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS config_snapshots ("
        "  id          TEXT PRIMARY KEY,"
        "  tenant_id   TEXT NOT NULL,"
        "  version     TEXT NOT NULL,"
        "  display_name TEXT,"
        "  schema_sdl  TEXT,"
        "  is_active   INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL,"
        "  activated_at INTEGER,"
        "  resolver_bindings TEXT NOT NULL DEFAULT '[]'"
        ");";
    char* errmsg = nullptr;
    if (sqlite3_exec(config_db_.get(), ddl, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        const std::string msg = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        spdlog::warn("initialize_config_store: schema DDL failed: {}", msg);
        return DatabaseResult<void>{DatabaseError::SchemaValidationFailed};
    }
    // Migration: add resolver_bindings column to databases created before T048-007.
    // sqlite3_exec silently returns an error if the column already exists — we ignore it.
    sqlite3_exec(config_db_.get(),
        "ALTER TABLE config_snapshots ADD COLUMN resolver_bindings TEXT NOT NULL DEFAULT '[]';",
        nullptr, nullptr, nullptr);
    config_store_initialized_ = true;
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::save_config_snapshot(
    const ConfigurationSnapshot& snap)
{
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    auto db_res = get_config_db();
    if (!db_res) return DatabaseResult<void>{db_res.error()};
    sqlite3* db = db_res.value();

    const char* sql =
        "INSERT INTO config_snapshots"
        "  (id, tenant_id, version, display_name, schema_sdl, is_active, created_at, activated_at, resolver_bindings)"
        " VALUES (?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseResult<void>{DatabaseError::QueryFailed};
    }
    sqlite3_bind_text(stmt, 1, snap.id.c_str(),           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, snap.tenant_id.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, snap.version.c_str(),      -1, SQLITE_TRANSIENT);
    if (snap.display_name.empty())
        sqlite3_bind_null(stmt, 4);
    else
        sqlite3_bind_text(stmt, 4, snap.display_name.c_str(), -1, SQLITE_TRANSIENT);
    if (snap.schema_sdl.empty())
        sqlite3_bind_null(stmt, 5);
    else
        sqlite3_bind_text(stmt, 5, snap.schema_sdl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, snap.is_active ? 1 : 0);
    sqlite3_bind_int64(stmt, 7, tp_to_epoch(snap.created_at));
    if (snap.activated_at)
        sqlite3_bind_int64(stmt, 8, tp_to_epoch(*snap.activated_at));
    else
        sqlite3_bind_null(stmt, 8);
    const std::string bindings_json = snap.resolver_bindings.empty() ? "[]" : snap.resolver_bindings;
    sqlite3_bind_text(stmt, 9, bindings_json.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return DatabaseResult<void>{DatabaseError::QueryFailed};
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::activate_config_snapshot(
    const std::string& snapshot_id)
{
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    auto db_res = get_config_db();
    if (!db_res) return DatabaseResult<void>{db_res.error()};
    sqlite3* db = db_res.value();

    // Find tenant_id for this snapshot
    std::string tenant_id;
    {
        const char* sel = "SELECT tenant_id FROM config_snapshots WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sel, -1, &stmt, nullptr) != SQLITE_OK)
            return DatabaseResult<void>{DatabaseError::QueryFailed};
        sqlite3_bind_text(stmt, 1, snapshot_id.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char* raw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            tenant_id = raw ? raw : "";
        }
        sqlite3_finalize(stmt);
        if (tenant_id.empty()) return DatabaseResult<void>{DatabaseError::TenantNotFound};
    }

    // Atomic activation in a transaction
    if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK)
        return DatabaseResult<void>{DatabaseError::TransactionFailed};

    // Deactivate all for this tenant
    {
        const char* upd = "UPDATE config_snapshots SET is_active=0, activated_at=NULL"
                          " WHERE tenant_id=?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, upd, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return DatabaseResult<void>{DatabaseError::QueryFailed};
        }
        sqlite3_bind_text(stmt, 1, tenant_id.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return DatabaseResult<void>{DatabaseError::QueryFailed};
        }
    }

    // Activate target
    {
        auto now_epoch = tp_to_epoch(std::chrono::system_clock::now());
        const char* upd = "UPDATE config_snapshots SET is_active=1, activated_at=?"
                          " WHERE id=?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, upd, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return DatabaseResult<void>{DatabaseError::QueryFailed};
        }
        sqlite3_bind_int64(stmt, 1, now_epoch);
        sqlite3_bind_text(stmt, 2, snapshot_id.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return DatabaseResult<void>{DatabaseError::QueryFailed};
        }
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
        return DatabaseResult<void>{DatabaseError::TransactionFailed};

    return DatabaseResult<void>{};
}

namespace {
ConfigurationSnapshot row_to_snapshot(sqlite3_stmt* stmt) {
    ConfigurationSnapshot s;
    auto col_str = [&](int col) -> std::string {
        const char* raw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
        return raw ? raw : "";
    };
    s.id           = col_str(0);
    s.tenant_id    = col_str(1);
    s.version      = col_str(2);
    s.display_name = col_str(3);
    s.schema_sdl   = col_str(4);
    s.is_active    = sqlite3_column_int(stmt, 5) != 0;
    s.created_at   = epoch_to_tp(sqlite3_column_int64(stmt, 6));
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
        s.activated_at = epoch_to_tp(sqlite3_column_int64(stmt, 7));
    // Column 8: resolver_bindings (added T048-007; may be absent in old rows)
    if (sqlite3_column_count(stmt) > 8 && sqlite3_column_type(stmt, 8) != SQLITE_NULL)
        s.resolver_bindings = col_str(8);
    return s;
}
} // anonymous namespace

DatabaseResult<std::optional<ConfigurationSnapshot>>
DatabaseManager::get_active_config_snapshot(const std::string& tenant_id) const
{
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    auto db_res = const_cast<DatabaseManager*>(this)->get_config_db();
    if (!db_res) return DatabaseResult<std::optional<ConfigurationSnapshot>>{
        DatabaseError::ConnectionFailed};
    sqlite3* db = db_res.value();

    const char* sql =
        "SELECT id,tenant_id,version,display_name,schema_sdl,is_active,created_at,activated_at,resolver_bindings"
        " FROM config_snapshots WHERE tenant_id=? AND is_active=1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{DatabaseError::QueryFailed};
    sqlite3_bind_text(stmt, 1, tenant_id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto snap = row_to_snapshot(stmt);
        sqlite3_finalize(stmt);
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{std::optional<ConfigurationSnapshot>{snap}};
    }
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE)
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{std::optional<ConfigurationSnapshot>{}};
    return DatabaseResult<std::optional<ConfigurationSnapshot>>{DatabaseError::QueryFailed};
}

DatabaseResult<std::vector<ConfigurationSnapshot>>
DatabaseManager::list_config_snapshots(const std::string& tenant_id) const
{
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    auto db_res = const_cast<DatabaseManager*>(this)->get_config_db();
    if (!db_res) return DatabaseResult<std::vector<ConfigurationSnapshot>>{
        DatabaseError::ConnectionFailed};
    sqlite3* db = db_res.value();

    const char* sql =
        "SELECT id,tenant_id,version,display_name,schema_sdl,is_active,created_at,activated_at,resolver_bindings"
        " FROM config_snapshots WHERE tenant_id=? ORDER BY created_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseResult<std::vector<ConfigurationSnapshot>>{DatabaseError::QueryFailed};
    sqlite3_bind_text(stmt, 1, tenant_id.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<ConfigurationSnapshot> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(row_to_snapshot(stmt));
    }
    sqlite3_finalize(stmt);
    return DatabaseResult<std::vector<ConfigurationSnapshot>>{std::move(results)};
}

DatabaseResult<std::optional<ConfigurationSnapshot>>
DatabaseManager::get_config_snapshot(const std::string& snapshot_id) const
{
    std::lock_guard<std::mutex> lk(config_db_mutex_);
    auto db_res = const_cast<DatabaseManager*>(this)->get_config_db();
    if (!db_res) return DatabaseResult<std::optional<ConfigurationSnapshot>>{
        DatabaseError::ConnectionFailed};
    sqlite3* db = db_res.value();

    const char* sql =
        "SELECT id,tenant_id,version,display_name,schema_sdl,is_active,created_at,activated_at,resolver_bindings"
        " FROM config_snapshots WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{DatabaseError::QueryFailed};
    sqlite3_bind_text(stmt, 1, snapshot_id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto snap = row_to_snapshot(stmt);
        sqlite3_finalize(stmt);
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{std::optional<ConfigurationSnapshot>{snap}};
    }
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE)
        return DatabaseResult<std::optional<ConfigurationSnapshot>>{std::optional<ConfigurationSnapshot>{}};
    return DatabaseResult<std::optional<ConfigurationSnapshot>>{DatabaseError::QueryFailed};
}

// ============================================================================
// System database (T047-000)
// ============================================================================

DatabaseResult<void> DatabaseManager::ensure_system_db() {
    std::lock_guard<std::mutex> lock(system_db_mutex_);

    if (system_db_initialized_) {
        return DatabaseResult<void>{};
    }

    // Determine path: <DataHome>/isched/isched_system.db
    const std::string system_db_dir  = getDataHome() + "/isched";
    const std::string system_db_path = system_db_dir + "/isched_system.db";

    // Ensure the directory exists
    std::error_code ec;
    std::filesystem::create_directories(system_db_dir, ec);
    if (ec) {
        spdlog::error("ensure_system_db: cannot create directory '{}': {}", system_db_dir, ec.message());
        return DatabaseError::ConnectionFailed;
    }

    // Open/create the SQLite file
    sqlite3* raw_db = nullptr;
    int rc = sqlite3_open_v2(
        system_db_path.c_str(), &raw_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        if (raw_db) { sqlite3_close_v2(raw_db); }
        spdlog::error("ensure_system_db: sqlite3_open_v2 failed for '{}'", system_db_path);
        return DatabaseError::ConnectionFailed;
    }
    system_db_ = SqliteConnection{raw_db};

    // Apply pragmas for durability and concurrency
    const char* pragmas =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA foreign_keys=ON;";
    sqlite3_exec(system_db_.get(), pragmas, nullptr, nullptr, nullptr);

    // Create platform tables (idempotent)
    const char* ddl = R"sql(
        CREATE TABLE IF NOT EXISTS platform_admins (
            id            TEXT PRIMARY KEY,
            email         TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            display_name  TEXT NOT NULL DEFAULT '',
            is_active     INTEGER NOT NULL DEFAULT 1,
            created_at    TEXT NOT NULL DEFAULT (datetime('now')),
            last_login    TEXT
        );

        CREATE TABLE IF NOT EXISTS platform_roles (
            id          TEXT PRIMARY KEY,
            name        TEXT NOT NULL UNIQUE,
            description TEXT NOT NULL DEFAULT '',
            created_at  TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS organizations (
            id                TEXT PRIMARY KEY,
            name              TEXT NOT NULL,
            domain            TEXT,
            subscription_tier TEXT NOT NULL DEFAULT 'free',
            user_limit        INTEGER NOT NULL DEFAULT 10,
            storage_limit     INTEGER NOT NULL DEFAULT 1073741824,
            created_at        TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS tenant_settings (
            org_id  TEXT NOT NULL,
            key     TEXT NOT NULL,
            value   TEXT NOT NULL,
            PRIMARY KEY (org_id, key)
        );
    )sql";

    rc = sqlite3_exec(system_db_.get(), ddl, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("ensure_system_db: DDL failed: {}", sqlite3_errmsg(system_db_.get()));
        system_db_.reset();
        return DatabaseError::SchemaValidationFailed;
    }

    // Seed the four built-in platform roles (idempotent via INSERT OR IGNORE)
    const char* seed_sql = R"sql(
        INSERT OR IGNORE INTO platform_roles (id, name, description) VALUES
            ('role_platform_admin', 'platform_admin', 'Full platform administration access'),
            ('role_tenant_admin',   'tenant_admin',   'Administration access for a single organization'),
            ('role_user',           'user',           'Standard authenticated user'),
            ('role_service',        'service',        'Machine-to-machine service account');
    )sql";

    rc = sqlite3_exec(system_db_.get(), seed_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        // Non-fatal: roles may already exist or the insert was partially applied
        spdlog::warn("ensure_system_db: role seeding warning: {}", sqlite3_errmsg(system_db_.get()));
    }

    system_db_initialized_ = true;
    spdlog::info("System database ready at: {}", system_db_path);
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::create_platform_role(
    const std::string& id,
    const std::string& name,
    const std::string& description)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "INSERT INTO platform_roles (id, name, description) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return DatabaseError::DuplicateKey;
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("create_platform_role: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::delete_platform_role(const std::string& id)
{
    // Protect built-in roles
    static constexpr std::array<std::string_view, 4> kBuiltIn = {
        "role_platform_admin", "role_tenant_admin", "role_user", "role_service"
    };
    if (std::find(kBuiltIn.begin(), kBuiltIn.end(), id) != kBuiltIn.end()) {
        return DatabaseError::AccessDenied;
    }

    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    // Verify existence first
    {
        const char* check_sql = "SELECT COUNT(*) FROM platform_roles WHERE id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(system_db_.get(), check_sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        if (count == 0) {
            return DatabaseError::NotFound;
        }
    }

    const char* sql = "DELETE FROM platform_roles WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("delete_platform_role: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// =============================================================================
// T047-005 — platform_role read helpers
// =============================================================================

DatabaseResult<PlatformRoleRecord> DatabaseManager::get_platform_role(const std::string& id)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "SELECT id, name, description, created_at FROM platform_roles WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    PlatformRoleRecord rec;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec.id          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        auto* ca = sqlite3_column_text(stmt, 3);
        rec.created_at  = ca ? reinterpret_cast<const char*>(ca) : "";
    }
    sqlite3_finalize(stmt);

    if (!found) {
        return DatabaseError::NotFound;
    }
    return rec;
}

DatabaseResult<std::vector<PlatformRoleRecord>> DatabaseManager::list_platform_roles()
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "SELECT id, name, description, created_at FROM platform_roles ORDER BY created_at;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }

    std::vector<PlatformRoleRecord> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PlatformRoleRecord rec;
        rec.id          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        auto* ca = sqlite3_column_text(stmt, 3);
        rec.created_at  = ca ? reinterpret_cast<const char*>(ca) : "";
        rows.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return rows;
}

// =============================================================================
// T047-005 — Organization CRUD
// =============================================================================

DatabaseResult<void> DatabaseManager::create_organization(
    const std::string& id,
    const std::string& name,
    const std::string& domain,
    const std::string& subscription_tier,
    int user_limit,
    int storage_limit)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "INSERT INTO organizations (id, name, domain, subscription_tier, user_limit, storage_limit)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, domain.c_str(),            -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, subscription_tier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, user_limit);
    sqlite3_bind_int (stmt, 6, storage_limit);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return DatabaseError::DuplicateKey;
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("create_organization: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<OrganizationRecord> DatabaseManager::get_organization(const std::string& id)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "SELECT id, name, domain, subscription_tier, user_limit, storage_limit, created_at"
        " FROM organizations WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    OrganizationRecord rec;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec.id                = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name              = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        auto* dom             = sqlite3_column_text(stmt, 2);
        rec.domain            = dom ? reinterpret_cast<const char*>(dom) : "";
        rec.subscription_tier = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.user_limit        = sqlite3_column_int(stmt, 4);
        rec.storage_limit     = sqlite3_column_int(stmt, 5);
        auto* ca              = sqlite3_column_text(stmt, 6);
        rec.created_at        = ca ? reinterpret_cast<const char*>(ca) : "";
    }
    sqlite3_finalize(stmt);

    if (!found) {
        return DatabaseError::NotFound;
    }
    return rec;
}

DatabaseResult<std::vector<OrganizationRecord>> DatabaseManager::list_organizations()
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "SELECT id, name, domain, subscription_tier, user_limit, storage_limit, created_at"
        " FROM organizations ORDER BY created_at;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }

    std::vector<OrganizationRecord> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OrganizationRecord rec;
        rec.id                = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name              = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        auto* dom             = sqlite3_column_text(stmt, 2);
        rec.domain            = dom ? reinterpret_cast<const char*>(dom) : "";
        rec.subscription_tier = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.user_limit        = sqlite3_column_int(stmt, 4);
        rec.storage_limit     = sqlite3_column_int(stmt, 5);
        auto* ca              = sqlite3_column_text(stmt, 6);
        rec.created_at        = ca ? reinterpret_cast<const char*>(ca) : "";
        rows.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return rows;
}

DatabaseResult<void> DatabaseManager::update_organization(
    const std::string&         id,
    std::optional<std::string> name,
    std::optional<std::string> domain,
    std::optional<std::string> subscription_tier,
    std::optional<int>         user_limit,
    std::optional<int>         storage_limit)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    // Build SET clause dynamically from non-null optionals
    std::string set_clause;
    std::vector<std::string> text_vals;
    std::vector<int>         int_vals;

    auto append_text = [&](const char* col, const std::optional<std::string>& val) {
        if (val) {
            if (!set_clause.empty()) { set_clause += ", "; }
            set_clause += std::string(col) + " = ?";
            text_vals.push_back(*val);
        }
    };
    auto append_int = [&](const char* col, const std::optional<int>& val) {
        if (val) {
            if (!set_clause.empty()) { set_clause += ", "; }
            set_clause += std::string(col) + " = ?";
            int_vals.push_back(*val);
        }
    };

    append_text("name",              name);
    append_text("domain",            domain);
    append_text("subscription_tier", subscription_tier);
    append_int ("user_limit",        user_limit);
    append_int ("storage_limit",     storage_limit);

    if (set_clause.empty()) {
        // Nothing to update — treat as success
        return DatabaseResult<void>{};
    }

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM organizations WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(system_db_.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const std::string sql = "UPDATE organizations SET " + set_clause + " WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }

    int bind_idx = 1;
    for (const auto& v : text_vals) {
        sqlite3_bind_text(stmt, bind_idx++, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    for (const auto& v : int_vals) {
        sqlite3_bind_int(stmt, bind_idx++, v);
    }
    sqlite3_bind_text(stmt, bind_idx, id.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("update_organization: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::delete_organization(const std::string& id)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM organizations WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(system_db_.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const char* sql = "DELETE FROM organizations WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("delete_organization: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// =============================================================================
// T047-005 — Platform-admin CRUD
// =============================================================================

DatabaseResult<void> DatabaseManager::create_platform_admin(
    const std::string& id,
    const std::string& email,
    const std::string& password_hash,
    const std::string& display_name)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "INSERT INTO platform_admins (id, email, password_hash, display_name)"
        " VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(),            -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, display_name.c_str(),  -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        return DatabaseError::DuplicateKey;
    }
    if (rc != SQLITE_DONE) {
        spdlog::error("create_platform_admin: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// Internal helper — executes a SELECT query with a single TEXT bind parameter
// and returns the first matching PlatformAdminRecord (including password_hash).
static DatabaseResult<PlatformAdminRecord> fetch_platform_admin(
    sqlite3* db, const char* sql, const std::string& bind_val)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, bind_val.c_str(), -1, SQLITE_TRANSIENT);

    PlatformAdminRecord rec;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec.id            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.email         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        auto* dn          = sqlite3_column_text(stmt, 3);
        rec.display_name  = dn ? reinterpret_cast<const char*>(dn) : "";
        rec.is_active     = (sqlite3_column_int(stmt, 4) != 0);
        auto* ca          = sqlite3_column_text(stmt, 5);
        rec.created_at    = ca ? reinterpret_cast<const char*>(ca) : "";
        auto* ll          = sqlite3_column_text(stmt, 6);
        rec.last_login    = ll ? reinterpret_cast<const char*>(ll) : "";
    }
    sqlite3_finalize(stmt);

    if (!found) {
        return DatabaseError::NotFound;
    }
    return rec;
}

DatabaseResult<PlatformAdminRecord> DatabaseManager::get_platform_admin_by_id(
    const std::string& id)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }
    return fetch_platform_admin(system_db_.get(),
        "SELECT id, email, password_hash, display_name, is_active, created_at, last_login"
        " FROM platform_admins WHERE id = ?;",
        id);
}

DatabaseResult<PlatformAdminRecord> DatabaseManager::get_platform_admin_by_email(
    const std::string& email)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }
    return fetch_platform_admin(system_db_.get(),
        "SELECT id, email, password_hash, display_name, is_active, created_at, last_login"
        " FROM platform_admins WHERE email = ?;",
        email);
}

DatabaseResult<std::vector<PlatformAdminRecord>> DatabaseManager::list_platform_admins()
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    const char* sql =
        "SELECT id, email, display_name, is_active, created_at, last_login"
        " FROM platform_admins ORDER BY created_at;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }

    std::vector<PlatformAdminRecord> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PlatformAdminRecord rec;
        // password_hash intentionally excluded from list
        rec.id         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.email      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        auto* dn       = sqlite3_column_text(stmt, 2);
        rec.display_name = dn ? reinterpret_cast<const char*>(dn) : "";
        rec.is_active  = (sqlite3_column_int(stmt, 3) != 0);
        auto* ca       = sqlite3_column_text(stmt, 4);
        rec.created_at = ca ? reinterpret_cast<const char*>(ca) : "";
        auto* ll       = sqlite3_column_text(stmt, 5);
        rec.last_login = ll ? reinterpret_cast<const char*>(ll) : "";
        rows.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return rows;
}

DatabaseResult<void> DatabaseManager::update_platform_admin(
    const std::string&         id,
    std::optional<std::string> display_name,
    std::optional<bool>        is_active)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    std::string set_clause;
    std::vector<std::string> text_vals;
    std::vector<int>         int_vals;

    if (display_name) {
        set_clause += "display_name = ?";
        text_vals.push_back(*display_name);
    }
    if (is_active) {
        if (!set_clause.empty()) { set_clause += ", "; }
        set_clause += "is_active = ?";
        int_vals.push_back(*is_active ? 1 : 0);
    }

    if (set_clause.empty()) {
        return DatabaseResult<void>{};
    }

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM platform_admins WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(system_db_.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const std::string sql = "UPDATE platform_admins SET " + set_clause + " WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }

    int bind_idx = 1;
    for (const auto& v : text_vals) {
        sqlite3_bind_text(stmt, bind_idx++, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    for (const auto& v : int_vals) {
        sqlite3_bind_int(stmt, bind_idx++, v);
    }
    sqlite3_bind_text(stmt, bind_idx, id.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("update_platform_admin: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::delete_platform_admin(const std::string& id)
{
    std::lock_guard<std::mutex> lk(system_db_mutex_);
    if (!system_db_initialized_ || !system_db_) {
        return DatabaseError::ConnectionFailed;
    }

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM platform_admins WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(system_db_.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const char* sql = "DELETE FROM platform_admins WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("delete_platform_admin: sqlite3_step error {} for id='{}'", rc, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// =============================================================================
// T047-010 / T047-012..015 — Tenant user CRUD
// =============================================================================

DatabaseResult<void> DatabaseManager::ensure_users_table(const std::string& tenant_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* ddl = R"sql(
        CREATE TABLE IF NOT EXISTS users (
            id            TEXT PRIMARY KEY,
            email         TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            display_name  TEXT NOT NULL DEFAULT '',
            roles         TEXT NOT NULL DEFAULT '[]',
            is_active     INTEGER NOT NULL DEFAULT 1,
            created_at    TEXT NOT NULL DEFAULT (datetime('now')),
            last_login    TEXT
        );
    )sql";

    if (sqlite3_exec(conn.get(), ddl, nullptr, nullptr, nullptr) != SQLITE_OK) {
        spdlog::error("ensure_users_table: DDL failed for tenant '{}'", tenant_id);
        return DatabaseError::SchemaValidationFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::create_user(
    const std::string& tenant_id,
    const std::string& id,
    const std::string& email,
    const std::string& password_hash,
    const std::string& display_name,
    const std::string& roles_json)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "INSERT INTO users (id, email, password_hash, display_name, roles)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(),            -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, display_name.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, roles_json.c_str(),    -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) { return DatabaseError::DuplicateKey; }
    if (rc != SQLITE_DONE) {
        spdlog::error("create_user: sqlite3_step error {} for tenant='{}' id='{}'", rc, tenant_id, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// Internal: parse a UserRecord from a row (columns: id, email, password_hash,
// display_name, roles JSON, is_active, created_at, last_login).
static UserRecord parse_user_row(sqlite3_stmt* stmt, bool include_hash)
{
    using isched::v0_0_1::backend::UserRecord;
    UserRecord rec;
    rec.id           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    rec.email        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (include_hash) {
        auto* h = sqlite3_column_text(stmt, 2);
        rec.password_hash = h ? reinterpret_cast<const char*>(h) : "";
    }
    auto* dn = sqlite3_column_text(stmt, 3);
    rec.display_name = dn ? reinterpret_cast<const char*>(dn) : "";
    auto* rj = sqlite3_column_text(stmt, 4);
    if (rj) {
        try {
            auto j = nlohmann::json::parse(reinterpret_cast<const char*>(rj));
            if (j.is_array()) {
                for (const auto& v : j) {
                    rec.roles.push_back(v.get<std::string>());
                }
            }
        } catch (...) { /* ignore malformed JSON */ }
    }
    rec.is_active    = (sqlite3_column_int(stmt, 5) != 0);
    auto* ca = sqlite3_column_text(stmt, 6);
    rec.created_at   = ca ? reinterpret_cast<const char*>(ca) : "";
    auto* ll = sqlite3_column_text(stmt, 7);
    rec.last_login   = ll ? reinterpret_cast<const char*>(ll) : "";
    return rec;
}

DatabaseResult<UserRecord> DatabaseManager::get_user_by_id(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "SELECT id, email, password_hash, display_name, roles, is_active, created_at, last_login"
        " FROM users WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    UserRecord rec;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec = parse_user_row(stmt, /*include_hash=*/true);
    }
    sqlite3_finalize(stmt);
    if (!found) { return DatabaseError::NotFound; }
    return rec;
}

DatabaseResult<UserRecord> DatabaseManager::get_user_by_email(
    const std::string& tenant_id,
    const std::string& email)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "SELECT id, email, password_hash, display_name, roles, is_active, created_at, last_login"
        " FROM users WHERE email = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    UserRecord rec;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec = parse_user_row(stmt, /*include_hash=*/true);
    }
    sqlite3_finalize(stmt);
    if (!found) { return DatabaseError::NotFound; }
    return rec;
}

DatabaseResult<std::vector<UserRecord>> DatabaseManager::list_users(
    const std::string& tenant_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    // password_hash placeholder as empty string to keep column indices consistent
    const char* sql =
        "SELECT id, email, '' AS password_hash, display_name, roles, is_active, created_at, last_login"
        " FROM users ORDER BY created_at;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    std::vector<UserRecord> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows.push_back(parse_user_row(stmt, /*include_hash=*/false));
    }
    sqlite3_finalize(stmt);
    return rows;
}

DatabaseResult<void> DatabaseManager::update_user(
    const std::string&         tenant_id,
    const std::string&         id,
    std::optional<std::string> display_name,
    std::optional<std::string> roles_json,
    std::optional<bool>        is_active)
{
    std::string set_clause;
    std::vector<std::string> text_vals;
    std::vector<int>         int_vals;

    auto append_text = [&](const char* col, const std::optional<std::string>& val) {
        if (val) {
            if (!set_clause.empty()) { set_clause += ", "; }
            set_clause += std::string(col) + " = ?";
            text_vals.push_back(*val);
        }
    };
    if (display_name) { append_text("display_name", display_name); }
    if (roles_json)   { append_text("roles",         roles_json); }
    if (is_active) {
        if (!set_clause.empty()) { set_clause += ", "; }
        set_clause += "is_active = ?";
        int_vals.push_back(*is_active ? 1 : 0);
    }
    if (set_clause.empty()) { return DatabaseResult<void>{}; }

    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM users WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(conn.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const std::string sql = "UPDATE users SET " + set_clause + " WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    int bind_idx = 1;
    for (const auto& v : text_vals) {
        sqlite3_bind_text(stmt, bind_idx++, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    for (const auto& v : int_vals) {
        sqlite3_bind_int(stmt, bind_idx++, v);
    }
    sqlite3_bind_text(stmt, bind_idx, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        spdlog::error("update_user: error {} tenant='{}' id='{}'", rc, tenant_id, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::delete_user(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    // Verify existence
    {
        const char* check = "SELECT COUNT(*) FROM users WHERE id = ?;";
        sqlite3_stmt* cs = nullptr;
        if (sqlite3_prepare_v2(conn.get(), check, -1, &cs, nullptr) != SQLITE_OK) {
            return DatabaseError::QueryFailed;
        }
        sqlite3_bind_text(cs, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        int count = 0;
        if (sqlite3_step(cs) == SQLITE_ROW) { count = sqlite3_column_int(cs, 0); }
        sqlite3_finalize(cs);
        if (count == 0) { return DatabaseError::NotFound; }
    }

    const char* sql = "DELETE FROM users WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        spdlog::error("delete_user: error {} tenant='{}' id='{}'", rc, tenant_id, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// ── Session management (T049-001) ─────────────────────────────────────────────

DatabaseResult<void> DatabaseManager::ensure_sessions_table(const std::string& tenant_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* ddl = R"sql(
        CREATE TABLE IF NOT EXISTS sessions (
            id               TEXT PRIMARY KEY,
            user_id          TEXT NOT NULL,
            access_token_id  TEXT NOT NULL DEFAULT '',
            permissions      TEXT NOT NULL DEFAULT '[]',
            roles            TEXT NOT NULL DEFAULT '[]',
            issued_at        TEXT NOT NULL DEFAULT (datetime('now')),
            expires_at       TEXT NOT NULL,
            last_activity    TEXT NOT NULL DEFAULT (datetime('now')),
            transport_scope  TEXT NOT NULL DEFAULT 'any',
            is_revoked       INTEGER NOT NULL DEFAULT 0
        );
    )sql";

    if (sqlite3_exec(conn.get(), ddl, nullptr, nullptr, nullptr) != SQLITE_OK) {
        spdlog::error("ensure_sessions_table: DDL failed for tenant '{}'", tenant_id);
        return DatabaseError::SchemaValidationFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::create_session(
    const std::string& tenant_id,
    const std::string& id,
    const std::string& user_id,
    const std::string& access_token_id,
    const std::string& permissions_json,
    const std::string& roles_json,
    const std::string& expires_at,
    const std::string& transport_scope)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "INSERT INTO sessions (id, user_id, access_token_id, permissions, roles,"
        " expires_at, transport_scope)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(),              -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user_id.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, access_token_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, permissions_json.c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, roles_json.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, expires_at.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, transport_scope.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_CONSTRAINT) { return DatabaseError::DuplicateKey; }
    if (rc != SQLITE_DONE) {
        spdlog::error("create_session: error {} tenant='{}' id='{}'", rc, tenant_id, id);
        return DatabaseError::QueryFailed;
    }
    return DatabaseResult<void>{};
}

// Internal: parse a SessionRecord from a prepared statement row.
// Expected column order: id, user_id, access_token_id, permissions, roles,
//   issued_at, expires_at, last_activity, transport_scope, is_revoked
static SessionRecord parse_session_row(sqlite3_stmt* stmt)
{
    using isched::v0_0_1::backend::SessionRecord;
    auto text = [&](int col) -> std::string {
        auto* p = sqlite3_column_text(stmt, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };
    auto parse_json_array = [](const std::string& json_str) -> std::vector<std::string> {
        std::vector<std::string> result;
        try {
            auto j = nlohmann::json::parse(json_str);
            if (j.is_array()) {
                for (const auto& v : j) {
                    result.push_back(v.get<std::string>());
                }
            }
        } catch (...) {}
        return result;
    };

    SessionRecord rec;
    rec.id               = text(0);
    rec.user_id          = text(1);
    rec.access_token_id  = text(2);
    rec.permissions      = parse_json_array(text(3));
    rec.roles            = parse_json_array(text(4));
    rec.issued_at        = text(5);
    rec.expires_at       = text(6);
    rec.last_activity    = text(7);
    rec.transport_scope  = text(8);
    rec.is_revoked       = (sqlite3_column_int(stmt, 9) != 0);
    return rec;
}

DatabaseResult<SessionRecord> DatabaseManager::get_session(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "SELECT id, user_id, access_token_id, permissions, roles,"
        "       issued_at, expires_at, last_activity, transport_scope, is_revoked"
        " FROM sessions WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto rec = parse_session_row(stmt);
        sqlite3_finalize(stmt);
        return rec;
    }
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) { return DatabaseError::NotFound; }
    return DatabaseError::QueryFailed;
}

DatabaseResult<void> DatabaseManager::update_session_activity(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "UPDATE sessions SET last_activity = datetime('now') WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { return DatabaseError::QueryFailed; }
    if (sqlite3_changes(conn.get()) == 0) { return DatabaseError::NotFound; }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::revoke_session(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "UPDATE sessions SET is_revoked = 1 WHERE id = ? AND is_revoked = 0;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { return DatabaseError::QueryFailed; }
    if (sqlite3_changes(conn.get()) == 0) { return DatabaseError::NotFound; }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::revoke_all_sessions_for_user(
    const std::string& tenant_id,
    const std::string& user_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "UPDATE sessions SET is_revoked = 1 WHERE user_id = ? AND is_revoked = 0;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { return DatabaseError::QueryFailed; }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::revoke_all_sessions_for_org(
    const std::string& tenant_id,
    const std::string& exclude_role)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    // When exclude_role is non-empty, skip sessions whose roles JSON contains it.
    // SQLite's json_each() is available; use instr() as a simpler fallback that
    // avoids a dependency on the json1 extension.
    std::string sql;
    if (exclude_role.empty()) {
        sql = "UPDATE sessions SET is_revoked = 1 WHERE is_revoked = 0;";
    } else {
        // Revoke rows where the roles JSON does NOT contain the excluded role string.
        // Using instr() for portability; this is intentionally conservative.
        sql = "UPDATE sessions SET is_revoked = 1"
              " WHERE is_revoked = 0 AND instr(roles, ?) = 0;";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return DatabaseError::QueryFailed;
    }
    if (!exclude_role.empty()) {
        sqlite3_bind_text(stmt, 1, exclude_role.c_str(), -1, SQLITE_TRANSIENT);
    }
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { return DatabaseError::QueryFailed; }
    return DatabaseResult<void>{};
}

// ── Data sources (T048) ───────────────────────────────────────────────────────

DatabaseResult<void> DatabaseManager::ensure_data_sources_table(const std::string& tenant_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* ddl = R"sql(
        CREATE TABLE IF NOT EXISTS data_sources (
            id                       TEXT PRIMARY KEY,
            name                     TEXT NOT NULL,
            base_url                 TEXT NOT NULL,
            auth_kind                TEXT NOT NULL DEFAULT 'none',
            api_key_header           TEXT NOT NULL DEFAULT '',
            api_key_value_encrypted  TEXT NOT NULL DEFAULT '',
            timeout_ms               INTEGER NOT NULL DEFAULT 5000,
            created_at               TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )sql";

    if (sqlite3_exec(conn.get(), ddl, nullptr, nullptr, nullptr) != SQLITE_OK) {
        spdlog::error("ensure_data_sources_table: DDL failed for tenant '{}'", tenant_id);
        return DatabaseError::SchemaValidationFailed;
    }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::create_data_source(
    const std::string& tenant_id,
    const std::string& id,
    const std::string& name,
    const std::string& base_url,
    const std::string& auth_kind,
    const std::string& api_key_header,
    const std::string& api_key_value_encrypted,
    int timeout_ms)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "INSERT INTO data_sources "
        "(id, name, base_url, auth_kind, api_key_header, api_key_value_encrypted, timeout_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    sqlite3_bind_text(stmt, 1, id.c_str(),                       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(),                     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, base_url.c_str(),                 -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, auth_kind.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, api_key_header.c_str(),           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, api_key_value_encrypted.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 7, timeout_ms);

    const int rc2 = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc2 == SQLITE_CONSTRAINT) { return DatabaseError::DuplicateKey; }
    if (rc2 != SQLITE_DONE)       { return DatabaseError::QueryFailed;   }
    return DatabaseResult<void>{};
}

namespace {
DataSourceRecord row_to_data_source(sqlite3_stmt* s)
{
    DataSourceRecord r;
    auto col_text = [s](int col) -> std::string {
        const char* p = reinterpret_cast<const char*>(sqlite3_column_text(s, col));
        return p ? p : "";
    };
    r.id                      = col_text(0);
    r.name                    = col_text(1);
    r.base_url                = col_text(2);
    r.auth_kind               = col_text(3);
    r.api_key_header          = col_text(4);
    r.api_key_value_encrypted = col_text(5);
    r.timeout_ms              = sqlite3_column_int(s, 6);
    r.created_at              = col_text(7);
    return r;
}
} // anonymous namespace — row_to_data_source

DatabaseResult<DataSourceRecord> DatabaseManager::get_data_source_by_id(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "SELECT id, name, base_url, auth_kind, api_key_header, "
        "       api_key_value_encrypted, timeout_ms, created_at "
        "FROM data_sources WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc2 = sqlite3_step(stmt);
    if (rc2 == SQLITE_ROW) {
        auto rec = row_to_data_source(stmt);
        sqlite3_finalize(stmt);
        return rec;
    }
    sqlite3_finalize(stmt);
    if (rc2 == SQLITE_DONE) { return DatabaseError::NotFound; }
    return DatabaseError::QueryFailed;
}

DatabaseResult<std::vector<DataSourceRecord>> DatabaseManager::list_data_sources(
    const std::string& tenant_id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql =
        "SELECT id, name, base_url, auth_kind, api_key_header, "
        "       api_key_value_encrypted, timeout_ms, created_at "
        "FROM data_sources ORDER BY created_at ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    std::vector<DataSourceRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        records.push_back(row_to_data_source(stmt));
    }
    sqlite3_finalize(stmt);
    return records;
}

DatabaseResult<void> DatabaseManager::update_data_source(
    const std::string& tenant_id,
    const std::string& id,
    std::optional<std::string> name,
    std::optional<std::string> base_url,
    std::optional<std::string> auth_kind,
    std::optional<std::string> api_key_header,
    std::optional<std::string> api_key_value_encrypted,
    std::optional<int>         timeout_ms)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    // Build a dynamic SET clause from the non-null optionals
    std::string set_clause;
    auto append_col = [&](const char* col) {
        if (!set_clause.empty()) set_clause += ", ";
        set_clause += std::string(col) + " = ?";
    };
    if (name)                      append_col("name");
    if (base_url)                  append_col("base_url");
    if (auth_kind)                 append_col("auth_kind");
    if (api_key_header)            append_col("api_key_header");
    if (api_key_value_encrypted)   append_col("api_key_value_encrypted");
    if (timeout_ms)                append_col("timeout_ms");

    if (set_clause.empty()) {
        // Nothing to update — just verify the record exists
        const auto check = get_data_source_by_id(tenant_id, id);
        if (!check) return check.error();
        return DatabaseResult<void>{};
    }

    const std::string dyn_sql = "UPDATE data_sources SET " + set_clause + " WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), dyn_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    int col = 1;
    if (name)                    sqlite3_bind_text(stmt, col++, name->c_str(),                     -1, SQLITE_TRANSIENT);
    if (base_url)                sqlite3_bind_text(stmt, col++, base_url->c_str(),                 -1, SQLITE_TRANSIENT);
    if (auth_kind)               sqlite3_bind_text(stmt, col++, auth_kind->c_str(),                -1, SQLITE_TRANSIENT);
    if (api_key_header)          sqlite3_bind_text(stmt, col++, api_key_header->c_str(),           -1, SQLITE_TRANSIENT);
    if (api_key_value_encrypted) sqlite3_bind_text(stmt, col++, api_key_value_encrypted->c_str(), -1, SQLITE_TRANSIENT);
    if (timeout_ms)              sqlite3_bind_int (stmt, col++, *timeout_ms);
    sqlite3_bind_text(stmt, col, id.c_str(), -1, SQLITE_TRANSIENT);

    const int rc2 = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc2 != SQLITE_DONE)                     { return DatabaseError::QueryFailed; }
    if (sqlite3_changes(conn.get()) == 0)        { return DatabaseError::NotFound;   }
    return DatabaseResult<void>{};
}

DatabaseResult<void> DatabaseManager::delete_data_source(
    const std::string& tenant_id,
    const std::string& id)
{
    auto pool_result = get_tenant_pool(tenant_id);
    if (!pool_result) { return pool_result.error(); }
    auto conn_result = pool_result.value()->acquire();
    if (!conn_result) { return conn_result.error(); }
    auto conn = std::move(conn_result.value());

    const char* sql = "DELETE FROM data_sources WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const int rc2 = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc2 != SQLITE_DONE)                     { return DatabaseError::QueryFailed; }
    if (sqlite3_changes(conn.get()) == 0)        { return DatabaseError::NotFound;   }
    return DatabaseResult<void>{};
}

// ---------------------------------------------------------------------------
// T050-001: tenant_settings key-value helpers (isched_system.db)
// ---------------------------------------------------------------------------

DatabaseResult<void> DatabaseManager::set_tenant_setting(
    const std::string& org_id,
    const std::string& key,
    const std::string& value)
{
    if (!system_db_) return DatabaseError::ConnectionFailed;

    const char* sql =
        "INSERT INTO tenant_settings (org_id, key, value) VALUES (?, ?, ?)"
        " ON CONFLICT(org_id, key) DO UPDATE SET value=excluded.value;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    sqlite3_bind_text(stmt, 1, org_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, value.c_str(),  -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return DatabaseError::QueryFailed;
    return DatabaseResult<void>{};
}

DatabaseResult<std::string> DatabaseManager::get_tenant_setting(
    const std::string& org_id,
    const std::string& key)
{
    if (!system_db_) return DatabaseError::ConnectionFailed;

    const char* sql =
        "SELECT value FROM tenant_settings WHERE org_id = ? AND key = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(system_db_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return DatabaseError::QueryFailed;

    sqlite3_bind_text(stmt, 1, org_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key.c_str(),    -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string result = val ? val : "";
        sqlite3_finalize(stmt);
        return result;
    }
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) return DatabaseError::NotFound;
    return DatabaseError::QueryFailed;
}


} // namespace isched::v0_0_1::backend