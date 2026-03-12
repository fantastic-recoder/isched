// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_TenantManager.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the TenantManager class for multi-tenant process pool management
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * This file contains the implementation of the TenantManager class for the
 * Universal Application Server Backend with process pool management,
 * database isolation, and load balancing.
 * 
 * @note All implementations follow C++ Core Guidelines with smart pointer usage.
 */

#include "isched_TenantManager.hpp"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <algorithm>
#include <random>

namespace isched::v0_0_1::backend {

/**
 * @brief PIMPL implementation details for TenantManager class
 */
class TenantManager::Implementation {
public:
    // Type aliases
    using TenantMap = std::unordered_map<TenantId, TenantConfiguration>;
    using SessionMap = std::unordered_map<TenantId, std::shared_ptr<TenantSession>>;
    using ProcessMap = std::unordered_map<ProcessId, std::vector<TenantId>>;
    
    explicit Implementation(const PoolConfiguration& config) 
        : config_(config)
        , next_process_id_(1)
        , maintenance_thread_running_(false)
    {
        start_time_ = std::chrono::steady_clock::now();
    }

    ~Implementation() {
        if (maintenance_thread_running_.load()) {
            maintenance_thread_running_.store(false);
            if (maintenance_thread_.joinable()) {
                maintenance_thread_.join();
            }
        }
    }

    PoolConfiguration config_;
    TenantMap tenants_;
    SessionMap active_sessions_;
    ProcessMap process_assignments_;
    std::unordered_set<ProcessId> idle_processes_;
    
    std::atomic<ProcessId> next_process_id_;
    std::atomic<bool> maintenance_thread_running_;
    std::thread maintenance_thread_;
    
    std::chrono::steady_clock::time_point start_time_;
    
    // Performance metrics
    std::atomic<uint64_t> total_tenant_requests_{0};
    std::atomic<double> avg_response_time_{0.0};
    
    // Random number generator for load balancing
    std::random_device rd_;
    std::mt19937 gen_{rd_()};
};

// Configuration validation implementation
bool TenantManager::PoolConfiguration::validate() const {
    if (min_processes == 0 || max_processes == 0) {
        throw std::invalid_argument("Process pool sizes must be greater than 0");
    }
    
    if (min_processes > max_processes) {
        throw std::invalid_argument("Minimum processes cannot exceed maximum processes");
    }
    
    if (max_tenants_per_process == 0) {
        throw std::invalid_argument("Max tenants per process must be greater than 0");
    }
    
    if (process_idle_timeout.count() <= 0) {
        throw std::invalid_argument("Process idle timeout must be positive");
    }
    
    if (health_check_interval.count() <= 0) {
        throw std::invalid_argument("Health check interval must be positive");
    }
    
    return true;
}

// Factory method implementation
TenantManager::UniquePtr TenantManager::create(const PoolConfiguration& config) {
    // Validate configuration first
    PoolConfiguration validated_config = config;
    if (!validated_config.validate()) {
        throw std::runtime_error("Invalid tenant manager configuration");
    }
    
    // Use custom deleter to ensure proper cleanup
    return std::unique_ptr<TenantManager>(new TenantManager(validated_config));
}

// Constructor implementation
TenantManager::TenantManager(const PoolConfiguration& config) 
    : m_config(config)
    , m_impl(std::make_unique<Implementation>(config))
    , m_start_time(std::chrono::steady_clock::now())
{
    std::cout << "TenantManager instance created with " << config.min_processes 
              << "-" << config.max_processes << " process pool" << std::endl;
}

// Destructor implementation
TenantManager::~TenantManager() {
    if (get_status() == Status::RUNNING) {
        std::cout << "TenantManager destructor called, stopping manager..." << std::endl;
        stop();
    }
    std::cout << "TenantManager instance destroyed" << std::endl;
}

// Start manager implementation
bool TenantManager::start() {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    
    if (m_status.load() != Status::STOPPED) {
        std::cout << "TenantManager start called but status is not STOPPED" << std::endl;
        return false;
    }
    
    m_status.store(Status::STARTING);
    std::cout << "Starting iSched Tenant Manager..." << std::endl;
    
    try {
        if (!initialize()) {
            m_status.store(Status::ERROR);
            return false;
        }
        
        // Start maintenance thread
        m_impl->maintenance_thread_running_.store(true);
        m_impl->maintenance_thread_ = std::thread([this]() {
            maintenance_thread_function();
        });
        
        m_status.store(Status::RUNNING);
        m_impl->start_time_ = std::chrono::steady_clock::now();
        
        std::cout << "TenantManager started successfully" << std::endl;
        std::cout << "Database root: " << m_config.database_root_path << std::endl;
        std::cout << "Process pool: " << m_config.min_processes << "-" << m_config.max_processes << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "TenantManager startup failed: " << e.what() << std::endl;
        m_status.store(Status::ERROR);
        return false;
    }
}

// Stop manager implementation
bool TenantManager::stop(Duration timeout_ms) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    
    if (m_status.load() != Status::RUNNING) {
        std::cout << "TenantManager stop called but status is not RUNNING" << std::endl;
        return true; // Already stopped
    }
    
