// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_subscription_broker_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for SubscriptionBroker.
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <tuple>
#include <vector>

#include "../../../main/cpp/isched/backend/isched_SubscriptionBroker.hpp"

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

TEST_CASE("SubscriptionBroker: create returns non-null", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();
    REQUIRE(broker != nullptr);
    REQUIRE(broker->get_subscriber_count() == 0);
}

TEST_CASE("SubscriptionBroker: subscribe returns unique ids", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    const auto id1 = broker->subscribe("sess-1", "topic-a", [](const SubscriptionEvent&) {});
    const auto id2 = broker->subscribe("sess-1", "topic-b", [](const SubscriptionEvent&) {});
    const auto id3 = broker->subscribe("sess-2", "topic-a", [](const SubscriptionEvent&) {});

    REQUIRE(id1 != id2);
    REQUIRE(id1 != id3);
    REQUIRE(id2 != id3);
    REQUIRE(broker->get_subscriber_count() == 3);
}

TEST_CASE("SubscriptionBroker: publish delivers to matching topic only", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    int hit_a = 0;
    int hit_b = 0;
    std::ignore = broker->subscribe("sess-1", "topic-a", [&](const SubscriptionEvent& e) {
        REQUIRE(e.topic == "topic-a");
        REQUIRE(e.type == "testEvent");
        ++hit_a;
    });
    std::ignore = broker->subscribe("sess-2", "topic-b", [&](const SubscriptionEvent&) {
        ++hit_b;
    });

    broker->publish("topic-a", "testEvent", json{{"key", "value"}});

    REQUIRE(hit_a == 1);
    REQUIRE(hit_b == 0);
}

TEST_CASE("SubscriptionBroker: publish delivers to all subscribers of the same topic", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    int calls = 0;
    std::ignore = broker->subscribe("sess-1", "shared", [&](const SubscriptionEvent&) { ++calls; });
    std::ignore = broker->subscribe("sess-2", "shared", [&](const SubscriptionEvent&) { ++calls; });
    std::ignore = broker->subscribe("sess-3", "shared", [&](const SubscriptionEvent&) { ++calls; });

    broker->publish("shared", "evt", json{});

    REQUIRE(calls == 3);
}

TEST_CASE("SubscriptionBroker: unsubscribe stops delivery", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    int calls = 0;
    const auto sub_id = broker->subscribe("sess-1", "topic", [&](const SubscriptionEvent&) {
        ++calls;
    });

    broker->publish("topic", "evt", json{});
    REQUIRE(calls == 1);

    broker->unsubscribe(sub_id);
    REQUIRE(broker->get_subscriber_count() == 0);

    broker->publish("topic", "evt", json{});
    REQUIRE(calls == 1); // no additional delivery
}

TEST_CASE("SubscriptionBroker: unsubscribe unknown id is a no-op", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();
    REQUIRE_NOTHROW(broker->unsubscribe("does-not-exist"));
}

