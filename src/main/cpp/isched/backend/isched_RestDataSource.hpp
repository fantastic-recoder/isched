// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_RestDataSource.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Outbound HTTP client wrapper used by resolver bindings (T048-005).
 */
#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace isched::v0_0_1::backend {

/**
 * @brief Decrypted configuration for one outbound-HTTP data source.
 *
 * Callers must decrypt @c api_key_value_encrypted using @c decrypt_secret()
 * before populating this struct.
 */
struct DataSourceConfig {
    std::string base_url;
    std::string auth_kind{"none"};  ///< none | bearer_passthrough | api_key
    std::string api_key_header;     ///< Header name for api_key auth (e.g. "X-API-Key")
    std::string api_key_value;      ///< Decrypted API key value
    int         timeout_ms{5000};
};

/**
 * @brief Stateless outbound HTTP client for GraphQL resolver bindings (T048-005).
 *
 * Auth forwarding strategy (applied before the request):
 * - @c none              — no extra auth header
 * - @c bearer_passthrough — forwards @p caller_bearer_token as
 *                          @c "Authorization: Bearer <token>"
 * - @c api_key           — sets @c config.api_key_header to @c config.api_key_value
 *
 * On success (2xx): returns the parsed JSON response body.
 * On non-2xx or network error: returns an @c HttpError JSON object as described
 * in the GraphQL schema (@c T048-006):
 * @code
 * { "statusCode": <int>, "message": <string>, "url": <string> }
 * @endcode
 */
class RestDataSource {
public:
    /**
     * @brief Execute a single HTTP request to the configured upstream.
     *
     * @param config               Data source configuration (with decrypted secrets).
     * @param path                 URL path appended to @c config.base_url (default: "/").
     * @param method               HTTP method: GET | POST | PUT | PATCH | DELETE.
     * @param body                 Request body; only sent for POST/PUT/PATCH.
     * @param caller_bearer_token  Forwarded verbatim when @c auth_kind == bearer_passthrough.
     * @return Parsed JSON body on 2xx, or an @c HttpError JSON object on failure.
     */
    [[nodiscard]] static nlohmann::json fetch(
        const DataSourceConfig& config,
        const std::string&      path                 = "/",
        const std::string&      method               = "GET",
        const nlohmann::json&   body                 = nlohmann::json{},
        const std::string&      caller_bearer_token  = "");
};

} // namespace isched::v0_0_1::backend
