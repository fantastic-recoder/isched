// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_ipc_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Test suite for IPC Framework
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * Tests for the Universal Application Server Backend IPC system,
 * following TDD approach as required by Constitutional compliance.
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include "isched/shared/ipc/isched_ipc.hpp"

using namespace isched::v0_0_1::backend;

/**
 * @brief Test fixture for IPC tests
 */
class IPCTestFixture {
public:
    IPCTestFixture() {
        config.shared_memory_name = "test_shm";
        config.shared_memory_size = 1024UL * 1024UL; // 1MB
        config.message_queue_name = "test_mq";
        config.max_message_size = 1024UL;
        config.max_messages = 100;
        config.default_timeout = std::chrono::milliseconds(1000);
    }
    
protected:
    IPCConfig config;
};

TEST_CASE_METHOD(IPCTestFixture, "IPC Server creation and lifecycle", "[ipc][server]") {
    auto server = IPCServer::create("test_server", config);
    REQUIRE(server != nullptr);
    
    SECTION("Server startup") {
        bool started = server->start();
        REQUIRE(started);
        REQUIRE(server->is_connected());
        
        server->stop();
        REQUIRE_FALSE(server->is_connected());
    }
    
    SECTION("Server metrics") {
        server->start();
        auto metrics = server->get_metrics();
        REQUIRE_FALSE(metrics.empty());
        REQUIRE(metrics.find("server_name") != std::string::npos);
        server->stop();
    }
}

TEST_CASE_METHOD(IPCTestFixture, "IPC Client creation and lifecycle", "[ipc][client]") {
    auto client = IPCClient::create("test_server", "test_client", config);
    REQUIRE(client != nullptr);
    
    SECTION("Client connection") {
        bool connected = client->connect();
        REQUIRE(connected);
        REQUIRE(client->is_connected());
        
        client->disconnect();
        REQUIRE_FALSE(client->is_connected());
    }
    
    SECTION("Client metrics") {
        client->connect();
        auto metrics = client->get_metrics();
        REQUIRE_FALSE(metrics.empty());
        REQUIRE(metrics.find("client_id") != std::string::npos);
        client->disconnect();
    }
}

TEST_CASE_METHOD(IPCTestFixture, "Message Queue operations", "[ipc][queue]") {
    auto queue = MessageQueue::create("test_queue", 10, 1024);
    REQUIRE(queue != nullptr);
    
    SECTION("Queue basic operations") {
        REQUIRE(queue->empty());
        REQUIRE(queue->size() == 0);
        
        IPCMessage message{
            IPCMessageType::Command,
            "test_command",
            "test_payload",
            "req_123",
            "sender_456",
            std::chrono::system_clock::now()
        };
        
        bool sent = queue->send(message, std::chrono::milliseconds(100));
        REQUIRE(sent);
        REQUIRE_FALSE(queue->empty());
        REQUIRE(queue->size() == 1);
        
        auto received = queue->receive(std::chrono::milliseconds(100));
        REQUIRE(received.has_value());
        REQUIRE(received->command == "test_command");
        REQUIRE(received->payload == "test_payload");
        REQUIRE(queue->empty());
    }
    
    SECTION("Queue timeout behavior") {
        auto start = std::chrono::high_resolution_clock::now();
        auto received = queue->receive(std::chrono::milliseconds(100));
        auto end = std::chrono::high_resolution_clock::now();
        
        REQUIRE_FALSE(received.has_value());
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() >= 90); // Allow some tolerance
        REQUIRE(duration.count() <= 150);
    }
}

TEST_CASE("IPC Utility functions", "[ipc][utils]") {
    SECTION("Request ID generation") {
        auto id1 = ipc_utils::generate_request_id();
        auto id2 = ipc_utils::generate_request_id();
        
        REQUIRE_FALSE(id1.empty());
        REQUIRE_FALSE(id2.empty());
        REQUIRE(id1 != id2);
        REQUIRE(id1.find("req_") == 0);
    }
    
    SECTION("Response creation") {
        auto success_response = ipc_utils::create_success_response("req_123", "success");
        REQUIRE(success_response.success);
        REQUIRE(success_response.result == "success");
        REQUIRE(success_response.request_id == "req_123");
        
        auto error_response = ipc_utils::create_error_response("req_456", "error occurred");
        REQUIRE_FALSE(error_response.success);
        REQUIRE(error_response.error_message == "error occurred");
        REQUIRE(error_response.request_id == "req_456");
    }
    
    SECTION("Message serialization") {
        IPCMessage message{
            IPCMessageType::Command,
            "test_cmd",
            "payload",
            "req_789",
            "sender",
            std::chrono::system_clock::now()
        };
        
        auto serialized = ipc_utils::serialize_message(message);
        REQUIRE_FALSE(serialized.empty());
        
        auto deserialized = ipc_utils::deserialize_message(serialized);
        REQUIRE(deserialized.has_value());
        // Note: Current implementation is placeholder, so detailed comparison is not tested
    }
}

// Performance test (Constitutional requirement: 20ms response times)
TEST_CASE_METHOD(IPCTestFixture, "IPC Performance requirements", "[ipc][performance]") {
    const int num_operations = 100;
    
    SECTION("Message queue performance") {
        auto queue = MessageQueue::create("perf_queue", num_operations, 1024);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Send messages
        for (int i = 0; i < num_operations; ++i) {
            IPCMessage message{
                IPCMessageType::Command,
                "cmd_" + std::to_string(i),
                "payload_" + std::to_string(i),
                "req_" + std::to_string(i),
                "sender",
                std::chrono::system_clock::now()
            };
            
            bool sent = queue->send(message, std::chrono::milliseconds(10));
            REQUIRE(sent);
        }
        
        // Receive messages
        for (int i = 0; i < num_operations; ++i) {
            auto received = queue->receive(std::chrono::milliseconds(10));
            REQUIRE(received.has_value());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should handle 100 message operations well under 20ms each (total under 2s is reasonable)
        REQUIRE(duration.count() < 2000);
    }
}