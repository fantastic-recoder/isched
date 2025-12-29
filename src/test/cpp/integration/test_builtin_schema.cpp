/**
 * @file test_builtin_schema.cpp
 * @brief Integration test for built-in GraphQL schema - User Story 1
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * Tests that frontend developers get an immediately available GraphQL endpoint
 * with built-in schema and resolvers without any additional configuration.
 * 
 * This test MUST FAIL initially (TDD approach) before implementation.
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include "isched/backend/isched_Server.hpp"
#include "isched/backend/isched_built_in_schema.hpp"
#include "isched/backend/isched_GqlExecutor.hpp"
#include "isched/shared/config/isched_config.hpp"

using namespace isched::v0_0_1::backend;
using namespace std::chrono_literals;

/**
 * @brief Test fixture for built-in GraphQL schema integration tests
 */
class BuiltInSchemaTestFixture {
public:
    BuiltInSchemaTestFixture() {
        // Create server with default configuration
        auto config = config_utils::create_default_server_config();
        config.port = 8889; // Different port for this test suite
        config.enable_playground = true; // Enable GraphQL playground for testing
        
        /*
        server = Server::create(config);
        schema = BuiltInSchema::create();
        executor = GqlExecutor::create(TODO);
    */
    }
    
    ~BuiltInSchemaTestFixture() {
        if (server && server->is_running()) {
            server->stop();
        }
    }
    
protected:
    std::unique_ptr<Server> server;
    std::unique_ptr<BuiltInSchema> schema;
    std::unique_ptr<GqlExecutor> executor;
};

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "Built-in schema creation and structure", "[integration][graphql][schema][us1]") {
    SECTION("Built-in schema can be created") {
        REQUIRE(schema != nullptr);
        REQUIRE(schema->is_valid());
    }
    
    SECTION("Built-in schema contains required queries") {
        auto schema_definition = schema->get_schema_definition();
        REQUIRE_FALSE(schema_definition.empty());
        
        // Verify core queries are present
        REQUIRE(schema_definition.find("hello") != schema_definition.end());
        REQUIRE(schema_definition.find("version") != schema_definition.end());
        REQUIRE(schema_definition.find("clientCount") != schema_definition.end());
        REQUIRE(schema_definition.find("uptime") != schema_definition.end());
        REQUIRE(schema_definition.find("health") != schema_definition.end());
    }
    
    SECTION("Built-in schema provides GraphQL playground endpoint") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100ms));
        
        REQUIRE(server->has_playground_endpoint());
        
        auto playground_url = server->get_playground_url();
        REQUIRE_FALSE(playground_url.empty());
        REQUIRE(playground_url.path().find("/graphql") != std::string::npos);
        
        server->stop();
    }
}

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "Basic GraphQL query execution", "[integration][graphql][queries][us1]") {
    SECTION("Hello query returns expected response") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string hello_query = R"(
            query {
                hello
            }
        )";
        
        auto result = executor->execute(hello_query, "test_tenant");
        REQUIRE(result.errors.empty());
        REQUIRE_FALSE(result.data.empty());
        REQUIRE(result.data.find("Hello from Isched") != result.data.end());
        
        server->stop();
    }
    
    SECTION("Version query returns server version") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string version_query = R"(
            query {
                version
            }
        )";
        
        auto result = executor->execute(version_query, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Version should be in semantic version format
        REQUIRE(result.data.find(".") != result.data.end());
        
        server->stop();
    }
    
    SECTION("Client count query returns numeric value") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string client_count_query = R"(
            query {
                clientCount
            }
        )";
        
        auto result = executor->execute(client_count_query, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Should contain numeric value (0 or higher)
        REQUIRE(result.data.find("clientCount") != result.data.end());
        
        server->stop();
    }
    
    SECTION("Uptime query returns time information") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200s)); // Let server run for a bit
        
        const std::string uptime_query = R"(
            query {
                uptime
            }
        )";
        
        auto result = executor->execute(uptime_query, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Should contain uptime information
        REQUIRE(result.data.find("uptime") != result.data.end());
        
        server->stop();
    }
}

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "GraphQL introspection support", "[integration][graphql][introspection][us1]") {
    SECTION("Schema supports introspection queries") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string introspection_query = R"(
            query {
                __schema {
                    types {
                        name
                    }
                }
            }
        )";
        
        auto result = executor->execute(introspection_query, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Should contain schema type information
        REQUIRE(result.data.find("__schema") != result.data.end());
        REQUIRE(result.data.find("types") != result.data.end());
        
        server->stop();
    }
    
    SECTION("Query type introspection") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string query_type_introspection = R"(
            query {
                __schema {
                    queryType {
                        name
                        fields {
                            name
                            type {
                                name
                            }
                        }
                    }
                }
            }
        )";
        
        auto result = executor->execute(query_type_introspection, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Should list available query fields
        REQUIRE(result.data.find("hello") != result.data.end());
        REQUIRE(result.data.find("version") != result.data.end());
        REQUIRE(result.data.find("clientCount") != result.data.end());
        
        server->stop();
    }
}

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "Performance requirements validation", "[integration][graphql][performance][us1]") {
    SECTION("GraphQL queries meet 20ms response requirement") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string simple_query = R"(
            query {
                hello
                version
            }
        )";
        
        // Execute multiple queries to get average performance
        const int num_queries = 10;
        auto total_start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_queries; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            auto result = executor->execute(simple_query, "test_tenant");
            REQUIRE(result.is_success());
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            // Each individual query should be under 20ms (Constitutional requirement)
            REQUIRE(duration.count() < 20);
        }
        
        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);
        
        // Average time per query should be well under 20ms
        auto avg_time = total_duration.count() / num_queries;
        REQUIRE(avg_time < 15);
        
        server->stop();
    }
}

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "Error handling and validation", "[integration][graphql][errors][us1]") {
    SECTION("Invalid GraphQL syntax returns proper error") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string invalid_query = R"(
            query {
                hello
                // missing closing brace
        )";
        
        auto result = executor->execute(invalid_query, "test_tenant");
        REQUIRE_FALSE(result.is_success());
        REQUIRE_FALSE(result.errors.empty());
        /*
        REQUIRE(result.errors.find("syntax") != result.data.end() ||
                result.errors.find("parse") != result.data.end());
                */

        server->stop();
    }
    
    SECTION("Non-existent field returns proper error") {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        const std::string invalid_field_query = R"(
            query {
                nonExistentField
            }
        )";
        
        auto result = executor->execute(invalid_field_query, "test_tenant");
        REQUIRE_FALSE(result.is_success());
        REQUIRE_FALSE(result.errors.empty());
        
        server->stop();
    }
}

TEST_CASE_METHOD(BuiltInSchemaTestFixture, "Frontend developer user story validation", "[integration][graphql][user_story][us1]") {
    SECTION("Complete GraphQL endpoint availability workflow") {
        // Simulate frontend developer experience:
        // "Frontend developers get immediate GraphQL endpoint with built-in schema"
        
        // Step 1: Start server
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100s));
        
        // Step 2: Verify GraphQL endpoint is immediately available
        REQUIRE(server->has_graphql_endpoint());
        
        // Step 3: Verify built-in schema is automatically loaded
        auto schema_def = schema->get_schema_definition();
        REQUIRE_FALSE(schema_def.empty());
        
        // Step 4: Execute basic queries without any setup
        const std::string test_query = R"(
            query {
                hello
                version
                clientCount
            }
        )";
        
        auto result = executor->execute(test_query, "test_tenant");
        REQUIRE(result.is_success());
        REQUIRE_FALSE(result.data.empty());
        
        // Step 5: Verify GraphQL playground is available for exploration
        REQUIRE(server->has_playground_endpoint());
        
        server->stop();
        
        // User story success: Frontend developer has working GraphQL endpoint with zero configuration
    }
}