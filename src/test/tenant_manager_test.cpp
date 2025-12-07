/**
 * @file tenant_manager_test.cpp
 * @brief Basic test for the TenantManager implementation
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
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
        // Create tenant manager configuration
        TenantManager::PoolConfiguration config;
        config.min_processes = 2;
        config.max_processes = 5;
        config.database_root_path = "./test_tenant_data";
        config.max_tenants_per_process = 3;
        
        std::cout << "Creating TenantManager with configuration:" << std::endl;
        std::cout << "  Process pool: " << config.min_processes << "-" << config.max_processes << std::endl;
        std::cout << "  Database root: " << config.database_root_path << std::endl;
        std::cout << "  Max tenants per process: " << config.max_tenants_per_process << std::endl;
        
        // Create tenant manager instance
        auto manager = TenantManager::create(config);
        std::cout << "✓ TenantManager created successfully" << std::endl;
        
        // Test configuration
        const auto& manager_config = manager->get_pool_configuration();
        if (manager_config.min_processes == 2) {
            std::cout << "✓ Configuration validation passed" << std::endl;
        } else {
            std::cout << "✗ Configuration validation failed" << std::endl;
            return 1;
        }
        
        // Test status
        if (manager->get_status() == TenantManager::Status::STOPPED) {
            std::cout << "✓ Initial status is STOPPED" << std::endl;
        } else {
            std::cout << "✗ Initial status check failed" << std::endl;
            return 1;
        }
        
        // Test start
        std::cout << "Starting TenantManager..." << std::endl;
        if (manager->start()) {
            std::cout << "✓ TenantManager started successfully" << std::endl;
        } else {
            std::cout << "✗ TenantManager start failed" << std::endl;
            return 1;
        }
        
        // Test running status
        if (manager->get_status() == TenantManager::Status::RUNNING) {
            std::cout << "✓ TenantManager status is RUNNING" << std::endl;
        } else {
            std::cout << "✗ TenantManager status check failed" << std::endl;
            return 1;
        }
        
        // Test tenant creation
        TenantManager::TenantConfiguration tenant_config;
        tenant_config.organization_name = "Test Organization";
        tenant_config.user_limit = 50;
        tenant_config.storage_limit_mb = 512;
        
        std::cout << "Creating test tenant..." << std::endl;
        if (manager->create_tenant("test-tenant-1", tenant_config)) {
            std::cout << "✓ Tenant created successfully" << std::endl;
        } else {
            std::cout << "✗ Tenant creation failed" << std::endl;
            return 1;
        }
        
        // Test tenant list
        auto tenant_list = manager->get_tenant_list();
        if (tenant_list.size() == 1 && tenant_list[0] == "test-tenant-1") {
            std::cout << "✓ Tenant list validation passed" << std::endl;
        } else {
            std::cout << "✗ Tenant list validation failed" << std::endl;
            return 1;
        }
        
        // Test tenant session creation
        std::cout << "Creating tenant session..." << std::endl;
        auto session = manager->get_tenant_session("test-tenant-1");
        if (session && session->tenant_id == "test-tenant-1") {
            std::cout << "✓ Tenant session created successfully" << std::endl;
        } else {
            std::cout << "✗ Tenant session creation failed" << std::endl;
            return 1;
        }
        
        // Test session release
        manager->release_tenant_session(session);
        std::cout << "✓ Tenant session released successfully" << std::endl;
        
        // Test multiple tenant creation
        std::cout << "Creating additional tenants..." << std::endl;
        for (int i = 2; i <= 4; ++i) {
            std::string tenant_id = "test-tenant-" + std::to_string(i);
            if (manager->create_tenant(tenant_id, tenant_config)) {
                std::cout << "✓ Created tenant: " << tenant_id << std::endl;
            } else {
                std::cout << "✗ Failed to create tenant: " << tenant_id << std::endl;
                return 1;
            }
        }
        
        // Test tenant list with multiple tenants
        tenant_list = manager->get_tenant_list();
        if (tenant_list.size() == 4) {
            std::cout << "✓ Multiple tenant creation successful" << std::endl;
        } else {
            std::cout << "✗ Multiple tenant creation failed (expected 4, got " << tenant_list.size() << ")" << std::endl;
            return 1;
        }
        
        // Test metrics
        auto metrics = manager->get_metrics();
        if (!metrics.empty() && metrics.find("active_tenants") != std::string::npos) {
            std::cout << "✓ Metrics available: " << metrics.substr(0, 100) << "..." << std::endl;
        } else {
            std::cout << "✗ Metrics not available" << std::endl;
            return 1;
        }
        
        // Test health check
        auto health = manager->get_health();
        if (!health.empty() && health.find("UP") != std::string::npos) {
            std::cout << "✓ Health check available: " << health.substr(0, 100) << "..." << std::endl;
        } else {
            std::cout << "✗ Health check not available" << std::endl;
            return 1;
        }
        
        // Let manager run briefly to test maintenance thread
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Test tenant removal
        std::cout << "Removing test tenant..." << std::endl;
        if (manager->remove_tenant("test-tenant-4")) {
            std::cout << "✓ Tenant removed successfully" << std::endl;
        } else {
            std::cout << "✗ Tenant removal failed" << std::endl;
            return 1;
        }
        
        // Verify tenant list after removal
        tenant_list = manager->get_tenant_list();
        if (tenant_list.size() == 3) {
            std::cout << "✓ Tenant removal validation passed" << std::endl;
        } else {
            std::cout << "✗ Tenant removal validation failed" << std::endl;
            return 1;
        }
        
        // Test stop
        std::cout << "Stopping TenantManager..." << std::endl;
        if (manager->stop()) {
            std::cout << "✓ TenantManager stopped successfully" << std::endl;
        } else {
            std::cout << "✗ TenantManager stop failed" << std::endl;
            return 1;
        }
        
        // Test stopped status
        if (manager->get_status() == TenantManager::Status::STOPPED) {
            std::cout << "✓ Final status is STOPPED" << std::endl;
        } else {
            std::cout << "✗ Final status check failed" << std::endl;
            return 1;
        }
        
        std::cout << "\n=== All tenant manager tests passed! ===" << std::endl;
        std::cout << "Tenant management system is working correctly." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}