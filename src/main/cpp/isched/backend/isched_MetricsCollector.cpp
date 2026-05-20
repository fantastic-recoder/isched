/**
 * @file isched_MetricsCollector.cpp
 * @brief Implementation of MetricsCollector (T051).
 */
#include "isched_MetricsCollector.hpp"

#include <algorithm>
#include <cstdint>

namespace isched::v0_0_1::backend {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void MetricsCollector::maybe_reset_interval(TenantCounters& tc) {
    const auto now = std::chrono::steady_clock::now();
    const int64_t now_ns = now.time_since_epoch().count();
    const int64_t start_ns = tc.interval_start_ns.load(std::memory_order_relaxed);
    const int64_t duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::minutes{tc.interval_duration_m.load(std::memory_order_relaxed)}
    ).count();

    if (now_ns - start_ns >= duration_ns) {
        int64_t expected_start = start_ns;
        if (tc.interval_start_ns.compare_exchange_strong(expected_start, now_ns, std::memory_order_relaxed)) {
            tc.requests_in_interval.store(0, std::memory_order_relaxed);
            tc.errors_in_interval.store(0, std::memory_order_relaxed);
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MetricsCollector::record_request(const std::string& tenant_id,
                                      double duration_ms,
                                      bool is_error)
{
    TenantCounters* tc_ptr = nullptr;

    // 1. Try to find the tenant with a shared lock (fast path)
    {
        std::shared_lock lock(m_mutex);
        auto it = m_tenants.find(tenant_id);
        if (it != m_tenants.end()) {
            tc_ptr = &it->second;
        }
    }

    // 2. If not found, acquire exclusive lock to create it (slow path)
    if (!tc_ptr) {
        std::unique_lock lock(m_mutex);
        auto it = m_tenants.find(tenant_id);
        if (it != m_tenants.end()) {
            tc_ptr = &it->second;
        } else {
            tc_ptr = &m_tenants[tenant_id];
        }
    }

    // 3. Atomically check/reset interval (lock-free)
    maybe_reset_interval(*tc_ptr);

    // All increments and EMA update proceed lock-free.
    tc_ptr->total_requests.fetch_add(1, std::memory_order_relaxed);
    tc_ptr->requests_in_interval.fetch_add(1, std::memory_order_relaxed);
    if (is_error) {
        tc_ptr->total_errors.fetch_add(1, std::memory_order_relaxed);
        tc_ptr->errors_in_interval.fetch_add(1, std::memory_order_relaxed);
    }

    // Exponential moving average: new_avg = old * 0.9 + sample * 0.1
    double old_avg = tc_ptr->avg_response_ms.load(std::memory_order_relaxed);
    double new_avg{};
    do {
        new_avg = old_avg * 0.9 + duration_ms * 0.1;
    } while (!tc_ptr->avg_response_ms.compare_exchange_weak(
                 old_avg, new_avg,
                 std::memory_order_relaxed, std::memory_order_relaxed));
}

nlohmann::json MetricsCollector::get_server_metrics(uint64_t active_connections,
                                                    uint64_t active_subscriptions,
                                                    uint64_t tenant_count) const
{
    uint64_t requests_in_interval_sum{0};
    uint64_t errors_in_interval_sum{0};
    uint64_t total_requests_sum{0};
    uint64_t total_errors_sum{0};
    double   avg_ms_sum{0.0};
    std::size_t tenant_entry_count{0};

    {
        std::shared_lock lock(m_mutex);
        for (const auto& [key, tc] : m_tenants) {
            (void)key;
            requests_in_interval_sum += tc.requests_in_interval.load(std::memory_order_relaxed);
            errors_in_interval_sum   += tc.errors_in_interval.load(std::memory_order_relaxed);
            total_requests_sum       += tc.total_requests.load(std::memory_order_relaxed);
            total_errors_sum         += tc.total_errors.load(std::memory_order_relaxed);
            avg_ms_sum               += tc.avg_response_ms.load(std::memory_order_relaxed);
            ++tenant_entry_count;
        }
    }

    const double avg_response_ms =
        (tenant_entry_count > 0)
            ? (avg_ms_sum / static_cast<double>(tenant_entry_count))
            : 0.0;

    return nlohmann::json{
        {"requestsInInterval",       static_cast<int64_t>(requests_in_interval_sum)},
        {"errorsInInterval",         static_cast<int64_t>(errors_in_interval_sum)},
        {"totalRequestsSinceStartup",static_cast<int64_t>(total_requests_sum)},
        {"totalErrorsSinceStartup",  static_cast<int64_t>(total_errors_sum)},
        {"activeConnections",        static_cast<int64_t>(active_connections > 0 ? active_connections
                                        : m_active_connections.load(std::memory_order_relaxed))},
        {"activeSubscriptions",      static_cast<int64_t>(active_subscriptions > 0 ? active_subscriptions
                                        : m_active_subscriptions.load(std::memory_order_relaxed))},
        {"avgResponseTimeMs",        avg_response_ms},
        {"tenantCount",              static_cast<int64_t>(tenant_count > 0 ? tenant_count
                                        : static_cast<uint64_t>(tenant_entry_count))}
    };
}

nlohmann::json MetricsCollector::get_tenant_metrics(const std::string& org_id) const
{
    std::shared_lock lock(m_mutex);
    auto it = m_tenants.find(org_id);
    if (it == m_tenants.end()) {
        return nlohmann::json{
            {"organizationId",          org_id},
            {"requestsInInterval",      0},
            {"errorsInInterval",        0},
            {"totalRequestsSinceStartup", 0},
            {"totalErrorsSinceStartup", 0},
            {"avgResponseTimeMs",       0.0}
        };
    }
    const TenantCounters& tc = it->second;
    return nlohmann::json{
        {"organizationId",              org_id},
        {"requestsInInterval",          static_cast<int64_t>(tc.requests_in_interval.load(std::memory_order_relaxed))},
        {"errorsInInterval",            static_cast<int64_t>(tc.errors_in_interval.load(std::memory_order_relaxed))},
        {"totalRequestsSinceStartup",   static_cast<int64_t>(tc.total_requests.load(std::memory_order_relaxed))},
        {"totalErrorsSinceStartup",     static_cast<int64_t>(tc.total_errors.load(std::memory_order_relaxed))},
        {"avgResponseTimeMs",           tc.avg_response_ms.load(std::memory_order_relaxed)}
    };
}

void MetricsCollector::set_interval_minutes(const std::string& org_id, int minutes)
{
    const int clamped = std::max(1, minutes);
    TenantCounters* tc_ptr = nullptr;

    // 1. Try to find the tenant with a shared lock (fast path)
    {
        std::shared_lock lock(m_mutex);
        auto it = m_tenants.find(org_id);
        if (it != m_tenants.end()) {
            tc_ptr = &it->second;
        }
    }

    // 2. If not found, acquire exclusive lock to create it (slow path)
    if (!tc_ptr) {
        std::unique_lock lock(m_mutex);
        auto it = m_tenants.find(org_id);
        if (it != m_tenants.end()) {
            tc_ptr = &it->second;
        } else {
            tc_ptr = &m_tenants[org_id];
        }
    }

    tc_ptr->interval_duration_m.store(clamped, std::memory_order_relaxed);
}

uint64_t MetricsCollector::tenant_count() const
{
    std::shared_lock lock(m_mutex);
    return static_cast<uint64_t>(m_tenants.size());
}

} // namespace isched::v0_0_1::backend