    m_status.store(Status::STOPPING);
    std::cout << "Stopping TenantManager gracefully (timeout: " << timeout_ms.count() << "ms)..." << std::endl;
    
    try {
        // Stop maintenance thread
        if (m_impl->maintenance_thread_running_.load()) {
            m_impl->maintenance_thread_running_.store(false);
            if (m_impl->maintenance_thread_.joinable()) {
                m_impl->maintenance_thread_.join();
            }
        }
        
        // Stop all tenant processes
        std::lock_guard<std::mutex> process_lock(m_processes_mutex);
        for (auto& [process_id, tenant_list] : m_impl->process_assignments_) {
            stop_tenant_process(process_id);
        }
        
        m_status.store(Status::STOPPED);
        std::cout << "TenantManager stopped successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Error during TenantManager shutdown: " << e.what() << std::endl;
        m_status.store(Status::ERROR);
        return false;
    }
}

// Status getter implementation
TenantManager::Status TenantManager::get_status() const noexcept {
    return m_status.load();
}

// Create tenant implementation
bool TenantManager::create_tenant(const TenantId& tenant_id, const TenantConfiguration& config) {
    std::lock_guard<std::mutex> lock(m_tenants_mutex);
    
    if (m_impl->tenants_.find(tenant_id) != m_impl->tenants_.end()) {
        throw std::invalid_argument("Tenant ID already exists: " + tenant_id);
    }
    
    try {
        // Create tenant database directory
        auto db_path = create_tenant_database_directory(tenant_id);
        std::cout << "Created tenant database directory: " << db_path << std::endl;
        
        // Store tenant configuration
        auto tenant_config = config;
        tenant_config.tenant_id = tenant_id;
        m_impl->tenants_[tenant_id] = tenant_config;
        
        // Update active tenant count
        m_active_tenants.fetch_add(1);
        
        std::cout << "Tenant created successfully: " << tenant_id << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Failed to create tenant " << tenant_id << ": " << e.what() << std::endl;
        throw std::runtime_error("Tenant creation failed: " + std::string(e.what()));
    }
}

