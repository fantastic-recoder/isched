// SPDX-License-Identifier: MPL-2.0
/**
 * @file hello_plugin.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Example resolver plugin exposing an echo field in the `isched_examples` scope.
 *
 * The plugin demonstrates the resolver plugin contract used by `PluginManager`:
 * it advertises one resolver binding, registers the resolver against the
 * `isched_examples` namespace, and returns the provided message unchanged.
 *
 * Example GraphQL query:
 * @code
 * query {
 *   isched_examples {
 *     echo(message: "hello world")
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

namespace isched::v0_0_1::example {
namespace {
    using backend::IResolverRegistrar;
    using backend::PluginKind;
    using backend::PluginManifest;
    using backend::PluginResolverBinding;
    using backend::ResolverCtx;
    using backend::ResolverPlugin;
    using json = nlohmann::json;

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
        manifest.description =
            "Example resolver plugin that exposes isched_examples.echo as a simple echo query.";
        manifest.resolverBindings.push_back(PluginResolverBinding{
            .scope = {"isched_examples"},
            .fieldName = "echo",
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

        void registerResolvers(IResolverRegistrar& registrar) override {
            registrar.register_resolver({"isched_examples"}, "echo",
                [](const json&, const json& args, const ResolverCtx&) -> json {
                    return resolveEcho(args);
                });
        }

        void unregisterResolvers(IResolverRegistrar& registrar) override {
            registrar.unregister_resolver({"isched_examples"}, "echo");
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

