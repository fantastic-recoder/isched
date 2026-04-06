// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_SubscriptionBroker.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Thread-safe fan-out broker for GraphQL subscription events.
 */

#include "isched_SubscriptionBroker.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
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

    // Primary storage: contiguous subscription records.
    std::vector<SubscriptionRecord> subscriptions;

    // Primary lookup: subscription_id -> index into subscriptions.
    std::unordered_map<std::string, SubscriptionBroker::SubscriptionIndex> subscription_id_index;

    // Secondary indices: session/topic -> record indices.
    std::unordered_map<std::string, std::vector<SubscriptionBroker::SubscriptionIndex>> session_index;
    std::unordered_map<std::string, std::vector<SubscriptionBroker::SubscriptionIndex>> topic_index;

    // Auth-session tracking (T049-007):
    //   auth_session_id → AuthSessionRecord
    std::unordered_map<std::string, AuthSessionRecord> auth_sessions;
    //   ws_session_id → auth_session_id  (reverse lookup for cleanup)
    std::unordered_map<std::string, std::string> ws_to_auth;

    // T050-002: count-change callback (set once before listen(), then read-only)
    mutable std::mutex count_cb_mutex;
    std::function<void(std::size_t)> count_change_cb;

    [[nodiscard]] bool isValidIndex(SubscriptionBroker::SubscriptionIndex index) const noexcept {
        return index < subscriptions.size();
    }

    [[nodiscard]] const SubscriptionRecord* getRecordByIndex(SubscriptionBroker::SubscriptionIndex index) const noexcept {
        if (SubscriptionBroker::isInvalidSubscriptionIndex(index) || !isValidIndex(index)) {
            return nullptr;
        }
        return &subscriptions[index];
    }

    static void removeIndex(std::vector<SubscriptionBroker::SubscriptionIndex>& indices,
                            SubscriptionBroker::SubscriptionIndex index) {
        indices.erase(std::remove(indices.begin(), indices.end(), index), indices.end());
    }

    static void remapIndex(std::vector<SubscriptionBroker::SubscriptionIndex>& indices,
                           SubscriptionBroker::SubscriptionIndex from,
                           SubscriptionBroker::SubscriptionIndex to) {
        for (auto& existing : indices) {
            if (existing == from) {
                existing = to;
                return;
            }
        }
    }

    void removeFromSessionIndex(const std::string& session_id,
                                SubscriptionBroker::SubscriptionIndex index) {
        const auto it = session_index.find(session_id);
        if (it == session_index.end()) {
            return;
        }

        removeIndex(it->second, index);
        if (it->second.empty()) {
            session_index.erase(it);
        }
    }

    void removeFromTopicIndex(const std::string& topic,
                              SubscriptionBroker::SubscriptionIndex index) {
        const auto it = topic_index.find(topic);
        if (it == topic_index.end()) {
            return;
        }

        removeIndex(it->second, index);
        if (it->second.empty()) {
            topic_index.erase(it);
        }
    }

    [[nodiscard]] bool eraseSubscriptionAt(SubscriptionBroker::SubscriptionIndex index) {
        if (SubscriptionBroker::isInvalidSubscriptionIndex(index) || !isValidIndex(index)) {
            return false;
        }

        const auto removed_id = subscriptions[index].subscription_id;
        const auto removed_session_id = subscriptions[index].session_id;
        const auto removed_topic = subscriptions[index].topic;

        removeFromSessionIndex(removed_session_id, index);
        removeFromTopicIndex(removed_topic, index);

        const SubscriptionBroker::SubscriptionIndex last = subscriptions.size() - 1;
        if (index != last) {
            auto& moved = subscriptions[last];
            const auto moved_id = moved.subscription_id;
            const auto moved_session_id = moved.session_id;
            const auto moved_topic = moved.topic;

            subscriptions[index] = std::move(moved);
            subscription_id_index[moved_id] = index;

            auto moved_session_it = session_index.find(moved_session_id);
            if (moved_session_it != session_index.end()) {
                remapIndex(moved_session_it->second, last, index);
            }

            auto moved_topic_it = topic_index.find(moved_topic);
            if (moved_topic_it != topic_index.end()) {
                remapIndex(moved_topic_it->second, last, index);
            }
        }

        subscriptions.pop_back();
        subscription_id_index.erase(removed_id);
        return true;
    }
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
    std::size_t new_count = 0;
    {
        std::unique_lock lock(m_impl->mutex);
        const SubscriptionIndex index = m_impl->subscriptions.size();
        m_impl->subscriptions.push_back(SubscriptionRecord{sub_id, session_id, topic, std::move(handler)});
        m_impl->subscription_id_index[sub_id] = index;
        m_impl->session_index[session_id].push_back(index);
        m_impl->topic_index[topic].push_back(index);
        new_count = m_impl->subscriptions.size();
    }
    // T050-002: notify outside the main lock
    std::function<void(std::size_t)> cb;
    {
        std::lock_guard<std::mutex> lk(m_impl->count_cb_mutex);
        cb = m_impl->count_change_cb;
    }
    if (cb) cb(new_count);

    spdlog::debug("SubscriptionBroker: registered {} for session {} on topic '{}'",
                  sub_id, session_id, topic);
    return sub_id;
}

