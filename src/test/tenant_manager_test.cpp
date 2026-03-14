// SPDX-License-Identifier: MPL-2.0
/**
 * @file tenant_manager_test.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Basic smoke-test for the refactored in-process TenantManager
 * @author isched Development Team
 * @version 2.0.0
 * @date 2026-03-13
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "isched/backend/isched_TenantManager.hpp"

using namespace isched::v0_0_1::backend;

int main() {
    std::cout << "=== iSched Tenant Manager Test ===" << std::endl;
    
    try {
        TenantManager::Configuration config;
        config.database_root_path = "/tmp/isched_tenant_test_" + std::to_string(std::time(nullptr));
        config.max_tenants = 100;
        
        std::cout << "Creating TenantManager with configuration:" << std::endl;
        std::cout << "  database_root: " << config.database_root_path << std::endl;
        std::cout << "  max_tenants:   " << config.max_tenants << std::endl;
        
        auto manager = TenantManager::create(config);
        std::cout << "✓ TenantManager created" << std::endl;
        
        // Configuration getter
        const auto& stored = manager->get_configuration();
        if (stored.database_root_path == config.database_root_path) {
            std::cout << "✓ Configuration getter OK" << std::endl;
        } else {
            std::cout << "✗ Configuration getter failed" << std::endl;
            return 1;
        }
        
        // Initial status
        if (manager->get_status() == TenantManager::Status::STOPPED) {
            std::cout << "✓ Initial status STOPPED" << std::endl;
        } else {
            std::cout << "✗ Initial status check failed" << std::endl;
            return 1;
        }
        
        // Start
        if (manager->start()) {
            std::cout << "✓ TenantManager started" << std::endl;
        } else {
            std::cout << "✗ TenantManager start failed" << std::endl;
            return 1;
        }
        
        if (manager->get_status() == TenantManager::Status::RUNNING) {
            std::cout << "✓ Status RUNNING" << std::endl;
        } else {
            std::cout << "✗ Status RUNNING check failed" << std::endl;
            return 1;
        }
        
        // Create tenants
        TenantManager::TenantConfiguration tenant_cfg;
        tenant_cfg.organization_name = "Test Org";
        tenant_cfg.user_limit = 50;
        
        if (manager->create_tenant("tenant-01", tenant_cfg)) {
            std::cout << "✓ Tenant created" << std::endl;
        } else {
            std::cout << "✗ Tenant creation failed" << std::endl;
            return 1;
        }
        
        // Tenant list
        auto list = manager->get_tenant_list();
        if (list.size() == 1 && list[0] == "tenant-01") {
            std::cout << "✓ Tenant list OK" << std::endl;
        } else {
            std::cout << "✗ Tenant list failed" << std::endl;
            return 1;
        }
        
        // Session
        auto session = manager->get_tenant_session("tenant-01");
        if (session && session->tenant_id == "tenant-01" && session->database) {
            std::cout << "✓ Tenant session OK (has typed DatabaseManager)" << std::endl;
        } else {
            std::cout << "✗ Tenant session failed" << std::endl;
            return 1;
        }
        
        manager->release_tenant_session(session);
        std::cout << "✓ Session released" << std::endl;
        
        // Multiple tenants
        for (int i = 2; i <= 4; ++i) {
            std::string id = "tenant-0" + std::to_string(i);
            if (!manager->create_tenant(id, tenant_cfg)) {
                std::cout << "✗ Failed to create " << id << std::endl;
                return 1;
            }
        }
        list = manager->get_tenant_list();
        if (list.size() == 4) {
            std::cout << "✓ 4 tenants registered" << std::endl;
        } else {
            std::cout << "✗ Expected 4 tenants, got " << list.size() << std::endl;
            return 1;
        }
        
        // Metrics
        auto metrics = manager->get_metrics();
        if (metrics.find("active_tenants") != std::string::npos) {
            std::cout << "✓ Metrics: " << metrics.substr(0, 80) << "..." << std::endl;
        } else {
            std::cout << "✗ Metrics not available" << std::endl;
            return 1;
        }
        
        // Health
        auto health = manager->get_health();
        if (health.find("UP") != std::string::npos) {
            std::cout << "✓ Health: " << health.substr(0, 80) << "..." << std::endl;
        } else {
            std::cout << "✗ Health check failed" << std::endl;
            return 1;
        }
        
        // Remove
        if (!manager->remove_tenant("tenant-04")) {
            std::cout << "✗ Tenant removal failed" << std::endl;
            return 1;
        }
        list = manager->get_tenant_list();
        if (list.size() == 3) {
            std::cout << "✓ Tenant removed, 3 remain" << std::endl;
        } else {
            std::cout << "✗ Post-removal count wrong" << std::endl;
            return 1;
        }
        
        // Stop
        if (manager->stop()) {
            std::cout << "✓ TenantManager stopped" << std::endl;
        } else {
            std::cout << "✗ TenantManager stop failed" << std::endl;
            return 1;
        }
        
        if (manager->get_status() == TenantManager::Status::STOPPED) {
            std::cout << "✓ Final status STOPPED" << std::endl;
        } else {
            std::cout << "✗ Final status check failed" << std::endl;
            return 1;
        }
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}
