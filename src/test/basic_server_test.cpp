/**
 * @file basic_server_test.cpp
 * @brief Basic test for the Server foundation implementation
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "../main/cpp/isched/backend/isched_server.hpp"

using namespace isched::v0_0_1::backend;

int main() {
    std::cout << "=== iSched Server Foundation Test ===" << std::endl;
    
    try {
        // Create server configuration
        Server::Configuration config;
        config.port = 8080;
        config.host = "localhost";
        config.min_threads = 4;
        config.max_threads = 10;
        
        std::cout << "Creating server with configuration:" << std::endl;
        std::cout << "  Port: " << config.port << std::endl;
        std::cout << "  Host: " << config.host << std::endl;
        std::cout << "  Thread pool: " << config.min_threads << "-" << config.max_threads << std::endl;
        
        // Create server instance
        auto server = Server::create(config);
        std::cout << "✓ Server created successfully" << std::endl;
        
        // Test configuration
        const auto& server_config = server->get_configuration();
        if (server_config.port == 8080) {
            std::cout << "✓ Configuration validation passed" << std::endl;
        } else {
            std::cout << "✗ Configuration validation failed" << std::endl;
            return 1;
        }
        
        // Test status
        if (server->get_status() == Server::Status::STOPPED) {
            std::cout << "✓ Initial status is STOPPED" << std::endl;
        } else {
            std::cout << "✗ Initial status check failed" << std::endl;
            return 1;
        }
        
        // Test start
        std::cout << "Starting server..." << std::endl;
        if (server->start()) {
            std::cout << "✓ Server started successfully" << std::endl;
        } else {
            std::cout << "✗ Server start failed" << std::endl;
            return 1;
        }
        
        // Test running status
        if (server->get_status() == Server::Status::RUNNING) {
            std::cout << "✓ Server status is RUNNING" << std::endl;
        } else {
            std::cout << "✗ Server status check failed" << std::endl;
            return 1;
        }
        
        // Test metrics
        auto metrics = server->get_metrics();
        if (!metrics.empty()) {
            std::cout << "✓ Metrics available: " << metrics.substr(0, 100) << "..." << std::endl;
        } else {
            std::cout << "✗ Metrics not available" << std::endl;
            return 1;
        }
        
        // Test health check
        auto health = server->get_health();
        if (!health.empty()) {
            std::cout << "✓ Health check available: " << health.substr(0, 100) << "..." << std::endl;
        } else {
            std::cout << "✗ Health check not available" << std::endl;
            return 1;
        }
        
        // Let server run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Server foundation is working - GraphQL processing will be tested
        // via HTTP endpoints in integration tests
        std::cout << "✓ Server foundation ready for GraphQL integration" << std::endl;
        
        // Test stop
        std::cout << "Stopping server..." << std::endl;
        if (server->stop()) {
            std::cout << "✓ Server stopped successfully" << std::endl;
        } else {
            std::cout << "✗ Server stop failed" << std::endl;
            return 1;
        }
        
        // Test stopped status
        if (server->get_status() == Server::Status::STOPPED) {
            std::cout << "✓ Final status is STOPPED" << std::endl;
        } else {
            std::cout << "✗ Final status check failed" << std::endl;
            return 1;
        }
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        std::cout << "Server foundation implementation is working correctly." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}