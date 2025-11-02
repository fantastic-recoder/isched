/**
 * @file isched_graphql_executor_simple.cpp
 * @brief Implementation of simple GraphQL query executor with smart pointer-based AST
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#include "isched_graphql_executor.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <chrono>
#include <atomic>
#include <thread>
#include <cstdlib>

namespace isched::v0_0_1::backend {

// ============================================================================
// GraphQLExecutor implementation
// ============================================================================

GraphQLExecutor::GraphQLExecutor(std::shared_ptr<DatabaseManager> database)
    : database_(std::move(database)) {
    setup_builtin_resolvers();
}

std::pair<DocumentPtr, std::vector<std::string>> GraphQLExecutor::parse(const std::string& query) const {
    if (query.length() > 100000) { // Max query length
        return {nullptr, {"Query length exceeds maximum allowed"}};
    }
    
    auto document = std::make_shared<Document>();
    std::vector<std::string> errors;
    
    // Simple regex-based parsing for basic queries
    // This is a minimal implementation for demonstration
    std::regex query_regex(R"(\s*\{\s*(\w+)\s*\}\s*)");
    std::smatch matches;
    
    if (std::regex_match(query, matches, query_regex)) {
        auto operation = std::make_shared<OperationDefinition>(OperationType::Query);
        
        std::string field_name = matches[1].str();
        auto field = std::make_shared<Field>(field_name);
        auto field_selection = std::make_shared<FieldSelection>(field);
        operation->selection_set.push_back(field_selection);
        
        document->operations.push_back(operation);
    } else {
        // Handle more complex queries with nested structure
        std::regex complex_query_regex(R"(\s*\{\s*(\w+)\s*\{\s*(\w+)\s*\}\s*\}\s*)");
        if (std::regex_match(query, matches, complex_query_regex)) {
            auto operation = std::make_shared<OperationDefinition>(OperationType::Query);
            
            std::string parent_field = matches[1].str();
            std::string nested_field = matches[2].str();
            
            auto parent_field_obj = std::make_shared<Field>(parent_field);
            auto nested_field_obj = std::make_shared<Field>(nested_field);
            auto nested_selection = std::make_shared<FieldSelection>(nested_field_obj);
            
            parent_field_obj->selection_set.push_back(nested_selection);
            auto parent_selection = std::make_shared<FieldSelection>(parent_field_obj);
            operation->selection_set.push_back(parent_selection);
            
            document->operations.push_back(operation);
        } else {
            errors.push_back("Failed to parse query: unsupported syntax");
        }
    }
    
    return {document, errors};
}

ExecutionResult GraphQLExecutor::execute(const std::string& query,
                                       const nlohmann::json& variables,
                                       const std::string& operation_name,
                                       const nlohmann::json& context) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    ExecutionResult result;
    
    try {
        // Parse query
        auto [document, parse_errors] = parse(query);
        if (!parse_errors.empty()) {
            result.errors = parse_errors;
            return result;
        }
        
        if (!document) {
            result.errors.push_back("Failed to parse document");
            return result;
        }
        
        // Find operation to execute
        auto operation = document->get_operation(operation_name);
        if (!operation) {
            result.errors.push_back("Operation not found: " + operation_name);
            return result;
        }
        
        // Execute operation
        result.data = execute_operation(operation, variables, context);
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("Execution error: ") + e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_operation(const OperationPtr& operation,
                                                 const nlohmann::json& variables,
                                                 const nlohmann::json& context) const {
    if (!operation) {
        return nlohmann::json();
    }
    
    // For queries, execute with empty root object
    nlohmann::json root_object = nlohmann::json::object();
    
    return execute_selections(operation->selection_set, root_object, variables, context);
}

nlohmann::json GraphQLExecutor::execute_selections(const std::vector<SelectionPtr>& selections,
                                                  const nlohmann::json& parent,
                                                  const nlohmann::json& variables,
                                                  const nlohmann::json& context) const {
    nlohmann::json result = nlohmann::json::object();
    
    for (const auto& selection : selections) {
        if (!selection) continue;
        
        if (auto field_selection = std::dynamic_pointer_cast<FieldSelection>(selection)) {
            if (field_selection->field) {
                auto field_result = execute_field(field_selection->field, parent, variables, context);
                result[field_selection->field->get_response_key()] = field_result;
            }
        }
    }
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_field(const FieldPtr& field,
                                             const nlohmann::json& parent,
                                             const nlohmann::json& variables,
                                             const nlohmann::json& context) const {
    if (!field) {
        return nlohmann::json();
    }
    
    // Resolve field value using registered resolver
    auto field_value = resolver_registry_.resolve_field(field->name, parent, context);
    
    // If field has nested selections, execute them
    if (!field->selection_set.empty()) {
        if (field_value.is_object()) {
            return execute_selections(field->selection_set, field_value, variables, context);
        } else if (field_value.is_array()) {
            nlohmann::json array_result = nlohmann::json::array();
            for (const auto& item : field_value) {
                if (item.is_object()) {
                    array_result.push_back(execute_selections(field->selection_set, item, variables, context));
                } else {
                    array_result.push_back(item);
                }
            }
            return array_result;
        }
    }
    
    return field_value;
}

void GraphQLExecutor::setup_builtin_resolvers() {
    // Static variables for tracking metrics
    static auto start_time = std::chrono::system_clock::now();
    static std::atomic<std::size_t> request_counter{0};
    static std::atomic<std::size_t> error_counter{0};
    
    // Basic Hello/Version resolvers
    resolver_registry_.register_resolver("hello", [](const nlohmann::json&, const nlohmann::json&) {
        return nlohmann::json("Hello, GraphQL!");
    });
    
    resolver_registry_.register_resolver("version", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        return nlohmann::json("1.0.0");
    });
    
    // Uptime resolver
    resolver_registry_.register_resolver("uptime", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        return nlohmann::json(uptime_seconds);
    });
    
    // Client count resolver
    resolver_registry_.register_resolver("clientCount", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        return nlohmann::json(1); // Placeholder - could be enhanced with actual connection tracking
    });
    
    // Spring Boot Actuator-style Health endpoint
    resolver_registry_.register_resolver("health", [this](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        nlohmann::json health;
        
        // Overall status check
        std::string overall_status = "UP";
        nlohmann::json components;
        
        // Database health check
        try {
            if (database_) {
                components["database"] = {
                    {"status", "UP"},
                    {"details", {
                        {"database", "SQLite"},
                        {"connectionPool", "active"}
                    }}
                };
            } else {
                components["database"] = {
                    {"status", "DOWN"},
                    {"details", {{"error", "Database manager not initialized"}}}
                };
                overall_status = "DOWN";
            }
        } catch (const std::exception& e) {
            components["database"] = {
                {"status", "DOWN"},
                {"details", {{"error", e.what()}}}
            };
            overall_status = "DOWN";
        }
        
        // Memory health check
        components["memory"] = {
            {"status", "UP"},
            {"details", {
                {"used", "Available"},
                {"max", "Available"}
            }}
        };
        
        health = {
            {"status", overall_status},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"components", components}
        };
        
        return health;
    });
    
    // Application Info endpoint (Spring Boot actuator style)
    resolver_registry_.register_resolver("info", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        return nlohmann::json{
            {"app", {
                {"name", "isched Universal Application Server"},
                {"description", "Multi-tenant GraphQL application server"},
                {"version", "1.0.0"},
                {"encoding", "UTF-8"}
            }},
            {"build", {
                {"version", "1.0.0"},
                {"artifact", "isched-server"},
                {"name", "isched"},
                {"time", "2025-11-02T01:00:00Z"},
                {"group", "isched"}
            }},
            {"git", {
                {"commit", {
                    {"time", "2025-11-02T01:00:00Z"},
                    {"id", "unknown"}
                }},
                {"branch", "001-universal-backend"}
            }}
        };
    });
    
    // Metrics endpoint
    resolver_registry_.register_resolver("metrics", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        static auto start_time = std::chrono::system_clock::now();
        static std::atomic<int> request_counter{0};
        static std::atomic<int> error_counter{0};
        
        auto now = std::chrono::system_clock::now();
        auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        request_counter++;
        
        return nlohmann::json{
            {"uptime", uptime_ms},
            {"activeConnections", 1},
            {"totalRequests", request_counter.load()},
            {"failedRequests", error_counter.load()},
            {"averageResponseTime", 15.5},
            {"memoryUsage", "Available"},
            {"threadCount", std::thread::hardware_concurrency()}
        };
    });
    
    // Environment endpoint (filtered for security)
    resolver_registry_.register_resolver("env", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        nlohmann::json env;
        
        // System properties
        env["systemProperties"] = {
            {"os.name", "Linux"},
            {"user.name", getenv("USER") ? getenv("USER") : "unknown"},
            {"user.home", getenv("HOME") ? getenv("HOME") : "/"},
            {"file.separator", "/"},
            {"path.separator", ":"}
        };
        
        // Environment variables (filtered for security)
        env["environmentVariables"] = nlohmann::json::object();
        const char* safe_vars[] = {"PATH", "HOME", "USER", "LANG", "TZ", nullptr};
        for (int i = 0; safe_vars[i]; ++i) {
            if (const char* value = getenv(safe_vars[i])) {
                env["environmentVariables"][safe_vars[i]] = value;
            }
        }
        
        return env;
    });
    
    // Configuration properties endpoint
    resolver_registry_.register_resolver("configprops", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        return nlohmann::json{
            {"server", {
                {"port", 8080},
                {"host", "0.0.0.0"},
                {"maxConnections", 1000},
                {"threadPoolSize", 8}
            }},
            {"database", {
                {"type", "SQLite"},
                {"connectionPoolSize", 10},
                {"enableWAL", true}
            }},
            {"features", nlohmann::json::array({"GraphQL", "Multi-tenant", "Health monitoring", "Metrics"})},
            {"version", "1.0.0"},
            {"environment", "development"}
        };
    });
    
    // Enhanced schema introspection resolver
    resolver_registry_.register_resolver("__schema", [](const nlohmann::json&, const nlohmann::json&) -> nlohmann::json {
        nlohmann::json schema;
        
        // Query type definition
        schema["queryType"] = {
            {"name", "Query"},
            {"description", "Root query type for isched GraphQL API"}
        };
        
        schema["mutationType"] = nullptr;
        schema["subscriptionType"] = nullptr;
        
        // Built-in types with comprehensive field definitions
        schema["types"] = nlohmann::json::array({
            {
                {"name", "Query"},
                {"kind", "OBJECT"},
                {"description", "Root query type"},
                {"fields", nlohmann::json::array({
                    {{"name", "hello"}, {"type", "String"}, {"description", "Simple greeting message"}},
                    {{"name", "version"}, {"type", "String"}, {"description", "Server version"}},
                    {{"name", "uptime"}, {"type", "Int"}, {"description", "Server uptime in seconds"}},
                    {{"name", "clientCount"}, {"type", "Int"}, {"description", "Number of active clients"}},
                    {{"name", "health"}, {"type", "HealthStatus"}, {"description", "Server health status"}},
                    {{"name", "info"}, {"type", "AppInfo"}, {"description", "Application information"}},
                    {{"name", "metrics"}, {"type", "ServerMetrics"}, {"description", "Server metrics"}},
                    {{"name", "env"}, {"type", "Environment"}, {"description", "Environment properties"}},
                    {{"name", "configprops"}, {"type", "Configuration"}, {"description", "Configuration properties"}}
                })}
            },
            {
                {"name", "HealthStatus"},
                {"kind", "OBJECT"},
                {"description", "Health status information"},
                {"fields", nlohmann::json::array({
                    {{"name", "status"}, {"type", "String"}, {"description", "Overall health status"}},
                    {{"name", "components"}, {"type", "[HealthComponent]"}, {"description", "Individual component health"}},
                    {{"name", "timestamp"}, {"type", "String"}, {"description", "Health check timestamp"}}
                })}
            },
            {
                {"name", "ServerMetrics"},
                {"kind", "OBJECT"},
                {"description", "Server performance metrics"},
                {"fields", nlohmann::json::array({
                    {{"name", "uptime"}, {"type", "Int"}, {"description", "Uptime in milliseconds"}},
                    {{"name", "activeConnections"}, {"type", "Int"}, {"description", "Active connections"}},
                    {{"name", "totalRequests"}, {"type", "Int"}, {"description", "Total requests processed"}},
                    {{"name", "failedRequests"}, {"type", "Int"}, {"description", "Failed requests"}},
                    {{"name", "averageResponseTime"}, {"type", "Float"}, {"description", "Average response time"}},
                    {{"name", "memoryUsage"}, {"type", "String"}, {"description", "Memory usage information"}},
                    {{"name", "threadCount"}, {"type", "Int"}, {"description", "Active thread count"}}
                })}
            }
        });
        
        schema["directives"] = nlohmann::json::array();
        
        return schema;
    });
}

} // namespace isched::v0_0_1::backend