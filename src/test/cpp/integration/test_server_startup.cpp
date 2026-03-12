// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_server_startup.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Integration tests for server startup functionality (User Story 1)
 * 
 * Tests the basic server lifecycle and confirms that the built-in GraphQL
 * schema is available through the server-facing GraphQL execution path.
 * 
 * Constitutional Requirements:
 * - All response times must be under 20ms
 * - Security-first design validation
 * - GraphQL specification compliance
 * - Multi-tenant architecture validation
 * 
 * @author Isched Development Team
 * @date 2025-11-02
 */

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
#include <memory>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_common.hpp>
#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;

/**
 * @brief Test fixture for server startup integration tests
 * 
 * Provides a clean server instance for each test case with proper
 * setup and teardown to ensure test isolation.
 */
class ServerStartupTestFixture {
public:
    std::unique_ptr<Server> server;
    
    ServerStartupTestFixture() {
        // Initialize test configuration using correct Server::Configuration type
        Server::Configuration config;
        config.port = 8888;
        config.max_threads = 4;
        config.enable_introspection = true;
        config.max_query_complexity = 500;
        
        server = Server::create(config);
    }
    
    ~ServerStartupTestFixture() {
        if (server && server->get_status() != Server::Status::STOPPED) {
            server->stop();
        }
    }
};

TEST_CASE_METHOD(ServerStartupTestFixture, "Basic server creation and lifecycle", "[integration][server][us1]") {
    SECTION("Server can be created with default configuration") {
        REQUIRE(server != nullptr);
        REQUIRE(server->get_status() == Server::Status::STOPPED);
    }
    
    SECTION("Server can start and stop successfully") {
        bool started = server->start();
        REQUIRE(started);
        REQUIRE(server->get_status() == Server::Status::RUNNING);
        
        // Verify server is serving
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        REQUIRE(server->get_status() == Server::Status::RUNNING);
        
        bool stopped = server->stop();
        REQUIRE(stopped);
        REQUIRE(server->get_status() == Server::Status::STOPPED);
        
        // Verify port is accessible as configured
        const auto& config = server->get_configuration();
        REQUIRE(config.port == 8888);
    }
}

TEST_CASE_METHOD(ServerStartupTestFixture, "Built-in GraphQL schema availability", "[integration][graphql][us1]") {
    SECTION("Server exposes the GraphQL endpoint metadata") {
        bool started = server->start();
        REQUIRE(started);
        REQUIRE(server->get_status() == Server::Status::RUNNING);

        REQUIRE(server->has_graphql_endpoint());
        REQUIRE(server->get_graphql_endpoint_path() == "/graphql");
        REQUIRE_FALSE(server->has_playground_endpoint());
        
        server->stop();
    }
    
    SECTION("Server executes built-in GraphQL queries") {
        bool started = server->start();
        REQUIRE(started);

        auto graphql_response = server->execute_graphql("{ hello version }");
        REQUIRE_FALSE(graphql_response.empty());

        auto graphql_json = nlohmann::json::parse(graphql_response);
        REQUIRE(graphql_json.contains("data"));
        REQUIRE(graphql_json["data"]["hello"] == "Hello, GraphQL!");
        REQUIRE(graphql_json["data"]["version"] == "0.0.1");
        REQUIRE(graphql_json["extensions"]["endpoint"] == "/graphql");

        server->stop();
    }

    SECTION("Server exposes operational status through GraphQL fields") {
        bool started = server->start();
        REQUIRE(started);

        auto graphql_response = server->execute_graphql(
            "{ health { status components } serverInfo { version activeTenants transportModes } }");
        auto graphql_json = nlohmann::json::parse(graphql_response);

        REQUIRE(graphql_json.contains("data"));
        REQUIRE(graphql_json["data"]["health"]["status"] == "UP");
        REQUIRE(graphql_json["data"]["serverInfo"]["version"] == "0.0.1");
        REQUIRE(graphql_json["data"]["serverInfo"]["activeTenants"] == 1);
        REQUIRE(graphql_json["data"]["serverInfo"]["transportModes"].is_array());
        
        server->stop();
    }
}

TEST_CASE_METHOD(ServerStartupTestFixture, "Performance validation", "[integration][performance][us1]") {
    SECTION("Built-in GraphQL query response time meets constitutional requirements") {
        bool started = server->start();
        REQUIRE(started);

        auto start_time = std::chrono::high_resolution_clock::now();
        auto graphql_response = server->execute_graphql("{ health { status } }");
        auto end_time = std::chrono::high_resolution_clock::now();

        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        REQUIRE(duration_ms.count() < 20);
        REQUIRE_FALSE(graphql_response.empty());

        server->stop();
    }
}

TEST_CASE_METHOD(ServerStartupTestFixture, "Configuration validation", "[integration][config][us1]") {
    SECTION("Server configuration is accessible and valid") {
        const auto& config = server->get_configuration();
        
        // Verify configuration values
        REQUIRE(config.port == 8888);
        REQUIRE(config.max_threads == 4);
        REQUIRE(config.enable_introspection == true);
        REQUIRE(config.max_query_complexity == 500);
        
        // Verify configuration validates correctly
        REQUIRE(config.validate());
    }
    
    SECTION("Server can be configured for different environments") {
        // Create production-like configuration
        Server::Configuration prod_config;
        prod_config.port = 80;
        prod_config.max_threads = 16;
        prod_config.enable_introspection = false;  // Production security
        prod_config.max_query_complexity = 2000;
        
        auto prod_server = Server::create(prod_config);
        REQUIRE(prod_server != nullptr);
        
        const auto& retrieved_config = prod_server->get_configuration();
        REQUIRE(retrieved_config.port == 80);
        REQUIRE(retrieved_config.max_threads == 16);
    }
}