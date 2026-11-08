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
#include <isched/backend/isched_SubscriptionBroker.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

namespace {

std::shared_ptr<DatabaseManager> make_test_db() {
    static std::atomic<int> counter{0};
    DatabaseManager::Config cfg;
    cfg.base_path = "/tmp/isched_plugin_test_" + std::to_string(static_cast<unsigned long>(getpid())) +
                    "_" + std::to_string(counter.fetch_add(1));
    cfg.system_db_path = cfg.base_path + "/isched_system.db";
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

struct ScopedTempDir final {
    explicit ScopedTempDir(std::filesystem::path path) : dir(std::move(path)) {
        std::filesystem::create_directories(dir);
    }
    ~ScopedTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::filesystem::path dir;
};

class ScopedEnvVar final {
public:
    ScopedEnvVar(std::string key, std::optional<std::string> value)
        : m_key(std::move(key)) {
        if (const char* existing = std::getenv(m_key.c_str()); existing != nullptr) {
            m_previous = std::string(existing);
        }
        if (value.has_value()) {
            setenv(m_key.c_str(), value->c_str(), 1);
        } else {
            unsetenv(m_key.c_str());
        }
    }

    ~ScopedEnvVar() {
        if (m_previous.has_value()) {
            setenv(m_key.c_str(), m_previous->c_str(), 1);
        } else {
            unsetenv(m_key.c_str());
        }
    }

private:
    std::string m_key;
    std::optional<std::string> m_previous;
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
    StubResolverPlugin(PluginManifest manifest,
                       bool should_throw = false,
                       String schema_override = {},
                       bool throw_on_register = false)
        : m_manifest(std::move(manifest))
        , m_should_throw(should_throw)
        , m_schema_override(std::move(schema_override))
        , m_throw_on_register(throw_on_register) {}

    [[nodiscard]] PluginManifest manifest() const override { return m_manifest; }

    [[nodiscard]] String graphqlSchema() const override { return m_schema_override; }

    void registerResolvers(IResolverRegistrar& registrar) override {
        if (m_throw_on_register) {
            throw std::runtime_error("registerResolvers failure");
        }
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
    String m_schema_override;
    bool m_throw_on_register{false};
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
    manifest.resolverSchemaSdl = "extend type IschedQueryNamespace { " + field + ": String! }";
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

TEST_CASE("US5 inject_loaded_plugin supports request lifecycle without dynamic loading", "[plugin][us5][inject]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginManager manager{db};
    const auto manifest = make_request_manifest("inject-request", "/plugin/inject/{id}");
    manager.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest, "injected"), manifest);

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/inject/42";
    const auto response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->status == 200);
    CHECK(response->body.find("injected") != String::npos);

    const auto unload = manager.unload_plugin("inject-request");
    CHECK((unload.success || !unload.errors.empty()));
    CHECK_FALSE(manager.dispatch_request(request).has_value());
    const auto request_record = manager.get_plugin("inject-request");
    REQUIRE(request_record.has_value());
    CHECK_FALSE(request_record->isLoaded);
}

TEST_CASE("US5 inject_loaded_plugin supports resolver lifecycle with executor wiring", "[plugin][us5][inject]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    const auto manifest = make_resolver_manifest("inject-resolver", "injectField");
    manager->inject_loaded_plugin(std::make_shared<StubResolverPlugin>(manifest), manifest);

    CHECK(executor.has_resolver({}, "injectField"));
    const auto query = executor.execute("{ injectField }");
    REQUIRE(query.is_success());
    CHECK(query.data.value("injectField", "") == "resolver:injectField");

    const auto unload = manager->unload_plugin("inject-resolver");
    CHECK((unload.success || !unload.errors.empty()));
    CHECK_FALSE(executor.has_resolver({}, "injectField"));
    const auto resolver_record = manager->get_plugin("inject-resolver");
    REQUIRE(resolver_record.has_value());
    CHECK_FALSE(resolver_record->isLoaded);
}

TEST_CASE("US5 load edge-cases reject with descriptive errors and no partial side effects", "[plugin][us5][edge-cases]") {
#if !defined(ISCHED_REQUEST_PLUGIN_PATH) || !defined(ISCHED_HELLO_PLUGIN_PATH)
    SKIP("request/hello demo plugin paths are not configured for this test target");
#else
    const std::string request_plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    const std::string resolver_plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(request_plugin_path));
    REQUIRE(std::filesystem::exists(resolver_plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);
    manager->set_plugins_root("/");

    auto list_count = [&]() -> std::size_t {
        const auto records = db->list_plugin_records();
        REQUIRE(records);
        return records.value().size();
    };

    SECTION("missing request export is rejected") {
        const auto baseline_count = list_count();
        const auto result = manager->load_plugin(resolver_plugin_path, PluginKind::Request, "us5-missing-request-export");
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK((
            result.errors.front().find("isched_create_request_plugin") != std::string::npos ||
            result.errors.front().find("create_request_plugin") != std::string::npos));
        CHECK(list_count() == baseline_count);
        CHECK_FALSE(manager->get_plugin("us5-missing-request-export").has_value());
    }

    SECTION("missing resolver export is rejected") {
        const auto baseline_count = list_count();
        const auto result = manager->load_plugin(request_plugin_path, PluginKind::Resolver, "us5-missing-resolver-export");
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK((
            result.errors.front().find("isched_create_resolver_plugin") != std::string::npos ||
            result.errors.front().find("create_resolver_plugin") != std::string::npos));
        CHECK(list_count() == baseline_count);
        CHECK_FALSE(manager->get_plugin("us5-missing-resolver-export").has_value());
    }

    SECTION("invalid shared library file is rejected") {
        static std::atomic<int> invalid_counter{0};
        ScopedTempDir sandbox{std::filesystem::temp_directory_path() /
            ("isched_invalid_plugin_" + std::to_string(static_cast<unsigned long>(getpid())) +
             "_" + std::to_string(invalid_counter.fetch_add(1)))};
        const auto fake_library = sandbox.dir / "not_a_real_plugin.so";
        {
            std::ofstream out(fake_library);
            out << "not an elf shared library";
        }

        manager->set_plugins_root(sandbox.dir);
        const auto baseline_count = list_count();
        const auto result = manager->load_plugin(fake_library.string(), PluginKind::Request, "us5-invalid-library");
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK(list_count() == baseline_count);
        CHECK_FALSE(manager->get_plugin("us5-invalid-library").has_value());
    }

    SECTION("duplicate manifest identity/path and ABI mismatch are rejected") {
        manager->set_plugins_root(std::filesystem::path(request_plugin_path).parent_path());
        ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/us5/base/{id}"};
        ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", "GET"};
        ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};
        ScopedEnvVar abi_ok{"ISCHED_REQUEST_PLUGIN_ABI_VERSION", "1.0.0"};

        const auto base = manager->load_plugin(request_plugin_path, PluginKind::Request, "us5-dup-base");
        REQUIRE(base.success);

        const auto duplicate_id = manager->load_plugin(request_plugin_path, PluginKind::Request, "us5-dup-base");
        CHECK_FALSE(duplicate_id.success);
        REQUIRE_FALSE(duplicate_id.errors.empty());
        CHECK(duplicate_id.errors.front().find("already loaded") != std::string::npos);

        const auto duplicate_path = manager->load_plugin(request_plugin_path, PluginKind::Request, "us5-dup-path");
        CHECK_FALSE(duplicate_path.success);
        REQUIRE_FALSE(duplicate_path.errors.empty());
        CHECK(duplicate_path.errors.front().find("conflict") != std::string::npos);
        CHECK_FALSE(manager->get_plugin("us5-dup-path").has_value());

        REQUIRE(manager->unload_plugin("us5-dup-base").success);

        ScopedEnvVar abi_bad{"ISCHED_REQUEST_PLUGIN_ABI_VERSION", "2.0.0"};
        const auto abi_mismatch = manager->load_plugin(request_plugin_path, PluginKind::Request, "us5-abi-mismatch");
        CHECK_FALSE(abi_mismatch.success);
        REQUIRE_FALSE(abi_mismatch.errors.empty());
        CHECK(abi_mismatch.errors.front().find("ABI version mismatch") != std::string::npos);
        CHECK_FALSE(manager->get_plugin("us5-abi-mismatch").has_value());
    }
#endif
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

TEST_CASE("US1 request plugin load + dispatch + unload persists lifecycle state", "[plugin][us1][load][dispatch]") {
#ifndef ISCHED_REQUEST_PLUGIN_PATH
    SKIP("request_demo_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginManager manager{db};
    manager.set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/items/{id}"};
    ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", "get"};
    ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};

    const auto load_result = manager.load_plugin(plugin_path, PluginKind::Request, "us1-request");
    REQUIRE(load_result.success);
    REQUIRE(load_result.pluginId == "us1-request");

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/items/42/";
    const auto response = manager.dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->status == 200);

