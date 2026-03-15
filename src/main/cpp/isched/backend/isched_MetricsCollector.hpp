/**
 * @file isched_MetricsCollector.hpp
 * @brief Performance-metrics collection for server- and tenant-level monitoring (T051).
 *
 * MetricsCollector tracks request and error counts per tenant within a sliding
 * interval window, cumulative totals since startup, and a rolling exponential
 * moving average of response latency.
 *
 * Design:
 * - Per-tenant counters stored in a hash-map guarded by a shared_mutex.
 * - Server-level aggregates are derived on-the-fly from the tenant map.
 * - `record_request()` is safe to call concurrently from many HTTP threads.
 * - Interval resets happen when the configured interval boundary is crossed.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace isched::v0_0_1::backend {

/**
 * @brief Collects per-tenant and server-wide request metrics (T051-001).
 */
class MetricsCollector {
public:
    /**
     * @brief Per-tenant counters.
     *
     * Individual counters use relaxed atomics for lock-free increments.
     * The `maybe_reset_interval` path is serialised via the collector mutex.
     */
    struct TenantCounters {
        // Interval-window accumulators (reset at each interval boundary)
        std::atomic<uint64_t> requests_in_interval{0};
        std::atomic<uint64_t> errors_in_interval{0};
        // Cumulative totals since server startup
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> total_errors{0};
        // Exponential moving average of response times (ms): α = 0.1
        std::atomic<double>   avg_response_ms{0.0};
        // Interval boundary tracking
        std::chrono::steady_clock::time_point interval_start{std::chrono::steady_clock::now()};
        std::chrono::minutes interval_duration{60};
    };

    MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&)            = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    /**
     * @brief Record a completed request for a tenant.
     *
     * @param tenant_id     Organisation identifier (empty = anonymous / system).
     * @param duration_ms   Request latency in milliseconds.
     * @param is_error       True if the request produced a GraphQL error response.
     */
    void record_request(const std::string& tenant_id,
                        double duration_ms,
                        bool is_error);

    /**
     * @brief Return a JSON object matching the `ServerMetrics` GraphQL type (T051-001).
     *
     * @param active_connections    Current open HTTP connections.
     * @param active_subscriptions  Current live WebSocket subscriptions.
     * @param tenant_count          Number of active tenants.
     */
    nlohmann::json get_server_metrics(uint64_t active_connections,
                                      uint64_t active_subscriptions,
                                      uint64_t tenant_count) const;

    /**
     * @brief Return a JSON object matching the `TenantMetrics` GraphQL type (T051-002).
     *
     * Returns all-zero metrics if @p org_id has never been seen before.
     */
    nlohmann::json get_tenant_metrics(const std::string& org_id) const;

    /**
     * @brief Number of distinct tenants that have recorded at least one request.
     *
     * Used to populate the `tenantCount` field of `ServerMetrics`.
     */
    [[nodiscard]] uint64_t tenant_count() const;

    /**
     * @brief Update the live active-connection counter (called by Server on each request).
     */
    void set_active_connections(uint64_t n) {
        m_active_connections.store(n, std::memory_order_relaxed);
    }

    /**
     * @brief Update the live active-subscription counter (called by Server via broker callback).
     */
    void set_active_subscriptions(uint64_t n) {
        m_active_subscriptions.store(n, std::memory_order_relaxed);
    }

    /**
     * @brief Update the interval-reset duration for a specific tenant (T051-003).
     *
     * @param org_id  Organisation identifier.
     * @param minutes New interval length in minutes (clamped to ≥ 1).
     */
    void set_interval_minutes(const std::string& org_id, int minutes);

private:
    // Returns (or lazily creates) the counters entry for a tenant.
    // Caller must hold at least a shared lock before calling; this method
    // upgrades to exclusive when the entry must be created.
    TenantCounters& get_or_create(const std::string& tenant_id);

    // Atomically reset interval counters if the boundary has passed.
    void maybe_reset_interval(TenantCounters& tc);

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, TenantCounters> m_tenants;

    std::atomic<uint64_t> m_active_connections{0};
    std::atomic<uint64_t> m_active_subscriptions{0};

    const std::chrono::steady_clock::time_point m_server_start{
        std::chrono::steady_clock::now()};
};

} // namespace isched::v0_0_1::backend
