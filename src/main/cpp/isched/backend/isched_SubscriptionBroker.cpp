// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_SubscriptionBroker.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Thread-safe fan-out broker for GraphQL subscription events.
 */

#include "isched_SubscriptionBroker.hpp"

#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

namespace isched::v0_0_1::backend {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

namespace {
/** Monotonically-increasing counter for generating subscription IDs. */
std::atomic<std::uint64_t> g_sub_id_counter{0};

std::string make_subscription_id() {
    return "sub-" + std::to_string(++g_sub_id_counter);
}
} // anonymous namespace

struct SubscriptionRecord {
    std::string subscription_id;
    std::string session_id;
    std::string topic;
    SubscriptionHandler handler;
};

struct SubscriptionBroker::Impl {
    mutable std::shared_mutex mutex;

    // Primary index: subscription_id → record
    std::unordered_map<std::string, SubscriptionRecord> subscriptions;

    // Secondary index: session_id → set of subscription_ids (for fast disconnect)
    std::unordered_map<std::string, std::vector<std::string>> session_index;
};

// ---------------------------------------------------------------------------
// Factory + constructor + destructor
// ---------------------------------------------------------------------------

SubscriptionBroker::UniquePtr SubscriptionBroker::create() {
    return std::unique_ptr<SubscriptionBroker>(new SubscriptionBroker{});
}

SubscriptionBroker::SubscriptionBroker()
    : m_impl(std::make_unique<Impl>()) {}

SubscriptionBroker::~SubscriptionBroker() = default;

// ---------------------------------------------------------------------------
// subscribe / unsubscribe / disconnect_session
// ---------------------------------------------------------------------------

std::string SubscriptionBroker::subscribe(const std::string& session_id,
                                           const std::string& topic,
                                           SubscriptionHandler handler) {
    const std::string sub_id = make_subscription_id();

    std::unique_lock lock(m_impl->mutex);
    m_impl->subscriptions.emplace(sub_id,
        SubscriptionRecord{sub_id, session_id, topic, std::move(handler)});
    m_impl->session_index[session_id].push_back(sub_id);

    spdlog::debug("SubscriptionBroker: registered {} for session {} on topic '{}'",
                  sub_id, session_id, topic);
    return sub_id;
}

void SubscriptionBroker::unsubscribe(const std::string& subscription_id) {
    std::unique_lock lock(m_impl->mutex);

    auto it = m_impl->subscriptions.find(subscription_id);
    if (it == m_impl->subscriptions.end()) {
        return;
    }

    const std::string session_id = it->second.session_id;
    m_impl->subscriptions.erase(it);

    // Remove from session index
    auto sess_it = m_impl->session_index.find(session_id);
    if (sess_it != m_impl->session_index.end()) {
        auto& ids = sess_it->second;
        ids.erase(std::remove(ids.begin(), ids.end(), subscription_id), ids.end());
        if (ids.empty()) {
            m_impl->session_index.erase(sess_it);
        }
    }

    spdlog::debug("SubscriptionBroker: removed {}", subscription_id);
}

void SubscriptionBroker::disconnect_session(const std::string& session_id) {
    std::unique_lock lock(m_impl->mutex);

    auto sess_it = m_impl->session_index.find(session_id);
    if (sess_it == m_impl->session_index.end()) {
        return;
    }

    for (const auto& sub_id : sess_it->second) {
        m_impl->subscriptions.erase(sub_id);
    }
    m_impl->session_index.erase(sess_it);

    spdlog::debug("SubscriptionBroker: disconnected session {}", session_id);
}

// ---------------------------------------------------------------------------
// publish
// ---------------------------------------------------------------------------

void SubscriptionBroker::publish(const std::string& topic,
                                  const std::string& type,
                                  const nlohmann::json& data) {
    // Collect matching handlers under a shared lock, then call them outside
    // the lock to avoid holding the mutex while executing user callbacks.
    std::vector<SubscriptionHandler> handlers;
    {
        std::shared_lock lock(m_impl->mutex);
        for (const auto& [sub_id, record] : m_impl->subscriptions) {
            if (record.topic == topic) {
                handlers.push_back(record.handler);
            }
        }
    }

    const SubscriptionEvent event{type, topic, data};
    for (const auto& h : handlers) {
        try {
            h(event);
        } catch (const std::exception& e) {
            spdlog::error("SubscriptionBroker: handler threw for topic '{}': {}",
                          topic, e.what());
        } catch (...) {
            spdlog::error("SubscriptionBroker: handler threw unknown exception for topic '{}'",
                          topic);
        }
    }
}

// ---------------------------------------------------------------------------
// get_subscriber_count
// ---------------------------------------------------------------------------

std::size_t SubscriptionBroker::get_subscriber_count(const std::string& topic) const {
    std::shared_lock lock(m_impl->mutex);

    if (topic.empty()) {
        return m_impl->subscriptions.size();
    }

    std::size_t count = 0;
    for (const auto& [sub_id, record] : m_impl->subscriptions) {
        if (record.topic == topic) {
            ++count;
        }
    }
    return count;
}

} // namespace isched::v0_0_1::backend
