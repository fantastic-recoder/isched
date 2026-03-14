// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_config_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Test suite for Configuration Management System
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * Tests for the Universal Application Server Backend configuration system,
 * following TDD approach as required by Constitutional compliance.
 */

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "isched/shared/config/isched_config.hpp"

using namespace isched::v0_0_1::backend;

/**
 * @brief Test fixture for Configuration tests
 */
class ConfigTestFixture {
public:
    ConfigTestFixture() 
        : config_manager(ConfigManager::create()),
          test_config_file((std::filesystem::temp_directory_path() /
                            ("test_config_" + std::to_string(getpid()) + ".json")).string()) {
        config_manager->add_provider(static_cast<std::unique_ptr<ConfigProvider>>(new JsonConfigProvider(test_config_file)));
    }

    // Delete copy operations to make it non-copyable
    ConfigTestFixture(const ConfigTestFixture&) = delete;
    ConfigTestFixture& operator=(const ConfigTestFixture&) = delete;

    virtual ~ConfigTestFixture() {
        std::error_code ec;
        if (std::filesystem::exists(test_config_file, ec)) {
            std::filesystem::remove(test_config_file, ec);
        }
    }

protected:
    std::unique_ptr<ConfigManager> config_manager;
    std::string test_config_file;
};

TEST_CASE_METHOD(ConfigTestFixture, "ConfigManager creation and basic operations", "[config][manager]") {
    REQUIRE(config_manager != nullptr);
    
    SECTION("Default server configuration") {
        auto server_config = config_manager->get_server_config();
        REQUIRE(server_config.host == "localhost");
        REQUIRE(server_config.port == 8080);
        REQUIRE(server_config.max_connections == 1000);
        REQUIRE(server_config.log_level == "info");
    }
}

TEST_CASE_METHOD(ConfigTestFixture, "Configuration providers", "[config][providers]") {
    SECTION("JSON configuration provider") {
        // Create test configuration file
        std::ofstream file(test_config_file);
        file << "server.host=testhost\n";
        file << "server.port=9090\n";
        file << "server.log_level=debug\n";
        file.close();
        
        auto json_provider = JsonConfigProvider::create(test_config_file, 100);
        REQUIRE(json_provider != nullptr);
        
        bool loaded = json_provider->load();
        REQUIRE(loaded);
        
        auto host = json_provider->get("server.host");
        REQUIRE(host.has_value());
        REQUIRE(std::get<std::string>(*host) == "testhost");
        
        auto port = json_provider->get("server.port");
        REQUIRE(port.has_value());
        REQUIRE(std::get<int>(*port) == 9090);
    }
    
    SECTION("Environment configuration provider") {
        auto env_provider = EnvironmentConfigProvider::create("TEST_", 200);
        REQUIRE(env_provider != nullptr);
        
        // Set environment variable for testing
        setenv("TEST_SERVER_HOST", "envhost", 1);
        setenv("TEST_SERVER_PORT", "8888", 1);
        
        bool loaded = env_provider->load();
        REQUIRE(loaded);
        
        auto host = env_provider->get("server.host");
        if (host.has_value()) {
            REQUIRE(std::get<std::string>(*host) == "envhost");
        }
        
        // Cleanup
        unsetenv("TEST_SERVER_HOST");
        unsetenv("TEST_SERVER_PORT");
    }
}

