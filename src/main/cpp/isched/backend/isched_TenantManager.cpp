// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_TenantManager.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief In-process multi-tenant manager — no process pool, per-tenant SQLite isolation
 * @author isched Development Team
 * @version 2.0.0
 * @date 2026-03-13
 */

#include "isched_TenantManager.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <filesystem>
#include <chrono>

namespace isched::v0_0_1::backend {

// ============================================================================
// PIMPL
// ============================================================================

class TenantManager::Implementation {
public:
    using TenantMap  = std::unordered_map<TenantId, TenantConfiguration>;
    using SessionMap = std::unordered_map<TenantId, std::shared_ptr<TenantSession>>;

    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    TenantMap  tenants;
    SessionMap active_sessions;
};

// ============================================================================
// Configuration
// ============================================================================

bool TenantManager::Configuration::validate() const {
    if (database_root_path.empty()) {
        throw std::invalid_argument("database_root_path must not be empty");
    }
    if (max_tenants == 0) {
        throw std::invalid_argument("max_tenants must be greater than 0");
    }
    return true;
}

// ============================================================================
// Factory / ctor / dtor
// ============================================================================

TenantManager::UniquePtr TenantManager::create(const Configuration& config) {
    Configuration validated = config;
    validated.validate();
    return UniquePtr{new TenantManager(validated)};
}

TenantManager::TenantManager(const Configuration& config)
    : m_config(config)
    , m_impl(std::make_unique<Implementation>())
    , m_start_time(std::chrono::steady_clock::now())
{
    spdlog::info("TenantManager created (database_root={})", config.database_root_path);
}

TenantManager::~TenantManager() {
    if (get_status() == Status::RUNNING) {
        stop();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool TenantManager::start() {
    std::lock_guard<std::mutex> lk(m_status_mutex);

    if (m_status.load() != Status::STOPPED) {
        spdlog::warn("TenantManager::start() called but status is not STOPPED");
        return false;
    }

    try {
        if (!initialize()) {
            m_status.store(Status::ERROR);
            return false;
        }
        m_status.store(Status::RUNNING);
        m_impl->start_time = std::chrono::steady_clock::now();
        spdlog::info("TenantManager started (database_root={})", m_config.database_root_path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("TenantManager startup failed: {}", e.what());
        m_status.store(Status::ERROR);
        return false;
    }
}

bool TenantManager::stop(Duration /*timeout_ms*/) {
    std::lock_guard<std::mutex> lk(m_status_mutex);

    if (m_status.load() != Status::RUNNING) {
        return true; // idempotent
    }

    {
        std::lock_guard<std::mutex> tl(m_tenants_mutex);
        m_impl->active_sessions.clear();
    }

    m_status.store(Status::STOPPED);
    spdlog::info("TenantManager stopped");
    return true;
}

TenantManager::Status TenantManager::get_status() const noexcept {
    return m_status.load();
}

// ============================================================================
// Tenant management
// ============================================================================

bool TenantManager::create_tenant(const TenantId& tenant_id, const TenantConfiguration& config) {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    if (m_impl->tenants.count(tenant_id)) {
        throw std::invalid_argument("Tenant ID already exists: " + tenant_id);
    }

    if (m_impl->tenants.size() >= m_config.max_tenants) {
        throw std::runtime_error("Maximum tenant count reached");
    }

    // Initialise the tenant's database (creates directory + schema tables)
    auto init_result = m_database_manager->initialize_tenant(tenant_id);
    if (!init_result) {
        throw std::runtime_error("Failed to initialise database for tenant: " + tenant_id);
    }

    TenantConfiguration stored = config;
    stored.tenant_id = tenant_id;
    m_impl->tenants[tenant_id] = std::move(stored);
    m_active_tenants.fetch_add(1, std::memory_order_relaxed);

    spdlog::info("Tenant created: {}", tenant_id);
    return true;
}

bool TenantManager::remove_tenant(const TenantId& tenant_id) {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    if (!m_impl->tenants.count(tenant_id)) {
        spdlog::warn("remove_tenant: tenant not found: {}", tenant_id);
        return false;
    }

    m_impl->active_sessions.erase(tenant_id);
    m_impl->tenants.erase(tenant_id);
    m_active_tenants.fetch_sub(1, std::memory_order_relaxed);

    spdlog::info("Tenant removed: {}", tenant_id);
    return true;
}

std::shared_ptr<TenantManager::TenantSession>
TenantManager::get_tenant_session(const TenantId& tenant_id) {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    if (!m_impl->tenants.count(tenant_id)) {
        throw std::runtime_error("Tenant not found: " + tenant_id);
    }

    auto it = m_impl->active_sessions.find(tenant_id);
    if (it != m_impl->active_sessions.end()) {
        it->second->last_activity = std::chrono::steady_clock::now();
        it->second->request_count.fetch_add(1, std::memory_order_relaxed);
        return it->second;
    }

    auto session = std::make_shared<TenantSession>(tenant_id, m_database_manager);
    m_impl->active_sessions[tenant_id] = session;
    m_total_requests.fetch_add(1, std::memory_order_relaxed);

    spdlog::debug("New session created for tenant: {}", tenant_id);
    return session;
}

void TenantManager::release_tenant_session(std::shared_ptr<TenantSession> session) {
    if (session) {
        session->last_activity = std::chrono::steady_clock::now();
    }
}

std::vector<TenantManager::TenantId> TenantManager::get_tenant_list() const {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    std::vector<TenantId> list;
    list.reserve(m_impl->tenants.size());
    for (const auto& [id, _] : m_impl->tenants) {
        list.push_back(id);
    }
    return list;
}

TenantManager::TenantConfiguration
TenantManager::get_tenant_configuration(const TenantId& tenant_id) const {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    auto it = m_impl->tenants.find(tenant_id);
    if (it == m_impl->tenants.end()) {
        throw std::runtime_error("Tenant not found: " + tenant_id);
    }
    return it->second;
}

const TenantManager::Configuration& TenantManager::get_configuration() const noexcept {
    return m_config;
}

// ============================================================================
// Observability
// ============================================================================

TenantManager::String TenantManager::get_metrics() const {
    std::lock_guard<std::mutex> lk(m_tenants_mutex);

    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_impl->start_time).count();

    return "{\"active_tenants\":" + std::to_string(m_active_tenants.load()) +
           ",\"total_requests\":" + std::to_string(m_total_requests.load()) +
           ",\"uptime_seconds\":"  + std::to_string(uptime) +
           ",\"max_tenants\":"     + std::to_string(m_config.max_tenants) + "}";
}

TenantManager::String TenantManager::get_health() const {
    const bool healthy = (get_status() == Status::RUNNING);
    const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return "{\"status\":\"" + std::string(healthy ? "UP" : "DOWN") + "\""
           ",\"active_tenants\":" + std::to_string(m_active_tenants.load()) +
           ",\"timestamp\":" + std::to_string(ts) +
           ",\"version\":\"2.0.0\"}";
}

// ============================================================================
// Private helpers
// ============================================================================

bool TenantManager::initialize() {
    std::filesystem::create_directories(m_config.database_root_path);

    DatabaseManager::Config db_cfg;
    db_cfg.base_path           = m_config.database_root_path;
    db_cfg.connection_pool_size = 10;
    db_cfg.query_timeout        = std::chrono::milliseconds{5000};

    m_database_manager = std::make_shared<DatabaseManager>(std::move(db_cfg));

    spdlog::info("TenantManager initialised. database_root={}", m_config.database_root_path);
    return true;
}

} // namespace isched::v0_0_1::backend
