/**
 * @file isched_ipc.hpp
 * @brief Shared memory IPC framework for CLI process coordination
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * This file contains the inter-process communication framework for coordinating
 * between the main Isched server and CLI processes (isched-cli-python, isched-cli-typescript).
 * Uses shared memory segments with message queues for efficient command/response handling.
 * 
 * @example Basic IPC usage:
 * @code{cpp}
 * #include "isched_ipc.hpp"
 * 
 * // Server side
 * auto server = isched::v0_0_1::backend::IPCServer::create("isched-server");
 * server->start();
 * 
 * // CLI side
 * auto client = isched::v0_0_1::backend::IPCClient::create("isched-server");
 * client->connect();
 * auto response = client->send_command("execute_script", script_data);
 * @endcode
 * 
 * @note All resource management uses smart pointers as required by FR-021.
 * @see FR-016, FR-017 for IPC and configuration exchange requirements
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <optional>

namespace isched::v0_0_1::backend {
    using namespace std::chrono_literals;
/**
 * @brief IPC message types for command/response coordination
 */
enum class IPCMessageType {
    Command,
    Response,
    Error,
    Heartbeat,
    Shutdown
};

/**
 * @brief IPC message structure for shared memory communication
 */
struct IPCMessage {
    IPCMessageType type;
    std::string command;
    std::string payload;
    std::string request_id;
    std::string sender_id;
    std::chrono::system_clock::time_point timestamp;
    
    /// @brief Check if message is still valid (not expired)
    bool is_valid(std::chrono::seconds timeout = std::chrono::seconds(30s)) const noexcept {
        auto now = std::chrono::system_clock::now();
        return (now - timestamp) < timeout;
    }
};

/**
 * @brief IPC response containing result and status information
 */
struct IPCResponse {
    bool success = false;
    std::string result;
    std::string error_message;
    std::string request_id;
    std::chrono::system_clock::time_point timestamp;
    
    /// @brief Check if response indicates success
    bool is_success() const noexcept {
        return success && error_message.empty();
    }
};

/**
 * @brief Configuration data for IPC setup
 */
struct IPCConfig {
    std::string shared_memory_name;
    size_t shared_memory_size = 1024 * 1024; // 1MB default
    std::string message_queue_name;
    size_t max_message_size = 64 * 1024; // 64KB default
    int max_clients = 10;
    std::chrono::seconds timeout = std::chrono::seconds(30s);
    int max_messages = 1000;
    std::chrono::milliseconds default_timeout = 100ms;
};

/**
 * @brief Abstract base class for IPC communication
 * 
 * Provides common functionality for both server and client IPC operations
 * with smart pointer-based resource management and thread safety.
 */
class IPCBase {
public:
    /**
     * @brief Virtual destructor for proper inheritance
     */
    virtual ~IPCBase() = default;
    
    // Non-copyable and non-movable due to atomic members
    IPCBase(const IPCBase&) = delete;
    IPCBase& operator=(const IPCBase&) = delete;
    IPCBase(IPCBase&&) = delete;
    IPCBase& operator=(IPCBase&&) = delete;
    
    /**
     * @brief Get IPC connection status
     * @return True if IPC is connected and operational
     */
    virtual bool is_connected() const = 0;
    
    /**
     * @brief Get IPC metrics for monitoring
     * @return JSON string with IPC statistics
     */
    virtual std::string get_metrics() const = 0;

protected:
    /**
     * @brief Protected constructor for inheritance
     * @param config IPC configuration parameters
     */
    explicit IPCBase(const IPCConfig& config) : config_(config) {}
    
    IPCConfig config_;
    mutable std::mutex metrics_mutex_;
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> errors_count_{0};
};

/**
 * @brief IPC Server for handling CLI process communication
 * 
 * The IPCServer manages shared memory segments and message queues for
 * coordinating with multiple CLI processes. Provides command dispatch
 * and response handling for configuration script execution.
 */
class IPCServer : public IPCBase {
public:
    /**
     * @brief Factory method to create IPCServer instance
     * @param name Server name for IPC identification
     * @param config IPC configuration parameters
     * @return Smart pointer to IPCServer instance
     */
    static std::unique_ptr<IPCServer> create(const std::string& name, 
                                           const IPCConfig& config = {});
    
    /**
     * @brief Virtual destructor
     */
    ~IPCServer() override = default;
    
    /**
     * @brief Start IPC server and listen for client connections
     * @return True if server started successfully
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop IPC server and cleanup resources
     */
    virtual void stop() = 0;
    
    /**
     * @brief Register command handler for specific command types
     * @param command Command name to handle
     * @param handler Function to process the command
     */
    virtual void register_command_handler(
        const std::string& command,
        std::function<IPCResponse(const IPCMessage&)> handler) = 0;
    
    /**
     * @brief Send message to specific client
     * @param client_id Target client identifier
     * @param message Message to send
     * @return True if message sent successfully
     */
    virtual bool send_to_client(const std::string& client_id, 
                               const IPCMessage& message) = 0;
    
    /**
     * @brief Broadcast message to all connected clients
     * @param message Message to broadcast
     * @return Number of clients that received the message
     */
    virtual int broadcast_message(const IPCMessage& message) = 0;
    
