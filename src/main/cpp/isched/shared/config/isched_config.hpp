// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_config.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Configuration management system for Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * This module provides comprehensive configuration management for the Universal Application
 * Server Backend, supporting multi-tenant configurations, server settings, and runtime parameters.
 * Implements the C++ Core Guidelines and follows the Constitutional requirements for performance,
 * security, and maintainability.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <chrono>
#include <variant>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace isched::v0_0_1::backend {

    bool match_pattern(const std::string& pattern, const std::string& str);
/**
 * @brief Configuration value types supported by the configuration system
 */
using ConfigValue = std::variant<
    std::string,
    int,
    bool,
    double,
    std::vector<std::string>
>;

/**
 * @brief Server configuration settings
 */
struct ServerConfig {
    std::string host = "localhost";
    int port = 8080;
    int max_connections = 1000;
    std::chrono::seconds request_timeout{30};
    std::chrono::seconds session_timeout{3600};
    std::string log_level = "info";
    std::string log_file_path = "./logs/isched.log";
    bool enable_ssl = false;
    std::string ssl_cert_path;
    std::string ssl_key_path;
    
    // Performance settings (Constitutional requirement: 20ms response times)
    std::chrono::milliseconds max_response_time{20};
    int thread_pool_size = 8;
    size_t max_request_size = 1024UL * 1024UL; // 1MB
    size_t max_response_size = 10UL * 1024UL * 1024UL; // 10MB
    
    // Security settings (Constitutional requirement: Security-First)
    bool require_authentication = true;
    std::string jwt_secret_key;
    std::chrono::seconds jwt_expiration{3600};
    bool enable_cors = true;
    std::vector<std::string> allowed_origins{"*"};
    
    // GraphQL settings (Constitutional requirement: GraphQL Spec compliance)
    bool enable_introspection = true;
    bool enable_playground = false;
    std::string graphql_endpoint = "/graphql";
    int max_query_depth = 10;
    std::chrono::seconds query_timeout{5};
    bool enable_health_endpoint = true;
    int health_check_interval = 0;
};

/**
 * @brief Tenant-specific configuration settings
 */
struct TenantConfig {
    std::string tenant_id;
    std::string display_name;
    std::string database_path;
    std::string schema_version = "1.0.0";
    
    // Resource limits per tenant
    int max_concurrent_requests = 100;
    size_t max_storage_size = 100UL * 1024UL * 1024UL; // 100MB
    std::chrono::seconds session_timeout{1800};
    
    // Feature flags
    bool enable_custom_schema = false;
    bool enable_file_uploads = true;
    bool enable_webhooks = false;
    
    // Custom settings
    std::unordered_map<std::string, ConfigValue> custom_settings;
};

/**
 * @brief Versioned, persisted tenant configuration snapshot.
 *
 * A snapshot is the authoritative record of a tenant's active configuration.
 * Only one snapshot is active per tenant at a time.  Full persistence to
 * SQLite is implemented in Phase 4 (T029); this struct establishes the type.
 */
struct ConfigurationSnapshot {
    std::string id;                ///< UUID identifying the snapshot
    std::string tenant_id;         ///< Owning tenant
    std::string version;           ///< Monotonic version label
    std::string display_name;      ///< Human-readable label
    std::string schema_sdl;        ///< Generated SDL fragment for tenant types
    bool is_active = false;        ///< Whether this is the current active snapshot
    /// JSON array of resolver binding objects (T048-007); stored as serialised JSON string.
    /// Each object: { fieldName, resolverKind, dataSourceId, pathPattern?, httpMethod? }.
    std::string resolver_bindings{"[]"};

    std::chrono::system_clock::time_point created_at  = std::chrono::system_clock::now();
    std::optional<std::chrono::system_clock::time_point> activated_at;
};

/**
 * @brief Configuration change notification callback type
 */
using ConfigChangeCallback = std::function<void(const std::string& key, const ConfigValue& old_value, const ConfigValue& new_value)>;

/**
 * @brief Abstract base class for configuration providers
 * 
 * This class defines the interface for configuration sources such as JSON files,
 * environment variables, command-line arguments, and remote configuration services.
 */
class ConfigProvider {
public:
    virtual ~ConfigProvider() = default;
    
    /**
     * @brief Load configuration from the provider source
     * @return true if configuration was loaded successfully, false otherwise
     */
    virtual bool load() = 0;
    
    /**
     * @brief Get a configuration value by key
     * @param key Configuration key
     * @return Configuration value if found, std::nullopt otherwise
     */
    virtual std::optional<ConfigValue> get(const std::string& key) const = 0;
    
    /**
     * @brief Set a configuration value
     * @param key Configuration key
     * @param value Configuration value
     * @return true if value was set successfully, false otherwise
     */
    virtual bool set(const std::string& key, const ConfigValue& value) = 0;
    