    const auto record_loaded = db->get_plugin_record("us1-request");
    REQUIRE(record_loaded);
    REQUIRE(record_loaded.value().has_value());
    CHECK(record_loaded.value()->isLoaded);
    CHECK_FALSE(record_loaded.value()->loadedAt.empty());

    const auto unload_result = manager.unload_plugin("us1-request");
    REQUIRE(unload_result.success);
    CHECK(unload_result.pluginId == "us1-request");

    const auto record_unloaded = db->get_plugin_record("us1-request");
    REQUIRE(record_unloaded);
    REQUIRE(record_unloaded.value().has_value());
    CHECK_FALSE(record_unloaded.value()->isLoaded);
    CHECK_FALSE(record_unloaded.value()->unloadedAt.empty());

    const auto idempotent = manager.unload_plugin("us1-request");
    CHECK(idempotent.success);
#endif
}

TEST_CASE("FR-006 route conflict detection is deterministic across normalization rules", "[plugin][us1][route-conflicts]") {
#ifndef ISCHED_REQUEST_PLUGIN_PATH
    SKIP("request_demo_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginManager manager{db};
    manager.set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};

    auto load_base = [&](const std::string& route_path, const std::string& method_name, const std::string& plugin_id) {
        ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", route_path};
        ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", method_name};
        return manager.load_plugin(plugin_path, PluginKind::Request, plugin_id);
    };

    auto reset_base = [&]() {
        std::ignore = manager.unload_plugin("base");
    };

    SECTION("method normalization conflicts") {
        REQUIRE(load_base("/plugin/items/{id}", "get", "base").success);
        const auto candidate = load_base("/plugin/items/{id}", "GET", "candidate");
        CHECK_FALSE(candidate.success);
        REQUIRE_FALSE(candidate.errors.empty());
        CHECK(candidate.errors.front().find("conflicts") != String::npos);
        reset_base();
    }

    SECTION("trailing slash normalization conflicts") {
        REQUIRE(load_base("/plugin/items/{id}/", "GET", "base").success);
        const auto candidate = load_base("/plugin/items/{id}", "GET", "candidate");
        CHECK_FALSE(candidate.success);
        REQUIRE_FALSE(candidate.errors.empty());
        CHECK(candidate.errors.front().find("conflicts") != String::npos);
        reset_base();
    }

    SECTION("different segment count does not conflict") {
        REQUIRE(load_base("/plugin/items/{id}", "GET", "base").success);
        const auto candidate = load_base("/plugin/items/{id}/details", "GET", "candidate");
        CHECK(candidate.success);
        std::ignore = manager.unload_plugin("candidate");
        reset_base();
    }

    SECTION("templated and static same-shape paths conflict") {
        REQUIRE(load_base("/plugin/items/{id}", "GET", "base").success);
        const auto candidate = load_base("/plugin/items/latest", "GET", "candidate");
        CHECK_FALSE(candidate.success);
        REQUIRE_FALSE(candidate.errors.empty());
        CHECK(candidate.errors.front().find("conflicts") != String::npos);
        reset_base();
    }

    SECTION("path segment case remains sensitive") {
        REQUIRE(load_base("/plugin/items/{id}", "GET", "base").success);
        const auto candidate = load_base("/plugin/Items/{id}", "GET", "candidate");
        CHECK(candidate.success);
        std::ignore = manager.unload_plugin("candidate");
        reset_base();
    }
#endif
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
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());
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

