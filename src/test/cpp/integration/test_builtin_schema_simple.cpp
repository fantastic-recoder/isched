/**
 * @file test_builtin_schema_simple.cpp
 * @brief Simple integration tests for built-in GraphQL schema functionality
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 * 
 * These tests validate the built-in GraphQL schema and resolvers that provide
 * server health monitoring, introspection, and administrative endpoints.
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include "isched/backend/built_in_schema.hpp"
#include "isched/backend/isched_server.hpp"
#include "isched/backend/isched_graphql_executor.hpp"
#include "isched/backend/isched_tenant_manager.hpp"

using namespace isched::v0_0_1::backend;
using namespace std::chrono_literals;

/**
 * @brief Test fixture for built-in schema integration tests
 */
class BuiltInSchemaTestFixture {
public:
    BuiltInSchemaTestFixture() {
        server = Server::create();
        auto config = server->get_configuration();
        config.port = 8891; // Different port from other tests
        config.host = "localhost";
        config.min_threads = 2;
        config.max_threads = 10;
        config.response_timeout = 20ms;
        config.enable_introspection = true;
        server->set_configuration(config);
        
        tenant_manager = TenantManager::create();
        executor = GraphQLExecutor::create();
        built_in_schema = BuiltInSchema::create();
        
        // Set up built-in resolvers
        executor->setup_builtin_resolvers();
        
        // Start server
        server->start();
        std::this_thread::sleep_for(100ms); // Allow server to start
    }
    
    ~BuiltInSchemaTestFixture() {
        if (server) {
            server->stop();
            std::this_thread::sleep_for(50ms); // Allow cleanup
        }
    }
    
    std::shared_ptr<Server> server;
    std::shared_ptr<TenantManager> tenant_manager;
    std::shared_ptr<GraphQLExecutor> executor;
    std::shared_ptr<BuiltInSchema> built_in_schema;
};

TEST_CASE("Built-in schema registration and validation", "[builtin_schema][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Built-in schema can be retrieved") {
        auto schema_definition = fixture.built_in_schema->get_schema_definition();
        REQUIRE(!schema_definition.empty());
        
        // Check for key schema elements
        REQUIRE(schema_definition.find("health") != std::string::npos);
        REQUIRE(schema_definition.find("Query") != std::string::npos);
        REQUIRE(schema_definition.find("HealthStatus") != std::string::npos);
    }
    
    SECTION("Server started successfully") {
        REQUIRE(fixture.server->get_status() == Server::Status::Running);
    }
}

TEST_CASE("GraphQL resolver functionality", "[builtin_schema][resolvers][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Hello query executes successfully") {
        std::string hello_query = R"(
            query {
                hello
            }
        )";
        
        auto result = fixture.executor->execute(hello_query, "test_tenant");
        REQUIRE(!result.data.is_null());
        REQUIRE(result.errors.empty());
        REQUIRE(result.data.contains("hello"));
        REQUIRE(result.data["hello"].is_string());
    }
    
    SECTION("Version query executes successfully") {
        std::string version_query = R"(
            query {
                version
            }
        )";
        
        auto result = fixture.executor->execute(version_query, "test_tenant");
        REQUIRE(!result.data.is_null());
        REQUIRE(result.errors.empty());
        REQUIRE(result.data.contains("version"));
        REQUIRE(result.data["version"].is_string());
    }
    
    SECTION("Health query executes successfully") {
        std::string health_query = R"(
            query {
                health {
                    status
                    uptime
                    memoryUsage
                }
            }
        )";
        
        auto result = fixture.executor->execute(health_query, "test_tenant");
        REQUIRE(!result.data.is_null());
        REQUIRE(result.errors.empty());
        REQUIRE(result.data.contains("health"));
        REQUIRE(result.data["health"].is_object());
        REQUIRE(result.data["health"].contains("status"));
    }
}

TEST_CASE("GraphQL introspection functionality", "[builtin_schema][introspection][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Schema introspection query works") {
        std::string introspection_query = R"(
            query {
                __schema {
                    queryType {
                        name
                    }
                }
            }
        )";
        
        auto result = fixture.executor->execute(introspection_query, "test_tenant");
        REQUIRE(!result.data.is_null());
        REQUIRE(result.errors.empty());
        REQUIRE(result.data.contains("__schema"));
    }
}

TEST_CASE("Performance and response time validation", "[builtin_schema][performance][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Query execution meets 20ms performance requirement") {
        std::string simple_query = R"(
            query {
                hello
                version
            }
        )";
        
        // Execute multiple times to get average
        auto total_time = 0ms;
        const int iterations = 10;
        
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = fixture.executor->execute(simple_query, "test_tenant");
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            total_time += duration;
            
            REQUIRE(!result.data.is_null());
            REQUIRE(result.errors.empty());
        }
        
        auto average_time = total_time / iterations;
        REQUIRE(average_time <= 20ms); // Constitutional requirement
    }
}

TEST_CASE("Error handling for invalid queries", "[builtin_schema][error_handling][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Invalid syntax returns error") {
        std::string invalid_query = R"(
            query {
                hello {
            }
        )";
        
        auto result = fixture.executor->execute(invalid_query, "test_tenant");
        REQUIRE(!result.errors.empty());
    }
    
    SECTION("Invalid field returns error") {
        std::string invalid_field_query = R"(
            query {
                nonExistentField
            }
        )";
        
        auto result = fixture.executor->execute(invalid_field_query, "test_tenant");
        REQUIRE(!result.errors.empty());
    }
}

TEST_CASE("Complete User Story 1 validation", "[builtin_schema][user_story_1][integration]") {
    BuiltInSchemaTestFixture fixture;
    
    SECTION("Frontend developer can immediately use GraphQL") {
        // Verify server is running
        REQUIRE(fixture.server->get_status() == Server::Status::Running);
        
        // Verify GraphQL endpoint responds to queries
        std::string test_query = R"(
            query {
                hello
                version
                health {
                    status
                }
            }
        )";
        
        auto result = fixture.executor->execute(test_query, "test_tenant");
        REQUIRE(!result.data.is_null());
        REQUIRE(result.errors.empty());
        REQUIRE(result.data.contains("hello"));
        REQUIRE(result.data.contains("version"));
        REQUIRE(result.data.contains("health"));
    }
}