// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_RateLimiter.hpp
 * @brief Token-bucket rate limiter keyed on client IP address.
 *
 * Used to protect the `createPlatformAdmin` mutation from bruteforce during
 * the seed-mode window.  The maximum rate is configurable via the
 * @c ISCHED_SEED_RATE_LIMIT environment variable (fallthrough to
 * @c server.seed_rate_limit config key; default: 5 requests per minute).
 */
#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace isched::v0_0_1::backend {

/**
 * @brief A single token-bucket for one client IP.
 *
 * Each bucket starts full.  One token is consumed per request; tokens refill
 * at @c max_per_minute / 60 tokens per second.
 */
struct TokenBucket {
    float    tokens;          ///< Current token count (fractional)
    float    capacity;        ///< Maximum tokens (== initial count)
    std::chrono::steady_clock::time_point last_refill;

    explicit TokenBucket(int max_per_minute) noexcept
        : tokens{static_cast<float>(max_per_minute)}
        , capacity{static_cast<float>(max_per_minute)}
        , last_refill{std::chrono::steady_clock::now()} {}

    /**
     * @brief Attempt to consume one token.
     * @return @c true if the request is allowed; @c false if rate-limited.
     */
    bool try_consume() noexcept {
        using namespace std::chrono;
        const auto now = steady_clock::now();
        const float elapsed_secs =
            duration_cast<duration<float>>(now - last_refill).count();
        last_refill = now;

        // Refill proportionally to elapsed time
        tokens = std::min(capacity, tokens + elapsed_secs * (capacity / 60.0f));

        if (tokens >= 1.0f) {
            tokens -= 1.0f;
            return true;
        }
        return false;
    }
};

/**
 * @brief Thread-safe rate limiter backed by per-IP token buckets.
 *
 * Example (createPlatformAdmin resolver):
 * @code{cpp}
 * static RateLimiter limiter;
 * if (!limiter.allow(ctx.remote_ip, 5)) {
 *     throw GraphqlError{"rate limit exceeded", "RATE_LIMITED"};
 * }
 * @endcode
 */
class RateLimiter {
public:
    RateLimiter() = default;

    /**
     * @brief Check whether a request from @p ip is within the rate limit.
     *
     * Creates a new bucket for IPs seen for the first time.
     *
     * @param ip            Client IP address string.
     * @param max_per_minute Maximum requests allowed per 60-second window.
     * @return @c true if the request should be allowed; @c false otherwise.
     */
    bool allow(std::string_view ip, int max_per_minute) {
        std::lock_guard<std::mutex> lock{m_mutex};
        auto [it, inserted] =
            m_buckets.try_emplace(std::string{ip}, max_per_minute);
        return it->second.try_consume();
    }

private:
    std::mutex                               m_mutex;
    std::unordered_map<std::string, TokenBucket> m_buckets;
};

} // namespace isched::v0_0_1::backend
