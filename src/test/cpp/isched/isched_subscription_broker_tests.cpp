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
