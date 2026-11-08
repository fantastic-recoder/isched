// SPDX-License-Identifier: MPL-2.0
/**
 * @file hello_plugin.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Example resolver plugin exposing an echo field in the `isched` namespace.
 *
 * The plugin demonstrates the resolver plugin contract used by `PluginManager`:
 * it advertises one resolver binding, registers the resolver against the
 * `isched` namespace, and returns the provided message unchanged.
 *
 * Example GraphQL query:
 * @code
 * query {
 *   isched {
 *     pluginEcho(message: "hello world")
 *   }
 * }
 * @endcode
 */

#include <isched/backend/isched_PluginSystem.hpp>

#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <cstdlib>

namespace isched::v0_0_1::example {
namespace {
    using backend::IResolverRegistrar;
    using backend::PluginKind;
    using backend::PluginManifest;
    using backend::PluginResolverBinding;
    using backend::ResolverCtx;
    using backend::ResolverPlugin;
    using backend::String;
    using json = nlohmann::json;

    [[nodiscard]] std::string env_or(std::string_view key, std::string fallback) {
        if (const char* value = std::getenv(std::string(key).c_str()); value != nullptr && *value != '\0') {
            return value;
        }
        return fallback;
    }

    /**
     * @brief Build the stable manifest advertised by the example plugin.
     * @return Manifest describing the plugin id, ABI version, and resolver binding.
     */
    [[nodiscard]] PluginManifest makeManifest() {
        PluginManifest manifest;
        manifest.id = "hello_plugin";
        manifest.kind = PluginKind::Resolver;
        manifest.name = "hello_plugin";
        manifest.version = "1.0.0";
        manifest.abi_version = env_or("ISCHED_HELLO_PLUGIN_ABI_VERSION", "1.0.0");
        manifest.description =
            "Example resolver plugin that exposes isched.pluginEcho as a simple echo query.";
        manifest.resolverSchemaSdl = env_or("ISCHED_HELLO_PLUGIN_MANIFEST_SCHEMA", "extend type IschedQueryNamespace { manifestOnlyField: String }");
        const auto field_name = env_or("ISCHED_HELLO_PLUGIN_FIELD", "pluginEcho");
        manifest.resolverBindings.push_back(PluginResolverBinding{
            .scope = {"isched"},
            .fieldName = field_name,
            .requiredRoles = {}
        });
        return manifest;
    }

    /**
     * @brief Resolve the echo field by returning the provided message unchanged.
     * @param args GraphQL field arguments.
     * @return The incoming message, or a helpful default when no message was provided.
     */
    [[nodiscard]] json resolveEcho(const json& args) {
        if (args.contains("message") && args["message"].is_string()) {
            return args["message"].get<std::string>();
        }
        return "hello from hello_plugin";
    }

    /**
     * @brief Concrete resolver plugin implementation for the hello example.
     */
    class HelloPlugin final : public ResolverPlugin {
    public:
        [[nodiscard]] PluginManifest manifest() const override {
            return makeManifest();
        }

        [[nodiscard]] String graphqlSchema() const override {
            const auto mode = env_or("ISCHED_HELLO_PLUGIN_SCHEMA_MODE", "valid");
            const auto field_name = env_or("ISCHED_HELLO_PLUGIN_FIELD", "pluginEcho");
            if (mode == "empty") {
                return "";
            }
            if (mode == "invalid") {
                return "extend type IschedQueryNamespace {";
            }
            if (mode == "conflict") {
                return "extend type IschedQueryNamespace { version: String! }";
            }
            return "extend type IschedQueryNamespace { " + field_name + "(message: String): String! }";
        }

        void registerResolvers(IResolverRegistrar& registrar) override {
            const bool should_throw = env_or("ISCHED_HELLO_PLUGIN_THROW", "0") == "1";
            const auto field_name = env_or("ISCHED_HELLO_PLUGIN_FIELD", "pluginEcho");
            registrar.register_resolver({"isched"}, field_name,
                [should_throw, field_name](const json&, const json& args, const ResolverCtx&) -> json {
                    if (should_throw) {
                        throw std::runtime_error("hello_plugin forced resolver crash");
                    }
                    if (field_name != "pluginEcho") {
                        if (args.contains("message") && args["message"].is_string()) {
                            return args["message"].get<std::string>();
                        }
                        return "hello from hello_plugin";
                    }
                    return resolveEcho(args);
                });
        }

        void unregisterResolvers(IResolverRegistrar& registrar) override {
            const auto field_name = env_or("ISCHED_HELLO_PLUGIN_FIELD", "pluginEcho");
            registrar.unregister_resolver({"isched"}, field_name);
        }
    };
} // namespace

/**
 * @brief Factory symbol loaded by `PluginManager` via Boost.DLL.
 * @return Resolver plugin instance ready to be registered into the executor.
 */
extern "C" std::shared_ptr<backend::ResolverPlugin> isched_create_resolver_plugin() {
    return std::make_shared<HelloPlugin>();
}

} // namespace isched::v0_0_1::example