// Remove tenant implementation
bool TenantManager::remove_tenant(const TenantId& tenant_id) {
    std::lock_guard<std::mutex> tenants_lock(m_tenants_mutex);
    std::lock_guard<std::mutex> processes_lock(m_processes_mutex);
    
    auto tenant_it = m_impl->tenants_.find(tenant_id);
    if (tenant_it == m_impl->tenants_.end()) {
        std::cout << "Tenant not found for removal: " << tenant_id << std::endl;
        return false;
    }
    
    try {
        // Remove from active sessions
        auto session_it = m_impl->active_sessions_.find(tenant_id);
        if (session_it != m_impl->active_sessions_.end()) {
            // Mark session as stopping
            session_it->second->status = ProcessStatus::STOPPING;
            m_impl->active_sessions_.erase(session_it);
        }
        
        // Remove tenant from process assignments
        for (auto& [process_id, tenant_list] : m_impl->process_assignments_) {
            auto it = std::find(tenant_list.begin(), tenant_list.end(), tenant_id);
            if (it != tenant_list.end()) {
                tenant_list.erase(it);
                
                // If process has no more tenants, mark as idle
                if (tenant_list.empty()) {
                    m_impl->idle_processes_.insert(process_id);
                }
                break;
            }
        }
        
        // Remove tenant configuration
        m_impl->tenants_.erase(tenant_it);
        
        // Update active tenant count
        m_active_tenants.fetch_sub(1);
        
        std::cout << "Tenant removed successfully: " << tenant_id << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Error removing tenant " << tenant_id << ": " << e.what() << std::endl;
        return false;
    }
}

// Get tenant session implementation
std::shared_ptr<TenantManager::TenantSession> TenantManager::get_tenant_session(const TenantId& tenant_id) {
    std::lock_guard<std::mutex> lock(m_tenants_mutex);
    
    // Check if tenant exists
    auto tenant_it = m_impl->tenants_.find(tenant_id);
    if (tenant_it == m_impl->tenants_.end()) {
        throw std::runtime_error("Tenant not found: " + tenant_id);
    }
    
    // Check for existing session
    auto session_it = m_impl->active_sessions_.find(tenant_id);
    if (session_it != m_impl->active_sessions_.end()) {
        // Update last activity and return existing session
        session_it->second->last_activity = std::chrono::steady_clock::now();
        session_it->second->request_count.fetch_add(1);
        return session_it->second;
    }
    
    try {
        // Create new session
        auto process_id = assign_tenant_to_process(tenant_id);
        auto session = std::make_shared<TenantSession>(tenant_id, process_id);
        
        // Set up database path
        session->database_path = m_config.database_root_path + "/" + tenant_id + "/main.db";
        session->status = ProcessStatus::READY;
        
        // Store session
        m_impl->active_sessions_[tenant_id] = session;
        
        // Update metrics
        m_total_requests.fetch_add(1);
        m_impl->total_tenant_requests_.fetch_add(1);
        
        std::cout << "Created new tenant session: " << tenant_id << " -> process " << process_id << std::endl;
        return session;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create tenant session: " + std::string(e.what()));
    }
}

// Release tenant session implementation
void TenantManager::release_tenant_session(std::shared_ptr<TenantSession> session) {
    if (!session) return;
    
    session->status = ProcessStatus::IDLE;
    session->last_activity = std::chrono::steady_clock::now();
    
    std::cout << "Released tenant session: " << session->tenant_id << std::endl;
}

// Get tenant list implementation
std::vector<TenantManager::TenantId> TenantManager::get_tenant_list() const {
    std::lock_guard<std::mutex> lock(m_tenants_mutex);
    
    std::vector<TenantId> tenant_list;
    tenant_list.reserve(m_impl->tenants_.size());
    
    for (const auto& [tenant_id, config] : m_impl->tenants_) {
        tenant_list.push_back(tenant_id);
    }
    
    return tenant_list;
}

// Get tenant configuration implementation
TenantManager::TenantConfiguration TenantManager::get_tenant_configuration(const TenantId& tenant_id) const {
    std::lock_guard<std::mutex> lock(m_tenants_mutex);
    
    auto it = m_impl->tenants_.find(tenant_id);
    if (it == m_impl->tenants_.end()) {
        throw std::runtime_error("Tenant not found: " + tenant_id);
    }
    
    return it->second;
}

// Get pool configuration implementation
const TenantManager::PoolConfiguration& TenantManager::get_pool_configuration() const noexcept {
    return m_config;
}