TEST_CASE("US3 plugins query returns empty state and loaded records", "[plugin][us3][plugins]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    GqlExecutor executor{db};

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto baseline = executor.execute("query { isched { plugins { id } } }", "{}", admin);
    REQUIRE(baseline.is_success());
    REQUIRE(baseline.data["isched"]["plugins"].is_array());
    const auto baseline_size = baseline.data["isched"]["plugins"].size();

    static std::atomic<int> us3_counter{0};
    const auto suffix = std::to_string(static_cast<unsigned long>(::getpid())) + "_" + std::to_string(us3_counter.fetch_add(1));
    const auto request_id = "us3-request-" + suffix;
    const auto resolver_id = "us3-resolver-" + suffix;

    PluginRecord request_record;
    request_record.id = request_id;
    request_record.kind = "request";
    request_record.name = request_id;
    request_record.version = "1.0.0";
    request_record.libraryPath = "/tmp/" + request_id + ".so";
    request_record.isLoaded = true;
    request_record.manifestJson = json{{"id", request_id}, {"kind", "request"}, {"name", request_id},
        {"abi_version", "1.0.0"}, {"openApiFragmentJson", R"({\"openapi\":\"3.0.0\"})"}}.dump();
    REQUIRE(db->upsert_plugin_record(request_record).has_value());

    PluginRecord resolver_record;
    resolver_record.id = resolver_id;
    resolver_record.kind = "resolver";
    resolver_record.name = resolver_id;
    resolver_record.version = "1.0.0";
    resolver_record.libraryPath = "/tmp/" + resolver_id + ".so";
    resolver_record.isLoaded = true;
    resolver_record.manifestJson = json{{"id", resolver_id}, {"kind", "resolver"}, {"name", resolver_id},
        {"abi_version", "1.0.0"}, {"openApiFragmentJson", ""}}.dump();
    REQUIRE(db->upsert_plugin_record(resolver_record).has_value());

    const auto loaded = executor.execute("query { isched { plugins { id kind name isLoaded } } }", "{}", admin);
    REQUIRE(loaded.is_success());
    REQUIRE(loaded.data["isched"]["plugins"].is_array());
    REQUIRE(loaded.data["isched"]["plugins"].size() >= baseline_size + 2);

    std::vector<std::string> ids;
    for (const auto& row : loaded.data["isched"]["plugins"]) {
        ids.push_back(row.value("id", ""));
    }
    std::sort(ids.begin(), ids.end());
    CHECK(std::find(ids.begin(), ids.end(), request_id) != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), resolver_id) != ids.end());
}

TEST_CASE("US3 plugin(id) returns record and manifest metadata payload", "[plugin][us3][plugin]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    GqlExecutor executor{db};

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    static std::atomic<int> us3_manifest_counter{0};
    const auto plugin_id = "us3-manifest-" + std::to_string(static_cast<unsigned long>(::getpid())) + "_" + std::to_string(us3_manifest_counter.fetch_add(1));
    PluginRecord record;
    record.id = plugin_id;
    record.kind = "request";
    record.name = plugin_id;
    record.version = "1.0.0";
    record.libraryPath = "/tmp/" + plugin_id + ".so";
    record.isLoaded = true;
    record.manifestJson = json{{"id", plugin_id}, {"kind", "request"}, {"name", plugin_id},
        {"abi_version", "1.2.0"}, {"openApiFragmentJson", R"({\"openapi\":\"3.0.0\",\"paths\":{}})"}}.dump();
    REQUIRE(db->upsert_plugin_record(record).has_value());

    const auto found = executor.execute(
        "query { isched { plugin(id:\"" + plugin_id + "\") { id manifestJson } } }",
        "{}",
        admin);
    REQUIRE(found.is_success());
    REQUIRE(found.data["isched"]["plugin"].is_object());
    CHECK(found.data["isched"]["plugin"]["id"] == plugin_id);
    const auto parsed_manifest = json::parse(found.data["isched"]["plugin"]["manifestJson"].get<std::string>(), nullptr, false);
    REQUIRE(parsed_manifest.is_object());
    CHECK(parsed_manifest.value("abi_version", "") == "1.2.0");
    CHECK(parsed_manifest.value("openApiFragmentJson", "").find("openapi") != std::string::npos);

    const auto missing = executor.execute(
        "query { isched { plugin(id:\"missing-plugin\") { id manifestJson } } }",
        "{}",
        admin);
    REQUIRE(missing.is_success());
    CHECK(missing.data["isched"]["plugin"].is_null());
}

