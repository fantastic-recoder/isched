/**
 * @file isched_database.cpp
 * @brief Implementation of SQLite-based database management layer
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#include "isched_database.hpp"
#include <spdlog/spdlog.h>
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
    
    while (available_connections_.empty() && active_connections_ >= max_connections_) {
        // Wait for a connection to become available or timeout
        std::condition_variable cv;
        if (cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            return DatabaseError::PoolExhausted;
        }
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
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (active_connections_ > 0) {
        active_connections_--;
    }
    
    if (available_connections_.size() < max_connections_) {
        available_connections_.push(std::move(connection));
    }
    // If pool is full, connection will be destroyed automatically
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
    const nlohmann::json& data_model) {
    
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

DatabaseResult<void> DatabaseManager::configure_connection(sqlite3* connection) const {
    // This would apply tenant-specific connection configuration
    // For now, basic configuration is applied in ConnectionPool::create_connection
    return DatabaseResult<void>{};
}

} // namespace isched::v0_0_1::backend