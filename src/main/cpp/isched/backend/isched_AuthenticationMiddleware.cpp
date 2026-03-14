// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_AuthenticationMiddleware.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of authentication middleware for JWT and OAuth support
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-02
 */

#include "isched_AuthenticationMiddleware.hpp"
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <random>
#include <sstream>
#include <iomanip>
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

        try {
            std::string secret;
            {
                std::lock_guard<std::mutex> lock(jwt_mutex_);
                secret = jwt_secret_;
            }
            if (secret.empty()) {
                ++failed_auths_;
                return {false, "", "", {}, "JWT secret not configured"};
            }

            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret});
            verifier.verify(decoded);

            const std::string user_id = decoded.get_subject();

            std::string token_tenant_id;
            if (decoded.has_payload_claim("tenant_id")) {
                token_tenant_id = decoded.get_payload_claim("tenant_id").as_string();
            }
            if (!tenant_id.empty() && token_tenant_id != tenant_id) {
                ++failed_auths_;
                return {false, "", "", {}, "Token tenant mismatch"};
            }

            std::vector<std::string> permissions;
            if (decoded.has_payload_claim("permissions")) {
                for (const auto& p : decoded.get_payload_claim("permissions").as_array()) {
                    permissions.push_back(p.get<std::string>());
                }
            }

            std::chrono::system_clock::time_point expires_at;
            if (decoded.has_expires_at()) {
                expires_at = decoded.get_expires_at();
            } else {
                expires_at = std::chrono::system_clock::now() + std::chrono::hours(1);
            }

            ++successful_auths_;
            return {true, user_id, token_tenant_id, permissions, "", expires_at};

        } catch (const std::exception& e) {
            ++failed_auths_;
            return {false, "", "", {}, std::string("JWT validation failed: ") + e.what()};
        }
    }
    
    std::string generate_jwt_token(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions,
        int expires_in_minutes) override {
        
        std::string secret;
        {
            std::lock_guard<std::mutex> lock(jwt_mutex_);
            secret = jwt_secret_;
        }
        if (secret.empty()) {
            throw std::runtime_error("JWT secret not configured");
        }

        auto now = std::chrono::system_clock::now();
        auto expires_at = now + std::chrono::minutes(expires_in_minutes);

        nlohmann::json::array_t perms_array;
        perms_array.reserve(permissions.size());
        for (const auto& p : permissions) {
            perms_array.emplace_back(p);
        }

        return jwt::create()
            .set_subject(user_id)
            .set_payload_claim("tenant_id", jwt::claim(tenant_id))
            .set_payload_claim("permissions", jwt::claim(perms_array))
            .set_issued_at(now)
            .set_expires_at(expires_at)
            .sign(jwt::algorithm::hs256{secret});
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

// =============================================================================
// T047-011: Argon2id helpers — OpenSSL 3.x EVP_KDF
// =============================================================================

namespace {

// Custom deleter for EVP_KDF_CTX
struct EvpKdfCtxDeleter {
    void operator()(EVP_KDF_CTX* p) const noexcept { if (p) EVP_KDF_CTX_free(p); }
};
using EvpKdfCtxPtr = std::unique_ptr<EVP_KDF_CTX, EvpKdfCtxDeleter>;

struct EvpKdfDeleter {
    void operator()(EVP_KDF* p) const noexcept { if (p) EVP_KDF_free(p); }
};
using EvpKdfPtr = std::unique_ptr<EVP_KDF, EvpKdfDeleter>;

constexpr uint32_t kArgonT    = 3;       ///< iterations
constexpr uint32_t kArgonM    = 65536;   ///< memory in KiB (64 MiB)
constexpr uint32_t kArgonP    = 1;       ///< parallelism
constexpr std::size_t kSaltLen = 16;     ///< bytes
constexpr std::size_t kHashLen = 32;     ///< bytes

std::string bytes_to_hex(const unsigned char* data, std::size_t len)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex)
{
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("verify_password: invalid hex string length");
    }
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<unsigned char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::vector<unsigned char> argon2id_derive(
    const std::string& password,
    const unsigned char* salt, std::size_t salt_len,
    uint32_t t, uint32_t m, uint32_t p)
{
    EvpKdfPtr kdf{EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr)};
    if (!kdf) {
        throw std::runtime_error("hash_password: EVP_KDF_fetch(ARGON2ID) failed — OpenSSL ≥ 3.2 required");
    }
    EvpKdfCtxPtr ctx{EVP_KDF_CTX_new(kdf.get())};
    if (!ctx) {
        throw std::runtime_error("hash_password: EVP_KDF_CTX_new failed");
    }

    OSSL_PARAM params[6];
    int idx = 0;
    params[idx++] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD,
        const_cast<char*>(password.data()),
        password.size());
    params[idx++] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT,
        const_cast<unsigned char*>(salt),
        salt_len);
    params[idx++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &t);
    params[idx++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &m);
    params[idx++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &p);
    params[idx]   = OSSL_PARAM_END;

    std::vector<unsigned char> out(kHashLen);
    if (EVP_KDF_derive(ctx.get(), out.data(), out.size(), params) != 1) {
        throw std::runtime_error("hash_password: EVP_KDF_derive failed");
    }
    return out;
}

} // anonymous namespace

