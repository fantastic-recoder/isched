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

/// Auth-session → WebSocket-session connection record (T049-007).
struct AuthSessionRecord {
    std::string ws_session_id;
    std::function<void()> close_callback;
};

struct SubscriptionBroker::Impl {
    mutable std::shared_mutex mutex;

    // Primary index: subscription_id → record
    std::unordered_map<std::string, SubscriptionRecord> subscriptions;

    // Secondary index: session_id → set of subscription_ids (for fast disconnect)
    std::unordered_map<std::string, std::vector<std::string>> session_index;

    // Auth-session tracking (T049-007):
    //   auth_session_id → AuthSessionRecord
    std::unordered_map<std::string, AuthSessionRecord> auth_sessions;
    //   ws_session_id → auth_session_id  (reverse lookup for cleanup)
    std::unordered_map<std::string, std::string> ws_to_auth;
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

// ---------------------------------------------------------------------------
// Auth-session tracking (T049-007)
// ---------------------------------------------------------------------------

void SubscriptionBroker::register_auth_session(const std::string& auth_session_id,
                                                const std::string& ws_session_id,
                                                std::function<void()> close_callback) {
    std::unique_lock lock(m_impl->mutex);
    // Remove any previous registration for this ws_session_id first.
    auto rev_it = m_impl->ws_to_auth.find(ws_session_id);
    if (rev_it != m_impl->ws_to_auth.end()) {
        m_impl->auth_sessions.erase(rev_it->second);
        m_impl->ws_to_auth.erase(rev_it);
    }
    m_impl->auth_sessions[auth_session_id] = {ws_session_id, std::move(close_callback)};
    m_impl->ws_to_auth[ws_session_id]      = auth_session_id;
    spdlog::debug("SubscriptionBroker: registered auth session {} for WS session {}",
                  auth_session_id, ws_session_id);
}

void SubscriptionBroker::unregister_auth_session(const std::string& ws_session_id) {
    std::unique_lock lock(m_impl->mutex);
    auto rev_it = m_impl->ws_to_auth.find(ws_session_id);
    if (rev_it == m_impl->ws_to_auth.end()) return;
    m_impl->auth_sessions.erase(rev_it->second);
    m_impl->ws_to_auth.erase(rev_it);
}

void SubscriptionBroker::revoke_auth_session(const std::string& auth_session_id) {
    // Extract the record under the lock, then call the close callback outside.
    std::string ws_session_id;
    std::function<void()> close_cb;
    {
        std::unique_lock lock(m_impl->mutex);
        auto it = m_impl->auth_sessions.find(auth_session_id);
        if (it == m_impl->auth_sessions.end()) return;
        ws_session_id = it->second.ws_session_id;
        close_cb      = std::move(it->second.close_callback);
        m_impl->auth_sessions.erase(it);
        m_impl->ws_to_auth.erase(ws_session_id);
    }
    // Disconnect all subscriptions for this WS session.
    disconnect_session(ws_session_id);
    // Fire the close callback (sends connection_terminate and closes socket).
    if (close_cb) {
        try {
            close_cb();
        } catch (const std::exception& e) {
            spdlog::error("SubscriptionBroker: close callback threw for auth session {}: {}",
                          auth_session_id, e.what());
        } catch (...) {
            spdlog::error("SubscriptionBroker: close callback threw unknown exception for auth session {}",
                          auth_session_id);
        }
    }
    spdlog::info("SubscriptionBroker: revoked auth session {} (WS {})",
                 auth_session_id, ws_session_id);
}

} // namespace isched::v0_0_1::backend
