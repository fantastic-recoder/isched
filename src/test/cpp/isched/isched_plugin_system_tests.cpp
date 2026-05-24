// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_plugin_system_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for the plugin system (PluginManager, SP-012).
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/backend/isched_PluginSystem.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <unistd.h>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

namespace {

std::shared_ptr<DatabaseManager> make_test_db() {
    static std::atomic<int> counter{0};
    DatabaseManager::Config cfg;
    cfg.base_path = "/tmp/isched_plugin_test_" + std::to_string(static_cast<unsigned long>(getpid())) +
                    "_" + std::to_string(counter.fetch_add(1));
    return std::make_shared<DatabaseManager>(cfg);
}

struct PassthroughRegistrar final : IResolverRegistrar {
    explicit PassthroughRegistrar(GqlExecutor& executor) : m_executor(executor) {}

    void register_resolver(const ResolverPath& scope,
                           const std::string& field_name,
                           ResolverFunction resolver) override {
        m_executor.register_resolver(scope, field_name, std::move(resolver));
    }

    void unregister_resolver(const ResolverPath& scope,
                             const std::string& field_name) override {
        m_executor.unregister_resolver(scope, field_name);
    }

    GqlExecutor& m_executor;
};

class StubRequestPlugin final : public RequestPlugin {
public:
    StubRequestPlugin(PluginManifest manifest, String body = "ok", bool should_throw = false)
        : m_manifest(std::move(manifest)), m_body(std::move(body)), m_should_throw(should_throw) {}

    [[nodiscard]] PluginManifest manifest() const override { return m_manifest; }

    [[nodiscard]] std::optional<PluginHttpResponse> handleRequest(const PluginHttpRequest& request) override {
        if (m_should_throw) {
            throw std::runtime_error("request plugin crash");
        }
        PluginHttpResponse response;
        response.status = 200;
        response.contentType = "text/plain";
        response.body = m_body + ":" + request.target;
        return response;
    }

private:
    PluginManifest m_manifest;
    String m_body;
    bool m_should_throw{false};
};

class StubResolverPlugin final : public ResolverPlugin {
public:
    StubResolverPlugin(PluginManifest manifest, bool should_throw = false)
        : m_manifest(std::move(manifest)), m_should_throw(should_throw) {}

    [[nodiscard]] PluginManifest manifest() const override { return m_manifest; }

    void registerResolvers(IResolverRegistrar& registrar) override {
        for (const auto& binding : m_manifest.resolverBindings) {
            const String field = binding.fieldName;
            const bool should_throw = m_should_throw;
            registrar.register_resolver({}, field,
                [field, should_throw](const json&, const json&, const ResolverCtx&) -> json {
                    if (should_throw) {
                        throw std::runtime_error("resolver crash");
                    }
                    return "resolver:" + field;
                });
        }
    }

    void unregisterResolvers(IResolverRegistrar& registrar) override {
        for (const auto& binding : m_manifest.resolverBindings) {
            registrar.unregister_resolver({}, binding.fieldName);
        }
    }

private:
    PluginManifest m_manifest;
    bool m_should_throw{false};
};

PluginManifest make_request_manifest(const std::string& id,
                                     const std::string& route,
                                     const std::string& method = "GET") {
    PluginManifest manifest;
    manifest.id = id;
    manifest.kind = PluginKind::Request;
    manifest.name = id;
    manifest.version = "1.0.0";
    manifest.libraryPath = "/tmp/" + id + ".so";
    manifest.requestRoutes.push_back({method, route});
    return manifest;
}

PluginManifest make_resolver_manifest(const std::string& id,
                                      const std::string& field) {
    PluginManifest manifest;
    manifest.id = id;
    manifest.kind = PluginKind::Resolver;
    manifest.name = id;
    manifest.version = "1.0.0";
    manifest.libraryPath = "/tmp/" + id + ".so";
    manifest.resolverBindings.push_back({.scope = {}, .fieldName = field, .requiredRoles = {}});
    return manifest;
}

} // namespace

TEST_CASE("Plugin ABI defaults to server ABI version", "[plugin][abi]") {
    PluginManifest manifest;
    CHECK(manifest.abi_version == PLUGIN_ABI_VERSION);
}

TEST_CASE("Plugin manifest JSON persists abi_version", "[plugin][abi]") {
    PluginManager manager{make_test_db()};
    auto manifest = make_request_manifest("abi-test", "/plugin/abi");
    manifest.abi_version = "1.5.3";

    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest), manifest);
    const auto record = manager.get_plugin("abi-test");
    REQUIRE(record.has_value());

    const auto parsed = json::parse(record->manifestJson, nullptr, false);
    REQUIRE_FALSE(parsed.is_discarded());
    REQUIRE(parsed.is_object());
    CHECK(parsed.value("abi_version", "") == "1.5.3");
}

