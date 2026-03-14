// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_SubscriptionBroker.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Thread-safe fan-out broker for GraphQL subscription events.
 *
 * The SubscriptionBroker routes published events to all registered handlers
 * that are subscribed to a matching topic.  It is the single point of fan-out
 * for WebSocket subscription sessions and is safe for concurrent use from
 * multiple HTTP/WebSocket threads.
 *
 * ## Design notes
 * - Each subscription is identified by a server-generated UUID string.
 * - Topics are plain strings; matching is exact (prefix/glob routing is
 *   deferred to Phase 5 when full subscription semantics are implemented).
 * - Handlers are called outside the internal lock to avoid reentrancy.
 * - `disconnect_session()` removes **all** subscriptions for a WebSocket
 *   session in one atomic step.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace isched::v0_0_1::backend {

// ---------------------------------------------------------------------------
// Event type
// ---------------------------------------------------------------------------

/**
 * @brief A single subscription event delivered to registered handlers.
 */
struct SubscriptionEvent {
    std::string type;          ///< Event type string, e.g. "configurationApplied"
    std::string topic;         ///< Topic the event was published on
    nlohmann::json data;       ///< Event payload
};

// ---------------------------------------------------------------------------
// Handler alias
// ---------------------------------------------------------------------------

/**
 * @brief Callback invoked when an event matching the handler's topic arrives.
 *
 * The callback is invoked from whichever thread calls `publish()`.
 * Implementations must be thread-safe and must not call back into the broker.
 */
using SubscriptionHandler = std::function<void(const SubscriptionEvent&)>;

// ---------------------------------------------------------------------------
// Broker
// ---------------------------------------------------------------------------

/**
 * @brief Thread-safe GraphQL subscription fan-out broker.
 *
 * @note Non-copyable, non-movable.  Create via `SubscriptionBroker::create()`.
 */
class SubscriptionBroker {
public:
    using UniquePtr = std::unique_ptr<SubscriptionBroker>;

    // ----- Factory --------------------------------------------------------

    /** Create a new, empty broker. */
    [[nodiscard]] static UniquePtr create();

    // ----- Lifecycle ------------------------------------------------------

    ~SubscriptionBroker();

    SubscriptionBroker(const SubscriptionBroker&)            = delete;
    SubscriptionBroker& operator=(const SubscriptionBroker&) = delete;
    SubscriptionBroker(SubscriptionBroker&&)                 = delete;
    SubscriptionBroker& operator=(SubscriptionBroker&&)      = delete;

    // ----- Subscription management ----------------------------------------

    /**
     * @brief Register a new subscription.
     *
     * @param session_id  Opaque WebSocket session identifier.
     * @param topic       Topic string to subscribe to.
     * @param handler     Callback invoked when matching events are published.
     * @return            A unique subscription ID that can be passed to
     *                    `unsubscribe()`.
     */
    [[nodiscard]] std::string subscribe(const std::string& session_id,
                                        const std::string& topic,
                                        SubscriptionHandler handler);

    /**
     * @brief Remove a single subscription by ID.
     *
     * No-op if the subscription_id is unknown.
     */
    void unsubscribe(const std::string& subscription_id);

    /**
     * @brief Remove all subscriptions belonging to a session.
     *
     * Called when a WebSocket connection closes.
     */
    void disconnect_session(const std::string& session_id);

    // ----- Event delivery -------------------------------------------------

    /**
     * @brief Publish an event to all handlers whose topic matches.
     *
     * Handlers are called synchronously in the calling thread after all
     * internal locks are released, so `publish()` never holds a lock while
     * invoking user code.
     *
     * @param topic  Topic to match against registered subscriptions.
     * @param type   Event type string included in the delivered event.
     * @param data   JSON payload included in the delivered event.
     */
    void publish(const std::string& topic,
                 const std::string& type,
                 const nlohmann::json& data);

    // ----- Observability --------------------------------------------------

    /**
     * @brief Return the total count of active subscriptions.
     *
     * If @p topic is non-empty, count only subscriptions for that topic.
     */
    [[nodiscard]] std::size_t get_subscriber_count(const std::string& topic = "") const;

private:
    SubscriptionBroker();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace isched::v0_0_1::backend