TEST_CASE_METHOD(ConfigTestFixture, "Tenant configuration management", "[config][tenants]") {
    SECTION("Add and retrieve tenant configuration") {
        TenantConfig tenant_config = config_utils::create_default_tenant_config("test_tenant");
        tenant_config.display_name = "Test Tenant";
        tenant_config.max_concurrent_requests = 50;
        
        config_manager->set_tenant_config(tenant_config);
        
        auto retrieved = config_manager->get_tenant_config("test_tenant");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->tenant_id == "test_tenant");
        REQUIRE(retrieved->display_name == "Test Tenant");
        REQUIRE(retrieved->max_concurrent_requests == 50);
    }
    
    SECTION("Remove tenant configuration") {
        TenantConfig tenant_config = config_utils::create_default_tenant_config("temp_tenant");
        config_manager->set_tenant_config(tenant_config);
        
        auto retrieved = config_manager->get_tenant_config("temp_tenant");
        REQUIRE(retrieved.has_value());
        
        bool removed = config_manager->remove_tenant_config("temp_tenant");
        REQUIRE(removed);
        
        auto after_removal = config_manager->get_tenant_config("temp_tenant");
        REQUIRE_FALSE(after_removal.has_value());
    }
    
    SECTION("List tenant IDs") {
        config_manager->set_tenant_config(config_utils::create_default_tenant_config("tenant1"));
        config_manager->set_tenant_config(config_utils::create_default_tenant_config("tenant2"));
        
        auto tenant_ids = config_manager->get_tenant_ids();
        REQUIRE(tenant_ids.size() >= 2);
        
        bool found_tenant1 = std::find(tenant_ids.begin(), tenant_ids.end(), "tenant1") != tenant_ids.end();
        bool found_tenant2 = std::find(tenant_ids.begin(), tenant_ids.end(), "tenant2") != tenant_ids.end();
        
        REQUIRE(found_tenant1);
        REQUIRE(found_tenant2);
    }
}

TEST_CASE("Configuration utility functions", "[config][utils]") {
    SECTION("Value parsing") {
        auto string_val = config_utils::parse_value("hello", "string");
        REQUIRE(std::holds_alternative<std::string>(string_val));
        REQUIRE(std::get<std::string>(string_val) == "hello");
        
        auto int_val = config_utils::parse_value("42", "int");
        REQUIRE(std::holds_alternative<int>(int_val));
        REQUIRE(std::get<int>(int_val) == 42);
        
        auto bool_val = config_utils::parse_value("true", "bool");
        REQUIRE(std::holds_alternative<bool>(bool_val));
        REQUIRE(std::get<bool>(bool_val) == true);
        
        auto double_val = config_utils::parse_value("3.14", "double");
        REQUIRE(std::holds_alternative<double>(double_val));
        REQUIRE(std::get<double>(double_val) == 3.14);
    }
    
    SECTION("Value to string conversion") {
        REQUIRE(config_utils::to_string(std::string("test")) == "test");
        REQUIRE(config_utils::to_string(42) == "42");
        REQUIRE(config_utils::to_string(true) == "true");
        REQUIRE(config_utils::to_string(false) == "false");
        REQUIRE(config_utils::to_string(3.14) == "3.140000");
    }
    
    SECTION("Key validation") {
        REQUIRE(config_utils::is_valid_key("server.host"));
        REQUIRE(config_utils::is_valid_key("server_port"));
        REQUIRE(config_utils::is_valid_key("log_level"));
        REQUIRE_FALSE(config_utils::is_valid_key(""));
        REQUIRE_FALSE(config_utils::is_valid_key("invalid-key"));
        REQUIRE_FALSE(config_utils::is_valid_key("key with spaces"));
    }
}

TEST_CASE_METHOD(ConfigTestFixture, "Configuration validation", "[config][validation]") {
    SECTION("Valid configuration") {
        auto errors = config_manager->validate_configuration();
        // Default configuration should be valid
        REQUIRE(errors.empty());
    }
    
    SECTION("Invalid configuration detection") {
        // Set invalid values
        config_manager->set("server.port", -1);
        config_manager->set("server.max_connections", 0);
        
        auto errors = config_manager->validate_configuration();
        REQUIRE_FALSE(errors.empty());
        
        // Check for specific error messages
        bool found_port_error = false;
        bool found_connections_error = false;
        
        for (const auto& error : errors) {
            if (error.find("Invalid server port") != std::string::npos) {
                found_port_error = true;
            }
            if (error.find("Invalid max_connections") != std::string::npos) {
                found_connections_error = true;
            }
        }
        
        REQUIRE(found_port_error);
        REQUIRE(found_connections_error);
    }
}

