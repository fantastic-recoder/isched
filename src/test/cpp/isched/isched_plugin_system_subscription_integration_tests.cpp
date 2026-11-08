// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_plugin_system_subscription_integration_tests.cpp
 * @brief SP-012 lifecycle subscription integration checks (T076-T079).
 */

#include <catch2/catch_test_macros.hpp>

#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_PluginSystem.hpp>
#include <isched/backend/isched_SubscriptionBroker.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace isched::v0_0_1::backend;

namespace {

std::shared_ptr<DatabaseManager> make_test_db() {
    static std::atomic<int> counter{0};
    DatabaseManager::Config cfg;
    cfg.base_path = "/tmp/isched_plugin_sub_test_" + std::to_string(static_cast<unsigned long>(getpid())) +
                    "_" + std::to_string(counter.fetch_add(1));
    return std::make_shared<DatabaseManager>(cfg);
}

struct CapturedLifecycleEvent {
    nlohmann::json payload;
    std::chrono::steady_clock::time_point received_at;
};

class LifecycleHarness {
public:
    LifecycleHarness()
        : db(make_test_db()),
          manager(std::make_shared<PluginManager>(db)),
          broker(SubscriptionBroker::create()),
          executor(db) {
        REQUIRE(db->ensure_system_db());
        REQUIRE(db->initialize_plugin_store());
        REQUIRE(broker);

        executor.set_plugin_manager(manager);
        executor.set_subscription_broker(broker.get());
        manager->set_executor(&executor);

        admin.roles = {"role_platform_admin"};
        admin.tenant_id = "tenant-a";
        admin.db = db;

        non_admin.roles = {"role_tenant_admin"};
        non_admin.tenant_id = "tenant-a";
        non_admin.db = db;

        subscription_id = broker->subscribe("plugin-lifecycle-test", "__plugin_lifecycle",
            [this](const SubscriptionEvent& evt) {
                if (!evt.data.is_object() || !evt.data.contains("pluginLifecycleEvents") ||
                    !evt.data["pluginLifecycleEvents"].is_object()) {
                    return;
                }
                lifecycle_events.push_back({evt.data["pluginLifecycleEvents"], std::chrono::steady_clock::now()});
            });
        REQUIRE_FALSE(subscription_id.empty());
    }

    ~LifecycleHarness() {
        if (!subscription_id.empty()) {
            broker->unsubscribe(subscription_id);
        }
    }

    [[nodiscard]] ExecutionResult exec_admin(const std::string& query) const {
        return executor.execute(query, "{}", admin);
    }

    [[nodiscard]] ExecutionResult exec_non_admin(const std::string& query) const {
        return executor.execute(query, "{}", non_admin);
    }

    bool wait_for_events(std::size_t expected, std::chrono::milliseconds timeout = std::chrono::milliseconds(250)) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (lifecycle_events.size() >= expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return lifecycle_events.size() >= expected;
    }

    std::shared_ptr<DatabaseManager> db;
    std::shared_ptr<PluginManager> manager;
    SubscriptionBroker::UniquePtr broker;
    GqlExecutor executor;
    ResolverCtx admin;
    ResolverCtx non_admin;
    std::vector<CapturedLifecycleEvent> lifecycle_events;
    std::string subscription_id;
};

std::string load_plugin_and_get_id(LifecycleHarness& h, const std::string& plugin_path, const std::string& requested_id = {}) {
    const std::string plugin_id_part = requested_id.empty()
        ? std::string{}
        : (", pluginId:\"" + requested_id + "\"");
    const auto load = h.exec_admin(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path +
        "\", kind:\"resolver\"" + plugin_id_part +
        "}) { success pluginId errors } } }");
    REQUIRE(load.is_success());
    REQUIRE(load.data["isched"]["loadPlugin"]["success"].get<bool>());
    return load.data["isched"]["loadPlugin"]["pluginId"].get<std::string>();
}

void unload_plugin(LifecycleHarness& h, const std::string& plugin_id) {
    const auto unload = h.exec_admin(
        "mutation { isched { unloadPlugin(id:\"" + plugin_id + "\") { success errors } } }");
    REQUIRE(unload.is_success());
    REQUIRE(unload.data["isched"]["unloadPlugin"]["success"].get<bool>());
}

} // namespace

TEST_CASE("Lifecycle subscription emits exactly one event per successful transition", "[plugin][subscription][T076]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    LifecycleHarness h;

    const auto plugin_id = load_plugin_and_get_id(h, plugin_path);
    REQUIRE(h.wait_for_events(1));
    REQUIRE(h.lifecycle_events.size() == 1);
    CHECK(h.lifecycle_events[0].payload.value("eventType", "") == "LOADED");
    CHECK(h.lifecycle_events[0].payload.value("pluginId", "") == plugin_id);

    unload_plugin(h, plugin_id);
    REQUIRE(h.wait_for_events(2));
    REQUIRE(h.lifecycle_events.size() == 2);
    CHECK(h.lifecycle_events[1].payload.value("eventType", "") == "UNLOADED");
    CHECK(h.lifecycle_events[1].payload.value("pluginId", "") == plugin_id);
#endif
}

