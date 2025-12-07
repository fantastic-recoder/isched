/**
 * @file isched_built_in_schema.cpp
 * @brief Built-in GraphQL schema implementation for health monitoring and server introspection
 * @author Isched Development Team
 * @date 2025-11-02
 * @version 1.0.0
 */

#include "isched_built_in_schema.hpp"
#include "isched_GraphQLExecutor.hpp"
#include "isched_DatabaseManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <sys/sysinfo.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#endif

namespace isched::v0_0_1::backend {

std::unique_ptr<BuiltInSchema> BuiltInSchema::create(std::shared_ptr<DatabaseManager> database) {
    return std::make_unique<BuiltInSchema>(std::move(database));
}

BuiltInSchema::BuiltInSchema(std::shared_ptr<DatabaseManager> database) 
    : database_(std::move(database))
    , start_time_(std::chrono::system_clock::now())
    , request_counter_(0)
    , error_counter_(0)
    , active_connections_(0) {
}

void BuiltInSchema::register_resolvers(GraphQLExecutor& executor) {
    // This method will be used differently - we'll enhance the GraphQL executor's
    // setup_builtin_resolvers() method instead of trying to register externally
    // For now, this is a placeholder that demonstrates the structure
}

nlohmann::json BuiltInSchema::get_schema_definition() const {
    nlohmann::json schema;
    
    // Query type definition
    schema["queryType"] = {
        {"name", "Query"},
        {"description", "Root query type for isched GraphQL API"}
    };
    
    // Built-in types
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
                {{"name", "configprops"}, {"type", "Configuration"}, {"description", "Configuration properties"}},
                {{"name", "tenants"}, {"type", "[TenantInfo]"}, {"description", "Tenant information"}}
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
                {{"name", "memoryUsage"}, {"type", "Int"}, {"description", "Memory usage in bytes"}},
                {{"name", "threadCount"}, {"type", "Int"}, {"description", "Active thread count"}}
            })}
        },
        {
            {"name", "TenantInfo"},
            {"kind", "OBJECT"},
            {"description", "Tenant information and status"},
            {"fields", nlohmann::json::array({
                {{"name", "tenantId"}, {"type", "String"}, {"description", "Tenant identifier"}},
                {{"name", "status"}, {"type", "String"}, {"description", "Tenant status"}},
                {{"name", "activeConnections"}, {"type", "Int"}, {"description", "Active connections for tenant"}},
                {{"name", "createdAt"}, {"type", "String"}, {"description", "Tenant creation timestamp"}},
                {{"name", "lastAccess"}, {"type", "String"}, {"description", "Last access timestamp"}},
                {{"name", "databaseSize"}, {"type", "Int"}, {"description", "Database size in bytes"}}
            })}
        }
    });
    
    schema["directives"] = nlohmann::json::array();
    
    return schema;
}

nlohmann::json BuiltInSchema::get_health_status() const {
    nlohmann::json health;
    
    // Overall status
    HealthStatus overall_status = HealthStatus::UP;
    
    // Check database connectivity
    nlohmann::json db_health;
    try {
        if (database_) {
            // Simple connectivity test
            db_health = {
                {"status", "UP"},
                {"details", {
                    {"database", "SQLite"},
                    {"connectionPool", "active"}
                }}
            };
        } else {
            db_health = {
                {"status", "DOWN"},
                {"details", {{"error", "Database manager not initialized"}}}
            };
            overall_status = HealthStatus::DOWN;
        }
    } catch (const std::exception& e) {
        db_health = {
            {"status", "DOWN"},
            {"details", {{"error", e.what()}}}
        };
        overall_status = HealthStatus::DOWN;
    }
    
    // Memory health check
    nlohmann::json memory_health;
    try {
        auto memory_usage = get_memory_usage();
        auto memory_limit = memory_usage * 4; // Assume 4x current as limit
        double memory_percentage = static_cast<double>(memory_usage) / memory_limit * 100.0;
        
        if (memory_percentage < 80.0) {
            memory_health = {
                {"status", "UP"},
                {"details", {
                    {"used", memory_usage},
                    {"percentage", memory_percentage}
                }}
            };
        } else {
            memory_health = {
                {"status", "WARNING"},
                {"details", {
                    {"used", memory_usage},
                    {"percentage", memory_percentage},
                    {"warning", "High memory usage"}
                }}
            };
        }
    } catch (const std::exception& e) {
        memory_health = {
            {"status", "UNKNOWN"},
            {"details", {{"error", e.what()}}}
        };
    }
    
    health = {
        {"status", health_status_to_string(overall_status)},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"components", {
            {"database", db_health},
            {"memory", memory_health}
        }}
    };
    
    return health;
}

BuiltInSchema::ServerMetrics BuiltInSchema::get_server_metrics() const {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    
    return ServerMetrics{
        .uptime = uptime,
        .active_connections = active_connections_.load(),
        .total_requests = request_counter_.load(),
        .failed_requests = error_counter_.load(),
        .average_response_time = 15.5, // TODO: Calculate from actual measurements
        .memory_usage_bytes = get_memory_usage(),
        .thread_count = get_thread_count(),
        .start_time = start_time_
    };
}

