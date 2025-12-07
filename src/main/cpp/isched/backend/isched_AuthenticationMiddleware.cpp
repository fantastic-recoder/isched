/**
 * @file isched_AuthenticationMiddleware.cpp
 * @brief Implementation of authentication middleware for JWT and OAuth support
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 */

#include "isched_AuthenticationMiddleware.hpp"
#include <random>
#include <sstream>
#include <stdexcept>

namespace isched::v0_0_1::backend {

/**
 * @brief Implementation of AuthenticationMiddleware
 */
class AuthenticationMiddlewareImpl : public AuthenticationMiddleware {
public:
    AuthenticationMiddlewareImpl() = default;
    ~AuthenticationMiddlewareImpl() override = default;
    
    void configure_jwt_secret(const std::string& secret_key) override {
        std::lock_guard<std::mutex> lock(jwt_mutex_);
        jwt_secret_ = secret_key;
    }
    
    void add_oauth_provider(const std::string& provider_name, 
                          const OAuthConfig& config) override {
        oauth_providers_[provider_name] = config;
    }
    
    AuthenticationResult validate_request(
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& tenant_id) override {
        
        ++total_auth_attempts_;
        
        auto auth_it = headers.find("Authorization");
        if (auth_it == headers.end()) {
            ++failed_auths_;
            return {false, "", "", {}, "Missing Authorization header"};
        }
        
        std::string token = extract_jwt_token(auth_it->second);
        if (token.empty()) {
            ++failed_auths_;
            return {false, "", "", {}, "Invalid Authorization header format"};
        }
        
        // TODO: Implement JWT validation when jwt-cpp is properly configured
        // For now, return a basic validation result
        auto result = AuthenticationResult{
            true, 
            "test-user", 
            tenant_id, 
            {"read", "write"}, 
            "",
            std::chrono::system_clock::now() + std::chrono::hours(1)
        };
        
        ++successful_auths_;
        return result;
    }
    
    std::string generate_jwt_token(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions,
        int expires_in_minutes) override {
        
        if (jwt_secret_.empty()) {
            throw std::runtime_error("JWT secret not configured");
        }
        
        // TODO: Implement JWT generation when jwt-cpp is properly configured
        // For now, return a placeholder token
        std::ostringstream oss;
        oss << "jwt:" << user_id << ":" << tenant_id << ":" << expires_in_minutes;
        return oss.str();
    }
    
    SessionInfo create_session(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions) override {
        
        auto session_id = generate_session_id();
        auto now = std::chrono::system_clock::now();
        auto expires_at = now + std::chrono::hours(8); // 8-hour session
        
        SessionInfo session{
            session_id,
            user_id,
            tenant_id,
            permissions,
            now,
            expires_at,
            now
        };
        
        {
            std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
            active_sessions_[session_id] = session;
            ++active_sessions_count_;
        }
        
        return session;
    }
    
    std::optional<SessionInfo> get_session(const std::string& session_id) override {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end() && it->second.is_valid()) {
            // Update last activity
            const_cast<SessionInfo&>(it->second).touch();
            return it->second;
        }
        return std::nullopt;
    }
    
    void invalidate_session(const std::string& session_id) override {
        std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            active_sessions_.erase(it);
            --active_sessions_count_;
        }
    }
    
    std::string get_metrics() const override {
        cleanup_expired_sessions();
        
        std::ostringstream oss;
        oss << "{"
            << "\"total_auth_attempts\":" << total_auth_attempts_.load() << ","
            << "\"successful_auths\":" << successful_auths_.load() << ","
            << "\"failed_auths\":" << failed_auths_.load() << ","
            << "\"active_sessions\":" << active_sessions_count_.load() << ","
            << "\"oauth_providers\":" << oauth_providers_.size()
            << "}";
        return oss.str();
    }

private:
    std::string extract_jwt_token(const std::string& auth_header) const {
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.length() > bearer_prefix.length() &&
            auth_header.substr(0, bearer_prefix.length()) == bearer_prefix) {
            return auth_header.substr(bearer_prefix.length());
        }
        return "";
    }
    
    std::string generate_session_id() const {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::ostringstream oss;
        for (int i = 0; i < 32; ++i) {
            int val = dis(gen);
            if (val < 10) {
                oss << static_cast<char>('0' + val);
            } else {
                oss << static_cast<char>('a' + val - 10);
            }
        }
        return oss.str();
    }
    
    void cleanup_expired_sessions() const {
        std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
        
        for (auto it = active_sessions_.begin(); it != active_sessions_.end();) {
            if (!it->second.is_valid()) {
                it = active_sessions_.erase(it);
                --active_sessions_count_;
            } else {
                ++it;
            }
        }
    }

    // Member variables
    std::string jwt_secret_;
    std::unordered_map<std::string, OAuthConfig> oauth_providers_;
    
    // Session management with thread safety
    mutable std::shared_mutex sessions_mutex_;
    mutable std::unordered_map<std::string, SessionInfo> active_sessions_;
    
    // Metrics
    mutable std::mutex metrics_mutex_;
    mutable std::atomic<uint64_t> total_auth_attempts_{0};
    mutable std::atomic<uint64_t> successful_auths_{0};
    mutable std::atomic<uint64_t> failed_auths_{0};
    mutable std::atomic<uint64_t> active_sessions_count_{0};
    
    // JWT verifier (placeholder for now)
    mutable std::mutex jwt_mutex_;
};

// Factory method implementation
std::unique_ptr<AuthenticationMiddleware> AuthenticationMiddleware::create() {
    return std::make_unique<AuthenticationMiddlewareImpl>();
}

} // namespace isched::v0_0_1::backend