TEST_CASE_METHOD(ConfigTestFixture, "Configuration change callbacks", "[config][callbacks]") {
    SECTION("Register and trigger callback") {
        bool callback_triggered = false;
        std::string callback_key;
        ConfigValue callback_new_value;
        
        auto registration_id = config_manager->register_change_callback(
            "server.*",
            [&](const std::string& key, const ConfigValue& old_val, const ConfigValue& new_val) {
                callback_triggered = true;
                callback_key = key;
                callback_new_value = new_val;
            }
        );
        
        // Trigger callback by setting a value
        config_manager->set("server.test_setting", std::string("test_value"));
        
        REQUIRE(callback_triggered);
        REQUIRE(callback_key == "server.test_setting");
        REQUIRE(std::get<std::string>(callback_new_value) == "test_value");
        
        // Unregister callback
        config_manager->unregister_change_callback(registration_id);
    }
}

// Performance test (Constitutional requirement: 20ms response times)
TEST_CASE_METHOD(ConfigTestFixture, "Configuration performance", "[config][performance]") {
    const int num_operations = 1000;
    
    SECTION("Configuration retrieval performance") {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_operations; ++i) {
            auto server_config = config_manager->get_server_config();
            REQUIRE_FALSE(server_config.host.empty());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should retrieve configuration well under 20ms each (total under 2s is reasonable)
        REQUIRE(duration.count() < 2000);
    }
    
    SECTION("Configuration setting performance") {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_operations; ++i) {
            config_manager->set("test.setting" + std::to_string(i), std::string("value" + std::to_string(i)));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should set configuration values well under 20ms each (total under 2s is reasonable)
        REQUIRE(duration.count() < 2000);
    }
}

// ---------------------------------------------------------------------------
// T014: Configuration snapshot management tests
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(ConfigTestFixture, "Configuration snapshot management", "[config][snapshot]") {
    SECTION("Store and retrieve an active snapshot") {
        ConfigurationSnapshot snap;
        snap.id         = "snap-001";
        snap.tenant_id  = "tenant-a";
        snap.version    = "1";
        snap.display_name = "Initial configuration";
        snap.schema_sdl = "type Query { hello: String }";
        snap.is_active  = true;

        config_manager->set_active_snapshot(snap);

        auto retrieved = config_manager->get_active_snapshot("tenant-a");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->id == "snap-001");
        REQUIRE(retrieved->tenant_id == "tenant-a");
        REQUIRE(retrieved->version == "1");
        REQUIRE(retrieved->is_active == true);
        REQUIRE(retrieved->schema_sdl == "type Query { hello: String }");
    }

    SECTION("Return nullopt for unknown tenant") {
        REQUIRE_FALSE(config_manager->get_active_snapshot("no-such-tenant").has_value());
    }

    SECTION("Overwrite snapshot for same tenant") {
        ConfigurationSnapshot v1;
        v1.id = "snap-v1"; v1.tenant_id = "t"; v1.version = "1"; v1.is_active = true;
        config_manager->set_active_snapshot(v1);

        ConfigurationSnapshot v2;
        v2.id = "snap-v2"; v2.tenant_id = "t"; v2.version = "2"; v2.is_active = true;
        config_manager->set_active_snapshot(v2);

        auto result = config_manager->get_active_snapshot("t");
        REQUIRE(result.has_value());
        REQUIRE(result->id == "snap-v2");
        REQUIRE(result->version == "2");
    }

    SECTION("Remove active snapshot") {
        ConfigurationSnapshot snap;
        snap.id = "s"; snap.tenant_id = "tx"; snap.version = "1";
        config_manager->set_active_snapshot(snap);
        REQUIRE(config_manager->get_active_snapshot("tx").has_value());

        REQUIRE(config_manager->remove_active_snapshot("tx") == true);
        REQUIRE_FALSE(config_manager->get_active_snapshot("tx").has_value());

        // Second removal returns false
        REQUIRE(config_manager->remove_active_snapshot("tx") == false);
    }

    SECTION("Multiple tenants maintain independent snapshots") {
        ConfigurationSnapshot s1; s1.id = "1"; s1.tenant_id = "ta"; s1.version = "A";
        ConfigurationSnapshot s2; s2.id = "2"; s2.tenant_id = "tb"; s2.version = "B";
        config_manager->set_active_snapshot(s1);
        config_manager->set_active_snapshot(s2);

        REQUIRE(config_manager->get_active_snapshot("ta")->id == "1");
        REQUIRE(config_manager->get_active_snapshot("tb")->id == "2");
    }
}