TEST_CASE("Lifecycle stream emits zero events on failed and non-transition paths; unauthorized subscribe is rejected", "[plugin][subscription][T077]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    LifecycleHarness h;

    const auto unauthorized_sub = h.exec_non_admin(
        "subscription { isched { pluginLifecycleEvents { eventType pluginId } } }");
    REQUIRE_FALSE(unauthorized_sub.is_success());
    REQUIRE_FALSE(unauthorized_sub.errors.empty());

    const auto fail_load = h.exec_admin(
        "mutation { isched { loadPlugin(input:{libraryPath:\"/tmp/missing-plugin.so\", kind:\"resolver\"}) { success errors } } }");
    REQUIRE(fail_load.is_success());
    CHECK_FALSE(fail_load.data["isched"]["loadPlugin"]["success"].get<bool>());
    CHECK(h.lifecycle_events.empty());

    const auto plugin_id = load_plugin_and_get_id(h, plugin_path);
    REQUIRE(h.wait_for_events(1));
    REQUIRE(h.lifecycle_events.size() == 1);

    const auto fail_unload = h.exec_admin(
        "mutation { isched { unloadPlugin(id:\"missing-plugin\") { success errors } } }");
    REQUIRE(fail_unload.is_success());
    CHECK_FALSE(fail_unload.data["isched"]["unloadPlugin"]["success"].get<bool>());
    CHECK(h.lifecycle_events.size() == 1);

    unload_plugin(h, plugin_id);
    REQUIRE(h.wait_for_events(2));
    REQUIRE(h.lifecycle_events.size() == 2);

    const auto idempotent_unload = h.exec_admin(
        "mutation { isched { unloadPlugin(id:\"" + plugin_id + "\") { success errors } } }");
    REQUIRE(idempotent_unload.is_success());
    CHECK(idempotent_unload.data["isched"]["unloadPlugin"]["success"].get<bool>());
    CHECK(h.lifecycle_events.size() == 2);
#endif
}

TEST_CASE("Lifecycle payload fields are populated and mapped to plugin record values", "[plugin][subscription][T078]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    LifecycleHarness h;

    const auto plugin_id = load_plugin_and_get_id(h, plugin_path);
    REQUIRE(h.wait_for_events(1));
    REQUIRE(h.lifecycle_events.size() == 1);

    const auto record = h.manager->get_plugin(plugin_id);
    REQUIRE(record.has_value());

    const auto& event = h.lifecycle_events[0].payload;
    CHECK(event.value("eventType", "") == "LOADED");
    CHECK(event.value("pluginId", "") == record->id);
    CHECK(event.value("pluginKind", "") == "RESOLVER");
    CHECK(event.value("pluginName", "") == record->name);
    CHECK(event.value("libraryPath", "") == record->libraryPath);
    CHECK_FALSE(event.value("occurredAt", "").empty());
#endif
}

TEST_CASE("Lifecycle delivery latency remains within SC-017 threshold", "[plugin][subscription][T079]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    LifecycleHarness h;

    constexpr std::size_t k_trials = 20;
    std::vector<long long> latency_ms;
    latency_ms.reserve(k_trials);

    for (std::size_t i = 0; i < k_trials; ++i) {
        const auto before_count = h.lifecycle_events.size();
        const auto plugin_id = load_plugin_and_get_id(h, plugin_path, "latency-plugin-" + std::to_string(i));

        const auto mutation_completed = std::chrono::steady_clock::now();
        REQUIRE(h.wait_for_events(before_count + 1));
        const auto received_at = h.lifecycle_events[before_count].received_at;
        const auto delta = std::max<long long>(0, std::chrono::duration_cast<std::chrono::milliseconds>(received_at - mutation_completed).count());
        latency_ms.push_back(delta);

        unload_plugin(h, plugin_id);
    }

    REQUIRE(latency_ms.size() == k_trials);
    std::sort(latency_ms.begin(), latency_ms.end());

    const std::size_t p95_index = (k_trials * 95 + 99) / 100 - 1;
    const auto p95 = latency_ms[std::min(p95_index, latency_ms.size() - 1)];
    const auto within_target = static_cast<std::size_t>(std::count_if(latency_ms.begin(), latency_ms.end(), [](long long value) {
        return value <= 2000;
    }));

    CHECK(p95 <= 2000);
    CHECK(within_target * 100 >= 95 * k_trials);
#endif
}