TEST_CASE("US4 startup restore reloads persisted loaded request and resolver plugins", "[plugin][us4][restore]") {
#if !defined(ISCHED_REQUEST_PLUGIN_PATH) || !defined(ISCHED_HELLO_PLUGIN_PATH)
    SKIP("request/hello demo plugin paths are not configured for this test target");
#else
    const std::string request_plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    const std::string resolver_plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(request_plugin_path));
    REQUIRE(std::filesystem::exists(resolver_plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    static std::atomic<int> restore_counter{0};
    const auto suffix = std::to_string(static_cast<unsigned long>(::getpid())) + "_" +
                        std::to_string(restore_counter.fetch_add(1));
    const auto request_id = "us4-request-" + suffix;
    const auto resolver_id = "us4-resolver-" + suffix;
    const auto resolver_field = "us4Field" + suffix;

    ScopedEnvVar request_route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/us4/" + suffix + "/{id}"};
    ScopedEnvVar request_method{"ISCHED_REQUEST_PLUGIN_METHOD", "GET"};
    ScopedEnvVar request_openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};
    ScopedEnvVar resolver_schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
    ScopedEnvVar resolver_field_name{"ISCHED_HELLO_PLUGIN_FIELD", resolver_field};
    ScopedEnvVar resolver_throw{"ISCHED_HELLO_PLUGIN_THROW", "0"};

    {
        auto initial_manager = std::make_shared<PluginManager>(db);
        initial_manager->set_plugins_root("/");
        GqlExecutor initial_executor{db};
        initial_executor.set_plugin_manager(initial_manager);
        initial_manager->set_executor(&initial_executor);

        const auto load_request = initial_manager->load_plugin(request_plugin_path, PluginKind::Request, request_id);
        REQUIRE(load_request.success);
        const auto load_resolver = initial_manager->load_plugin(resolver_plugin_path, PluginKind::Resolver, resolver_id);
        REQUIRE(load_resolver.success);
    }

    auto restored_manager = std::make_shared<PluginManager>(db);
    restored_manager->set_plugins_root("/");
    GqlExecutor restored_executor{db};
    restored_executor.set_plugin_manager(restored_manager);
    restored_manager->set_executor(&restored_executor);

    restored_manager->restore_persisted_plugins();

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/us4/" + suffix + "/42";
    const auto response = restored_manager->dispatch_request(request);
    REQUIRE(response.has_value());
    CHECK(response->status == 200);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto gql = restored_executor.execute(
        "query { isched { " + resolver_field + "(message: \"restored\") } }",
        "{}",
        admin);
    REQUIRE(gql.is_success());
    CHECK(gql.data["isched"][resolver_field] == "restored");

    const auto request_record = db->get_plugin_record(request_id);
    REQUIRE(request_record);
    REQUIRE(request_record.value().has_value());
    CHECK(request_record.value()->restoreAttempts == 0);
    CHECK(request_record.value()->lastRestoreError.empty());
    CHECK_FALSE(request_record.value()->lastRestoreAttemptAt.empty());

    const auto resolver_record = db->get_plugin_record(resolver_id);
    REQUIRE(resolver_record);
    REQUIRE(resolver_record.value().has_value());
    CHECK(resolver_record.value()->restoreAttempts == 0);
    CHECK(resolver_record.value()->lastRestoreError.empty());
    CHECK_FALSE(resolver_record.value()->lastRestoreAttemptAt.empty());
#endif
}

TEST_CASE("US4 startup restore skips persisted plugins marked as unloaded", "[plugin][us4][restore]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginRecord record;
    record.id = "us4-unloaded";
    record.kind = "request";
    record.name = "us4-unloaded";
    record.version = "1.0.0";
    record.libraryPath = "/tmp/us4-unloaded.so";
    record.manifestJson = json{{"id", record.id}, {"kind", "request"}, {"name", record.name},
                               {"version", record.version}, {"libraryPath", record.libraryPath},
                               {"abi_version", "1.0.0"}, {"requestRoutes", json::array()}}.dump();
    record.isLoaded = false;
    REQUIRE(db->upsert_plugin_record(record));

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root("/");
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    manager->restore_persisted_plugins();

    const auto persisted = db->get_plugin_record(record.id);
    REQUIRE(persisted);
    REQUIRE(persisted.value().has_value());
    CHECK_FALSE(persisted.value()->isLoaded);
    CHECK(persisted.value()->restoreAttempts == 0);
    CHECK(persisted.value()->lastRestoreError.empty());
    CHECK(persisted.value()->lastRestoreAttemptAt.empty());
}

