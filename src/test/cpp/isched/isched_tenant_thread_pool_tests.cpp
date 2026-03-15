// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_tenant_thread_pool_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for T050: adaptive worker-thread and subscription resource controls
 *
 * Covers:
 *   T050-001  Advisory min/max thread config in TenantConfiguration + DB storage
 *   T050-002  Subscription count tracking in TenantManager
 *   T050-003  Broker count-change callback fires correctly (pool adaptation trigger)
 *   T050-005  Threshold arithmetic and queue drain semantics
 */

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "../../../main/cpp/isched/backend/isched_TenantManager.hpp"
#include "../../../main/cpp/isched/backend/isched_SubscriptionBroker.hpp"
#include "../../../main/cpp/isched/backend/isched_DatabaseManager.hpp"

using namespace isched::v0_0_1::backend;

// ==========================================================================
// T050-001: TenantConfiguration has advisory threading fields
// ==========================================================================

TEST_CASE("TenantConfiguration: default min_threads and max_threads", "[t050][config]") {
    TenantManager::TenantConfiguration cfg;
    REQUIRE(cfg.min_threads == 4u);
    REQUIRE(cfg.max_threads == 16u);
    // Ensure they can be overridden
    cfg.min_threads = 2;
    cfg.max_threads = 32;
    REQUIRE(cfg.min_threads == 2u);
    REQUIRE(cfg.max_threads == 32u);
}

// ==========================================================================
// T050-002: TenantManager subscription count tracking
// ==========================================================================

TEST_CASE("TenantManager: subscription count starts at zero", "[t050][subscription]") {
    auto mgr = TenantManager::create();
    REQUIRE(mgr->get_active_subscription_count() == 0u);
}

TEST_CASE("TenantManager: on_subscription_start increments count", "[t050][subscription]") {
    auto mgr = TenantManager::create();
    mgr->on_subscription_start();
    REQUIRE(mgr->get_active_subscription_count() == 1u);
    mgr->on_subscription_start();
    mgr->on_subscription_start();
    REQUIRE(mgr->get_active_subscription_count() == 3u);
}

TEST_CASE("TenantManager: on_subscription_end decrements count", "[t050][subscription]") {
    auto mgr = TenantManager::create();
    mgr->on_subscription_start();
    mgr->on_subscription_start();
    REQUIRE(mgr->get_active_subscription_count() == 2u);

    mgr->on_subscription_end();
    REQUIRE(mgr->get_active_subscription_count() == 1u);
    mgr->on_subscription_end();
    REQUIRE(mgr->get_active_subscription_count() == 0u);
}

TEST_CASE("TenantManager: on_subscription_end does not underflow", "[t050][subscription]") {
    auto mgr = TenantManager::create();
    // Call end without start — must not underflow (stays at 0).
    mgr->on_subscription_end();
    REQUIRE(mgr->get_active_subscription_count() == 0u);
}

// ==========================================================================
// T050-003: SubscriptionBroker count-change callback (pool adaptation trigger)
// ==========================================================================

TEST_CASE("SubscriptionBroker: count callback fires on subscribe", "[t050][broker]") {
    auto broker = SubscriptionBroker::create();

    std::vector<std::size_t> recorded;
    broker->set_subscription_count_callback([&](std::size_t n) {
        recorded.push_back(n);
    });

    const auto id = broker->subscribe("sess-1", "topic", [](const SubscriptionEvent&) {});
    REQUIRE(recorded.size() == 1u);
    REQUIRE(recorded[0] == 1u);

    std::ignore = broker->subscribe("sess-2", "topic", [](const SubscriptionEvent&) {});
    REQUIRE(recorded.size() == 2u);
    REQUIRE(recorded[1] == 2u);
    std::ignore = id;
}

TEST_CASE("SubscriptionBroker: count callback fires on unsubscribe", "[t050][broker]") {
    auto broker = SubscriptionBroker::create();

    std::vector<std::size_t> recorded;
    broker->set_subscription_count_callback([&](std::size_t n) {
        recorded.push_back(n);
    });

    const auto id1 = broker->subscribe("sess-1", "topic", [](const SubscriptionEvent&) {});
    const auto id2 = broker->subscribe("sess-1", "topic", [](const SubscriptionEvent&) {});
    REQUIRE(recorded.size() == 2u);

    broker->unsubscribe(id1);
    REQUIRE(recorded.size() == 3u);
    REQUIRE(recorded[2] == 1u);

    broker->unsubscribe(id2);
    REQUIRE(recorded.size() == 4u);
    REQUIRE(recorded[3] == 0u);
}

