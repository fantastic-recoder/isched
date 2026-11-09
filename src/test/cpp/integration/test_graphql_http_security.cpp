// SPDX-License-Identifier: MPL-2.0
/**
 * @file test_graphql_http_security.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief HTTP regression tests for the browser session security contract.
 */

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <isched/backend/isched_Server.hpp>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

namespace {
constexpr int http_port = 18108;
constexpr int ws_port = 18109;
const std::string origin = "http://localhost:18108";
const std::string email = "security-test@example.com";
const std::string password = "SecurityTest123!";

struct Fixture {
    std::unique_ptr<Server> server;

    Fixture() {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::string data_home = "/tmp/isched-http-security-" + suffix;
        (void)::setenv("XDG_DATA_HOME", data_home.c_str(), 1);
        Server::Configuration config;
        config.port = http_port;
        config.ws_port = ws_port;
        config.max_threads = 4;
        config.allowed_origins = {origin};
        server = Server::create(config);
        REQUIRE(server->start());
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    ~Fixture() {
        if (server && server->get_status() != Server::Status::STOPPED) server->stop();
    }
};

httplib::Result post(httplib::Client& client, const std::string& query,
                     const httplib::Headers& headers, const json& variables = json::object(),
                     const std::string& operation_name = {}) {
    json body = {{"query", query}, {"variables", variables}};
    if (!operation_name.empty()) body["operationName"] = operation_name;
    return client.Post("/graphql", headers,
                       body.dump(),
                       "application/json");
}

const std::string bootstrap =
    R"(mutation($input: BootstrapPlatformAdminInput!) {
      isched { bootstrapPlatformAdmin(input: $input) { csrfToken expiresAt } }
    })";
const json bootstrap_variables = {
    {"input", {{"email", email}, {"password", password}, {"displayName", "Security test"}}}
};
const std::string login =
    R"(mutation($email: String!, $password: String!) {
      isched { login(email: $email, password: $password) { csrfToken expiresAt } }
    })";
const json login_variables = {{"email", email}, {"password", password}};
const std::string current_csrf = "query { isched { currentSessionCsrfToken } }";
const std::string logout = "mutation { isched { logout } }";

std::string session_cookie(const httplib::Result& response) {
    const auto set_cookie = response->get_header_value("Set-Cookie");
    REQUIRE_FALSE(set_cookie.empty());
    CHECK(set_cookie.find("Secure") != std::string::npos);
    CHECK(set_cookie.find("HttpOnly") != std::string::npos);
    CHECK(set_cookie.find("SameSite=Strict") != std::string::npos);
    const auto end = set_cookie.find(';');
    REQUIRE(end != std::string::npos);
    return set_cookie.substr(0, end);
}

std::string csrf_from(const httplib::Result& response, const char* field) {
    REQUIRE(response);
    REQUIRE(response->status == 200);
    const auto body = json::parse(response->body);
    if (body.contains("errors") && body["errors"].is_array()) {
        for (const auto& error : body["errors"]) {
            // Only report fixed diagnostic fields, never server-supplied messages or data.
            if (error.contains("code") && error["code"].is_number_integer())
                UNSCOPED_INFO("GraphQL error code: " << error["code"].get<int>());
        }
    }
    REQUIRE_FALSE(body.contains("errors"));
    const auto& payload = body.at("data").at("isched").at(field);
    REQUIRE_FALSE(payload.contains("token"));
    const auto value = payload.at("csrfToken").get<std::string>();
    REQUIRE_FALSE(value.empty());
    return value;
}

std::string read_session_csrf(httplib::Client& client, const std::string& cookie) {
    const auto response = post(client, current_csrf, {{"Origin", origin}, {"Cookie", cookie}});
    REQUIRE(response);
    REQUIRE(response->status == 200);
    const auto body = json::parse(response->body);
    REQUIRE_FALSE(body.contains("errors"));
    const auto csrf = body.at("data").at("isched").at("currentSessionCsrfToken").get<std::string>();
    REQUIRE_FALSE(csrf.empty());
    return csrf;
}
} // namespace

