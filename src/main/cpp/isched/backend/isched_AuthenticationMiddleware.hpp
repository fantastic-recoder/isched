// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_AuthenticationMiddleware.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Authentication middleware for JWT and OAuth support
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 * 
 * This file contains the authentication middleware for the Universal Application Server Backend.
 * It provides JWT token validation, OAuth integration, session management, and per-tenant
 * authentication isolation as required by the constitutional security-first principle.
 * 
 * @note All resource management uses smart pointers as required by FR-021.
 * @see FR-002, FR-002-A, FR-017-A for authentication requirements
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <vector>

namespace isched::v0_0_1::backend {

/**
 * @brief Authentication result containing user context and validation status
 */
struct AuthenticationResult {
    bool is_authenticated = false;
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> permissions;
    std::string error_message;
    std::chrono::system_clock::time_point expires_at;
    
    /// @brief Check if authentication is valid and not expired
    bool is_valid() const noexcept {
        return is_authenticated && 
               std::chrono::system_clock::now() < expires_at;
    }
};

/**
 * @brief OAuth provider configuration
 */
struct OAuthConfig {
    std::string client_id;
    std::string client_secret;
    std::string auth_url;
    std::string token_url;
    std::vector<std::string> scopes;
};

/**
 * @brief Session information for authenticated users
 */
struct SessionInfo {
    std::string session_id;
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> permissions;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    std::chrono::system_clock::time_point last_activity;
    
    /// @brief Check if session is still valid
    bool is_valid() const noexcept {
        auto now = std::chrono::system_clock::now();
        return now < expires_at;
    }
    
    /// @brief Update last activity timestamp
    void touch() noexcept {
        last_activity = std::chrono::system_clock::now();
    }
};

/**
 * @brief Authentication middleware class providing JWT, OAuth, and session management
 * 
 * The AuthenticationMiddleware implements industry-standard authentication protocols
 * with per-tenant isolation and smart pointer-based resource management.
 * 
 * Constitutional Compliance:
 * - Security: Industry-standard JWT and OAuth protocols (FR-SEC-001)
 * - Performance: In-memory session caching with shared store backup (FR-002-A)
 * - Portability: Standard C++23 with cross-platform JWT library
 * - C++ Core Guidelines: Smart pointer usage, const-correctness, RAII
 */
class AuthenticationMiddleware {
public:
    /**
     * @brief Factory method to create AuthenticationMiddleware instance
     * @return Smart pointer to AuthenticationMiddleware instance
     */
    static std::unique_ptr<AuthenticationMiddleware> create();
    
    /**
     * @brief Virtual destructor for proper inheritance
     */
    virtual ~AuthenticationMiddleware() = default;
    
    // Non-copyable but movable
    AuthenticationMiddleware(const AuthenticationMiddleware&) = delete;
    AuthenticationMiddleware& operator=(const AuthenticationMiddleware&) = delete;
    AuthenticationMiddleware(AuthenticationMiddleware&&) = default;
    AuthenticationMiddleware& operator=(AuthenticationMiddleware&&) = default;
    
    /**
     * @brief Configure JWT secret key for token validation
     * @param secret_key Secret key for JWT signing and validation
     */
    virtual void configure_jwt_secret(const std::string& secret_key) = 0;
    
    /**
     * @brief Add OAuth provider configuration
     * @param provider_name Name of OAuth provider (e.g., "google", "github")
     * @param config OAuth configuration parameters
     */
    virtual void add_oauth_provider(const std::string& provider_name, 
                                  const OAuthConfig& config) = 0;
    
    /**
     * @brief Validate authentication from request headers
     * @param headers Request headers containing authorization information
     * @param tenant_id Tenant context for authentication validation
     * @return Authentication result with user context
     */
    virtual AuthenticationResult validate_request(
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& tenant_id) = 0;
    
    /**
     * @brief Generate JWT token for authenticated user
     * @param user_id User identifier
     * @param tenant_id Tenant context
     * @param permissions User permissions array
     * @param expires_in_minutes Token expiration duration in minutes
     * @return Generated JWT token string
     */
    virtual std::string generate_jwt_token(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions,
        int expires_in_minutes = 60) = 0;
    
    /**
     * @brief Create user session with in-memory caching
     * @param user_id User identifier
     * @param tenant_id Tenant context
     * @param permissions User permissions
     * @return Session information
     */
    virtual SessionInfo create_session(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions) = 0;
    
    /**
     * @brief Get session information by session ID
     * @param session_id Session identifier
     * @return Optional session information if found and valid
     */
    virtual std::optional<SessionInfo> get_session(
        const std::string& session_id) = 0;
    
    /**
     * @brief Invalidate user session
     * @param session_id Session identifier to invalidate
     */
    virtual void invalidate_session(const std::string& session_id) = 0;
    
    /**
     * @brief Get authentication metrics for monitoring
     * @return JSON string with authentication statistics
     */
    virtual std::string get_metrics() const = 0;

protected:
    /**
     * @brief Protected constructor for factory pattern
     */
    AuthenticationMiddleware() = default;
};

} // namespace isched::v0_0_1::backend