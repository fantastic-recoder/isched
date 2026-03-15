// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_RestDataSource.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Outbound-HTTP fetch implementation using cpp-httplib (T048-005).
 */
#include "isched_RestDataSource.hpp"

#include <spdlog/spdlog.h>

// cpp-httplib is a header-only library; include it in exactly one .cpp per target.
// OpenSSL is already linked by the isched library, so HTTPS is available.
#include "httplib.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace isched::v0_0_1::backend {

namespace {

/// Build an HttpError JSON payload (T048-006).
nlohmann::json make_http_error(int status_code, const std::string& message,
                                const std::string& url) {
    return nlohmann::json{
        {"statusCode", status_code},
        {"message",    message},
        {"url",        url}
    };
}

/// Split a base URL of the form  http[s]://host[:port][/prefix]  into the
/// scheme+host+port part (used to construct httplib::Client) and the optional
/// path prefix.
/// Returns {host_part, path_prefix} where host_part is suitable for
/// httplib::Client(host_part).
std::pair<std::string, std::string> split_base_url(const std::string& base_url) {
    // Find scheme end ("://")
    const auto scheme_end = base_url.find("://");
    if (scheme_end == std::string::npos)
        throw std::invalid_argument("RestDataSource: base_url missing scheme: " + base_url);

    const std::string scheme     = base_url.substr(0, scheme_end);   // "http" or "https"
    const std::string after_sch  = base_url.substr(scheme_end + 3);  // "host[:port][/...]"

    const auto slash_pos = after_sch.find('/');
    const std::string host_port    = after_sch.substr(0, slash_pos);
    const std::string path_prefix  =
        (slash_pos != std::string::npos) ? after_sch.substr(slash_pos) : "";

    return {scheme + "://" + host_port, path_prefix};
}

} // anonymous namespace

// ── RestDataSource::fetch ─────────────────────────────────────────────────────

nlohmann::json RestDataSource::fetch(const DataSourceConfig& config,
                                      const std::string&      path,
                                      const std::string&      method,
                                      const nlohmann::json&   body,
                                      const std::string&      caller_bearer_token) {
    if (config.base_url.empty())
        return make_http_error(0, "base_url is empty", "");

    std::string host_part, path_prefix;
    try {
        auto [h, p] = split_base_url(config.base_url);
        host_part   = std::move(h);
        path_prefix = std::move(p);
    } catch (const std::exception& ex) {
        return make_http_error(0, ex.what(), config.base_url);
    }

    const std::string full_path = path_prefix + path;
    const std::string full_url  = config.base_url + path;

    // ── Build httplib::Client ────────────────────────────────────────────────
    httplib::Client client(host_part);

    const int timeout_sec  = config.timeout_ms / 1000;
    const int timeout_usec = (config.timeout_ms % 1000) * 1000;
    client.set_connection_timeout(timeout_sec, timeout_usec);
    client.set_read_timeout      (timeout_sec, timeout_usec);
    client.set_write_timeout     (timeout_sec, timeout_usec);

    // ── Auth headers ─────────────────────────────────────────────────────────
    httplib::Headers headers;
    if (config.auth_kind == "bearer_passthrough") {
        if (!caller_bearer_token.empty())
            headers.emplace("Authorization", "Bearer " + caller_bearer_token);
    } else if (config.auth_kind == "api_key") {
        if (!config.api_key_header.empty() && !config.api_key_value.empty())
            headers.emplace(config.api_key_header, config.api_key_value);
    }
    // "none" → no extra header

    // ── Execute request ──────────────────────────────────────────────────────
    httplib::Result res;
    const std::string meth_upper = [&] {
        std::string m = method;
        std::transform(m.begin(), m.end(), m.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return m;
    }();

    const std::string body_str   = (body.is_null() || body.empty()) ? "" : body.dump();
    const std::string content_type = "application/json";

    if (meth_upper == "GET" || meth_upper == "DELETE") {
        res = client.Get(full_path, headers);
    } else if (meth_upper == "POST") {
        res = client.Post(full_path, headers, body_str, content_type);
    } else if (meth_upper == "PUT") {
        res = client.Put(full_path, headers, body_str, content_type);
    } else if (meth_upper == "PATCH") {
        res = client.Patch(full_path, headers, body_str, content_type);
    } else {
        return make_http_error(0, "Unsupported HTTP method: " + method, full_url);
    }

    // ── Handle result ────────────────────────────────────────────────────────
    if (!res) {
        const auto err_type = res.error();
        const std::string msg = "connection error: " + httplib::to_string(err_type);
        spdlog::warn("RestDataSource::fetch: {} → {}", full_url, msg);
        return make_http_error(0, msg, full_url);
    }

    if (res->status < 200 || res->status >= 300) {
        spdlog::warn("RestDataSource::fetch: {} returned HTTP {}", full_url, res->status);
        return make_http_error(res->status, res->body, full_url);
    }

    // ── Parse response body ──────────────────────────────────────────────────
    const auto& response_body = res->body;
    if (response_body.empty()) {
        return nlohmann::json{};
    }
    try {
        return nlohmann::json::parse(response_body);
    } catch (const nlohmann::json::parse_error&) {
        // Upstream returned non-JSON — wrap as a string value
        return nlohmann::json{{"data", response_body}};
    }
}

} // namespace isched::v0_0_1::backend