TEST_CASE_METHOD(Fixture, "HTTP Origin rejection precedes bootstrap side effects",
                 "[integration][security][origin][T052]") {
    httplib::Client client("localhost", http_port);
    client.set_read_timeout(10);
    for (const std::string candidate : {"", "null", "http://localhost:18108/path",
                                        "http://localhost.evil:18108", "https://unlisted.example"}) {
        INFO("Origin: " << candidate);
        httplib::Headers headers;
        if (!candidate.empty()) headers.emplace("Origin", candidate);
        const auto response = post(client, bootstrap, headers, bootstrap_variables);
        REQUIRE(response);
        REQUIRE(response->status == 403);
        REQUIRE(response->get_header_value("Access-Control-Allow-Origin").empty());
    }
    const auto status = post(client,
        "query { isched { platformBootstrapStatus { isBootstrapAllowed } } }",
        {{"Origin", origin}});
    REQUIRE(status);
    const auto body = json::parse(status->body);
    REQUIRE(body.at("data").at("isched").at("platformBootstrapStatus")
                .at("isBootstrapAllowed").get<bool>());
}

TEST_CASE_METHOD(Fixture, "Anonymous mixed bootstrap mutation cannot create an account",
                 "[integration][security][csrf]") {
    httplib::Client client("localhost", http_port);
    client.set_read_timeout(10);
    const auto denied = post(client,
        R"(mutation($input: BootstrapPlatformAdminInput!) {
          isched { ...Boot logout }
        }
        fragment Boot on IschedMutationNamespace {
          bootstrapPlatformAdmin(input: $input) { csrfToken expiresAt }
        })", {{"Origin", origin}}, bootstrap_variables);
    REQUIRE(denied);
    REQUIRE(json::parse(denied->body).contains("errors"));
    const auto status = post(client,
        "{ isched { platformBootstrapStatus { isBootstrapAllowed } } }", {{"Origin", origin}});
    REQUIRE(status);
    REQUIRE(json::parse(status->body).at("data").at("isched").at("platformBootstrapStatus")
                .at("isBootstrapAllowed").get<bool>());
}