TEST_CASE("US4 restore failure updates retry metadata and continues", "[plugin][us4][restore]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginRecord missing_record;
    missing_record.id = "us4-missing";
    missing_record.kind = "request";
    missing_record.name = "us4-missing";
    missing_record.version = "1.0.0";
    missing_record.libraryPath = "/tmp/isched_us4_missing_plugin.so";
    missing_record.manifestJson = json{{"id", missing_record.id}, {"kind", "request"}, {"name", missing_record.name},
                                       {"version", missing_record.version},
                                       {"libraryPath", missing_record.libraryPath}, {"abi_version", "1.0.0"},
                                       {"requestRoutes", json::array({json{{"method", "GET"}, {"path", "/plugin/missing"}}})}}.dump();
    missing_record.isLoaded = true;
    REQUIRE(db->upsert_plugin_record(missing_record));

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root("/");
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    manager->restore_persisted_plugins();
    auto persisted = db->get_plugin_record(missing_record.id);
    REQUIRE(persisted);
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->isLoaded);
    CHECK(persisted.value()->restoreAttempts == 1);
    CHECK_FALSE(persisted.value()->lastRestoreError.empty());
    CHECK_FALSE(persisted.value()->lastRestoreAttemptAt.empty());
    const auto unload_missing = manager->unload_plugin(missing_record.id);
    CHECK_FALSE(unload_missing.success);
    REQUIRE_FALSE(unload_missing.errors.empty());
    CHECK(unload_missing.errors.front().find("not loaded") != std::string::npos);

    manager->restore_persisted_plugins();
    persisted = db->get_plugin_record(missing_record.id);
    REQUIRE(persisted);
    REQUIRE(persisted.value().has_value());
    CHECK(persisted.value()->restoreAttempts == 2);
    CHECK_FALSE(persisted.value()->lastRestoreError.empty());
    CHECK_FALSE(persisted.value()->lastRestoreAttemptAt.empty());
}

TEST_CASE("FR-022 path-policy rejects unsafe plugin paths before load", "[plugin][path-policy]") {
    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginManager manager{db};

    static std::atomic<int> case_counter{0};
    ScopedTempDir sandbox{
        std::filesystem::temp_directory_path() /
        ("isched_path_policy_" + std::to_string(static_cast<unsigned long>(getpid())) + "_" +
         std::to_string(case_counter.fetch_add(1)))};

    const auto root = sandbox.dir / "plugins-root";
    const auto outside = sandbox.dir / "outside";
    std::filesystem::create_directories(root);
    std::filesystem::create_directories(outside);

    const auto outside_lib = outside / "plugin_escape.so";
    std::ofstream outside_file(outside_lib);
    outside_file << "stub";
    outside_file.close();

    const auto symlink_escape = root / "escape-link.so";
    std::error_code symlink_ec;
    std::filesystem::create_symlink(outside_lib, symlink_escape, symlink_ec);

    manager.set_plugins_root(root);

    const auto baseline_records = db->list_plugin_records();
    REQUIRE(baseline_records);
    const auto baseline_count = baseline_records.value().size();

    auto assert_no_side_effects = [&]() {
        CHECK(manager.list_plugins().size() == baseline_count);
        const auto persisted = db->list_plugin_records();
        REQUIRE(persisted);
        CHECK(persisted.value().size() == baseline_count);
    };

    SECTION("relative library path is rejected") {
        const auto result = manager.load_plugin("relative/plugin.so", PluginKind::Request);
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK(result.errors.front().find("libraryPath must be absolute") != String::npos);
        assert_no_side_effects();
    }

    SECTION("absolute path outside plugins_root is rejected") {
        const auto result = manager.load_plugin(outside_lib.string(), PluginKind::Request);
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK(result.errors.front().find("must resolve under plugins_root") != String::npos);
        assert_no_side_effects();
    }

    SECTION("canonicalization failure is rejected") {
        const auto missing = root / "missing" / "plugin.so";
        const auto result = manager.load_plugin(missing.string(), PluginKind::Request);
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK(result.errors.front().find("cannot canonicalize") != String::npos);
        assert_no_side_effects();
    }

    SECTION("symlink escape outside plugins_root is rejected") {
        if (symlink_ec) {
            SKIP("symlink creation unavailable on this platform");
        }
        const auto result = manager.load_plugin(symlink_escape.string(), PluginKind::Request);
        CHECK_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
        CHECK(result.errors.front().find("must resolve under plugins_root") != String::npos);
        assert_no_side_effects();
    }
}

TEST_CASE("US1 fail-fast load rejects invalid OpenAPI fragment with no side effects", "[plugin][us1][fail-fast]") {
#ifndef ISCHED_REQUEST_PLUGIN_PATH
    SKIP("request_demo_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());
    PluginManager manager{db};
    manager.set_plugins_root(std::filesystem::path(plugin_path).parent_path());

    const auto baseline_records = db->list_plugin_records();
    REQUIRE(baseline_records);
    const auto baseline_count = baseline_records.value().size();

    ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/invalid-openapi"};
    ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", "GET"};
    ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "invalid"};

    const auto result = manager.load_plugin(plugin_path, PluginKind::Request, "invalid-openapi");
    CHECK_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
    CHECK(result.errors.front().find("openApiFragmentJson") != String::npos);
    CHECK_FALSE(manager.get_plugin("invalid-openapi").has_value());

    const auto after_records = db->list_plugin_records();
    REQUIRE(after_records);
    CHECK(after_records.value().size() == baseline_count);
#endif
}

TEST_CASE("Request plugin crash containment persists unload state", "[plugin][us1][crash][persist]") {
#ifndef ISCHED_REQUEST_PLUGIN_PATH
    SKIP("request_demo_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    PluginManager manager{db};
    manager.set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/crash/{id}"};
    ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", "GET"};
    ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};

    const auto load_result = manager.load_plugin(plugin_path, PluginKind::Request, "crash-request");
    REQUIRE(load_result.success);

    PluginHttpRequest request;
    request.method = "GET";
    request.target = "/plugin/crash/boom";
    request.headers["X-ISCHED-CRASH"] = "1";

    const auto crash_response = manager.dispatch_request(request);
    REQUIRE(crash_response.has_value());
    CHECK(crash_response->status == 500);

    const auto record = db->get_plugin_record("crash-request");
    REQUIRE(record);
    REQUIRE(record.value().has_value());
    CHECK_FALSE(record.value()->isLoaded);
    CHECK_FALSE(record.value()->unloadedAt.empty());
#endif
}

