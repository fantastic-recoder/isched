// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_PluginSystem.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Runtime plugin manager for request and resolver plugins.
 */

#include "isched_PluginSystem.hpp"

#include "isched_DatabaseManager.hpp"
#include "isched_GqlExecutor.hpp"
#include "isched_gql_grammar.hpp"

#include <boost/dll/shared_library.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <spdlog/spdlog.h>

namespace isched::v0_0_1::backend {
namespace {
    using json = nlohmann::json;

    [[nodiscard]] bool is_blank(std::string_view value) {
        return std::ranges::all_of(value, [](unsigned char ch) {
            return std::isspace(ch) != 0;
        });
    }

    [[nodiscard]] static String routeKeyUpper(String value) {
        std::ranges::transform(value, value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    [[nodiscard]] bool is_template_segment(std::string_view segment) {
        return segment.size() >= 3 && segment.front() == '{' && segment.back() == '}';
    }

    [[nodiscard]] std::vector<std::string_view> split_path_segments(std::string_view path) {
        std::vector<std::string_view> segments;
        std::size_t start = 0;
        while (start < path.size()) {
            while (start < path.size() && path[start] == '/') {
                ++start;
            }
            if (start >= path.size()) {
                break;
            }
            auto end = path.find('/', start);
            if (end == std::string_view::npos) {
                end = path.size();
            }
            if (end > start) {
                segments.emplace_back(path.substr(start, end - start));
            }
            start = end;
        }
        return segments;
    }

    [[nodiscard]] bool routes_overlap(const PluginRoute& a, const PluginRoute& b) {
        if (routeKeyUpper(a.method) != routeKeyUpper(b.method)) {
            return false;
        }
        const auto lhs = split_path_segments(a.path);
        const auto rhs = split_path_segments(b.path);
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const auto left = lhs[i];
            const auto right = rhs[i];
            if (left == right) {
                continue;
            }
            if (!is_template_segment(left) && !is_template_segment(right)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] String normalize_route_path(std::string_view raw_path) {
        const auto segments = split_path_segments(raw_path);
        if (segments.empty()) {
            return "/";
        }
        String normalized{"/"};
        for (std::size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) {
                normalized.push_back('/');
            }
            normalized.append(segments[i]);
        }
        return normalized;
    }

    [[nodiscard]] std::vector<PluginRoute> normalize_manifest_routes(const std::vector<PluginRoute>& routes) {
        std::vector<PluginRoute> normalized;
        normalized.reserve(routes.size());
        for (const auto& route : routes) {
            normalized.push_back(PluginRoute{
                .method = routeKeyUpper(route.method),
                .path = normalize_route_path(route.path)
            });
        }
        return normalized;
    }

    [[nodiscard]] bool has_internal_route_conflict(const std::vector<PluginRoute>& routes) {
        for (std::size_t i = 0; i < routes.size(); ++i) {
            for (std::size_t j = i + 1; j < routes.size(); ++j) {
                if (routes_overlap(routes[i], routes[j])) {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool route_matches_request(const PluginRoute& route,
                                             std::string_view method,
                                             std::string_view target) {
        if (routeKeyUpper(route.method) != routeKeyUpper(String(method))) {
            return false;
        }
        const auto route_segments = split_path_segments(route.path);
        const auto target_segments = split_path_segments(target);
        if (route_segments.size() != target_segments.size()) {
            return false;
        }
        for (std::size_t i = 0; i < route_segments.size(); ++i) {
            if (is_template_segment(route_segments[i])) {
                continue;
            }
            if (route_segments[i] != target_segments[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::optional<int> extract_semver_major(std::string_view value) {
        if (value.empty()) {
            return std::nullopt;
        }
        const auto dot = value.find('.');
        const auto major_text = dot == std::string_view::npos ? value : value.substr(0, dot);
        if (major_text.empty()) {
            return std::nullopt;
        }
        int major = 0;
        const auto* begin = major_text.data();
        const auto* end = begin + major_text.size();
        auto [ptr, ec] = std::from_chars(begin, end, major);
        if (ec != std::errc{} || ptr != end || major < 0) {
            return std::nullopt;
        }
        return major;
    }

    [[nodiscard]] std::optional<String> validate_abi_version(const PluginManifest& manifest) {
        const auto plugin_major = extract_semver_major(manifest.abi_version);
        if (!plugin_major) {
            return std::format("Plugin abi_version '{}' is not a valid semantic version", manifest.abi_version);
        }
        if (*plugin_major != PLUGIN_ABI_MAJOR) {
            return std::format("ABI version mismatch: plugin declares '{}', server requires major '{}'",
                               manifest.abi_version,
                               PLUGIN_ABI_MAJOR);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<String> validate_openapi_fragment(const String& fragment_json) {
        if (fragment_json.empty()) {
            return std::nullopt;
        }
        const auto fragment = json::parse(fragment_json, nullptr, false);
        if (fragment.is_discarded()) {
            return "Plugin openApiFragmentJson is not valid JSON";
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<String> validate_resolver_schema_sdl(const String& schema_sdl) {
        if (schema_sdl.empty() || is_blank(schema_sdl)) {
            return "invalid GraphQL SDL: resolver plugin schema is empty";
        }
        try {
            tao::pegtl::string_input<> sdl_input(schema_sdl, "resolverSchemaSdl");
            auto parsed = gql::generate_ast_and_log<gql::Document>(
                sdl_input, "resolverSchemaSdl", false, false);
            if (!std::get<0>(parsed)) {
                return "invalid GraphQL SDL: resolver plugin schema failed to parse";
            }
        } catch (const tao::pegtl::parse_error& e) {
            return String{"invalid GraphQL SDL: "} + e.what();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string scope_to_type_name(const StringVector& scope) {
        if (scope.empty()) {
            return "Query";
        }
        if (scope.size() == 1 && scope.front() == "isched") {
            return "IschedQueryNamespace";
        }
        if (scope.size() == 1) {
            return scope.front();
        }
        return scope.back();
    }
}

String toString(PluginKind kind) {
    switch (kind) {
        case PluginKind::Request:  return "request";
        case PluginKind::Resolver: return "resolver";
    }
    return "request";
}

std::optional<PluginKind> pluginKindFromString(std::string_view value) {
    if (value == "request") {
        return PluginKind::Request;
    }
    if (value == "resolver") {
        return PluginKind::Resolver;
    }
    return std::nullopt;
}

PluginManager::PluginManager(std::shared_ptr<DatabaseManager> database)
    : m_database(std::move(database))
{
}

void PluginManager::set_executor(GqlExecutor* executor) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executor = executor;
}

void PluginManager::set_plugins_root(std::filesystem::path plugins_root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plugins_root = std::move(plugins_root);
}

std::optional<String> PluginManager::validate_library_path(const String& library_path) const {
    namespace fs = std::filesystem;

    fs::path library = fs::path(library_path);
    if (!library.is_absolute()) {
        return "path policy validation failed: libraryPath must be absolute";
    }

    std::optional<fs::path> configured_root;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        configured_root = m_plugins_root;
    }

    if (!configured_root.has_value()) {
        if (const char* env_root = std::getenv("ISCHED_PLUGINS_ROOT"); env_root != nullptr && *env_root != '\0') {
            configured_root = fs::path(env_root);
        } else {
            configured_root = fs::path{"/"};
        }
    }

    std::error_code ec;
    fs::path canonical_root = fs::canonical(*configured_root, ec);
    if (ec) {
        return std::format("path policy validation failed: cannot canonicalize plugins_root '{}': {}",
                           configured_root->string(),
                           ec.message());
    }

    ec.clear();
    fs::path canonical_library = fs::canonical(library, ec);
    if (ec) {
        return std::format("path policy validation failed: cannot canonicalize libraryPath '{}': {}",
                           library.string(),
                           ec.message());
    }

    auto root_it = canonical_root.begin();
    auto lib_it = canonical_library.begin();
    for (; root_it != canonical_root.end() && lib_it != canonical_library.end(); ++root_it, ++lib_it) {
        if (*root_it != *lib_it) {
            return "path policy validation failed: libraryPath must resolve under plugins_root";
        }
    }
    if (root_it != canonical_root.end()) {
        return "path policy validation failed: libraryPath must resolve under plugins_root";
    }

    return std::nullopt;
}

String PluginManager::normalize_route_key(const PluginRoute& route) {
    return normalize_route_key(route.method, route.path);
}

String PluginManager::normalize_route_key(std::string_view method, std::string_view path) {
    return routeKeyUpper(String(method)) + " " + String(path);
}

String PluginManager::manifest_to_json(const PluginManifest& manifest) {
    json root;
    root["id"] = manifest.id;
    root["kind"] = toString(manifest.kind);
    root["name"] = manifest.name;
    root["version"] = manifest.version;
    root["description"] = manifest.description;
    root["libraryPath"] = manifest.libraryPath;
    root["abi_version"] = manifest.abi_version;
    root["openApiFragmentJson"] = manifest.openApiFragmentJson;
    root["resolverSchemaSdl"] = manifest.resolverSchemaSdl;
    root["requestRoutes"] = json::array();
    for (const auto& route : manifest.requestRoutes) {
        root["requestRoutes"].push_back({
            {"method", route.method},
            {"path", route.path}
        });
    }
    root["resolverBindings"] = json::array();
    for (const auto& binding : manifest.resolverBindings) {
        root["resolverBindings"].push_back({
            {"scope", binding.scope},
            {"fieldName", binding.fieldName},
            {"requiredRoles", binding.requiredRoles}
        });
    }
    return root.dump();
}

std::optional<PluginManifest> PluginManager::manifest_from_json(const String& json_text) {
    if (json_text.empty()) {
        return std::nullopt;
    }
    const auto parsed = json::parse(json_text, nullptr, /*exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return std::nullopt;
    }
    PluginManifest manifest;
    manifest.id = parsed.value("id", "");
    manifest.kind = pluginKindFromString(parsed.value("kind", "request")).value_or(PluginKind::Request);
    manifest.name = parsed.value("name", "");
    manifest.version = parsed.value("version", "");
    manifest.description = parsed.value("description", "");
    manifest.libraryPath = parsed.value("libraryPath", "");
    manifest.abi_version = parsed.value("abi_version", String(PLUGIN_ABI_VERSION));
    manifest.openApiFragmentJson = parsed.value("openApiFragmentJson", "");
    manifest.resolverSchemaSdl = parsed.value("resolverSchemaSdl", "");
    if (parsed.contains("requestRoutes") && parsed["requestRoutes"].is_array()) {
        for (const auto& route : parsed["requestRoutes"]) {
            if (!route.is_object()) continue;
            manifest.requestRoutes.push_back({
                .method = route.value("method", "GET"),
                .path = route.value("path", "")
            });
        }
    }
    if (parsed.contains("resolverBindings") && parsed["resolverBindings"].is_array()) {
        for (const auto& binding : parsed["resolverBindings"]) {
            if (!binding.is_object()) continue;
            PluginResolverBinding out;
            if (binding.contains("scope") && binding["scope"].is_array()) {
                for (const auto& scope_item : binding["scope"]) {
                    if (scope_item.is_string()) {
                        out.scope.push_back(scope_item.get<String>());
                    }
                }
            }
            out.fieldName = binding.value("fieldName", "");
            if (binding.contains("requiredRoles") && binding["requiredRoles"].is_array()) {
                for (const auto& role : binding["requiredRoles"]) {
                    if (role.is_string()) {
                        out.requiredRoles.push_back(role.get<String>());
                    }
                }
            }
            manifest.resolverBindings.push_back(std::move(out));
        }
    }
    return manifest;
}

PluginRecord PluginManager::record_from_manifest(
    const PluginManifest& manifest,
    bool is_loaded,
    const String& loaded_at,
    const String& unloaded_at)
{
    PluginRecord record;
    record.id = manifest.id;
    record.kind = toString(manifest.kind);
    record.name = manifest.name;
    record.version = manifest.version;
    record.description = manifest.description;
    record.libraryPath = manifest.libraryPath;
    record.manifestJson = manifest_to_json(manifest);
    record.isLoaded = is_loaded;
    record.loadedAt = loaded_at;
    record.unloadedAt = unloaded_at;
    return record;
}

String PluginManager::iso_now() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    return std::to_string(now.count());
}

PluginOperationResult PluginManager::persist_record(
    const PluginManifest& manifest,
    bool is_loaded,
    const String& loaded_at,
    const String& unloaded_at)
{
    PluginOperationResult result;
    if (!m_database) {
        result.errors.push_back("Plugin persistence unavailable");
        return result;
    }

    const auto record = record_from_manifest(manifest, is_loaded, loaded_at, unloaded_at);
    const auto db_result = m_database->upsert_plugin_record(record);
    if (!db_result) {
        result.errors.push_back("Failed to persist plugin record");
        return result;
    }

    result.success = true;
    result.pluginId = manifest.id;
    return result;
}

bool PluginManager::route_conflicts_with_loaded(const PluginManifest& manifest) const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [_, loaded] : m_loaded_plugins) {
        if (loaded.manifest.kind != PluginKind::Request) {
            continue;
        }
        for (const auto& route : manifest.requestRoutes) {
            for (const auto& existing : loaded.manifest.requestRoutes) {
                if (routes_overlap(route, existing)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool PluginManager::resolver_conflicts_with_loaded(const PluginManifest& manifest) const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [_, loaded] : m_loaded_plugins) {
        if (loaded.manifest.kind != PluginKind::Resolver) {
            continue;
        }
        for (const auto& binding : manifest.resolverBindings) {
            for (const auto& existing : loaded.manifest.resolverBindings) {
                if (binding.scope == existing.scope && binding.fieldName == existing.fieldName) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool PluginManager::resolver_schema_conflicts_with_loaded(const String& schema_sdl, String& error_out) const {
    if (schema_sdl.empty()) {
        return false;
    }

    if (!m_database) {
        error_out = "Resolver schema validation requires database context";
        return true;
    }

    GqlExecutor probe{m_database};
    std::vector<String> loaded_sdls;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        loaded_sdls.reserve(m_loaded_plugins.size());
        for (const auto& [_, loaded] : m_loaded_plugins) {
            if (loaded.manifest.kind == PluginKind::Resolver && !loaded.manifest.resolverSchemaSdl.empty()) {
                loaded_sdls.push_back(loaded.manifest.resolverSchemaSdl);
            }
        }
    }

    for (const auto& existing_sdl : loaded_sdls) {
        auto existing_load = probe.load_schema(existing_sdl);
        if (!existing_load.is_success()) {
            error_out = "schema conflict: failed to validate existing resolver schema baseline";
            return true;
        }
    }

    auto merge_result = probe.load_schema(schema_sdl);
    if (merge_result.is_success()) {
        return false;
    }

    if (!merge_result.errors.empty()) {
        error_out = "schema conflict: " + merge_result.errors.front().message;
    } else {
        error_out = "schema conflict: failed to merge resolver schema";
    }
    return true;
}

bool PluginManager::resolver_binding_targets_exist(const PluginManifest& manifest, String& error_out) const {
    if (m_executor == nullptr) {
        error_out = "Resolver plugin loading requires an attached GraphQL executor";
        return false;
    }

    for (const auto& binding : manifest.resolverBindings) {
        const auto type_name = scope_to_type_name(binding.scope);
        if (!m_executor->schema_field_exists(type_name, binding.fieldName)) {
            error_out = "field not found: " + type_name + "." + binding.fieldName;
            return false;
        }
    }
    return true;
}

bool PluginManager::resolver_bindings_unclaimed(const PluginManifest& manifest, String& error_out) const {
    if (m_executor == nullptr) {
        error_out = "Resolver plugin loading requires an attached GraphQL executor";
        return false;
    }

    for (const auto& binding : manifest.resolverBindings) {
        if (m_executor->has_resolver(binding.scope, binding.fieldName)) {
            error_out = "resolver already registered: " + concat_vector(binding.scope, ".") +
                        (binding.scope.empty() ? "" : ".") + binding.fieldName;
            return false;
        }
    }
    return true;
}

void PluginManager::unregister_resolver_plugin_bindings(const LoadedPlugin& loaded) {
    if (m_executor == nullptr || loaded.manifest.kind != PluginKind::Resolver) {
        return;
    }
    if (auto resolver = std::dynamic_pointer_cast<ResolverPlugin>(loaded.plugin)) {
        class DirectRegistrar final : public IResolverRegistrar {
        public:
            explicit DirectRegistrar(GqlExecutor& executor) : m_executor(executor) {}
            void register_resolver(const ResolverPath& scope,
                                   const std::string& field_name,
                                   ResolverFunction resolver) override {
                m_executor.register_resolver(scope, field_name, std::move(resolver));
            }
            void unregister_resolver(const ResolverPath& scope,
                                     const std::string& field_name) override {
                m_executor.unregister_resolver(scope, field_name);
            }
        private:
            GqlExecutor& m_executor;
        } registrar{*m_executor};
        resolver->unregisterResolvers(registrar);
    }
}

std::optional<PluginManager::LoadedPlugin> PluginManager::find_loaded_plugin(const String& plugin_id) const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_loaded_plugins.find(plugin_id);
    if (it == m_loaded_plugins.end()) {
        return std::nullopt;
    }
    return it->second;
}

PluginOperationResult PluginManager::load_plugin(
    const String& library_path,
    PluginKind kind,
    std::optional<String> plugin_id)
{
    if (const auto path_error = validate_library_path(library_path); path_error) {
        PluginOperationResult result;
        result.errors.push_back(*path_error);
        return result;
    }

    switch (kind) {
        case PluginKind::Request:
            return load_request_plugin(library_path, std::move(plugin_id));
        case PluginKind::Resolver:
            return load_resolver_plugin(library_path, std::move(plugin_id));
    }
    PluginOperationResult result;
    result.errors.push_back("Unsupported plugin kind");
    return result;
}

PluginOperationResult PluginManager::load_request_plugin(
    const String& library_path,
    std::optional<String> plugin_id)
{
    PluginOperationResult result;
    try {
        auto library = std::make_shared<boost::dll::shared_library>(library_path);
        auto& factory = library->get<RequestFactory>("isched_create_request_plugin");
        auto plugin = factory();
        if (!plugin) {
            result.errors.push_back("Request plugin factory returned null");
            return result;
        }

        auto manifest = plugin->manifest();
        manifest.kind = PluginKind::Request;
        manifest.libraryPath = library_path;
        if (plugin_id && !plugin_id->empty()) {
            manifest.id = *plugin_id;
        }
        if (manifest.id.empty()) {
            manifest.id = std::filesystem::path(library_path).stem().string();
        }
        if (manifest.requestRoutes.empty()) {
            result.errors.push_back("Request plugin must declare at least one request route");
            return result;
        }
        manifest.requestRoutes = normalize_manifest_routes(manifest.requestRoutes);
        if (has_internal_route_conflict(manifest.requestRoutes)) {
            result.errors.push_back("Request plugin declares conflicting routes");
            return result;
        }
        if (const auto abi_error = validate_abi_version(manifest); abi_error) {
            result.errors.push_back(*abi_error);
            return result;
        }
        if (const auto openapi_error = validate_openapi_fragment(manifest.openApiFragmentJson); openapi_error) {
            result.errors.push_back(*openapi_error);
            return result;
        }
        if (route_conflicts_with_loaded(manifest)) {
            result.errors.push_back("Request plugin route conflicts with an already loaded plugin");
            return result;
        }

        PluginRecord record = record_from_manifest(manifest, true, iso_now(), {});
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_loaded_plugins.contains(manifest.id)) {
                result.errors.push_back("Plugin id already loaded: " + manifest.id);
                return result;
            }
            m_loaded_plugins.emplace(manifest.id, LoadedPlugin{std::move(library), std::move(plugin), manifest, record});
        }

        if (const auto persisted = persist_record(manifest, true, record.loadedAt); !persisted.success) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_loaded_plugins.erase(manifest.id);
            return persisted;
        }

        result.success = true;
        result.pluginId = manifest.id;
        return result;
    } catch (const std::exception& ex) {
        result.errors.push_back(ex.what());
        return result;
    } catch (...) {
        result.errors.push_back("Request plugin load failed with unknown exception");
        return result;
    }
}

PluginOperationResult PluginManager::load_resolver_plugin(
    const String& library_path,
    std::optional<String> plugin_id)
{
    PluginOperationResult result;
    try {
        auto library = std::make_shared<boost::dll::shared_library>(library_path);
        auto& factory = library->get<ResolverFactory>("isched_create_resolver_plugin");
        auto plugin = factory();
        if (!plugin) {
            result.errors.push_back("Resolver plugin factory returned null");
            return result;
        }

        auto manifest = plugin->manifest();
        manifest.kind = PluginKind::Resolver;
        manifest.libraryPath = library_path;
        if (plugin_id && !plugin_id->empty()) {
            manifest.id = *plugin_id;
        }
        if (manifest.id.empty()) {
            manifest.id = std::filesystem::path(library_path).stem().string();
        }
        if (manifest.resolverBindings.empty()) {
            result.errors.push_back("Resolver plugin must declare at least one resolver binding");
            return result;
        }
        const auto supplied_schema = plugin->graphqlSchema();
        const bool has_supplied_schema = !supplied_schema.empty() && !is_blank(supplied_schema);
        if (has_supplied_schema) {
            manifest.resolverSchemaSdl = supplied_schema;
            if (const auto schema_error = validate_resolver_schema_sdl(manifest.resolverSchemaSdl); schema_error) {
                result.errors.push_back(*schema_error);
                return result;
            }
            String schema_conflict_error;
            if (resolver_schema_conflicts_with_loaded(manifest.resolverSchemaSdl, schema_conflict_error)) {
                result.errors.push_back(schema_conflict_error);
                return result;
            }
        }
        if (const auto abi_error = validate_abi_version(manifest); abi_error) {
            result.errors.push_back(*abi_error);
            return result;
        }
        if (resolver_conflicts_with_loaded(manifest)) {
            result.errors.push_back("Resolver plugin binding conflicts with an already loaded plugin");
            return result;
        }
        if (m_executor == nullptr) {
            result.errors.push_back("Resolver plugin loading requires an attached GraphQL executor");
            return result;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_loaded_plugins.contains(manifest.id)) {
                result.errors.push_back("Plugin id already loaded: " + manifest.id);
                return result;
            }
        }

        String binding_error;
        if (!resolver_bindings_unclaimed(manifest, binding_error)) {
            result.errors.push_back(binding_error);
            return result;
        }
        if (!has_supplied_schema && !resolver_binding_targets_exist(manifest, binding_error)) {
            result.errors.push_back(binding_error);
            return result;
        }

        class CrashSafeRegistrar final : public IResolverRegistrar {
        public:
            CrashSafeRegistrar(GqlExecutor& executor,
                               PluginManager& manager,
                               String plugin_id)
                : m_executor(executor)
                , m_manager(manager)
                , m_plugin_id(std::move(plugin_id))
            {
            }

            void register_resolver(const ResolverPath& scope,
                                   const std::string& field_name,
                                   ResolverFunction resolver) override {
                String plugin_id = m_plugin_id;
                auto* manager = &m_manager;
                m_executor.register_resolver(scope, field_name,
                    [resolver = std::move(resolver), plugin_id, manager](const json& parent,
                                                                          const json& args,
                                                                          const ResolverCtx& ctx) -> json {
                        try {
                            return resolver(parent, args, ctx);
                        } catch (const std::exception& ex) {
                            manager->crash_unload_resolver_plugin(plugin_id, ex);
                            return json{{"errors", json::array({json{{"message", "Plugin resolver crashed"}}})}};
                        } catch (...) {
                            const std::runtime_error unknown_ex{"Plugin resolver threw non-std exception"};
                            manager->crash_unload_resolver_plugin(plugin_id, unknown_ex);
                            return json{{"errors", json::array({json{{"message", "Plugin resolver crashed"}}})}};
                        }
                    });
            }

            void unregister_resolver(const ResolverPath& scope,
                                     const std::string& field_name) override {
                m_executor.unregister_resolver(scope, field_name);
            }

        private:
            GqlExecutor& m_executor;
            PluginManager& m_manager;
            String m_plugin_id;
        };

        const auto loaded_at = iso_now();
        LoadedPlugin candidate{library, plugin, manifest, record_from_manifest(manifest, true, loaded_at, {})};

        try {
            CrashSafeRegistrar registrar{*m_executor, *this, manifest.id};
            plugin->registerResolvers(registrar);
        } catch (const std::exception& ex) {
            result.errors.push_back(String{"resolver registration failed: "} + ex.what());
            return result;
        }

        if (has_supplied_schema) {
            auto schema_load = m_executor->load_schema(manifest.resolverSchemaSdl);
            if (!schema_load.is_success()) {
                unregister_resolver_plugin_bindings(candidate);
                if (!schema_load.errors.empty()) {
                    result.errors.push_back("schema conflict: " + schema_load.errors.front().message);
                } else {
                    result.errors.push_back("schema conflict: resolver schema merge failed");
                }
                return result;
            }
        }

        if (const auto persisted = persist_record(manifest, true, loaded_at); !persisted.success) {
            unregister_resolver_plugin_bindings(candidate);
            return persisted;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_loaded_plugins.emplace(manifest.id, std::move(candidate));
        }

        result.success = true;
        result.pluginId = manifest.id;
        return result;
    } catch (const std::exception& ex) {
        result.errors.push_back(ex.what());
        return result;
    } catch (...) {
        result.errors.push_back("Resolver plugin load failed with unknown exception");
        return result;
    }
}

PluginOperationResult PluginManager::unload_plugin(const String& plugin_id) {
    PluginOperationResult result;
    if (plugin_id.empty()) {
        result.errors.push_back("Plugin id is required");
        return result;
    }

    if (m_database) {
        const auto persisted = m_database->get_plugin_record(plugin_id);
        if (persisted && persisted.value().has_value() && !persisted.value()->isLoaded) {
            result.success = true;
            result.pluginId = plugin_id;
            return result;
        }
    }

    std::optional<LoadedPlugin> runtime;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_loaded_plugins.find(plugin_id);
        if (it == m_loaded_plugins.end()) {
            result.errors.push_back("Plugin not found or not loaded: " + plugin_id);
            return result;
        }
        runtime = it->second;
    }

    if (runtime && runtime->manifest.kind == PluginKind::Resolver && m_executor != nullptr) {
        unregister_resolver_plugin_bindings(*runtime);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded_plugins.erase(plugin_id);
    }

    if (!m_database) {
        result.errors.push_back("Plugin persistence unavailable");
        return result;
    }

    auto record = runtime->record;
    if (const auto persisted = m_database->get_plugin_record(plugin_id); persisted && persisted.value()) {
        record = *persisted.value();
    }
    record.isLoaded = false;
    if (record.loadedAt.empty()) {
        record.loadedAt = runtime->record.loadedAt;
    }
    record.unloadedAt = iso_now();
    const auto db_result = m_database->upsert_plugin_record(record);
    if (!db_result) {
        result.errors.push_back("Failed to update plugin persistence state");
        return result;
    }

    result.success = true;
    result.pluginId = plugin_id;
    return result;
}

std::vector<PluginRecord> PluginManager::list_plugins() const {
    if (m_database) {
        if (const auto persisted = m_database->list_plugin_records(); persisted) {
            return persisted.value();
        }
    }

    std::vector<PluginRecord> records;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        records.reserve(m_loaded_plugins.size());
        for (const auto& [_, plugin] : m_loaded_plugins) {
            records.push_back(plugin.record);
        }
    }
    return records;
}

std::optional<PluginRecord> PluginManager::get_plugin(const String& plugin_id) const {
    if (plugin_id.empty()) {
        return std::nullopt;
    }
    if (m_database) {
        if (const auto persisted = m_database->get_plugin_record(plugin_id); persisted) {
            return persisted.value();
        }
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_loaded_plugins.find(plugin_id);
    if (it == m_loaded_plugins.end()) {
        return std::nullopt;
    }
    return it->second.record;
}

std::optional<PluginHttpResponse> PluginManager::dispatch_request(const PluginHttpRequest& request) {
    std::vector<LoadedPlugin> request_plugins;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        request_plugins.reserve(m_loaded_plugins.size());
        for (const auto& [_, loaded] : m_loaded_plugins) {
            if (loaded.manifest.kind == PluginKind::Request) {
                request_plugins.push_back(loaded);
            }
        }
    }

    for (const auto& loaded : request_plugins) {
        if (loaded.manifest.kind != PluginKind::Request) {
            continue;
        }
        for (const auto& route : loaded.manifest.requestRoutes) {
            if (!route_matches_request(route, request.method, request.target)) {
                continue;
            }
            if (const auto request_plugin = std::dynamic_pointer_cast<RequestPlugin>(loaded.plugin)) {
                try {
                    if (auto response = request_plugin->handleRequest(request)) {
                        return response;
                    }
                } catch (const std::exception& ex) {
                    return crash_unload_request_plugin(loaded.manifest.id, ex);
                } catch (...) {
                    const std::runtime_error unknown_ex{"Request plugin threw non-std exception"};
                    return crash_unload_request_plugin(loaded.manifest.id, unknown_ex);
                }
            }
        }
    }
    return std::nullopt;
}

PluginHttpResponse PluginManager::crash_unload_request_plugin(const String& plugin_id,
                                                              const std::exception& ex) noexcept {
    spdlog::error("Request plugin '{}' crashed: {}", plugin_id, ex.what());
    try {
        std::ignore = unload_plugin(plugin_id);
    } catch (const std::exception& cleanup_ex) {
        spdlog::error("Request plugin '{}' crash cleanup failed: {}", plugin_id, cleanup_ex.what());
    }
    PluginHttpResponse response;
    response.status = 500;
    response.contentType = "application/json";
    response.body = R"({"errors":[{"message":"Plugin error"}]})";
    return response;
}

void PluginManager::crash_unload_resolver_plugin(const String& plugin_id,
                                                 const std::exception& ex) noexcept {
    spdlog::error("Resolver plugin '{}' crashed: {}", plugin_id, ex.what());
    try {
        std::thread([this, plugin_id]() {
            try {
                std::ignore = unload_plugin(plugin_id);
            } catch (const std::exception& cleanup_ex) {
                spdlog::error("Resolver plugin '{}' crash cleanup failed: {}", plugin_id, cleanup_ex.what());
            }
        }).detach();
    } catch (const std::exception& cleanup_ex) {
        spdlog::error("Resolver plugin '{}' crash cleanup failed: {}", plugin_id, cleanup_ex.what());
    }
}

nlohmann::json PluginManager::export_openapi_document() const {
    json document = {
        {"openapi", "3.0.3"},
        {"info", {{"title", "isched plugins"}, {"version", "1.0.0"}}},
        {"paths", json::object()},
        {"x-plugins", json::array()}
    };

    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [_, loaded] : m_loaded_plugins) {
        document["x-plugins"].push_back({
            {"id", loaded.manifest.id},
            {"kind", toString(loaded.manifest.kind)},
            {"name", loaded.manifest.name},
            {"version", loaded.manifest.version}
        });

        if (loaded.manifest.kind != PluginKind::Request || loaded.manifest.openApiFragmentJson.empty()) {
            continue;
        }

        const auto fragment = json::parse(loaded.manifest.openApiFragmentJson, nullptr, /*exceptions=*/false);
        if (fragment.is_discarded() || !fragment.is_object()) {
            continue;
        }
        if (fragment.contains("paths") && fragment["paths"].is_object()) {
            for (auto it = fragment["paths"].begin(); it != fragment["paths"].end(); ++it) {
                document["paths"][it.key()] = it.value();
            }
        }
    }

    return document;
}

void PluginManager::inject_loaded_plugin(std::shared_ptr<Plugin> plugin, PluginManifest manifest) {
    class DirectRegistrar final : public IResolverRegistrar {
    public:
        explicit DirectRegistrar(GqlExecutor& executor) : m_executor(executor) {}

        void register_resolver(const ResolverPath& scope,
                               const std::string& field_name,
                               ResolverFunction resolver) override {
            m_executor.register_resolver(scope, field_name, std::move(resolver));
        }

        void unregister_resolver(const ResolverPath& scope,
                                 const std::string& field_name) override {
            m_executor.unregister_resolver(scope, field_name);
        }

    private:
        GqlExecutor& m_executor;
    };

    PluginRecord record = record_from_manifest(manifest, true, iso_now(), {});

    if (manifest.kind == PluginKind::Resolver && m_executor != nullptr) {
        const auto resolver_plugin = std::dynamic_pointer_cast<ResolverPlugin>(plugin);
        if (resolver_plugin) {
            DirectRegistrar registrar{*m_executor};
            resolver_plugin->registerResolvers(registrar);
        }
    }

    std::lock_guard<std::mutex> guard(m_mutex);
    m_loaded_plugins.emplace(manifest.id,
        LoadedPlugin{/*library=*/nullptr, std::move(plugin), manifest, record});
}

void PluginManager::restore_persisted_plugins() {
    if (!m_database) {
        return;
    }
    const auto records = m_database->list_plugin_records();
    if (!records) {
        spdlog::warn("PluginManager::restore_persisted_plugins: unable to list plugin records");
        return;
    }
    for (const auto& record : records.value()) {
        if (!record.isLoaded) {
            continue;
        }
        const auto kind = pluginKindFromString(record.kind);
        if (!kind) {
            continue;
        }
        if (m_loaded_plugins.contains(record.id)) {
            continue;
        }
        const auto attempted_at = iso_now();
        auto result = load_plugin(record.libraryPath, *kind, record.id);
        if (m_database) {
            const auto error_message = result.success || result.errors.empty() ? "" : result.errors.front();
            std::ignore = m_database->record_plugin_restore_attempt(
                record.id,
                result.success,
                error_message,
                attempted_at);
        }
        if (!result.success) {
            spdlog::warn("PluginManager: failed to restore plugin '{}': {}", record.id,
                result.errors.empty() ? "unknown error" : result.errors.front());
        }
    }
}

} // namespace isched::v0_0_1::backend
