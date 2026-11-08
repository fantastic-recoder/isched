// SPDX-License-Identifier: MPL-2.0
/**
 * @file request_demo_plugin.cpp
 * @brief Example request plugin used by SP-012 runtime load/dispatch tests.
 */

#include <isched/backend/isched_PluginSystem.hpp>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace isched::v0_0_1::example {
namespace {
    using backend::PluginHttpRequest;
    using backend::PluginHttpResponse;
    using backend::PluginKind;
    using backend::PluginManifest;
    using backend::PluginRoute;
    using backend::RequestPlugin;

    [[nodiscard]] std::string env_or(std::string_view key, std::string fallback) {
        if (const char* value = std::getenv(std::string(key).c_str()); value != nullptr && *value != '\0') {
            return value;
        }
        return fallback;
    }

    [[nodiscard]] PluginManifest make_manifest() {
        PluginManifest manifest;
        manifest.id = env_or("ISCHED_REQUEST_PLUGIN_ID", "request_demo_plugin");
        manifest.kind = PluginKind::Request;
        manifest.name = manifest.id;
        manifest.version = "1.0.0";
        manifest.abi_version = env_or("ISCHED_REQUEST_PLUGIN_ABI_VERSION", "1.0.0");
        manifest.description = "Example request plugin for load/dispatch/unload tests.";

        const auto route_path = env_or("ISCHED_REQUEST_PLUGIN_ROUTE", "/plugin/items/{id}");
        const auto route_method = env_or("ISCHED_REQUEST_PLUGIN_METHOD", "GET");
        manifest.requestRoutes.push_back(PluginRoute{.method = route_method, .path = route_path});

        const auto openapi_mode = env_or("ISCHED_REQUEST_PLUGIN_OPENAPI_MODE", "valid");
        if (openapi_mode == "invalid") {
            manifest.openApiFragmentJson = "{broken-openapi";
        } else {
            manifest.openApiFragmentJson = R"json({"paths":{"/plugin/items/{id}":{"get":{"responses":{"200":{"description":"ok"}}}}}})json";
        }
        return manifest;
    }

    class RequestDemoPlugin final : public RequestPlugin {
    public:
        [[nodiscard]] PluginManifest manifest() const override {
            return make_manifest();
        }

        [[nodiscard]] std::optional<PluginHttpResponse> handleRequest(const PluginHttpRequest& request) override {
            const auto crash_header = request.headers.find("X-ISCHED-CRASH");
            if (crash_header != request.headers.end() && crash_header->second == "1") {
                throw std::runtime_error("request-demo-plugin crash");
            }

            PluginHttpResponse response;
            response.status = 200;
            response.contentType = "application/json";
            response.body = std::string{"{\"plugin\":\"request_demo\",\"target\":\""} + request.target + "\"}";
            return response;
        }
    };
} // namespace

extern "C" std::shared_ptr<backend::RequestPlugin> isched_create_request_plugin() {
    return std::make_shared<RequestDemoPlugin>();
}

} // namespace isched::v0_0_1::example

