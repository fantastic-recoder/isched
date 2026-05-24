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
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <memory>

using namespace isched::v0_0_1::backend;
using json = nlohmann::json;

// ── Test helpers ──────────────────────────────────────────────────────────────

namespace {

std::shared_ptr<DatabaseManager> make_test_db() {
    static std::atomic<int> counter{0};
    const auto pid   = static_cast<unsigned long>(getpid());
    const auto seqno = counter.fetch_add(1);
    DatabaseManager::Config cfg;
    cfg.base_path = "/tmp/isched_plugin_test_" +
                    std::to_string(pid) + "_" +
                    std::to_string(seqno);
    return std::make_shared<DatabaseManager>(cfg);
}

class StubRequestPlugin final : public RequestPlugin {
public:
    explicit StubRequestPlugin(PluginManifest m) : m_manifest(std::move(m)) {}
    [[nodiscard]] PluginManifest manifest() const override { return m_manifest; }
    [[nodiscard]] std::optional<PluginHttpResponse>
    handleRequest(const PluginHttpRequest& req) override {
        PluginHttpResponse resp;
        resp.status      = 200;
        resp.contentType = "text/plain";
        resp.body        = "stub:" + req.target;
        return resp;
    }
private:
    PluginManifest m_manifest;
};

class StubResolverPlugin final : public ResolverPlugin {
public:
    explicit StubResolverPlugin(PluginManifest m) : m_manifest(std::move(m)) {}
    [[nodiscard]] PluginManifest manifest() const override { return m_manifest; }
    void registerResolvers(GqlExecutor& executor) override {
        for (const auto& binding : m_manifest.resolverBindings) {
            const String field = binding.fieldName;
            executor.register_resolver({}, field,
                [field](const json&, const json&, const ResolverCtx&) -> json {
                    return "stub_resolver:" + field;
                });
        }
    }
    void unregisterResolvers(GqlExecutor& executor) override {
        for (const auto& b : m_manifest.resolverBindings)
            executor.unregister_resolver({}, b.fieldName);
    }
private:
    PluginManifest m_manifest;
};

PluginManifest make_request_manifest(const std::string& id,
                                      const std::string& route_path,
                                      const std::string& method = "GET") {
    PluginManifest m;
    m.id          = id;
    m.kind        = PluginKind::Request;
    m.name        = "Test " + id;
    m.version     = "1.0.0";
    m.libraryPath = "/fake/" + id + ".so";
    m.requestRoutes.push_back({method, route_path});
    return m;
}

PluginManifest make_resolver_manifest(const std::string& id,
                                       const std::string& field_name) {
    PluginManifest m;
    m.id          = id;
    m.kind        = PluginKind::Resolver;
    m.name        = "Resolver " + id;
    m.version     = "1.0.0";
    m.libraryPath = "/fake/" + id + ".so";
    PluginResolverBinding b;
    b.fieldName = field_name;
    m.resolverBindings.push_back(std::move(b));
    return m;
}

} // anonymous namespace

// ── PluginKind helpers ────────────────────────────────────────────────────────

TEST_CASE("PluginKind toString/fromString round-trips", "[plugin][kind]") {
    SECTION("request") {
        REQUIRE(toString(PluginKind::Request) == "request");
        const auto k = pluginKindFromString("request");
        REQUIRE(k.has_value());
        REQUIRE(*k == PluginKind::Request);
    }
    SECTION("resolver") {
        REQUIRE(toString(PluginKind::Resolver) == "resolver");
        const auto k = pluginKindFromString("resolver");
        REQUIRE(k.has_value());
        REQUIRE(*k == PluginKind::Resolver);
    }
    SECTION("unknown value returns nullopt") {
        REQUIRE_FALSE(pluginKindFromString("unknown_xyz").has_value());
    }
    SECTION("empty string returns nullopt") {
        REQUIRE_FALSE(pluginKindFromString("").has_value());
    }
}

// ── PluginManager lifecycle ───────────────────────────────────────────────────

TEST_CASE("PluginManager constructs with a DatabaseManager", "[plugin][manager]") {
    REQUIRE_NOTHROW(PluginManager{make_test_db()});
}