TEST_CASE("SubscriptionBroker: disconnect_session removes all session subscriptions", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    std::ignore = broker->subscribe("sess-A", "topic-1", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("sess-A", "topic-2", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("sess-B", "topic-1", [](const SubscriptionEvent&) {});

    REQUIRE(broker->get_subscriber_count() == 3);
    REQUIRE(broker->get_subscriber_count("topic-1") == 2);

    broker->disconnect_session("sess-A");

    REQUIRE(broker->get_subscriber_count() == 1);
    REQUIRE(broker->get_subscriber_count("topic-1") == 1);
    REQUIRE(broker->get_subscriber_count("topic-2") == 0);
}

TEST_CASE("SubscriptionBroker: disconnect_session for unknown session is a no-op", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();
    REQUIRE_NOTHROW(broker->disconnect_session("unknown-session"));
    REQUIRE(broker->get_subscriber_count() == 0);
}

TEST_CASE("SubscriptionBroker: get_subscriber_count filters by topic", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    std::ignore = broker->subscribe("s1", "alpha", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("s2", "alpha", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("s3", "beta",  [](const SubscriptionEvent&) {});

    REQUIRE(broker->get_subscriber_count()        == 3);
    REQUIRE(broker->get_subscriber_count("alpha") == 2);
    REQUIRE(broker->get_subscriber_count("beta")  == 1);
    REQUIRE(broker->get_subscriber_count("gamma") == 0);
}

TEST_CASE("SubscriptionBroker: event data is forwarded correctly", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    json received_data;
    std::string received_type;
    std::string received_topic;
    std::ignore = broker->subscribe("s", "data-topic", [&](const SubscriptionEvent& e) {
        received_data  = e.data;
        received_type  = e.type;
        received_topic = e.topic;
    });

    const json payload = {{"id", "abc"}, {"version", 42}};
    broker->publish("data-topic", "configActivated", payload);

    REQUIRE(received_type  == "configActivated");
    REQUIRE(received_topic == "data-topic");
    REQUIRE(received_data["id"]      == "abc");
    REQUIRE(received_data["version"] == 42);
}

TEST_CASE("SubscriptionBroker: throwing handler does not crash broker", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    std::ignore = broker->subscribe("s1", "t", [](const SubscriptionEvent&) {
        throw std::runtime_error("handler error");
    });
    int second_called = 0;
    std::ignore = broker->subscribe("s2", "t", [&](const SubscriptionEvent&) {
        ++second_called;
    });

    REQUIRE_NOTHROW(broker->publish("t", "evt", json{}));
    // Second handler should still have been reached (order not guaranteed
    // but both are collected before any handler is called)
    REQUIRE(second_called == 1);
}

TEST_CASE("SubscriptionBroker: publish parity holds after middle-record unsubscribe", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    int calls_1 = 0;
    int calls_2 = 0;
    int calls_3 = 0;
    int calls_4 = 0;

    const auto id_1 = broker->subscribe("sess-1", "shared", [&](const SubscriptionEvent&) { ++calls_1; });
    const auto id_2 = broker->subscribe("sess-2", "shared", [&](const SubscriptionEvent&) { ++calls_2; });
    const auto id_3 = broker->subscribe("sess-3", "shared", [&](const SubscriptionEvent&) { ++calls_3; });
    const auto id_4 = broker->subscribe("sess-4", "shared", [&](const SubscriptionEvent&) { ++calls_4; });
    REQUIRE(id_1 != id_2);
    REQUIRE(id_2 != id_3);
    REQUIRE(id_3 != id_4);

    broker->unsubscribe(id_2);
    REQUIRE(broker->get_subscriber_count("shared") == 3);

    broker->publish("shared", "evt", json{});
    REQUIRE(calls_1 == 1);
    REQUIRE(calls_2 == 0);
    REQUIRE(calls_3 == 1);
    REQUIRE(calls_4 == 1);
}

TEST_CASE("SubscriptionBroker: invalid-id and unknown-session guards remain safe after churn", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    int survivor_hits = 0;
    const auto stale_id_1 = broker->subscribe("sess-A", "alpha", [](const SubscriptionEvent&) {});
    const auto stale_id_2 = broker->subscribe("sess-A", "beta", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("sess-B", "beta", [&](const SubscriptionEvent&) { ++survivor_hits; });

    broker->disconnect_session("sess-A");
    REQUIRE(broker->get_subscriber_count() == 1);
    REQUIRE(broker->get_subscriber_count("alpha") == 0);
    REQUIRE(broker->get_subscriber_count("beta") == 1);

    REQUIRE_NOTHROW(broker->unsubscribe(stale_id_1));
    REQUIRE_NOTHROW(broker->unsubscribe(stale_id_2));
    REQUIRE_NOTHROW(broker->unsubscribe("non-existent-id"));
    REQUIRE_NOTHROW(broker->disconnect_session("sess-A"));
    REQUIRE_NOTHROW(broker->disconnect_session("missing-session"));

    broker->publish("beta", "evt", json{});
    REQUIRE(survivor_hits == 1);
}

TEST_CASE("SubscriptionBroker: count callback tracks subscribe/unsubscribe/disconnect", "[subscription][broker]") {
    auto broker = SubscriptionBroker::create();

    std::vector<std::size_t> observed_counts;
    broker->set_subscription_count_callback([&](std::size_t count) {
        observed_counts.push_back(count);
    });

    const auto id1 = broker->subscribe("sess-1", "topic-1", [](const SubscriptionEvent&) {});
    const auto id2 = broker->subscribe("sess-1", "topic-2", [](const SubscriptionEvent&) {});
    const auto id3 = broker->subscribe("sess-2", "topic-2", [](const SubscriptionEvent&) {});
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);

    broker->unsubscribe(id1);
    broker->disconnect_session("sess-1");

    REQUIRE(observed_counts == std::vector<std::size_t>{1, 2, 3, 2, 1});
    REQUIRE(broker->get_subscriber_count() == 1);

    broker->set_subscription_count_callback({});
    broker->unsubscribe(id3);
    REQUIRE(observed_counts == std::vector<std::size_t>{1, 2, 3, 2, 1});
    REQUIRE(broker->get_subscriber_count() == 0);
}

TEST_CASE("SubscriptionBroker: revoke_auth_session disconnects mapped websocket subscriptions", "[subscription][broker][auth]") {
    auto broker = SubscriptionBroker::create();

    int close_calls = 0;
    std::ignore = broker->subscribe("ws-1", "topic", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("ws-1", "topic", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("ws-2", "topic", [](const SubscriptionEvent&) {});
    REQUIRE(broker->get_subscriber_count() == 3);

    broker->register_auth_session("auth-1", "ws-1", [&]() { ++close_calls; });
    broker->revoke_auth_session("auth-1");

    REQUIRE(close_calls == 1);
    REQUIRE(broker->get_subscriber_count() == 1);

    // Revoke twice must be a no-op after removal.
    REQUIRE_NOTHROW(broker->revoke_auth_session("auth-1"));
    REQUIRE(close_calls == 1);
}

TEST_CASE("SubscriptionBroker: auth session registration replacement and unregister are safe", "[subscription][broker][auth]") {
    auto broker = SubscriptionBroker::create();

    int close_a = 0;
    int close_b = 0;
    broker->register_auth_session("auth-a", "ws-77", [&]() { ++close_a; });
    broker->register_auth_session("auth-b", "ws-77", [&]() { ++close_b; });

    // Replacement keeps only the latest registration for ws-77.
    broker->revoke_auth_session("auth-a");
    REQUIRE(close_a == 0);
    broker->revoke_auth_session("auth-b");
    REQUIRE(close_b == 1);

    // Unregister on missing mapping is a no-op.
    REQUIRE_NOTHROW(broker->unregister_auth_session("ws-77"));
    REQUIRE_NOTHROW(broker->unregister_auth_session("ws-missing"));
}

TEST_CASE("SubscriptionBroker: revoke_auth_session tolerates throwing close callbacks", "[subscription][broker][auth]") {
    auto broker = SubscriptionBroker::create();

    int survivor_hits = 0;
    std::ignore = broker->subscribe("ws-throw", "topic", [](const SubscriptionEvent&) {});
    std::ignore = broker->subscribe("ws-survivor", "topic", [&](const SubscriptionEvent&) { ++survivor_hits; });

    broker->register_auth_session("auth-throw", "ws-throw", []() {
        throw std::runtime_error("close failed");
    });

    REQUIRE_NOTHROW(broker->revoke_auth_session("auth-throw"));
    REQUIRE(broker->get_subscriber_count() == 1);

    broker->publish("topic", "evt", json{});
    REQUIRE(survivor_hits == 1);
}

TEST_CASE("SubscriptionBroker: unregister_auth_session removes existing reverse mapping", "[subscription][broker][auth]") {
    auto broker = SubscriptionBroker::create();

    int close_calls = 0;
    broker->register_auth_session("auth-unreg", "ws-unreg", [&]() { ++close_calls; });

    broker->unregister_auth_session("ws-unreg");
    REQUIRE_NOTHROW(broker->revoke_auth_session("auth-unreg"));
    REQUIRE(close_calls == 0);
}

TEST_CASE("SubscriptionBroker: revoke_auth_session catches non-std callback exceptions", "[subscription][broker][auth]") {
    auto broker = SubscriptionBroker::create();

    std::ignore = broker->subscribe("ws-unknown-throw", "topic", [](const SubscriptionEvent&) {});
    broker->register_auth_session("auth-unknown-throw", "ws-unknown-throw", []() {
        throw 42;
    });

    REQUIRE_NOTHROW(broker->revoke_auth_session("auth-unknown-throw"));
    REQUIRE(broker->get_subscriber_count() == 0);
}