    /**
     * @brief Get list of connected client IDs
     * @return Vector of client identifiers
     */
    virtual std::vector<std::string> get_connected_clients() const = 0;

protected:
    /**
     * @brief Protected constructor for factory pattern
     * @param name Server name
     * @param config IPC configuration
     */
    IPCServer(const std::string& name, const IPCConfig& config) 
        : IPCBase(config), server_name_(name) {}
    
    std::string server_name_;
    std::unordered_map<std::string, std::function<IPCResponse(const IPCMessage&)>> command_handlers_;
    mutable std::mutex handlers_mutex_;
};

/**
 * @brief IPC Client for CLI process communication with server
 * 
 * The IPCClient provides interface for CLI processes to communicate with
 * the main server, sending configuration commands and receiving responses.
 */
class IPCClient : public IPCBase {
public:
    /**
     * @brief Factory method to create IPCClient instance
     * @param server_name Name of server to connect to
     * @param client_id Unique client identifier
     * @param config IPC configuration parameters
     * @return Smart pointer to IPCClient instance
     */
    static std::unique_ptr<IPCClient> create(const std::string& server_name,
                                           const std::string& client_id,
                                           const IPCConfig& config = {});
    
    /**
     * @brief Virtual destructor
     */
    ~IPCClient() override = default;
    
    /**
     * @brief Connect to IPC server
     * @return True if connection established successfully
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Disconnect from IPC server
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Send command to server and wait for response
     * @param command Command name
     * @param payload Command payload data
     * @param timeout Maximum wait time for response
     * @return Response from server
     */
    virtual IPCResponse send_command(
        const std::string& command,
        const std::string& payload,
        std::chrono::seconds timeout = std::chrono::seconds(30)) = 0;
    
    /**
     * @brief Send asynchronous command without waiting for response
     * @param command Command name
     * @param payload Command payload data
     * @return Request ID for tracking
     */
    virtual std::string send_async_command(
        const std::string& command,
        const std::string& payload) = 0;
    
    /**
     * @brief Check for response to asynchronous command
     * @param request_id Request identifier from send_async_command
     * @return Optional response if available
     */
    virtual std::optional<IPCResponse> check_response(
        const std::string& request_id) = 0;

protected:
    /**
     * @brief Protected constructor for factory pattern
     * @param server_name Server name to connect to
     * @param client_id Client identifier
     * @param config IPC configuration
     */
    IPCClient(const std::string& server_name, const std::string& client_id, 
             const IPCConfig& config) 
        : IPCBase(config), server_name_(server_name), client_id_(client_id) {}
    
    std::string server_name_;
    std::string client_id_;
    std::unordered_map<std::string, IPCResponse> pending_responses_;
    mutable std::mutex responses_mutex_;
};

/**
 * @brief Message queue interface for IPC communication
 * 
 * Provides abstraction for platform-specific message queue implementations
 * with smart pointer-based resource management.
 */
class MessageQueue {
public:
    /**
     * @brief Factory method to create MessageQueue instance
     * @param name Queue name for identification
     * @param max_messages Maximum number of queued messages
     * @param max_message_size Maximum size of individual messages
     * @return Smart pointer to MessageQueue instance
     */
    static std::unique_ptr<MessageQueue> create(
        const std::string& name,
        size_t max_messages = 100,
        size_t max_message_size = 64 * 1024);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~MessageQueue() = default;
    
    // Non-copyable but movable
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = default;
    MessageQueue& operator=(MessageQueue&&) = default;
    
    /**
     * @brief Send message to queue
     * @param message Message to send
     * @param timeout Maximum wait time if queue is full
     * @return True if message sent successfully
     */
    virtual bool send(const IPCMessage& message, 
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) = 0;
    
    /**
     * @brief Receive message from queue
     * @param timeout Maximum wait time for message
     * @return Optional message if available
     */
    virtual std::optional<IPCMessage> receive(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) = 0;
    
    /**
     * @brief Check if queue is empty
     * @return True if no messages are queued
     */
    virtual bool empty() const = 0;
    
    /**
     * @brief Get current queue size
     * @return Number of messages in queue
     */
    virtual size_t size() const = 0;

protected:
    /**
     * @brief Protected constructor for inheritance
     */
    MessageQueue() = default;
};

/**
 * @brief Utility functions for IPC message handling
 */
namespace ipc_utils {

/**
 * @brief Generate unique request ID for IPC messages
 * @return Unique identifier string
 */
std::string generate_request_id();

/**
 * @brief Serialize IPC message to string format
 * @param message Message to serialize
 * @return Serialized message string
 */
std::string serialize_message(const IPCMessage& message);

/**
 * @brief Deserialize string to IPC message
 * @param data Serialized message data
 * @return Deserialized message or nullopt if invalid
 */
std::optional<IPCMessage> deserialize_message(const std::string& data);

/**
 * @brief Create error response message
 * @param request_id Original request identifier
 * @param error_message Error description
 * @return Error response structure
 */
IPCResponse create_error_response(const std::string& request_id, 
                                 const std::string& error_message);

/**
 * @brief Create success response message
 * @param request_id Original request identifier
 * @param result Response data
 * @return Success response structure
 */
IPCResponse create_success_response(const std::string& request_id, 
                                   const std::string& result);

} // namespace ipc_utils

} // namespace isched::v0_0_1::backend