    /**
     * @brief Check if a configuration key exists
     * @param key Configuration key
     * @return true if key exists, false otherwise
     */
    virtual bool has(const std::string& key) const = 0;
    
    /**
     * @brief Get all configuration keys
     * @return Vector of all configuration keys
     */
    virtual std::vector<std::string> get_keys() const = 0;
    
    /**
     * @brief Save configuration changes (if provider supports it)
     * @return true if saved successfully, false otherwise
     */
    virtual bool save() = 0;
    
    /**
     * @brief Get provider name for debugging
     * @return Provider name
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get provider priority for layered configuration
     * @return Priority value (higher values override lower values)
     */
    virtual int get_priority() const = 0;
    
    // Non-copyable
    ConfigProvider(const ConfigProvider&) = delete;
    ConfigProvider& operator=(const ConfigProvider&) = delete;
    
    // Moveable
    ConfigProvider(ConfigProvider&&) = default;
    ConfigProvider& operator=(ConfigProvider&&) = default;

protected:
    ConfigProvider() = default;
};

/**
 * @brief Configuration manager for the Universal Application Server Backend
 * 
 * This class provides centralized configuration management with support for multiple
 * configuration sources, tenant-specific settings, and runtime configuration changes.
 * Implements smart pointer usage and C++ Core Guidelines compliance.
 */
class ConfigManager {
public:
    /**
     * @brief Create a configuration manager
     * @return Unique pointer to ConfigManager instance
     */
    static std::unique_ptr<ConfigManager> create();
    
    /**
     * @brief Constructor
     */
    ConfigManager();
    
    /**
     * @brief Destructor
     */
    ~ConfigManager();
    
    // Non-copyable, moveable
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = default;
    ConfigManager& operator=(ConfigManager&&) = default;
    
    /**
     * @brief Add a configuration provider
     * @param provider Unique pointer to configuration provider
     */
    void add_provider(std::unique_ptr<ConfigProvider> provider);
    
    /**
     * @brief Load configuration from all providers
     * @return true if all providers loaded successfully, false otherwise
     */
    bool load_configuration();
    
    /**
     * @brief Get server configuration
     * @return Server configuration structure
     */
    ServerConfig get_server_config() const;
    
    /**
     * @brief Get tenant configuration
     * @param tenant_id Tenant identifier
     * @return Tenant configuration if found, std::nullopt otherwise
     */
    std::optional<TenantConfig> get_tenant_config(const std::string& tenant_id) const;
    
    /**
     * @brief Add or update tenant configuration
     * @param config Tenant configuration to add/update
     */
    void set_tenant_config(const TenantConfig& config);
    
    /**
     * @brief Remove tenant configuration
     * @param tenant_id Tenant identifier
     * @return true if tenant was removed, false if not found
     */
    bool remove_tenant_config(const std::string& tenant_id);
    
    /**
     * @brief Get all configured tenant IDs
     * @return Vector of tenant identifiers
     */
    std::vector<std::string> get_tenant_ids() const;
    
    /**
     * @brief Get a configuration value by key
     * @param key Configuration key (supports dot notation: "server.port")
     * @return Configuration value if found, std::nullopt otherwise
     */
    std::optional<ConfigValue> get(const std::string& key) const;
    
    /**
     * @brief Set a configuration value
     * @param key Configuration key (supports dot notation)
     * @param value Configuration value
     * @return true if value was set successfully, false otherwise
     */
    bool set(const std::string& key, const ConfigValue& value);
    
    /**
     * @brief Register callback for configuration changes
     * @param key Configuration key pattern (supports wildcards)
     * @param callback Callback function to invoke on changes
     * @return Registration ID for removing the callback
     */
    size_t register_change_callback(const std::string& key, ConfigChangeCallback callback);
    
    /**
     * @brief Unregister configuration change callback
     * @param registration_id Registration ID returned by register_change_callback
     */
    void unregister_change_callback(size_t registration_id);
    
    /**
     * @brief Validate current configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate_configuration() const;
    
    /**
     * @brief Reload configuration from all providers
     * @return true if reload was successful, false otherwise
     */
    bool reload_configuration();
    
    /**
     * @brief Save configuration changes to persistent providers
     * @return true if save was successful, false otherwise
     */
    bool save_configuration();
    
    /**
     * @brief Get configuration metrics and statistics
     * @return JSON string with configuration metrics
     */
    std::string get_metrics() const;

    // -------------------------------------------------------------------------
    // Configuration snapshot management (introduced T014)
    // -------------------------------------------------------------------------

