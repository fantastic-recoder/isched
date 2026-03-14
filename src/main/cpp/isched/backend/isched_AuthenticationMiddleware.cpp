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
#include "isched_DatabaseManager.hpp"
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <spdlog/spdlog.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <ctime>

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
            return {false, "", "", "", {}, {}, "Missing Authorization header", {}, ""};
        }
        
        std::string token = extract_jwt_token(auth_it->second);
        if (token.empty()) {
            ++failed_auths_;
            return {false, "", "", "", {}, {}, "Invalid Authorization header format", {}, ""};
        }

        try {
            std::string secret;
            {
                std::lock_guard<std::mutex> lock(jwt_mutex_);
                secret = jwt_secret_;
            }
            if (secret.empty()) {
                ++failed_auths_;
                return {false, "", "", "", {}, {}, "JWT secret not configured", {}, ""};
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
                return {false, "", "", "", {}, {}, "Token tenant mismatch", {}, ""};
            }

            std::vector<std::string> permissions;
            if (decoded.has_payload_claim("permissions")) {
                for (const auto& p : decoded.get_payload_claim("permissions").as_array()) {
                    permissions.push_back(p.get<std::string>());
                }
            }

            std::vector<std::string> roles;
            if (decoded.has_payload_claim("roles")) {
                for (const auto& r : decoded.get_payload_claim("roles").as_array()) {
                    roles.push_back(r.get<std::string>());
                }
            }

            std::string tok_user_name;
            if (decoded.has_payload_claim("name")) {
                tok_user_name = decoded.get_payload_claim("name").as_string();
            }

            std::chrono::system_clock::time_point expires_at;
            if (decoded.has_expires_at()) {
                expires_at = decoded.get_expires_at();
            } else {
                expires_at = std::chrono::system_clock::now() + std::chrono::hours(1);
            }

            std::string jti;
            if (decoded.has_id()) {
                jti = decoded.get_id();
            }

            ++successful_auths_;
            return {true, user_id, tok_user_name, token_tenant_id, permissions, roles, "", expires_at, jti};

        } catch (const std::exception& e) {
            ++failed_auths_;
            return {false, "", "", "", {}, {}, std::string("JWT validation failed: ") + e.what(), {}, ""};
        }
    }
    
    std::string generate_jwt_token(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions,
        int expires_in_minutes,
        const std::string& jti,
        const std::string& user_name,
        const std::vector<std::string>& roles) override {
        
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
        for (const auto& p : permissions) { perms_array.emplace_back(p); }

        auto builder = jwt::create()
            .set_subject(user_id)
            .set_payload_claim("tenant_id", jwt::claim(tenant_id))
            .set_payload_claim("permissions", jwt::claim(perms_array))
            .set_issued_at(now)
            .set_expires_at(expires_at);

        if (!jti.empty()) {
            builder = std::move(builder).set_id(jti);
        }
        if (!user_name.empty()) {
            builder = std::move(builder).set_payload_claim("name", jwt::claim(user_name));
        }
        if (!roles.empty()) {
            nlohmann::json::array_t roles_array;
            roles_array.reserve(roles.size());
            for (const auto& r : roles) { roles_array.emplace_back(r); }
            builder = std::move(builder).set_payload_claim("roles", jwt::claim(roles_array));
        }

        return std::move(builder).sign(jwt::algorithm::hs256{secret});
    }
    
    SessionInfo create_session(
        const std::string& user_id,
        const std::string& tenant_id,
        const std::vector<std::string>& permissions) override {
        
        auto session_id = generate_session_id();
        auto now = std::chrono::system_clock::now();
        auto expires_at = now + std::chrono::hours(8); // 8-hour session
        
        SessionInfo session{
            "",           // token — not generated for in-memory sessions
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

    // ── T049-002: DB-backed session creation ──────────────────────────────

    LoginSession create_session(
        DatabaseManager& db,
        const std::string& user_id,
        const std::string& user_name,
        const std::string& tenant_id,
        const std::vector<std::string>& roles,
        const std::string& transport_scope,
        int expires_in_minutes) override
    {
        const std::string session_id = generate_secure_uuid();
        const auto now = std::chrono::system_clock::now();
        const auto exp = now + std::chrono::minutes(expires_in_minutes);

        // Format expiry as ISO-8601 UTC: "2025-01-01T00:00:00Z"
        const auto exp_tt = std::chrono::system_clock::to_time_t(exp);
        std::tm exp_tm{};
        gmtime_r(&exp_tt, &exp_tm);
        char exp_buf[32];
        std::strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%dT%H:%M:%SZ", &exp_tm);
        const std::string expires_at_str(exp_buf);

        // Build the JWT with jti = session_id and roles snapshot.
        // permissions is the same as roles for now; future scopes can be added.
        const std::string token = generate_jwt_token(
            user_id, tenant_id, roles,
            expires_in_minutes, session_id, user_name, roles);

        // Serialise roles to JSON array for the DB column.
        nlohmann::json roles_json = roles;
        const std::string roles_str = roles_json.dump();

        // Persist to the tenant sessions table.
        const auto persist_res = db.create_session(
            tenant_id, session_id, user_id, session_id /*access_token_id = jti*/,
            "[]" /*permissions – not separately stored yet*/,
            roles_str, expires_at_str, transport_scope);
        if (!persist_res) {
            // Non-fatal: we still return the token; login resolver can choose to
            // surface this as an error.  Log a warning.
            spdlog::warn("create_session: failed to persist session '{}' for tenant '{}': {}",
                session_id, tenant_id,
                static_cast<int>(persist_res.error()));
        }

        return LoginSession{token, session_id, expires_at_str};
    }

    // ── T049-002: JWT + DB revocation validation ──────────────────────────

    AuthenticationResult validate_token(
        DatabaseManager& db,
        const std::string& token,
        const std::string& tenant_id) override
    {
        // Step 1: validate JWT (reuse validate_request logic via fake headers).
        std::unordered_map<std::string, std::string> hdrs;
        hdrs["Authorization"] = "Bearer " + token;
        auto auth_res = validate_request(hdrs, tenant_id);
        if (!auth_res.is_authenticated) {
            return auth_res;  // JWT invalid / expired / wrong tenant
        }

        // Step 2: extract jti and check DB revocation.
        try {
            auto decoded = jwt::decode(token);
            if (!decoded.has_id()) {
                // Token predates session tracking — accept on JWT-only basis.
                return auth_res;
            }
            const std::string jti = decoded.get_id();
            const auto sess_res = db.get_session(tenant_id, jti);
            if (!sess_res) {
                // Session not found — could be platform-level login or table not
                // yet initialised; fall back to JWT-only validation.
                spdlog::debug("validate_token: session '{}' not found in tenant '{}' DB, "
                              "accepting on JWT basis", jti, tenant_id);
                return auth_res;
            }
            if (sess_res.value().is_revoked) {
                ++failed_auths_;
                return {false, "", "", "", {}, {}, "Session has been revoked", {}, ""};
            }
            // Touch last_activity (best-effort).
            std::ignore = db.update_session_activity(tenant_id, jti);
        } catch (const std::exception& e) {
            // JWT decode failed (shouldn't happen since validate_request passed).
            spdlog::warn("validate_token: unexpected decode error: {}", e.what());
        }
        return auth_res;
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

    // Cryptographically secure UUID v4 using OpenSSL RAND_bytes.
    static std::string generate_secure_uuid() {
        unsigned char buf[16];
        if (RAND_bytes(buf, sizeof(buf)) != 1) {
            throw std::runtime_error("RAND_bytes failed — cannot generate session UUID");
        }
        // Set version (4) and variant bits (RFC 4122)
        buf[6] = (buf[6] & 0x0f) | 0x40;
        buf[8] = (buf[8] & 0x3f) | 0x80;
        char out[37];
        std::snprintf(out, sizeof(out),
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            buf[0],  buf[1],  buf[2],  buf[3],
            buf[4],  buf[5],
            buf[6],  buf[7],
            buf[8],  buf[9],
            buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
        return std::string(out);
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