TEST_CASE("PluginManager list_plugins returns empty on fresh start", "[plugin][manager]") {
    PluginManager pm{make_test_db()};
    REQUIRE(pm.list_plugins().empty());
}

TEST_CASE("PluginManager get_plugin returns nullopt for missing id", "[plugin][manager]") {
    PluginManager pm{make_test_db()};
    REQUIRE_FALSE(pm.get_plugin("does_not_exist").has_value());
}

// ── Request plugin dispatch ────────────────────────────────────────────────────

TEST_CASE("PluginManager dispatch_request routes to loaded request plugin", "[plugin][dispatch]") {
    PluginManager pm{make_test_db()};
    const auto manifest = make_request_manifest("plug1", "/plugin/hello");
    pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(manifest), manifest);

    SECTION("Matching GET request dispatched successfully") {
        PluginHttpRequest req;
        req.method   = "GET";
        req.target   = "/plugin/hello";
        req.remoteIp = "127.0.0.1";
        const auto resp = pm.dispatch_request(req);
        REQUIRE(resp.has_value());
        CHECK(resp->status == 200);
        CHECK(resp->body == "stub:/plugin/hello");
    }
    SECTION("Non-matching path returns nullopt") {
        PluginHttpRequest req;
        req.method = "GET";
        req.target = "/plugin/other";
        REQUIRE_FALSE(pm.dispatch_request(req).has_value());
    }
    SECTION("Wrong HTTP method returns nullopt") {
        PluginHttpRequest req;
        req.method = "POST";
        req.target = "/plugin/hello";
        REQUIRE_FALSE(pm.dispatch_request(req).has_value());
    }
}

TEST_CASE("dispatch_request ignores resolver plugins", "[plugin][dispatch]") {
    PluginManager pm{make_test_db()};
    const auto m = make_resolver_manifest("res1", "myField");
    pm.inject_loaded_plugin(std::make_shared<StubResolverPlugin>(m), m);
    PluginHttpRequest req;
    req.method = "GET";
    req.target = "/plugin/myField";
    REQUIRE_FALSE(pm.dispatch_request(req).has_value());
}

TEST_CASE("dispatch_request on empty plugin set returns nullopt", "[plugin][dispatch]") {
    PluginManager pm{make_test_db()};
    PluginHttpRequest req;
    req.method = "GET";
    req.target = "/plugin/anything";
    REQUIRE_FALSE(pm.dispatch_request(req).has_value());
}

// ── Plugin list / get ─────────────────────────────────────────────────────────

TEST_CASE("PluginManager list_plugins reports injected plugins", "[plugin][list]") {
    PluginManager pm{make_test_db()};
    const auto m1 = make_request_manifest("a", "/plugin/a");
    const auto m2 = make_request_manifest("b", "/plugin/b");
    pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m1), m1);
    pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m2), m2);
    REQUIRE(pm.list_plugins().size() == 2);
}

TEST_CASE("PluginManager get_plugin returns record for loaded plugin", "[plugin][get]") {
    PluginManager pm{make_test_db()};
    const auto m = make_request_manifest("myplug", "/plugin/myplug");
    pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m), m);
    const auto maybe = pm.get_plugin("myplug");
    REQUIRE(maybe.has_value());
    CHECK(maybe->id == "myplug");
    CHECK(maybe->isLoaded);
}

// ── Unload ────────────────────────────────────────────────────────────────────

TEST_CASE("PluginManager unload removes plugin from in-memory set", "[plugin][unload]") {
    PluginManager pm{make_test_db()};
    const auto m = make_request_manifest("removeme", "/plugin/removeme");
    pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m), m);
    REQUIRE(pm.get_plugin("removeme").has_value());
    const auto result = pm.unload_plugin("removeme");
    (void)result; // Unused result
    REQUIRE_FALSE(pm.get_plugin("removeme").has_value());
}

TEST_CASE("PluginManager unload_plugin fails for unknown id", "[plugin][unload]") {
    PluginManager pm{make_test_db()};
    const auto result = pm.unload_plugin("nonexistent_plugin");
    CHECK_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
}

// ── OpenAPI document export ───────────────────────────────────────────────────