TEST_CASE_METHOD(Fixture, "Cookie session CSRF lifecycle protects mutations",
                 "[integration][security][csrf][T052]") {
    httplib::Client client("localhost", http_port);
    client.set_read_timeout(10);
    const auto boot = post(client, bootstrap, {{"Origin", origin}}, bootstrap_variables);
    auto csrf1 = csrf_from(boot, "bootstrapPlatformAdmin");
    const auto cookie1 = session_cookie(boot);
    csrf1 = read_session_csrf(client, cookie1);

    // Select the parsed operation, not a textual "mutation" in another operation.
    const std::string multi_operation =
        "query Read { isched { currentSessionCsrfToken } } "
        "mutation Write { isched { logout } }";
    const auto selected_read = post(client, multi_operation,
        {{"Origin", origin}, {"Cookie", cookie1}}, json::object(), "Read");
    REQUIRE(selected_read);
    REQUIRE_FALSE(json::parse(selected_read->body).contains("errors"));
    const auto selected_write = post(client, multi_operation,
        {{"Origin", origin}, {"Cookie", cookie1}}, json::object(), "Write");
    REQUIRE(selected_write);
    REQUIRE(json::parse(selected_write->body).contains("errors"));
    REQUIRE(read_session_csrf(client, cookie1) == csrf1);

    // Duplicate session cookie names are ambiguous and cannot authenticate.
    const auto duplicate = post(client, current_csrf,
        {{"Origin", origin}, {"Cookie", cookie1 + "; isched_session="}});
    REQUIRE(duplicate);
    const auto duplicate_body = json::parse(duplicate->body);
    REQUIRE((duplicate_body.contains("errors") ||
             duplicate_body.at("data").at("isched").at("currentSessionCsrfToken").is_null()));

    // A cookie value used as a bearer credential is not an alternative transport.
    const auto separator = cookie1.find('=');
    REQUIRE(separator != std::string::npos);
    const auto bearer = post(client, current_csrf,
        {{"Origin", origin}, {"Authorization", "Bearer " + cookie1.substr(separator + 1)}});
    REQUIRE(bearer);
    const auto bearer_body = json::parse(bearer->body);
    REQUIRE((bearer_body.contains("errors") ||
             bearer_body.at("data").at("isched").at("currentSessionCsrfToken").is_null()));

    // Login is a pre-session exception; a valid session cannot bypass mutation CSRF.
    const auto denied_login = post(client, login, {{"Origin", origin}, {"Cookie", cookie1}},
                                   login_variables);
    REQUIRE(denied_login);
    REQUIRE(json::parse(denied_login->body).contains("errors"));
    csrf1 = read_session_csrf(client, cookie1);

    const auto second = post(client,
        R"(mutation($email: String!, $password: String!) {
          root: isched { ...LoginFields }
        }
        fragment LoginFields on IschedMutationNamespace {
          signedIn: login(email: $email, password: $password) { expiresAt }
        })", {{"Origin", origin}}, login_variables);
    REQUIRE(second);
    const auto second_body = json::parse(second->body);
    REQUIRE_FALSE(second_body.contains("errors"));
    REQUIRE_FALSE(second_body.at("data").at("root").at("signedIn").contains("token"));
    REQUIRE_FALSE(second_body.at("data").at("root").at("signedIn").at("expiresAt").get<std::string>().empty());
    const auto cookie2 = session_cookie(second);
    auto csrf2 = read_session_csrf(client, cookie2);
    REQUIRE(csrf1 != csrf2);

    // A missing, invalid, or cross-session value cannot execute logout.
    for (const auto& value : std::array<std::string, 3>{"", "invalid-csrf", csrf2}) {
        httplib::Headers headers{{"Origin", origin}, {"Cookie", cookie1}};
        if (!value.empty()) headers.emplace("X-CSRF-Token", value);
        const auto denied = post(client, logout, headers);
        REQUIRE(denied);
        REQUIRE(json::parse(denied->body).contains("errors"));
        csrf1 = read_session_csrf(client, cookie1);
    }

    const auto done = post(client,
        "mutation { aliasRoot: isched { ...Fields } } "
        "fragment Fields on IschedMutationNamespace { aliasLogout: logout }",
        {{"Origin", origin}, {"Cookie", cookie1}, {"X-CSRF-Token", csrf1}});
    REQUIRE(done);
    const auto body = json::parse(done->body);
    REQUIRE_FALSE(body.contains("errors"));
    REQUIRE(body.at("data").at("aliasRoot").at("aliasLogout") == true);
    const auto expired = done->get_header_value("Set-Cookie");
    REQUIRE(expired.find("Secure") != std::string::npos);
    REQUIRE(expired.find("HttpOnly") != std::string::npos);
    REQUIRE(expired.find("SameSite=Strict") != std::string::npos);
    REQUIRE((expired.find("Max-Age=0") != std::string::npos ||
             expired.find("Expires=") != std::string::npos));
    const auto revoked = post(client, current_csrf, {{"Origin", origin}, {"Cookie", cookie1}});
    REQUIRE(revoked);
    const auto revoked_body = json::parse(revoked->body);
    REQUIRE((revoked_body.contains("errors") ||
             revoked_body.at("data").at("isched").at("currentSessionCsrfToken").is_null()));
    REQUIRE(revoked->get_header_value("Set-Cookie").find("Max-Age=0") != std::string::npos);
    REQUIRE_FALSE(read_session_csrf(client, cookie2).empty());
}

TEST_CASE_METHOD(Fixture, "Pre-session bootstrap remains rate limited",
                 "[integration][security][csrf][ratelimit]") {
    httplib::Client client("localhost", http_port);
    client.set_read_timeout(10);
    bool limited = false;
    for (int i = 0; i < 7; ++i) {
        const auto response = post(client, bootstrap, {{"Origin", origin}},
            json{{"input", {{"email", ""}, {"password", ""}, {"displayName", ""}}}});
        REQUIRE(response);
        const auto body = json::parse(response->body);
        REQUIRE(body.contains("errors"));
        for (const auto& error : body["errors"])
            if (error.value("message", "").find("RATE_LIMITED") != std::string::npos)
                limited = true;
    }
    REQUIRE(limited);
    const auto status = post(client,
        "{ isched { platformBootstrapStatus { isBootstrapAllowed } } }", {{"Origin", origin}});
    REQUIRE(status);
    REQUIRE(json::parse(status->body).at("data").at("isched").at("platformBootstrapStatus")
                .at("isBootstrapAllowed").get<bool>());
}