    /**
     * @brief Store an active configuration snapshot for a tenant.
     *
     * Replaces any previously stored snapshot for the same tenant.
     */
    void set_active_snapshot(const ConfigurationSnapshot& snapshot);

    /**
     * @brief Retrieve the active configuration snapshot for a tenant.
     * @return The active snapshot, or std::nullopt if none is set.
     */
    [[nodiscard]] std::optional<ConfigurationSnapshot>
    get_active_snapshot(const std::string& tenant_id) const;

    /**
     * @brief Remove the active snapshot for a tenant.
     * @return true if a snapshot was removed, false if none existed.
     */
    bool remove_active_snapshot(const std::string& tenant_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    /**
     * @brief Notify change callbacks about configuration changes
     * @param key Configuration key that changed
     * @param new_value New configuration value
     */
    void notify_change_callbacks(const std::string& key, const ConfigValue& new_value);
};

/**
 * @brief JSON file configuration provider
 */
class JsonConfigProvider : public ConfigProvider {
public:
    /**
     * @brief Create JSON configuration provider
     * @param file_path Path to JSON configuration file
     * @param priority Provider priority
     * @return Unique pointer to JsonConfigProvider
     */
    static std::unique_ptr<JsonConfigProvider> create(
        const std::filesystem::path& file_path, 
        int priority = 100);
    
    /**
     * @brief Constructor
     * @param file_path Path to JSON configuration file
     * @param priority Provider priority
     */
    explicit JsonConfigProvider(const std::filesystem::path& file_path, int priority = 100);
    
    // ConfigProvider interface implementation
    bool load() override;
    std::optional<ConfigValue> get(const std::string& key) const override;
    bool set(const std::string& key, const ConfigValue& value) override;
    bool has(const std::string& key) const override;
    std::vector<std::string> get_keys() const override;
    bool save() override;
    std::string get_name() const override;
    int get_priority() const override;
    
private:
    std::filesystem::path file_path_;
    int priority_;
    mutable std::mutex data_mutex_;
    std::unordered_map<std::string, ConfigValue> data_;
    std::atomic<bool> loaded_{false};
    std::atomic<bool> modified_{false};
};

/**
 * @brief Environment variable configuration provider
 */
class EnvironmentConfigProvider : public ConfigProvider {
public:
    /**
     * @brief Create environment variable configuration provider
     * @param prefix Environment variable prefix (e.g., "ISCHED_")
     * @param priority Provider priority
     * @return Unique pointer to EnvironmentConfigProvider
     */
    static std::unique_ptr<EnvironmentConfigProvider> create(
        const std::string& prefix = "ISCHED_", 
        int priority = 200);
    
    /**
     * @brief Constructor
     * @param prefix Environment variable prefix
     * @param priority Provider priority
     */
    explicit EnvironmentConfigProvider(const std::string& prefix = "ISCHED_", int priority = 200);
    
    // ConfigProvider interface implementation
    bool load() override;
    std::optional<ConfigValue> get(const std::string& key) const override;
    bool set(const std::string& key, const ConfigValue& value) override;
    bool has(const std::string& key) const override;
    std::vector<std::string> get_keys() const override;
    bool save() override;
    std::string get_name() const override;
    int get_priority() const override;
    
private:
    std::string prefix_;
    int priority_;
    mutable std::mutex data_mutex_;
    std::unordered_map<std::string, ConfigValue> data_;
    std::atomic<bool> loaded_{false};
    
    std::string env_key_to_config_key(const std::string& env_key) const;
    std::string config_key_to_env_key(const std::string& config_key) const;
};

/**
 * @brief Configuration utility functions
 */
namespace config_utils {

/**
 * @brief Parse configuration value from string
 * @param value String value to parse
 * @param type_hint Type hint for parsing
 * @return Parsed configuration value
 */
ConfigValue parse_value(const std::string& value, const std::string& type_hint = "string");

/**
 * @brief Convert configuration value to string
 * @param value Configuration value to convert
 * @return String representation of the value
 */
std::string to_string(const ConfigValue& value);

/**
 * @brief Validate configuration key format
 * @param key Configuration key to validate
 * @return true if key format is valid, false otherwise
 */
bool is_valid_key(const std::string& key);

/**
 * @brief Create default server configuration
 * @return Default server configuration
 */
ServerConfig create_default_server_config();

/**
 * @brief Create default tenant configuration
 * @param tenant_id Tenant identifier
 * @return Default tenant configuration
 */
TenantConfig create_default_tenant_config(const std::string& tenant_id);

/**
 * @brief Merge two configuration structures
 * @param base Base configuration
 * @param override Override configuration
 * @return Merged configuration
 */
template<typename T>
T merge_config(const T& base, const T& override_config);

} // namespace config_utils

    std::string getDataHome();
} // namespace isched::v0_0_1::backend