TEST_CASE("GraphQL loadPlugin/unloadPlugin report validation conflict and not-found contracts", "[plugin][us1][graphql][contracts]") {
#ifndef ISCHED_REQUEST_PLUGIN_PATH
    SKIP("request_demo_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_REQUEST_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());

    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto validation = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"relative/plugin.so\", kind:\"request\"}) { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(validation.is_success());
    CHECK_FALSE(validation.data["isched"]["loadPlugin"]["success"].get<bool>());
    REQUIRE(validation.data["isched"]["loadPlugin"]["errors"].is_array());
    REQUIRE_FALSE(validation.data["isched"]["loadPlugin"]["errors"].empty());
    CHECK(validation.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("path policy") != std::string::npos);

    ScopedEnvVar route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/items/{id}"};
    ScopedEnvVar method{"ISCHED_REQUEST_PLUGIN_METHOD", "GET"};
    ScopedEnvVar openapi{"ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid"};
    const auto first_load = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"request\", pluginId:\"gql-base\"}) { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(first_load.is_success());
    REQUIRE(first_load.data["isched"]["loadPlugin"]["success"].get<bool>());

    ScopedEnvVar conflict_route{"ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/items/latest"};
    const auto conflict_load = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"request\", pluginId:\"gql-conflict\"}) { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(conflict_load.is_success());
    CHECK_FALSE(conflict_load.data["isched"]["loadPlugin"]["success"].get<bool>());
    REQUIRE_FALSE(conflict_load.data["isched"]["loadPlugin"]["errors"].empty());
    CHECK(conflict_load.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("conflict") != std::string::npos);

    const auto not_found = executor.execute(
        "mutation { isched { unloadPlugin(id:\"missing-plugin\") { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(not_found.is_success());
    CHECK_FALSE(not_found.data["isched"]["unloadPlugin"]["success"].get<bool>());
    REQUIRE_FALSE(not_found.data["isched"]["unloadPlugin"]["errors"].empty());
    CHECK(not_found.data["isched"]["unloadPlugin"]["errors"][0].get<std::string>().find("not") != std::string::npos);

    std::ignore = manager->unload_plugin("gql-base");
#endif
}

TEST_CASE("loadPlugin returns nullable pluginId on failure", "[plugin][graphql]") {
    auto db = make_test_db();
    auto manager = std::make_shared<PluginManager>(db);

    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"/tmp/missing.so\", kind:\"request\"}) { success pluginId errors } } }",
        "{}",
        admin);

    REQUIRE(result.is_success());
    REQUIRE(result.data.contains("isched"));
    REQUIRE(result.data["isched"].contains("loadPlugin"));
    const auto& payload = result.data["isched"]["loadPlugin"];
    REQUIRE(payload.is_object());
    CHECK_FALSE(payload.value("success", true));
    CHECK(payload.contains("pluginId"));
    CHECK(payload["pluginId"].is_null());
    CHECK(payload.contains("errors"));
    CHECK(payload["errors"].is_array());
    CHECK_FALSE(payload["errors"].empty());
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

TEST_CASE("FR-021 schema-supplied resolver load succeeds and plugin graphqlSchema() takes precedence", "[plugin][us2][fr021][schema]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
    ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "pluginEchoUs2"};
    ScopedEnvVar manifest_schema{"ISCHED_HELLO_PLUGIN_MANIFEST_SCHEMA", "extend type IschedQueryNamespace { ignoredManifestField: String }"};
    ScopedEnvVar resolver_throw{"ISCHED_HELLO_PLUGIN_THROW", "0"};

    const auto load_result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-schema-supplied\"}) { success pluginId errors } } }",
        "{}",
        admin);

    REQUIRE(load_result.is_success());
    REQUIRE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());

    const auto query_result = executor.execute(
        "query { isched { pluginEchoUs2(message: \"hello world\") } }",
        "{}",
        admin);
    REQUIRE(query_result.is_success());
    CHECK(query_result.data["isched"]["pluginEchoUs2"] == "hello world");

    const auto persisted = manager->get_plugin("us2-schema-supplied");
    REQUIRE(persisted.has_value());
    const auto parsed_manifest = json::parse(persisted->manifestJson, nullptr, false);
    REQUIRE(parsed_manifest.is_object());
    CHECK(parsed_manifest.value("resolverSchemaSdl", "").find("pluginEchoUs2") != std::string::npos);

#endif
}

TEST_CASE("FR-021 invalid SDL and schema conflict failures preserve no-partial-state", "[plugin][us2][fr021][reject]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto baseline = db->list_plugin_records();
    REQUIRE(baseline);
    const auto baseline_count = baseline.value().size();

    SECTION("invalid schema supplied by plugin is rejected") {
        ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "invalid"};
        ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "us2InvalidField"};
        const auto load_result = executor.execute(
            "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-invalid\"}) { success errors } } }",
            "{}",
            admin);
        REQUIRE(load_result.is_success());
        CHECK_FALSE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
        REQUIRE_FALSE(load_result.data["isched"]["loadPlugin"]["errors"].empty());
        CHECK(load_result.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("invalid GraphQL SDL") != std::string::npos);
        CHECK_FALSE(manager->get_plugin("us2-invalid").has_value());
    }

    SECTION("schema conflict is rejected with rollback") {
        ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
        ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "version"};
        const auto load_result = executor.execute(
            "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-conflict\"}) { success errors } } }",
            "{}",
            admin);
        REQUIRE(load_result.is_success());
        CHECK_FALSE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
        REQUIRE_FALSE(load_result.data["isched"]["loadPlugin"]["errors"].empty());
        CHECK(load_result.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("resolver already registered") != std::string::npos);
    }

    const auto after = db->list_plugin_records();
    REQUIRE(after);
    CHECK(after.value().size() == baseline_count);