std::string hash_password(const std::string& plaintext)
{
    if (plaintext.empty()) {
        throw std::invalid_argument("hash_password: plaintext must not be empty");
    }

    unsigned char salt[kSaltLen];
    if (RAND_bytes(salt, static_cast<int>(kSaltLen)) != 1) {
        throw std::runtime_error("hash_password: RAND_bytes failed");
    }

    const auto hash = argon2id_derive(plaintext, salt, kSaltLen, kArgonT, kArgonM, kArgonP);

    // Format: argon2id:t=<T>:m=<M>:p=<P>:<salt_hex>:<hash_hex>
    return "argon2id:t=" + std::to_string(kArgonT)
         + ":m=" + std::to_string(kArgonM)
         + ":p=" + std::to_string(kArgonP)
         + ":" + bytes_to_hex(salt, kSaltLen)
         + ":" + bytes_to_hex(hash.data(), hash.size());
}

bool verify_password(const std::string& plaintext, const std::string& stored_hash)
{
    // Parse: argon2id:t=<T>:m=<M>:p=<P>:<salt_hex>:<hash_hex>
    // Split by ':'
    std::vector<std::string> parts;
    {
        std::istringstream ss(stored_hash);
        std::string tok;
        while (std::getline(ss, tok, ':')) {
            parts.push_back(tok);
        }
    }
    if (parts.size() != 6 || parts[0] != "argon2id") {
        return false;
    }

    uint32_t t = 0, m = 0, p = 0;
    try {
        // parts[1] = "t=N", parts[2] = "m=N", parts[3] = "p=N"
        t = static_cast<uint32_t>(std::stoul(parts[1].substr(2)));
        m = static_cast<uint32_t>(std::stoul(parts[2].substr(2)));
        p = static_cast<uint32_t>(std::stoul(parts[3].substr(2)));
    } catch (...) {
        return false;
    }

    std::vector<unsigned char> salt;
    std::vector<unsigned char> expected_hash;
    try {
        salt          = hex_to_bytes(parts[4]);
        expected_hash = hex_to_bytes(parts[5]);
    } catch (...) {
        return false;
    }

    try {
        const auto computed = argon2id_derive(plaintext, salt.data(), salt.size(), t, m, p);
        if (computed.size() != expected_hash.size()) { return false; }
        // Constant-time comparison
        unsigned char diff = 0;
        for (std::size_t i = 0; i < computed.size(); ++i) {
            diff |= (computed[i] ^ expected_hash[i]);
        }
        return diff == 0;
    } catch (...) {
        return false;
    }
}

} // namespace isched::v0_0_1::backend