// Get metrics implementation
TenantManager::String TenantManager::get_metrics() const {
    std::lock_guard<std::mutex> lock(m_tenants_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_impl->start_time_);
    
    size_t active_processes = 0;
    size_t idle_processes = 0;
    
    {
        std::lock_guard<std::mutex> process_lock(m_processes_mutex);
        active_processes = m_impl->process_assignments_.size();
        idle_processes = m_impl->idle_processes_.size();
    }
    
    // Calculate utilization
    double utilization = active_processes > 0 ? 
        static_cast<double>(active_processes - idle_processes) / active_processes : 0.0;
    
    // Build metrics JSON
    return "{\"active_tenants\":" + std::to_string(m_active_tenants.load()) +
           ",\"process_pool\":{" +
           "\"active_processes\":" + std::to_string(active_processes) +
           ",\"idle_processes\":" + std::to_string(idle_processes) +
           ",\"utilization\":" + std::to_string(utilization) +
           "},\"total_requests\":" + std::to_string(m_total_requests.load()) +
           ",\"avg_response_time\":" + std::to_string(m_impl->avg_response_time_.load()) +
           ",\"uptime_seconds\":" + std::to_string(uptime.count()) +
           ",\"pool_config\":{" +
           "\"min_processes\":" + std::to_string(m_config.min_processes) +
           ",\"max_processes\":" + std::to_string(m_config.max_processes) +
           ",\"max_tenants_per_process\":" + std::to_string(m_config.max_tenants_per_process) +
           "}}";
}

// Get health implementation
TenantManager::String TenantManager::get_health() const {
    auto status = get_status();
    bool is_healthy = (status == Status::RUNNING);
    
    std::string health_status = is_healthy ? "UP" : "DOWN";
    
    return "{\"status\":\"" + health_status +
           "\",\"manager_status\":" + std::to_string(static_cast<int>(status)) +
           ",\"active_tenants\":" + std::to_string(m_active_tenants.load()) +
           ",\"timestamp\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count()) +
           ",\"version\":\"1.0.0\"" +
           ",\"checks\":{" +
           "\"database_root\":\"" + (std::filesystem::exists(m_config.database_root_path) ? "UP" : "DOWN") + "\"" +
           ",\"process_pool\":\"" + (is_healthy ? "UP" : "DOWN") + "\"" +
           "}}";
}

// Private initialization method
bool TenantManager::initialize() {
    std::cout << "Initializing TenantManager components..." << std::endl;
    
    try {
        // Create database root directory
        std::filesystem::create_directories(m_config.database_root_path);
        std::cout << "Database root directory ensured: " << m_config.database_root_path << std::endl;
        
        // Initialize minimum number of idle processes
        for (size_t i = 0; i < m_config.min_processes; ++i) {
            auto process_id = m_impl->next_process_id_.fetch_add(1);
            m_impl->idle_processes_.insert(process_id);
            m_impl->process_assignments_[process_id] = std::vector<TenantId>();
        }
        
        std::cout << "Initialized " << m_config.min_processes << " idle processes" << std::endl;
        std::cout << "TenantManager components initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Failed to initialize TenantManager components: " << e.what() << std::endl;
        return false;
    }
}

// Create tenant database directory
TenantManager::String TenantManager::create_tenant_database_directory(const TenantId& tenant_id) {
    auto db_dir = m_config.database_root_path + "/" + tenant_id;
    
    try {
        std::filesystem::create_directories(db_dir);
        return db_dir;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create tenant database directory: " + std::string(e.what()));
    }
}

// Start tenant process (placeholder implementation)
TenantManager::ProcessId TenantManager::start_tenant_process(const TenantId& tenant_id) {
    std::cout << "Starting tenant process for: " << tenant_id << " (placeholder implementation)" << std::endl;
    
    // TODO: Implement actual process creation
    auto process_id = m_impl->next_process_id_.fetch_add(1);
    
    // Simulate process startup
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    return process_id;
}

// Stop tenant process (placeholder implementation)
bool TenantManager::stop_tenant_process(ProcessId process_id) {
    std::cout << "Stopping tenant process: " << process_id << " (placeholder implementation)" << std::endl;
    
    // TODO: Implement actual process termination
    
    return true;
}