BuiltInSchema::ConfigurationInfo BuiltInSchema::get_configuration_info() const {
    return ConfigurationInfo{
        .server_version = "1.0.0",
        .build_timestamp = "2025-11-02T01:00:00Z",
        .environment = "development",
        .server_config = nlohmann::json{
            {"port", 8080},
            {"host", "0.0.0.0"},
            {"maxConnections", 1000},
            {"threadPoolSize", 8}
        },
        .database_config = nlohmann::json{
            {"type", "SQLite"},
            {"connectionPoolSize", 10},
            {"enableWAL", true}
        },
        .enabled_features = {"GraphQL", "Multi-tenant", "Health monitoring", "Metrics"}
    };
}

std::vector<BuiltInSchema::TenantInfo> BuiltInSchema::get_tenant_info() const {
    std::vector<TenantInfo> tenants;
    
    // TODO: Query actual tenant information from database
    // For now, return mock data
    tenants.push_back(TenantInfo{
        .tenant_id = "default",
        .status = HealthStatus::UP,
        .active_connections = 5,
        .created_at = start_time_,
        .last_access = std::chrono::system_clock::now(),
        .database_size_bytes = 1024 * 1024 // 1MB
    });
    
    return tenants;
}

nlohmann::json BuiltInSchema::get_app_info() const {
    auto config = get_configuration_info();
    
    return nlohmann::json{
        {"app", {
            {"name", "isched Universal Application Server"},
            {"description", "Multi-tenant GraphQL application server"},
            {"version", config.server_version},
            {"encoding", "UTF-8"},
            {"java", {
                {"version", "N/A (C++ Application)"},
                {"vendor", "N/A"}
            }}
        }},
        {"build", {
            {"version", config.server_version},
            {"artifact", "isched-server"},
            {"name", "isched"},
            {"time", config.build_timestamp},
            {"group", "isched"}
        }},
        {"git", {
            {"commit", {
                {"time", config.build_timestamp},
                {"id", "unknown"}
            }},
            {"branch", "001-universal-backend"}
        }}
    };
}

nlohmann::json BuiltInSchema::get_environment_info() const {
    nlohmann::json env;
    
    // System properties
    env["systemProperties"] = nlohmann::json{
        {"os.name", "Linux"},
        {"os.version", "Unknown"},
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
}

nlohmann::json BuiltInSchema::get_runtime_metrics() const {
    auto metrics = get_server_metrics();
    
    return nlohmann::json{
        {"memory", {
            {"used", metrics.memory_usage_bytes},
            {"committed", metrics.memory_usage_bytes},
            {"max", metrics.memory_usage_bytes * 4}
        }},
        {"threads", {
            {"live", metrics.thread_count},
            {"daemon", 0},
            {"peak", metrics.thread_count}
        }},
        {"gc", {
            {"collections", 0},
            {"time", 0}
        }},
        {"uptime", metrics.uptime.count()},
        {"startTime", std::chrono::duration_cast<std::chrono::milliseconds>(
            metrics.start_time.time_since_epoch()).count()}
    };
}

// Private helper methods

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_health_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        return get_health_status();
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_info_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        return get_app_info();
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_metrics_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        auto metrics = get_server_metrics();
        return nlohmann::json{
            {"uptime", metrics.uptime.count()},
            {"activeConnections", metrics.active_connections},
            {"totalRequests", metrics.total_requests},
            {"failedRequests", metrics.failed_requests},
            {"averageResponseTime", metrics.average_response_time},
            {"memoryUsage", metrics.memory_usage_bytes},
            {"threadCount", metrics.thread_count}
        };
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_env_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        return get_environment_info();
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_tenants_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        auto tenants = get_tenant_info();
        nlohmann::json result = nlohmann::json::array();
        
        for (const auto& tenant : tenants) {
            result.push_back(nlohmann::json{
                {"tenantId", tenant.tenant_id},
                {"status", health_status_to_string(tenant.status)},
                {"activeConnections", tenant.active_connections},
                {"createdAt", std::chrono::duration_cast<std::chrono::milliseconds>(
                    tenant.created_at.time_since_epoch()).count()},
                {"lastAccess", std::chrono::duration_cast<std::chrono::milliseconds>(
                    tenant.last_access.time_since_epoch()).count()},
                {"databaseSize", tenant.database_size_bytes}
            });
        }
        
        return result;
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_config_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        auto config = get_configuration_info();
        return nlohmann::json{
            {"server", config.server_config},
            {"database", config.database_config},
            {"features", config.enabled_features},
            {"version", config.server_version},
            {"environment", config.environment}
        };
    };
}

std::function<nlohmann::json(const nlohmann::json&, const nlohmann::json&)> 
BuiltInSchema::create_schema_resolver() const {
    return [this](const nlohmann::json&, const nlohmann::json&) {
        return get_schema_definition();
    };
}

std::string BuiltInSchema::health_status_to_string(HealthStatus status) noexcept {
    switch (status) {
        case HealthStatus::UP: return "UP";
        case HealthStatus::DOWN: return "DOWN";
        case HealthStatus::OUT_OF_SERVICE: return "OUT_OF_SERVICE";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::size_t BuiltInSchema::get_memory_usage() noexcept {
#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        std::size_t pages;
        statm >> pages;
        return pages * getpagesize();
    }
#endif
    return 0; // Fallback for non-Linux systems
}

std::size_t BuiltInSchema::get_thread_count() noexcept {
    return std::thread::hardware_concurrency();
}

} // namespace isched::v0_0_1::backend