// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_config.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of configuration management system for Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 */

#include "isched_config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <sago/platform_folders.h>

extern char **environ;

namespace isched::v0_0_1::backend {

// ConfigManager implementation details
struct ConfigManager::Impl {
    std::vector<std::unique_ptr<ConfigProvider>> providers_;
    mutable std::mutex providers_mutex_;
    
    std::unordered_map<std::string, TenantConfig> tenant_configs_;
    mutable std::mutex tenant_configs_mutex_;

    // Active configuration snapshots per tenant (tenant_id → snapshot)
    std::unordered_map<std::string, ConfigurationSnapshot> active_snapshots_;
    mutable std::mutex snapshots_mutex_;
    
    std::unordered_map<size_t, std::pair<std::string, ConfigChangeCallback>> change_callbacks_;
    std::atomic<size_t> next_callback_id_{1};
    mutable std::mutex callbacks_mutex_;
    
    std::atomic<size_t> loads_count_{0};
    std::atomic<size_t> saves_count_{0};
    std::atomic<size_t> get_calls_{0};
    std::atomic<size_t> set_calls_{0};
};

// ConfigManager implementation
ConfigManager::ConfigManager() : impl_(std::make_unique<Impl>()) {}

ConfigManager::~ConfigManager() = default;

bool match_pattern(const std::string &pattern, const std::string &str) {
    if (pattern.empty()) {
        return true;
    }

    if (pattern == "*") {
        return true;
    }

    if (pattern.back() == '*') {
        return str.find(pattern.substr(0, pattern.size() - 1)) == 0;
    }
    return false;
}

std::unique_ptr<ConfigManager> ConfigManager::create() {
    return std::make_unique<ConfigManager>();
}

void ConfigManager::add_provider(std::unique_ptr<ConfigProvider> provider) {
    if (!provider) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(impl_->providers_mutex_);
    
    // Insert provider in priority order (higher priority first)
    auto it = std::find_if(impl_->providers_.begin(), impl_->providers_.end(),
        [&provider](const auto& existing) {
            return existing->get_priority() < provider->get_priority();
        });
    
    impl_->providers_.insert(it, std::move(provider));
}

bool ConfigManager::load_configuration() {
    std::lock_guard<std::mutex> lock(impl_->providers_mutex_);
    
    bool all_loaded = true;
    for (auto& provider : impl_->providers_) {
        if (!provider->load()) {
            std::cerr << "Failed to load configuration from provider: " 
                      << provider->get_name() << std::endl;
            all_loaded = false;
        }
    }
    
    if (all_loaded) {
        ++impl_->loads_count_;
    }
    
    return all_loaded;
}

ServerConfig ConfigManager::get_server_config() const {
    auto config = config_utils::create_default_server_config();
    
    // Apply configuration from providers in priority order
    if (auto host = get("server.host")) {
        if (std::holds_alternative<std::string>(*host)) {
            config.host = std::get<std::string>(*host);
        }
    }
    
    if (auto port = get("server.port")) {
        if (std::holds_alternative<int>(*port)) {
            config.port = std::get<int>(*port);
        }
    }
    
    if (auto max_connections = get("server.max_connections")) {
        if (std::holds_alternative<int>(*max_connections)) {
            config.max_connections = std::get<int>(*max_connections);
        }
    }
    
    if (auto log_level = get("server.log_level")) {
        if (std::holds_alternative<std::string>(*log_level)) {
            config.log_level = std::get<std::string>(*log_level);
        }
    }
    
    if (auto enable_ssl = get("server.enable_ssl")) {
        if (std::holds_alternative<bool>(*enable_ssl)) {
            config.enable_ssl = std::get<bool>(*enable_ssl);
        }
    }
    
    return config;
}

std::optional<TenantConfig> ConfigManager::get_tenant_config(const std::string& tenant_id) const {
    std::lock_guard<std::mutex> lock(impl_->tenant_configs_mutex_);
    
    auto it = impl_->tenant_configs_.find(tenant_id);
    if (it != impl_->tenant_configs_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void ConfigManager::set_tenant_config(const TenantConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->tenant_configs_mutex_);
    impl_->tenant_configs_[config.tenant_id] = config;
}

bool ConfigManager::remove_tenant_config(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lock(impl_->tenant_configs_mutex_);
    return impl_->tenant_configs_.erase(tenant_id) > 0;
}

std::vector<std::string> ConfigManager::get_tenant_ids() const {
    std::lock_guard<std::mutex> lock(impl_->tenant_configs_mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(impl_->tenant_configs_.size());
    
    for (const auto& [tenant_id, config] : impl_->tenant_configs_) {
        ids.push_back(tenant_id);
    }
    
    return ids;
}

std::optional<ConfigValue> ConfigManager::get(const std::string& key) const {
    ++impl_->get_calls_;
    
    std::lock_guard<std::mutex> lock(impl_->providers_mutex_);
    
    // Check providers in priority order (highest first)
    for (const auto& provider : impl_->providers_) {
        if (auto value = provider->get(key)) {
            return value;
        }
    }
    
    return std::nullopt;
}

bool ConfigManager::set(const std::string& key, const ConfigValue& value) {
    ++impl_->set_calls_;
    
    std::lock_guard<std::mutex> lock(impl_->providers_mutex_);
    
    // Set in the first provider that accepts the value
    for (auto& provider : impl_->providers_) {
        if (provider->set(key, value)) {
            // Notify change callbacks
            notify_change_callbacks(key, value);
            return true;
        }
    }
    
    return false;
}


void ConfigManager::notify_change_callbacks(const std::string& key, const ConfigValue& new_value) {
    std::lock_guard<std::mutex> lock(impl_->callbacks_mutex_);
    
    ConfigValue old_value; // Default constructed
    
    for (const auto& [id, callback_pair] : impl_->change_callbacks_) {
        const auto& [pattern, callback] = callback_pair;
        
        // Simple pattern matching (exact match or wildcard)
        if (match_pattern(pattern, key)) {
            try {
                callback(key, old_value, new_value);
            } catch (const std::exception& e) {
                std::cerr << "Configuration change callback error: " << e.what() << std::endl;
            }
        }
    }
}

size_t ConfigManager::register_change_callback(const std::string& key, ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->callbacks_mutex_);
    
    auto id = impl_->next_callback_id_.fetch_add(1);
    impl_->change_callbacks_[id] = std::make_pair(key, std::move(callback));
    
    return id;
}

void ConfigManager::unregister_change_callback(size_t registration_id) {
    std::lock_guard<std::mutex> lock(impl_->callbacks_mutex_);
    impl_->change_callbacks_.erase(registration_id);
}

std::vector<std::string> ConfigManager::validate_configuration() const {
    std::vector<std::string> errors;
    
    auto server_config = get_server_config();
    
    // Validate server configuration
    if (server_config.port <= 0 || server_config.port > 65535) {
        errors.push_back("Invalid server port: " + std::to_string(server_config.port));
    }
    
    if (server_config.max_connections <= 0) {
        errors.push_back("Invalid max_connections: " + std::to_string(server_config.max_connections));
    }
    
    if (server_config.max_response_time.count() <= 0) {
        errors.push_back("Invalid max_response_time: " + std::to_string(server_config.max_response_time.count()));
    }
    
    if (server_config.enable_ssl) {
        if (server_config.ssl_cert_path.empty()) {
            errors.push_back("SSL enabled but ssl_cert_path is empty");
        }
        if (server_config.ssl_key_path.empty()) {
            errors.push_back("SSL enabled but ssl_key_path is empty");
        }
    }
    
    // Validate tenant configurations
    std::lock_guard<std::mutex> lock(impl_->tenant_configs_mutex_);
    for (const auto& [tenant_id, config] : impl_->tenant_configs_) {
        if (config.tenant_id.empty()) {
            errors.push_back("Tenant configuration has empty tenant_id");
        }
        if (config.database_path.empty()) {
            errors.push_back("Tenant " + tenant_id + " has empty database_path");
        }
    }
    
    return errors;
}

bool ConfigManager::reload_configuration() {
    return load_configuration();
}

bool ConfigManager::save_configuration() {
    std::lock_guard<std::mutex> lock(impl_->providers_mutex_);
    
    bool all_saved = true;
    for (auto& provider : impl_->providers_) {
        if (!provider->save()) {
            std::cerr << "Failed to save configuration to provider: " 
                      << provider->get_name() << std::endl;
            all_saved = false;
        }
    }
    
    if (all_saved) {
        ++impl_->saves_count_;
    }
    
    return all_saved;
}

std::string ConfigManager::get_metrics() const {
    std::ostringstream oss;
    oss << "{"
        << "\"loads_count\":" << impl_->loads_count_.load() << ","
        << "\"saves_count\":" << impl_->saves_count_.load() << ","
        << "\"get_calls\":" << impl_->get_calls_.load() << ","
        << "\"set_calls\":" << impl_->set_calls_.load() << ","
        << "\"providers_count\":" << impl_->providers_.size() << ","
        << "\"tenant_configs_count\":" << impl_->tenant_configs_.size() << ","
        << "\"change_callbacks_count\":" << impl_->change_callbacks_.size()
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Configuration snapshot management (T014)
// ---------------------------------------------------------------------------

void ConfigManager::set_active_snapshot(const ConfigurationSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(impl_->snapshots_mutex_);
    impl_->active_snapshots_[snapshot.tenant_id] = snapshot;
}

std::optional<ConfigurationSnapshot>
ConfigManager::get_active_snapshot(const std::string& tenant_id) const {
    std::lock_guard<std::mutex> lock(impl_->snapshots_mutex_);
    auto it = impl_->active_snapshots_.find(tenant_id);
    if (it != impl_->active_snapshots_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ConfigManager::remove_active_snapshot(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lock(impl_->snapshots_mutex_);
    return impl_->active_snapshots_.erase(tenant_id) > 0;
}

// JsonConfigProvider implementation
JsonConfigProvider::JsonConfigProvider(const std::filesystem::path& file_path, int priority)
    : file_path_(file_path), priority_(priority) {}

std::unique_ptr<JsonConfigProvider> JsonConfigProvider::create(
    const std::filesystem::path& file_path, int priority) {
    return std::make_unique<JsonConfigProvider>(file_path, priority);
}

bool JsonConfigProvider::load() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    try {
        if (!std::filesystem::exists(file_path_)) {
            // Create empty file if it doesn't exist
            std::ofstream file(file_path_);
            file << "{}";
            file.close();
        }
        
        std::ifstream file(file_path_);
        if (!file.is_open()) {
            return false;
        }
        
        // TODO: Implement proper JSON parsing
        // For now, implement simple key=value parsing
        std::string line;
        data_.clear();
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                auto key = line.substr(0, pos);
                auto value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(key.find_last_not_of(" \t") + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                
                data_[key] = config_utils::parse_value(value);
            }
        }
        
        loaded_ = true;
        modified_ = false;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration from " << file_path_ 
                  << ": " << e.what() << std::endl;
        return false;
    }
}

std::optional<ConfigValue> JsonConfigProvider::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool JsonConfigProvider::set(const std::string& key, const ConfigValue& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    data_[key] = value;
    modified_ = true;
    return true;
}

bool JsonConfigProvider::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return data_.find(key) != data_.end();
}

std::vector<std::string> JsonConfigProvider::get_keys() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    
    for (const auto& [key, value] : data_) {
        keys.push_back(key);
    }
    
    return keys;
}

bool JsonConfigProvider::save() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!modified_) {
        return true; // Nothing to save
    }
    
    try {
        std::ofstream file(file_path_);
        if (!file.is_open()) {
            return false;
        }
        
        // TODO: Implement proper JSON serialization
        // For now, write simple key=value format
        for (const auto& [key, value] : data_) {
            file << key << "=" << config_utils::to_string(value) << "\n";
        }
        
        modified_ = false;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving configuration to " << file_path_ 
                  << ": " << e.what() << std::endl;
        return false;
    }
}

