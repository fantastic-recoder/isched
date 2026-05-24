// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_PluginSystem.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Runtime plugin interfaces and manager for request/resolver plugins.
 */

#pragma once

#include "isched_common.hpp"
#include <boost/dll/shared_library.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace isched::v0_0_1::backend {
    class DatabaseManager;
    class GqlExecutor;

    /**
     * @brief Plugin kind supported by the runtime.
     */
    enum class PluginKind {
        Request,
        Resolver,
    };

    /**
     * @brief Convert a plugin kind to its stable string representation.
     */
    [[nodiscard]] String toString(PluginKind kind);

    /**
     * @brief Parse a plugin kind from its stable string representation.
     */
    [[nodiscard]] std::optional<PluginKind> pluginKindFromString(std::string_view value);

    /**
     * @brief Single request route advertised by a request plugin.
     */
    struct PluginRoute {
        String method{"GET"};
        String path;
    };

    /**
     * @brief Resolver binding advertised by a resolver plugin.
     */
    struct PluginResolverBinding {
        StringVector scope;
        String fieldName;
        StringVector requiredRoles;
    };

    /**
     * @brief Runtime metadata exported by a plugin.
     */
    struct PluginManifest {
        String id;
        PluginKind kind{PluginKind::Request};
        String name;
        String version;
        String description;
        String libraryPath;
        String openApiFragmentJson;
        std::vector<PluginRoute> requestRoutes;
        std::vector<PluginResolverBinding> resolverBindings;
    };

    /**
     * @brief Persisted plugin state.
     */
    struct PluginRecord {
        String id;
        String kind;
        String name;
        String version;
        String description;
        String libraryPath;
        String manifestJson;
        bool isLoaded{false};
        String loadedAt;
        String unloadedAt;
    };

    /**
     * @brief HTTP request snapshot passed to request plugins.
     */
    struct PluginHttpRequest {
        String method;
        String target;
        String body;
        String remoteIp;
        StringMap headers;
    };

    /**
     * @brief HTTP response snapshot returned by request plugins.
     */
    struct PluginHttpResponse {
        int status{200};
        String contentType{"application/json"};
        String body;
        StringMap headers;
    };

    /**
     * @brief Base plugin interface.
     */
    class Plugin {
    public:
        virtual ~Plugin() = default;
        [[nodiscard]] virtual PluginManifest manifest() const = 0;
    };

    /**
     * @brief Request plugin interface for HTTP callbacks.
     */
    class RequestPlugin : public Plugin {
    public:
        [[nodiscard]] virtual std::optional<PluginHttpResponse> handleRequest(const PluginHttpRequest& request) = 0;
    };

    /**
     * @brief Resolver plugin interface for GraphQL resolver DLLs.
     */
    class ResolverPlugin : public Plugin {
    public:
        virtual void registerResolvers(GqlExecutor& executor) = 0;
        virtual void unregisterResolvers(GqlExecutor& executor) = 0;
    };

    /**
     * @brief Load/unload result returned by the plugin manager.
     */
    struct PluginOperationResult {
        bool success{false};
        String pluginId;
        StringVector errors;
    };

    /**
     * @brief Manages runtime plugins loaded via Boost.DLL.
     */
    class PluginManager {
    public:
        explicit PluginManager(std::shared_ptr<DatabaseManager> database);

        void set_executor(GqlExecutor* executor) noexcept;

        [[nodiscard]] PluginOperationResult load_plugin(
            const String& library_path,
            PluginKind kind,
            std::optional<String> plugin_id = std::nullopt);

        [[nodiscard]] PluginOperationResult unload_plugin(const String& plugin_id);

        [[nodiscard]] std::vector<PluginRecord> list_plugins() const;

        [[nodiscard]] std::optional<PluginRecord> get_plugin(const String& plugin_id) const;

        [[nodiscard]] std::optional<PluginHttpResponse> dispatch_request(const PluginHttpRequest& request) const;

        [[nodiscard]] nlohmann::json export_openapi_document() const;

        void restore_persisted_plugins();

        /**
         * @brief Test-only: inject a pre-constructed plugin without loading a shared library.
         *
         * Available in all build types so integration-test helpers can seed the
         * manager without Boost.DLL.  Must NOT be called in production code paths.
         */
        void inject_loaded_plugin(std::shared_ptr<Plugin> plugin, PluginManifest manifest);

    private:
        struct LoadedPlugin {
            std::shared_ptr<boost::dll::shared_library> library;
            std::shared_ptr<Plugin> plugin;
            PluginManifest manifest;
            PluginRecord record;
        };

        using RequestFactory = std::shared_ptr<RequestPlugin> (*)();
        using ResolverFactory = std::shared_ptr<ResolverPlugin> (*)();

        [[nodiscard]] PluginOperationResult load_request_plugin(
            const String& library_path,
            std::optional<String> plugin_id);

        [[nodiscard]] PluginOperationResult load_resolver_plugin(
            const String& library_path,
            std::optional<String> plugin_id);

        [[nodiscard]] PluginOperationResult persist_record(
            const PluginManifest& manifest,
            bool is_loaded,
            const String& loaded_at,
            const String& unloaded_at = {});

        [[nodiscard]] static String normalize_route_key(const PluginRoute& route);
        [[nodiscard]] static String normalize_route_key(std::string_view method, std::string_view path);
        [[nodiscard]] static String manifest_to_json(const PluginManifest& manifest);
        [[nodiscard]] static std::optional<PluginManifest> manifest_from_json(const String& json_text);
        [[nodiscard]] static PluginRecord record_from_manifest(
            const PluginManifest& manifest,
            bool is_loaded,
            const String& loaded_at,
            const String& unloaded_at);
        [[nodiscard]] static String iso_now();

        [[nodiscard]] bool route_conflicts_with_loaded(const PluginManifest& manifest) const;
        [[nodiscard]] bool resolver_conflicts_with_loaded(const PluginManifest& manifest) const;
        [[nodiscard]] std::optional<LoadedPlugin> find_loaded_plugin(const String& plugin_id) const;

        std::shared_ptr<DatabaseManager> m_database;
        GqlExecutor* m_executor{nullptr};
        mutable std::mutex m_mutex;
        std::unordered_map<String, LoadedPlugin> m_loaded_plugins;
    };

} // namespace isched::v0_0_1::backend