void SubscriptionBroker::unsubscribe(const std::string& subscription_id) {
    bool changed = false;
    std::size_t new_count = 0;
    {
        std::unique_lock lock(m_impl->mutex);

        auto it = m_impl->subscription_id_index.find(subscription_id);
        if (it == m_impl->subscription_id_index.end()) {
            return;
        }

        if (!m_impl->eraseSubscriptionAt(it->second)) {
            return;
        }

        new_count = m_impl->subscriptions.size();
        changed = true;
    }
    // T050-002: notify outside the main lock
    if (changed) {
        std::function<void(std::size_t)> cb;
        {
            std::lock_guard<std::mutex> lk(m_impl->count_cb_mutex);
            cb = m_impl->count_change_cb;
        }
        if (cb) cb(new_count);
    }

    spdlog::debug("SubscriptionBroker: removed {}", subscription_id);
}

void SubscriptionBroker::disconnect_session(const std::string& session_id) {
    bool changed = false;
    std::size_t new_count = 0;
    {
        std::unique_lock lock(m_impl->mutex);

        const bool had_session = m_impl->session_index.find(session_id) != m_impl->session_index.end();
        if (!had_session) {
            return;
        }

        while (true) {
            auto sess_it = m_impl->session_index.find(session_id);
            if (sess_it == m_impl->session_index.end() || sess_it->second.empty()) {
                break;
            }

            const SubscriptionIndex index = sess_it->second.back();
            if (!m_impl->eraseSubscriptionAt(index)) {
                // Guard against stale/sentinel index entries.
                sess_it->second.pop_back();
            }
        }

        new_count = m_impl->subscriptions.size();
        changed = true;
    }
    // T050-002: notify outside the main lock
    if (changed) {
        std::function<void(std::size_t)> cb;
        {
            std::lock_guard<std::mutex> lk(m_impl->count_cb_mutex);
            cb = m_impl->count_change_cb;
        }
        if (cb) cb(new_count);
    }

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
        auto topic_it = m_impl->topic_index.find(topic);
        if (topic_it == m_impl->topic_index.end()) {
            return;
        }

        handlers.reserve(topic_it->second.size());
        for (const SubscriptionIndex index : topic_it->second) {
            const SubscriptionRecord* record = m_impl->getRecordByIndex(index);
            if (record == nullptr) {
                continue;
            }
            handlers.push_back(record->handler);
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

    auto topic_it = m_impl->topic_index.find(topic);
    if (topic_it == m_impl->topic_index.end()) {
        return 0;
    }

    std::size_t count = 0;
    for (const SubscriptionIndex index : topic_it->second) {
        const SubscriptionRecord* record = m_impl->getRecordByIndex(index);
        if (record != nullptr && record->topic == topic) {
            ++count;
        }
    }
    return count;
}

// T050-002: set_subscription_count_callback
void SubscriptionBroker::set_subscription_count_callback(std::function<void(std::size_t)> cb) {
    std::lock_guard<std::mutex> lk(m_impl->count_cb_mutex);
    m_impl->count_change_cb = std::move(cb);
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