std::string JsonConfigProvider::get_name() const {
    return "JsonConfigProvider(" + file_path_.string() + ")";
}

int JsonConfigProvider::get_priority() const {
    return priority_;
}

// EnvironmentConfigProvider implementation
EnvironmentConfigProvider::EnvironmentConfigProvider(const std::string& prefix, int priority)
    : prefix_(prefix), priority_(priority) {}

std::unique_ptr<EnvironmentConfigProvider> EnvironmentConfigProvider::create(
    const std::string& prefix, int priority) {
    return std::make_unique<EnvironmentConfigProvider>(prefix, priority);
}

bool EnvironmentConfigProvider::load() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Load all environment variables with the specified prefix
    data_.clear();
    
    char** env = environ;
    while (*env != nullptr) {
        std::string env_var(*env);
        auto pos = env_var.find('=');
        
        if (pos != std::string::npos) {
            auto env_key = env_var.substr(0, pos);
            auto env_value = env_var.substr(pos + 1);
            
            if (env_key.find(prefix_) == 0) {
                auto config_key = env_key_to_config_key(env_key);
                data_[config_key] = config_utils::parse_value(env_value);
            }
        }
        
        ++env;
    }
    
    loaded_ = true;
    return true;
}

std::optional<ConfigValue> EnvironmentConfigProvider::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool EnvironmentConfigProvider::set(const std::string& key, const ConfigValue& value) {
    // Environment variables are read-only in this implementation
    return false;
}

