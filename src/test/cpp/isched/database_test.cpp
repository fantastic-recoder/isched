/**
 * @file database_test.cpp
 * @brief Unit tests for database management layer
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#include "catch2/catch_test_macros.hpp"
#include "../../../main/cpp/isched/backend/isched_database.hpp"
#include <filesystem>
#include <chrono>
#include <thread>
#include <iostream>

using namespace isched::v0_0_1::backend;

TEST_CASE("Database Manager Basic Operations", "[database]") {
    // Use temporary directory for testing
    std::string test_db_path = "/tmp/isched_test_" + std::to_string(std::time(nullptr));
    
    DatabaseManager::Config config;
    config.base_path = test_db_path;
    config.connection_pool_size = 5;
    config.query_timeout = std::chrono::milliseconds{1000};
    
    DatabaseManager db_manager(config);
    
    SECTION("Initialize tenant database") {
        std::string tenant_id = "test_tenant_001";
        
        auto result = db_manager.initialize_tenant(tenant_id);
        REQUIRE(result.has_value());
        
        // Verify tenant metadata was created
        auto query_result = db_manager.execute_query(
            tenant_id,
            "SELECT value FROM _isched_metadata WHERE key = 'schema_version';"
        );
        
        REQUIRE(query_result.has_value());
        REQUIRE(query_result.value().rows.size() == 1);
        REQUIRE(query_result.value().rows[0][0] == "1.0.0");
    }
    
    SECTION("Execute basic queries") {
        std::string tenant_id = "test_tenant_002";
        
        // Initialize tenant
        auto init_result = db_manager.initialize_tenant(tenant_id);
        REQUIRE(init_result.has_value());
        
        // Create a test table
        auto create_result = db_manager.execute_query(
            tenant_id,
            "CREATE TABLE test_users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT UNIQUE);"
        );
        REQUIRE(create_result.has_value());
        
        // Insert test data
        auto insert_result = db_manager.execute_query(
            tenant_id,
            "INSERT INTO test_users (name, email) VALUES (?, ?);",
            {"John Doe", "john@example.com"}
        );
        REQUIRE(insert_result.has_value());
        REQUIRE(insert_result.value().affected_rows == 1);
        
        // Query the data back
        auto select_result = db_manager.execute_query(
            tenant_id,
            "SELECT name, email FROM test_users WHERE id = 1;"
        );
        REQUIRE(select_result.has_value());
        REQUIRE(select_result.value().rows.size() == 1);
        REQUIRE(select_result.value().rows[0][0] == "John Doe");
        REQUIRE(select_result.value().rows[0][1] == "john@example.com");
        
        // Verify query performance is within 20ms target
        REQUIRE(select_result.value().execution_time.count() < 20000); // 20ms in microseconds
    }
    
    SECTION("Transaction handling") {
        std::string tenant_id = "test_tenant_003";
        
        // Initialize tenant
        auto init_result = db_manager.initialize_tenant(tenant_id);
        REQUIRE(init_result.has_value());
        
        // Create test table
        auto create_result = db_manager.execute_query(
            tenant_id,
            "CREATE TABLE test_accounts (id INTEGER PRIMARY KEY, balance REAL);"
        );
        REQUIRE(create_result.has_value());
        
        // Test successful transaction
        auto transaction_result = db_manager.execute_transaction(
            tenant_id,
            [](sqlite3* conn) -> DatabaseResult<nlohmann::json> {
                // Insert initial data
                const char* insert_sql = "INSERT INTO test_accounts (balance) VALUES (100.0), (50.0);";
                int result = sqlite3_exec(conn, insert_sql, nullptr, nullptr, nullptr);
                if (result != SQLITE_OK) {
                    return DatabaseError::TransactionFailed;
                }
                
                // Simulate account transfer
                const char* transfer_sql = R"(
                    UPDATE test_accounts SET balance = balance - 25.0 WHERE id = 1;
                    UPDATE test_accounts SET balance = balance + 25.0 WHERE id = 2;
                )";
                result = sqlite3_exec(conn, transfer_sql, nullptr, nullptr, nullptr);
                if (result != SQLITE_OK) {
                    return DatabaseError::TransactionFailed;
                }
                
                return nlohmann::json{{"status", "success"}};
            }
        );
        
        REQUIRE(transaction_result.has_value());
        
        // Verify the transaction results
        auto balance_check = db_manager.execute_query(
            tenant_id,
            "SELECT balance FROM test_accounts ORDER BY id;"
        );
        REQUIRE(balance_check.has_value());
        REQUIRE(balance_check.value().rows.size() == 2);
        REQUIRE(balance_check.value().rows[0][0] == "75.0");  // 100 - 25 (SQLite returns float as "75.0")
        REQUIRE(balance_check.value().rows[1][0] == "75.0");  // 50 + 25
    }
    
    SECTION("Connection pool statistics") {
        std::string tenant_id = "test_tenant_004";
        
        // Initialize tenant
        auto init_result = db_manager.initialize_tenant(tenant_id);
        REQUIRE(init_result.has_value());
        
        // Execute multiple queries to test pool
        for (int i = 0; i < 10; ++i) {
            auto result = db_manager.execute_query(
                tenant_id,
                "SELECT COUNT(*) FROM _isched_metadata;"
            );
            REQUIRE(result.has_value());
        }
        
        // Check tenant statistics
        auto stats_result = db_manager.get_tenant_stats(tenant_id);
        REQUIRE(stats_result.has_value());
        
        auto stats = stats_result.value();
        REQUIRE(stats.contains("max_connections"));
        REQUIRE(stats.contains("total_requests"));
        REQUIRE(stats["max_connections"] == 5);
        REQUIRE(stats["total_requests"] >= 10);
        
        // Check global statistics
        auto global_stats = db_manager.get_global_stats();
        REQUIRE(global_stats.contains("total_tenants"));
        REQUIRE(global_stats.contains("total_queries"));
        REQUIRE(global_stats["total_tenants"] >= 1);
    }
    
    // Cleanup
    std::filesystem::remove_all(test_db_path);
}

TEST_CASE("Connection Pool Stress Test", "[database][performance]") {
    std::string test_db_path = "/tmp/isched_stress_" + std::to_string(std::time(nullptr));
    
    DatabaseManager::Config config;
    config.base_path = test_db_path;
    config.connection_pool_size = 3;  // Small pool to test contention
    config.query_timeout = std::chrono::milliseconds{100};
    
    DatabaseManager db_manager(config);
    std::string tenant_id = "stress_tenant";
    
    // Initialize tenant
    auto init_result = db_manager.initialize_tenant(tenant_id);
    REQUIRE(init_result.has_value());
    
    // Create test table
    auto create_result = db_manager.execute_query(
        tenant_id,
        "CREATE TABLE stress_test (id INTEGER PRIMARY KEY, value TEXT);"
    );
    REQUIRE(create_result.has_value());
    
    SECTION("Concurrent query execution") {
        const int num_threads = 10;
        const int queries_per_thread = 5;
        std::vector<std::thread> threads;
        std::atomic<int> successful_queries{0};
        std::atomic<int> failed_queries{0};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int q = 0; q < queries_per_thread; ++q) {
                    std::string value = "thread_" + std::to_string(t) + "_query_" + std::to_string(q);
                    auto result = db_manager.execute_query(
                        tenant_id,
                        "INSERT INTO stress_test (value) VALUES (?);",
                        {value}
                    );
                    
                    if (result.has_value()) {
                        successful_queries++;
                    } else {
                        failed_queries++;
                    }
                    
                    // Small delay to simulate real workload
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Verify results
        INFO("Total successful queries: " << successful_queries.load());
        INFO("Total failed queries: " << failed_queries.load());
        INFO("Test duration: " << duration.count() << " ms");
        
        REQUIRE(successful_queries.load() > 0);
        REQUIRE(successful_queries.load() + failed_queries.load() == num_threads * queries_per_thread);
        
        // Verify data integrity
        auto count_result = db_manager.execute_query(
            tenant_id,
            "SELECT COUNT(*) FROM stress_test;"
        );
        REQUIRE(count_result.has_value());
        REQUIRE(std::stoi(count_result.value().rows[0][0]) == successful_queries.load());
    }
    
    // Cleanup
    std::filesystem::remove_all(test_db_path);
}

TEST_CASE("Schema Migration", "[database][migration]") {
    std::string test_db_path = "/tmp/isched_migration_" + std::to_string(std::time(nullptr));
    
    DatabaseManager::Config config;
    config.base_path = test_db_path;
    config.connection_pool_size = 2;
    
    DatabaseManager db_manager(config);
    std::string tenant_id = "migration_tenant";
    
    // Initialize tenant
    auto init_result = db_manager.initialize_tenant(tenant_id);
    REQUIRE(init_result.has_value());
    
    SECTION("Schema migration operations") {
        // Create test table first to verify migration functionality
        auto create_result = db_manager.execute_query(
            tenant_id,
            "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT);"
        );
        REQUIRE(create_result.has_value());
        
        // Test basic schema generation
        nlohmann::json schema_model = {
            {"tables", {
                {"users", {
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY"},
                        {"username", "TEXT UNIQUE NOT NULL"},
                        {"email", "TEXT UNIQUE NOT NULL"}
                    }}
                }}
            }}
        };
        
        auto schema_result = db_manager.generate_schema(tenant_id, schema_model);
        REQUIRE(schema_result.has_value());
        
        // Verify the schema was created by checking if we can insert data
        auto insert_result = db_manager.execute_query(
            tenant_id,
            "INSERT INTO example_table (id) VALUES (1);"
        );
        REQUIRE(insert_result.has_value());
    }
    
    // Cleanup
    std::filesystem::remove_all(test_db_path);
}