#endif
}

TEST_CASE("FR-021 schema conflict rejection (type-field conflicts) leaves no partial merge", "[plugin][us2][fr021][conflict]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const std::string plugin_id = "us2-type-conflict-" + std::to_string(static_cast<unsigned long>(::getpid()));

    const auto baseline_records = db->list_plugin_records();
    REQUIRE(baseline_records);

    ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
    ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "version"};

    const auto load_result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"" + plugin_id + "\"}) { success errors } } }",
        "{}",
        admin);

    REQUIRE(load_result.is_success());
    CHECK_FALSE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
    REQUIRE_FALSE(load_result.data["isched"]["loadPlugin"]["errors"].empty());
    const auto first_error = load_result.data["isched"]["loadPlugin"]["errors"][0].get<std::string>();
    CHECK((
        first_error.find("schema conflict") != std::string::npos ||
        first_error.find("resolver already registered") != std::string::npos
    ));
    CHECK_FALSE(manager->get_plugin(plugin_id).has_value());

    const auto after_records = db->list_plugin_records();
    REQUIRE(after_records);
    CHECK(after_records.value().size() == baseline_records.value().size());

    const auto no_partial_merge = executor.execute(
        "query { isched { version } }",
        "{}",
        admin);
    REQUIRE(no_partial_merge.is_success());
    CHECK(no_partial_merge.data["isched"]["version"].is_string());
#endif
}

TEST_CASE("FR-021 no-schema branch enforces field and resolver preflight rejections", "[plugin][us2][fr021][no-schema]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    SECTION("empty graphqlSchema rejects missing field") {
        ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "empty"};
        ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "us2MissingField"};
        const auto load_result = executor.execute(
            "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-no-schema-missing\"}) { success errors } } }",
            "{}",
            admin);
        REQUIRE(load_result.is_success());
        CHECK_FALSE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
        REQUIRE_FALSE(load_result.data["isched"]["loadPlugin"]["errors"].empty());
        CHECK(load_result.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("field not found") != std::string::npos);
    }

    SECTION("empty graphqlSchema rejects resolver conflict") {
        ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "empty"};
        ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "version"};

        const auto second = executor.execute(
            "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-no-schema-conflict\"}) { success errors } } }",
            "{}",
            admin);
        REQUIRE(second.is_success());
        CHECK_FALSE(second.data["isched"]["loadPlugin"]["success"].get<bool>());
        REQUIRE_FALSE(second.data["isched"]["loadPlugin"]["errors"].empty());
        CHECK(second.data["isched"]["loadPlugin"]["errors"][0].get<std::string>().find("resolver already registered") != std::string::npos);
    }
#endif
}

TEST_CASE("FR-021 empty graphqlSchema() succeeds for existing unclaimed schema field", "[plugin][us2][fr021][no-schema][success]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "empty"};
    ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "pluginOpenApiDocument"};
    ScopedEnvVar resolver_throw{"ISCHED_HELLO_PLUGIN_THROW", "0"};

    const auto load_result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-no-schema-success\"}) { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(load_result.is_success());
    CHECK(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
    CHECK(load_result.data["isched"]["loadPlugin"]["pluginId"] == "us2-no-schema-success");

    const auto query_result = executor.execute(
        "query { isched { pluginOpenApiDocument } }",
        "{}",
        admin);
    REQUIRE(query_result.is_success());
    CHECK(query_result.data["isched"]["pluginOpenApiDocument"] == "hello from hello_plugin");

    CHECK(manager->get_plugin("us2-no-schema-success").has_value());
    std::ignore = manager->unload_plugin("us2-no-schema-success");
#endif
}

TEST_CASE("FR-021 merge/register failures rollback without persistence mutation", "[plugin][us2][fr021][rollback]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto baseline = db->list_plugin_records();
    REQUIRE(baseline);

    ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "invalid"};
    ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "us2RollbackField"};
    const auto failed = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-rollback\"}) { success errors } } }",
        "{}",
        admin);
    REQUIRE(failed.is_success());
    CHECK_FALSE(failed.data["isched"]["loadPlugin"]["success"].get<bool>());

    const auto after_fail = db->list_plugin_records();
    REQUIRE(after_fail);
    CHECK(after_fail.value().size() == baseline.value().size());

    ScopedEnvVar schema_mode_ok{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
    ScopedEnvVar field_name_ok{"ISCHED_HELLO_PLUGIN_FIELD", "us2RollbackField"};
    const auto retry = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-rollback-retry\"}) { success errors } } }",
        "{}",
        admin);
    REQUIRE(retry.is_success());
    CHECK(retry.data["isched"]["loadPlugin"]["success"].get<bool>());

    std::ignore = manager->unload_plugin("us2-rollback-retry");
#endif
}