// Assign tenant to process (load balancing implementation)
TenantManager::ProcessId TenantManager::assign_tenant_to_process(const TenantId& tenant_id) {
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    
    // Try to use an idle process first
    if (!m_impl->idle_processes_.empty()) {
        auto process_id = *m_impl->idle_processes_.begin();
        m_impl->idle_processes_.erase(m_impl->idle_processes_.begin());
        
        // Assign tenant to process
        m_impl->process_assignments_[process_id].push_back(tenant_id);
        
        std::cout << "Assigned tenant " << tenant_id << " to idle process " << process_id << std::endl;
        return process_id;
    }
    
    // Find process with least tenants
    ProcessId best_process = 0;
    size_t min_tenants = SIZE_MAX;
    
    for (const auto& [process_id, tenant_list] : m_impl->process_assignments_) {
        if (tenant_list.size() < min_tenants && tenant_list.size() < m_config.max_tenants_per_process) {
            min_tenants = tenant_list.size();
            best_process = process_id;
        }
    }
    
    // If we found a suitable process, use it
    if (best_process != 0) {
        m_impl->process_assignments_[best_process].push_back(tenant_id);
        std::cout << "Assigned tenant " << tenant_id << " to existing process " << best_process 
                  << " (load: " << m_impl->process_assignments_[best_process].size() << ")" << std::endl;
        return best_process;
    }
    
    // Create new process if under limit
    if (m_impl->process_assignments_.size() < m_config.max_processes) {
        auto new_process = start_tenant_process(tenant_id);
        m_impl->process_assignments_[new_process] = {tenant_id};
        
        std::cout << "Created new process " << new_process << " for tenant " << tenant_id << std::endl;
        return new_process;
    }
    
    // If all else fails, use random assignment
    std::uniform_int_distribution<> dist(0, m_impl->process_assignments_.size() - 1);
    auto it = m_impl->process_assignments_.begin();
    std::advance(it, dist(m_impl->gen_));
    
    it->second.push_back(tenant_id);
    std::cout << "Assigned tenant " << tenant_id << " to random process " << it->first 
              << " (overloaded: " << it->second.size() << ")" << std::endl;
    
    return it->first;
}

// Cleanup idle processes
void TenantManager::cleanup_idle_processes() {
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<ProcessId> processes_to_stop;
    
    for (auto process_id : m_impl->idle_processes_) {
        // TODO: Check actual process idle time
        // For now, just maintain minimum processes
        if (m_impl->process_assignments_.size() > m_config.min_processes) {
            processes_to_stop.push_back(process_id);
        }
    }
    
    for (auto process_id : processes_to_stop) {
        if (stop_tenant_process(process_id)) {
            m_impl->idle_processes_.erase(process_id);
            m_impl->process_assignments_.erase(process_id);
            std::cout << "Cleaned up idle process: " << process_id << std::endl;
        }
    }
}

// Health check processes
void TenantManager::health_check_processes() {
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    
    std::vector<ProcessId> failed_processes;
    
    for (const auto& [process_id, tenant_list] : m_impl->process_assignments_) {
        // TODO: Implement actual process health checking
        // For now, just log the check
        if (tenant_list.empty()) {
            m_impl->idle_processes_.insert(process_id);
        }
    }
    
    std::cout << "Health check completed for " << m_impl->process_assignments_.size() << " processes" << std::endl;
}

// Maintenance thread function
void TenantManager::maintenance_thread_function() {
    std::cout << "TenantManager maintenance thread started" << std::endl;
    
    while (m_impl->maintenance_thread_running_.load()) {
        try {
            // Health check processes
            health_check_processes();
            
            // Cleanup idle processes
            cleanup_idle_processes();
            
            // Update metrics
            // (metrics are updated in real-time via atomics)
            
        } catch (const std::exception& e) {
            std::cout << "Error in maintenance thread: " << e.what() << std::endl;
        }
        
        // Sleep for health check interval
        std::this_thread::sleep_for(m_config.health_check_interval);
    }
    
    std::cout << "TenantManager maintenance thread stopped" << std::endl;
}

} // namespace isched::v0_0_1::backend