TEST_CASE("PluginManager export_openapi_document returns valid structure", "[plugin][openapi]") {
    PluginManager pm{make_test_db()};

    SECTION("Empty plugin set yields minimal document") {
        const auto doc = pm.export_openapi_document();
        REQUIRE(doc.is_object());
        CHECK(doc["openapi"] == "3.0.3");
        CHECK(doc["paths"].is_object());
        CHECK(doc["x-plugins"].is_array());
        CHECK(doc["x-plugins"].empty());
    }

    SECTION("Request plugin OpenAPI fragment is merged into paths") {
        auto m = make_request_manifest("openapi_plug", "/plugin/v1/foo");
        m.openApiFragmentJson = R"({"paths":{"/plugin/v1/foo":{"get":{"summary":"Foo","responses":{"200":{"description":"ok"}}}}}})";
        pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m), m);
        const auto doc = pm.export_openapi_document();
        REQUIRE(doc["paths"].contains("/plugin/v1/foo"));
        REQUIRE(doc["x-plugins"].size() == 1);
        CHECK(doc["x-plugins"][0]["id"] == "openapi_plug");
    }

    SECTION("Invalid JSON OpenAPI fragment is silently skipped") {
        auto m = make_request_manifest("bad_fragment", "/plugin/bad");
        m.openApiFragmentJson = "not valid json";
        pm.inject_loaded_plugin(std::make_shared<StubRequestPlugin>(m), m);
        const auto doc = pm.export_openapi_document();
        CHECK(doc["paths"].empty());
    }

    SECTION("Resolver plugin appears in x-plugins but not in paths") {
        const auto m = make_resolver_manifest("res_openapi", "someField");
        pm.inject_loaded_plugin(std::make_shared<StubResolverPlugin>(m), m);
        const auto doc = pm.export_openapi_document();
        CHECK(doc["paths"].empty());
        REQUIRE(doc["x-plugins"].size() == 1);
        CHECK(doc["x-plugins"][0]["kind"] == "resolver");
    }
}

// ── ResolverPlugin + GqlExecutor integration ─────────────────────────────────

TEST_CASE("ResolverPlugin stub registers and unregisters with GqlExecutor", "[plugin][resolver]") {
    auto db = make_test_db();
    GqlExecutor executor{db};
    const auto m = make_resolver_manifest("res_plug", "plugTestField");
    auto stub    = std::make_shared<StubResolverPlugin>(m);

    SECTION("registerResolvers does not throw") {
        REQUIRE_NOTHROW(stub->registerResolvers(executor));
    }
    SECTION("unregisterResolvers after register does not throw") {
        stub->registerResolvers(executor);
        REQUIRE_NOTHROW(stub->unregisterResolvers(executor));
    }
}

// ── PluginRecord struct ───────────────────────────────────────────────────────

TEST_CASE("PluginRecord default state is unloaded with empty fields", "[plugin][record]") {
    PluginRecord r;
    CHECK_FALSE(r.isLoaded);
    CHECK(r.id.empty());
    CHECK(r.kind.empty());
}

// ── DatabaseManager plugin persistence ───────────────────────────────────────

TEST_CASE("PluginRecord can be created and populated", "[plugin][record]") {
    PluginRecord record;
    record.id           = "test_plugin";
    record.kind         = "request";
    record.name         = "Test Plugin";
    record.version      = "1.0.0";
    record.description  = "A test plugin";
    record.libraryPath  = "/path/to/plugin.so";
    record.manifestJson = "{\"test\": true}";
    record.isLoaded     = true;
    record.loadedAt     = "2026-01-01T00:00:00Z";
    record.unloadedAt   = "";

    CHECK(record.id == "test_plugin");
    CHECK(record.kind == "request");
    CHECK(record.isLoaded == true);
}

// ── Built-in GraphQL resolver smoke tests ────────────────────────────────────

TEST_CASE("GqlExecutor built-in plugins query executes without crash", "[plugin][graphql]") {
    GqlExecutor executor{make_test_db()};
    const auto result = executor.execute(R"(query { plugins { id kind name } })");
    REQUIRE_FALSE(result.data.is_discarded());
}

TEST_CASE("GqlExecutor plugin(id) query executes without crash", "[plugin][graphql]") {
    GqlExecutor executor{make_test_db()};
    const auto result = executor.execute(R"(query { plugin(id: "missing") { id } })");
    REQUIRE_FALSE(result.data.is_discarded());
}