bool EnvironmentConfigProvider::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return data_.find(key) != data_.end();
}

std::vector<std::string> EnvironmentConfigProvider::get_keys() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    
    for (const auto& [key, value] : data_) {
        keys.push_back(key);
    }
    
    return keys;
}

bool EnvironmentConfigProvider::save() {
    // Environment variables are read-only in this implementation
    return true;
}

std::string EnvironmentConfigProvider::get_name() const {
    return "EnvironmentConfigProvider(" + prefix_ + ")";
}

int EnvironmentConfigProvider::get_priority() const {
    return priority_;
}

std::string EnvironmentConfigProvider::env_key_to_config_key(const std::string& env_key) const {
    auto key = env_key.substr(prefix_.length());
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    std::replace(key.begin(), key.end(), '_', '.');
    return key;
}

std::string EnvironmentConfigProvider::config_key_to_env_key(const std::string& config_key) const {
    auto key = config_key;
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);
    std::replace(key.begin(), key.end(), '.', '_');
    return prefix_ + key;
}

std::string getDataHome() {
    return sago::getDataHome();
}

// Configuration utility functions
namespace config_utils {

ConfigValue parse_value(const std::string& value, const std::string& type_hint) {
    if (type_hint == "bool" || value == "true" || value == "false") {
        return value == "true";
    }
    
    if (type_hint == "int") {
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            return value;
        }
    }
    
    if (type_hint == "double") {
        try {
            return std::stod(value);
        } catch (const std::exception&) {
            return value;
        }
    }
    
    // Try to auto-detect numeric values
    try {
        if (value.find('.') != std::string::npos) {
            return std::stod(value);
        } else {
            return std::stoi(value);
        }
    } catch (const std::exception&) {
        // Not a number, return as string
        return value;
    }
}

std::string to_string(const ConfigValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::ostringstream oss;
            for (size_t i = 0; i < v.size(); ++i) {
                if (i > 0) oss << ",";
                oss << v[i];
            }
            return oss.str();
        } else {
            return "";
        }
    }, value);
}

bool is_valid_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    
    // Simple validation: alphanumeric, dots, and underscores only
    return std::all_of(key.begin(), key.end(), [](char c) {
        return std::isalnum(c) || c == '.' || c == '_';
    });
}

ServerConfig create_default_server_config() {
    return ServerConfig{};
}

TenantConfig create_default_tenant_config(const std::string& tenant_id) {
    TenantConfig config{};
    config.tenant_id = tenant_id;
    config.display_name = tenant_id;
    config.database_path = "./data/" + tenant_id + ".db";
    return config;
}

} // namespace config_utils

} // namespace isched::v0_0_1::backend