TEST_CASE("Resolver throwable returns GraphQL error-node payload and auto-unloads plugin", "[plugin][us2][crash]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    manager->set_plugins_root(std::filesystem::path(plugin_path).parent_path());
    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    manager->set_executor(&executor);

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    ScopedEnvVar schema_mode{"ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid"};
    ScopedEnvVar field_name{"ISCHED_HELLO_PLUGIN_FIELD", "us2CrashField"};
    ScopedEnvVar resolver_throw{"ISCHED_HELLO_PLUGIN_THROW", "1"};

    const auto load_result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\", pluginId:\"us2-crash\"}) { success errors } } }",
        "{}",
        admin);
    REQUIRE(load_result.is_success());
    REQUIRE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());

    const auto query_result = executor.execute("query { isched { us2CrashField(message:\"x\") } }", "{}", admin);
    REQUIRE(query_result.is_success());
    REQUIRE(query_result.data.contains("isched"));
    REQUIRE(query_result.data["isched"].contains("us2CrashField"));
    REQUIRE(query_result.data["isched"]["us2CrashField"].is_object());
    CHECK(query_result.data["isched"]["us2CrashField"].contains("errors"));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto record = db->get_plugin_record("us2-crash");
    REQUIRE(record);
    REQUIRE(record.value().has_value());
    CHECK_FALSE(record.value()->isLoaded);
    CHECK_FALSE(record.value()->unloadedAt.empty());
#endif
}

TEST_CASE("Plugin lifecycle subscription emits one event per successful transition", "[plugin][graphql][lifecycle]") {
#ifndef ISCHED_HELLO_PLUGIN_PATH
    SKIP("hello_plugin path is not configured for this test target");
#else
    const std::string plugin_path = ISCHED_HELLO_PLUGIN_PATH;
    REQUIRE(std::filesystem::exists(plugin_path));

    auto db = make_test_db();
    REQUIRE(db->ensure_system_db());
    REQUIRE(db->initialize_plugin_store());

    auto manager = std::make_shared<PluginManager>(db);
    auto broker = SubscriptionBroker::create();
    REQUIRE(broker);

    GqlExecutor executor{db};
    executor.set_plugin_manager(manager);
    executor.set_subscription_broker(broker.get());
    manager->set_executor(&executor);

    std::vector<json> lifecycle_events;
    const auto sub_id = broker->subscribe("plugin-test", "__plugin_lifecycle",
        [&lifecycle_events](const SubscriptionEvent& evt) {
            if (evt.data.is_object() && evt.data.contains("pluginLifecycleEvents") &&
                evt.data["pluginLifecycleEvents"].is_object()) {
                lifecycle_events.push_back(evt.data["pluginLifecycleEvents"]);
            }
        });
    REQUIRE_FALSE(sub_id.empty());

    ResolverCtx admin;
    admin.roles = {"role_platform_admin"};
    admin.tenant_id = "tenant-a";
    admin.db = db;

    const auto fail_load = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"/tmp/missing-plugin.so\", kind:\"resolver\"}) { success errors } } }",
        "{}",
        admin);
    REQUIRE(fail_load.is_success());
    CHECK_FALSE(fail_load.data["isched"]["loadPlugin"]["success"].get<bool>());
    CHECK(lifecycle_events.empty());

    const auto load_result = executor.execute(
        "mutation { isched { loadPlugin(input:{libraryPath:\"" + plugin_path + "\", kind:\"resolver\"}) { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(load_result.is_success());
    REQUIRE(load_result.data["isched"]["loadPlugin"]["success"].get<bool>());
    const auto loaded_plugin_id = load_result.data["isched"]["loadPlugin"]["pluginId"].get<std::string>();

    REQUIRE(lifecycle_events.size() == 1);
    const auto loaded_event = lifecycle_events.front();
    CHECK(loaded_event.value("eventType", "") == "LOADED");
    CHECK(loaded_event.value("pluginId", "") == loaded_plugin_id);
    CHECK_FALSE(loaded_event.value("pluginKind", "").empty());
    CHECK_FALSE(loaded_event.value("pluginName", "").empty());
    CHECK_FALSE(loaded_event.value("libraryPath", "").empty());
    CHECK_FALSE(loaded_event.value("occurredAt", "").empty());

    const auto fail_unload = executor.execute(
        "mutation { isched { unloadPlugin(id:\"missing\") { success errors } } }",
        "{}",
        admin);
    REQUIRE(fail_unload.is_success());
    CHECK_FALSE(fail_unload.data["isched"]["unloadPlugin"]["success"].get<bool>());
    CHECK(lifecycle_events.size() == 1);

    const auto unload_result = executor.execute(
        "mutation { isched { unloadPlugin(id:\"" + loaded_plugin_id + "\") { success pluginId errors } } }",
        "{}",
        admin);
    REQUIRE(unload_result.is_success());
    REQUIRE(unload_result.data["isched"]["unloadPlugin"]["success"].get<bool>());

    REQUIRE(lifecycle_events.size() == 2);
    const auto unloaded_event = lifecycle_events.back();
    REQUIRE(unloaded_event.is_object());
    CHECK(unloaded_event.value("eventType", "") == "UNLOADED");
    CHECK(unloaded_event.value("pluginId", "") == loaded_plugin_id);
    REQUIRE(loaded_event.is_object());
    REQUIRE(loaded_event.contains("pluginKind"));
    REQUIRE(unloaded_event.contains("pluginKind"));
    REQUIRE(loaded_event["pluginKind"].is_string());
    REQUIRE(unloaded_event["pluginKind"].is_string());
    CHECK(unloaded_event["pluginKind"].get<std::string>() == loaded_event["pluginKind"].get<std::string>());

    const auto idempotent_unload = executor.execute(
        "mutation { isched { unloadPlugin(id:\"" + loaded_plugin_id + "\") { success errors } } }",
        "{}",
        admin);
    REQUIRE(idempotent_unload.is_success());
    CHECK(idempotent_unload.data["isched"]["unloadPlugin"]["success"].get<bool>());
    CHECK(lifecycle_events.size() == 2);

    broker->unsubscribe(sub_id);
#endif
}