TEST_CASE("Dispatch matches template routes and normalizes repeated slashes", "[plugin][routes]") {
    PluginManager manager{make_test_db()};
    const auto manifest = make_request_manifest("templated", "/plugin/items/{id}");
    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest, "templated"), manifest);

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/items/42";
    auto response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->status == 200);

    request.target = "/plugin//items///latest/";
    response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->status == 200);

    request.target = "/plugin/items";
    CHECK_FALSE(manager.dispatch_request(request).has_value());
}

TEST_CASE("Dispatch keeps method isolation for template routes", "[plugin][routes]") {
    PluginManager manager{make_test_db()};
    manager.inject_loaded_plugin(
        std::make_shared<StubRequestPlugin>(make_request_manifest("get", "/plugin/a/{id}", "GET"), "get"),
        make_request_manifest("get", "/plugin/a/{id}", "GET"));
    manager.inject_loaded_plugin(
        std::make_shared<StubRequestPlugin>(make_request_manifest("post", "/plugin/a/{id}", "POST"), "post"),
        make_request_manifest("post", "/plugin/a/{id}", "POST"));

    PluginHttpRequest request;
    request.target = "/plugin/a/99";

    request.method = "GET";
    auto response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->body.find("get") != String::npos);

    request.method = "POST";
    response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->body.find("post") != String::npos);
}

TEST_CASE("Request plugin crash returns 500 and auto-unloads plugin", "[plugin][crash]") {
    PluginManager manager{make_test_db()};
    const auto crashing_manifest = make_request_manifest("crash", "/plugin/crash");
    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(crashing_manifest, "", true), crashing_manifest);

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/crash";
    const auto crash_response = manager.dispatch_request(request);
    REQUIRE(crash_response.has_value());
    CHECK(crash_response->status == 500);
    CHECK_FALSE(manager.get_plugin("crash").has_value());

    const auto replacement_manifest = make_request_manifest("replacement", "/plugin/crash");
    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(replacement_manifest, "replacement"), replacement_manifest);
    const auto replacement_response = manager.dispatch_request(request);
    REQUIRE(replacement_response.has_value());
    CHECK(replacement_response->status == 200);
}

TEST_CASE("Resolver plugin uses registrar interface", "[plugin][resolver]") {
    auto db = make_test_db();
    GqlExecutor executor{db};
    PassthroughRegistrar registrar{executor};

    const auto manifest = make_resolver_manifest("resolver", "plugField");
    auto plugin = std::make_shared<StubResolverPlugin>(manifest);
    REQUIRE_NOTHROW(plugin->registerResolvers(registrar));
    const auto result = executor.execute("{ plugField }");
    REQUIRE(result.is_success());
    REQUIRE_NOTHROW(plugin->unregisterResolvers(registrar));
}

TEST_CASE("Plugin admin GraphQL fields require platform_admin", "[plugin][graphql][auth]") {
    auto db = make_test_db();
    auto manager = std::make_shared<PluginManager>(db);
    auto manifest = make_request_manifest("gqlplug", "/plugin/gql");
    manager->inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest), manifest);

    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);

    ResolverCtx non_admin;
    non_admin.roles = {"role_tenant_admin"};
    non_admin.tenant_id = "tenant-a";
    non_admin.db = db;

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    CHECK_FALSE(executor.execute("query { isched { plugins { id } } }", "{}", non_admin).is_success());
    CHECK_FALSE(executor.execute("query { isched { plugin(id: \"gqlplug\") { id } } }", "{}", non_admin).is_success());
    CHECK_FALSE(executor.execute("mutation { isched { loadPlugin(input:{libraryPath:\"/tmp/nope.so\", kind:\"request\"}) { success } } }", "{}", non_admin).is_success());
    CHECK_FALSE(executor.execute("mutation { isched { unloadPlugin(id:\"gqlplug\") { success } } }", "{}", non_admin).is_success());

    CHECK(executor.execute("query { isched { plugins { id } } }", "{}", admin).is_success());
    CHECK(executor.execute("query { isched { plugin(id: \"gqlplug\") { id } } }", "{}", admin).is_success());
}

TEST_CASE("OpenAPI export skips invalid fragments for injected plugins", "[plugin][openapi]") {
    PluginManager manager{make_test_db()};
    auto manifest = make_request_manifest("bad-fragment", "/plugin/openapi");
    manifest.openApiFragmentJson = "not valid json";
    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest), manifest);

    const auto doc = manager.export_openapi_document();
    REQUIRE(doc.is_object());
    CHECK(doc["paths"].empty());
}