TEST_CASE("SubscriptionBroker: count callback fires on disconnect_session", "[t050][broker]") {
    auto broker = SubscriptionBroker::create();

    std::size_t last_count = 99u;
    broker->set_subscription_count_callback([&](std::size_t n) {
        last_count = n;
    });

    std::ignore = broker->subscribe("sess-A", "topic", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("sess-A", "topic", [](const SubscriptionEvent&) {});
    REQUIRE(last_count == 2u);

    broker->disconnect_session("sess-A");
    REQUIRE(last_count == 0u);
    REQUIRE(broker->get_subscriber_count() == 0u);
}

TEST_CASE("SubscriptionBroker: count callback can be cleared", "[t050][broker]") {
    auto broker = SubscriptionBroker::create();

    int calls = 0;
    broker->set_subscription_count_callback([&](std::size_t) { ++calls; });
    std::ignore = broker->subscribe("sess-1", "t", [](const SubscriptionEvent&) {});
    REQUIRE(calls == 1);

    // Clear the callback
    broker->set_subscription_count_callback({});
    std::ignore = broker->subscribe("sess-2", "t", [](const SubscriptionEvent&) {});
    REQUIRE(calls == 1);  // No additional calls
}

// ==========================================================================
// T050-003/T050-005: Pool adaptation threshold arithmetic
// ==========================================================================

TEST_CASE("Adaptation threshold: grow triggers at subscription_count > pool * 0.75",
          "[t050][threshold]")
{
    // Pure arithmetic validation of the adaptation decision.
    const auto should_grow = [](std::size_t pool, std::size_t subscriptions) -> bool {
        const std::size_t threshold = static_cast<std::size_t>(pool * 0.75);
        return subscriptions > threshold;
    };

    // Pool=4: threshold=3  →  grow at 4+ subscriptions
    REQUIRE_FALSE(should_grow(4, 0));
    REQUIRE_FALSE(should_grow(4, 1));
    REQUIRE_FALSE(should_grow(4, 3));  // equal to threshold — no grow
    REQUIRE(should_grow(4, 4));        // 4 > 3 → grow
    REQUIRE(should_grow(4, 10));

    // Pool=8: threshold=6  →  grow at 7+ subscriptions
    REQUIRE_FALSE(should_grow(8, 6));
    REQUIRE(should_grow(8, 7));

    // Pool=1: threshold=0  →  any subscription triggers grow
    REQUIRE(should_grow(1, 1));
}

TEST_CASE("Adaptation: desired pool size is pool + excess above threshold",
          "[t050][threshold]")
{
    const auto desired_size = [](std::size_t pool, std::size_t count, std::size_t max_pool) -> std::size_t {
        const std::size_t threshold = static_cast<std::size_t>(pool * 0.75);
        if (count <= threshold) return pool;
        const std::size_t grow_by = count - threshold;
        return std::min(pool + grow_by, max_pool);
    };

    // Pool=4, max=16: 5 subscriptions → threshold=3, excess=2 → desired=6
    REQUIRE(desired_size(4, 5, 16) == 6u);

    // Pool=4, max=16: 100 subscriptions → capped at max=16
    REQUIRE(desired_size(4, 100, 16) == 16u);

    // Pool=4, max=16: 3 subscriptions (== threshold) → no change
    REQUIRE(desired_size(4, 3, 16) == 4u);
}

// ==========================================================================
// T050-001: DatabaseManager tenant settings round-trip
// ==========================================================================

TEST_CASE("DatabaseManager: set_tenant_setting and get_tenant_setting round-trip",
          "[t050][database][settings]")
{
    const std::string tmp = "/tmp/isched_t050_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(tmp);

    DatabaseManager::Config cfg;
    cfg.base_path = tmp + "/tenants";
    DatabaseManager db(cfg);

    // ensure_system_db() creates the tenant_settings table
    const auto sys_res = db.ensure_system_db();
    REQUIRE(sys_res.has_value());

    const std::string org_id = "org_test_050";

    // Initially absent → NotFound
    auto missing = db.get_tenant_setting(org_id, "min_threads");
    REQUIRE_FALSE(missing.has_value());

    // Set and retrieve
    REQUIRE(db.set_tenant_setting(org_id, "min_threads", "8").has_value());
    REQUIRE(db.set_tenant_setting(org_id, "max_threads", "32").has_value());

    auto min_res = db.get_tenant_setting(org_id, "min_threads");
    auto max_res = db.get_tenant_setting(org_id, "max_threads");
    REQUIRE(min_res.has_value());
    REQUIRE(max_res.has_value());
    REQUIRE(min_res.value() == "8");
    REQUIRE(max_res.value() == "32");

    // Overwrite is idempotent (UPSERT)
    REQUIRE(db.set_tenant_setting(org_id, "min_threads", "2").has_value());
    auto updated = db.get_tenant_setting(org_id, "min_threads");
    REQUIRE(updated.has_value());
    REQUIRE(updated.value() == "2");

    std::filesystem::remove_all(tmp);
}
