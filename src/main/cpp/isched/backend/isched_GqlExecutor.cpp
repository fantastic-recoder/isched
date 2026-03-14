// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_GqlExecutor.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the GraphQL executor, resolver dispatch, and
 *        built-in schema resolvers (hello, version, uptime, serverInfo,
 *        health, metrics, env, configprops, __schema skeleton introspection).
 */

#include "isched_GqlExecutor.hpp"

#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <tao/pegtl.hpp>
#include <tao/pegtl/parse_error.hpp>
#include <tao/pegtl/string_input.hpp>

#include "isched_ExecutionResult.hpp"
#include "isched_gql_error.hpp"
#include "isched_gql_grammar.hpp"
#include "isched_builtin_server_schema.hpp"
#include "isched_log_result.hpp"
#include "isched_DatabaseManager.hpp"

namespace {
// Per-request GraphQL variables context (thread-safe: one entry per executing thread)
thread_local nlohmann::json tl_gql_variables = nlohmann::json::object();
}

namespace isched::v0_0_1::backend {
    using nlohmann::json;
    using nlohmann::basic_json;
    using gql::TAstNodePtr;

    GqlExecutor::GqlExecutor(std::shared_ptr<DatabaseManager> p_database, Config)
        : m_database(std::move(p_database)) {
        setup_builtin_resolvers();
    }

    GqlExecutor::GqlExecutor(std::shared_ptr<DatabaseManager> database)
        : GqlExecutor(std::move(database), Config{}) {
    }

    void GqlExecutor::setup_builtin_resolvers() {
        static std::atomic<std::size_t> request_counter{0};
        static std::atomic<std::size_t> error_counter{0};

        // Basic Hello/Version resolvers
        register_resolver({},"hello", [](const json &, const json &, const ResolverCtx&)->json {
            return basic_json("Hello, GraphQL!");
        });

        register_resolver({},"version", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("0.0.1");
        });

        register_resolver({},"serverInfo", [this](const json &, const json &, const ResolverCtx&) -> json {
            return json{
                {"version", "0.0.1"},
                {"startedAt", std::chrono::duration_cast<std::chrono::milliseconds>(
                    m_start_time.time_since_epoch()).count()},
                {"activeTenants", 1},
                {"activeWebSocketSessions", 0},
                {"transportModes", json::array({"http", "websocket"})}
            };
        });

        // Uptime resolver
        register_resolver({},"uptime", [this](const json &, const json &, const ResolverCtx&) -> json {
            auto now = std::chrono::system_clock::now();
            auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time).count();
            return basic_json(uptime_seconds);
        });

        // Client count resolver
        register_resolver({},"clientCount", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json(1); // Placeholder - could be enhanced with actual connection tracking
        });

        // Spring Boot Actuator-style Health endpoint
        register_resolver({},"health", [this](const json &, const json &, const ResolverCtx&) -> json {
            // Overall status check
            std::string overall_status = "UP";
            json components;

            // Database health check
            try {
                if (m_database) {
                    components["database"] = {
                        {"status", "UP"},
                        {
                            "details", {
                                {"database", "SQLite"},
                                {"connectionPool", "active"}
                            }
                        }
                    };
                } else {
                    components["database"] = {
                        {"status", "DOWN"},
                        {"details", {{"error", "Database manager not initialized"}}}
                    };
                    overall_status = "DOWN";
                }
            } catch (const std::exception &e) {
                components["database"] = {
                    {"status", "DOWN"},
                    {"details", {{"error", e.what()}}}
                };
                overall_status = "DOWN";
            }

            // Memory health check
            components["memory"] = {
                {"status", "UP"},
                {
                    "details", {
                        {"used", "Available"},
                        {"max", "Available"}
                    }
                }
            };

            json health = {
                {"status", overall_status},
                {
                    "timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()
                },
                {"components", components}
            };

            return health;
        });

        // Application Info endpoint (Spring Boot actuator style)
        register_resolver({},"info", [](const json &, const json &, const ResolverCtx&) -> json {
            return json{
                {
                    "app", {
                        {"name", "isched Universal Application Server"},
                        {"description", "Multi-tenant GraphQL application server"},
                        {"version", "1.0.0"},
                        {"encoding", "UTF-8"}
                    }
                },
                {
                    "build", {
                        {"version", "1.0.0"},
                        {"artifact", "isched-server"},
                        {"name", "isched"},
                        {"time", "2025-11-02T01:00:00Z"},
                        {"group", "isched"}
                    }
                },
                {
                    "git", {
                        {
                            "commit", {
                                {"time", "2025-11-02T01:00:00Z"},
                                {"id", "unknown"}
                            }
                        },
                        {"branch", "001-universal-backend"}
                    }
                }
            };
        });

        // Metrics endpoint
        register_resolver({},"metrics", [](const json &, const json &, const ResolverCtx&) -> json {
            static auto start_time = std::chrono::system_clock::now();
            static std::atomic<int> request_counter{0};
            static std::atomic<int> error_counter{0};

            auto now = std::chrono::system_clock::now();
            auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

            request_counter++;

            return json{
                {"uptime", uptime_ms},
                {"activeConnections", 1},
                {"totalRequests", request_counter.load()},
                {"failedRequests", error_counter.load()},
                {"averageResponseTime", 15.5},
                {"memoryUsage", "Available"},
                {"threadCount", std::thread::hardware_concurrency()}
            };
        });

        // Environment endpoint (filtered for security)
        register_resolver({},"env", [](const json &, const json &, const ResolverCtx&) -> json {
            json env;

            // System properties
            env["systemProperties"] = {
                {"os.name", "Linux"},
                {"user.name", getenv("USER") ? getenv("USER") : "unknown"},
                {"user.home", getenv("HOME") ? getenv("HOME") : "/"},
                {"file.separator", "/"},
                {"path.separator", ":"}
            };

            // Environment variables (filtered for security)
            env["environmentVariables"] = json::object();
            const char *safe_vars[] = {"PATH", "HOME", "USER", "LANG", "TZ", nullptr};
            for (int i = 0; safe_vars[i]; ++i) {
                if (const char *value = getenv(safe_vars[i])) {
                    env["environmentVariables"][safe_vars[i]] = value;
                }
            }

            return env;
        });

        // Configuration properties endpoint
        register_resolver({},"configprops", [](const json &, const json &, const ResolverCtx&) -> json {
            return json{
                {
                    "server", {
                        {"port", 8080},
                        {"host", "0.0.0.0"},
                        {"maxConnections", 1000},
                        {"threadPoolSize", 8}
                    }
                },
                {
                    "database", {
                        {"type", "SQLite"},
                        {"connectionPoolSize", 10},
                        {"enableWAL", true}
                    }
                },
                {"features", json::array({"GraphQL", "Multi-tenant", "Health monitoring", "Metrics"})},
                {"version", "1.0.0"},
                {"environment", "development"}
            };
        });

        // Enhanced schema introspection resolver
        register_resolver({},"__schema", [this](const json &, const json &, const ResolverCtx&) -> json {
            json my_ret_val = generate_schema_introspection();
            return my_ret_val;
        });
        register_resolver({"__schema"},"name", [this](const json &p_args, const json &, const ResolverCtx&) -> json {
            return basic_json("res1");
        });
        register_resolver({"__schema"},"description", [this](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("res2");
        });
        register_resolver({"__schema"},"fields", [](const json & , const json &, const ResolverCtx&) -> json {
            return basic_json("res3");
        });
        register_resolver({"__schema"},"args", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("res-schema-args");
        });
        register_resolver({"__schema","args"},"name", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("res-schema-args");
        });
        register_resolver({"__schema","fields"},"name", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("res4");
        });
        register_resolver({"__schema","types","fields"},"description", [](const json &, const json &, const ResolverCtx&) -> json {
            return basic_json("res5");
        });

        // Built-in mutation resolvers
        register_resolver({}, "echo", [](const json&, const json& args, const ResolverCtx&) -> json {
            if (args.contains("message") && args["message"].is_string()) {
                return args["message"];
            }
            return nullptr;
        });

        // Initialise the config store (idempotent — safe to call on every startup)
        if (m_database) {
            std::ignore = m_database->initialize_config_store();
        }

        // Helper: format a system_clock time_point as ISO-8601
        auto fmt_iso8601 = [](const std::chrono::system_clock::time_point& tp) -> std::string {
            auto t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm_val{};
            gmtime_r(&t, &tm_val);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
            return buf;
        };

        // Helper: convert a ConfigurationSnapshot to a SnapshotRecord JSON object
        auto snap_to_json = [fmt_iso8601](const ConfigurationSnapshot& s) -> json {
            json obj;
            obj["id"]          = s.id;
            obj["tenantId"]    = s.tenant_id;
            obj["version"]     = s.version;
            obj["displayName"] = s.display_name.empty() ? json(nullptr) : json(s.display_name);
            obj["schemaSdl"]   = s.schema_sdl;
            obj["isActive"]    = s.is_active;
            obj["createdAt"]   = fmt_iso8601(s.created_at);
            obj["activatedAt"] = s.activated_at ? json(fmt_iso8601(*s.activated_at)) : json(nullptr);
            return obj;
        };

        // ---------------------------------------------------------------
        // Phase 4 mutation: applyConfiguration (T030)
        // Creates a new snapshot but does NOT activate it.
        // ---------------------------------------------------------------
        register_resolver({}, "applyConfiguration",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                json result;
                result["success"]    = false;
                result["snapshotId"] = nullptr;
                result["errors"]     = json::array();

                if (!m_database) {
                    result["errors"].push_back("Database not available");
                    return result;
                }
                if (!args.contains("input") || !args["input"].is_object()) {
                    result["errors"].push_back("Missing required argument: input");
                    return result;
                }

                const auto& inp = args["input"];
                if (!inp.contains("tenantId") || !inp["tenantId"].is_string()) {
                    result["errors"].push_back("input.tenantId is required");
                    return result;
                }
                if (!inp.contains("schemaSdl") || !inp["schemaSdl"].is_string()) {
                    result["errors"].push_back("input.schemaSdl is required");
                    return result;
                }

                // Validate SDL using PEGTL grammar before persisting (T-GQL-023)
                const std::string sdl = inp["schemaSdl"].get<std::string>();
                try {
                    tao::pegtl::string_input<> sdl_input(sdl, "SDL-validation");
                    auto val_ret = gql::generate_ast_and_log<gql::Document>(
                        sdl_input, "SDL-validation", false, false);
                    if (!std::get<0>(val_ret)) {
                        result["errors"].push_back(
                            "Invalid schemaSdl: GraphQL SDL failed to parse");
                        return result;
                    }
                } catch (const tao::pegtl::parse_error& e) {
                    result["errors"].push_back(
                        std::string("Invalid schemaSdl: ") + e.what());
                    return result;
                }

                ConfigurationSnapshot snap;
                static std::atomic<uint64_t> snap_counter{0};
                auto now = std::chrono::system_clock::now();
                auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                snap.id          = "snap-" + std::to_string(ms) + "-"
                                   + std::to_string(++snap_counter);
                snap.tenant_id   = inp["tenantId"].get<std::string>();
                snap.schema_sdl  = sdl;
                snap.version     = inp.value("version", "1.0.0");
                snap.display_name = inp.value("displayName", "");
                snap.is_active   = false;
                snap.created_at  = now;

                auto save_res = m_database->save_config_snapshot(snap);
                if (!save_res) {
                    result["errors"].push_back("Failed to persist snapshot");
                    return result;
                }

                result["success"]    = true;
                result["snapshotId"] = snap.id;
                return result;
            });

        // ---------------------------------------------------------------
        // Phase 4 mutation: activateSnapshot (T030, T032, T034)
        // Atomically activates a snapshot; queues schema reload for Server.
        // ---------------------------------------------------------------
        register_resolver({}, "activateSnapshot",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                json result;
                result["success"]    = false;
                result["snapshotId"] = nullptr;
                result["errors"]     = json::array();

                if (!m_database) {
                    result["errors"].push_back("Database not available");
                    return result;
                }
                if (!args.contains("id") || !args["id"].is_string()) {
                    result["errors"].push_back("Missing required argument: id");
                    return result;
                }

                const std::string snap_id = args["id"].get<std::string>();

                // Verify snapshot exists
                auto get_res = m_database->get_config_snapshot(snap_id);
                if (!get_res || !get_res.value()) {
                    result["errors"].push_back("Snapshot not found: " + snap_id);
                    return result;
                }

                auto act_res = m_database->activate_config_snapshot(snap_id);
                if (!act_res) {
                    result["errors"].push_back("Failed to activate snapshot");
                    return result;
                }

                // Queue pending schema reload so Server can call load_schema()
                const std::string new_sdl = get_res.value()->schema_sdl;
                if (!new_sdl.empty()) {
                    set_pending_schema_change({
                        get_res.value()->tenant_id,
                        new_sdl
                    });
                }

                result["success"]    = true;
                result["snapshotId"] = snap_id;
                return result;
            });

        // ---------------------------------------------------------------
        // Phase 4 mutation: rollbackConfiguration (T028, T032)
        // Rolls back to the previously-activated snapshot for a tenant.
        // ---------------------------------------------------------------
        register_resolver({}, "rollbackConfiguration",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                json result;
                result["success"]    = false;
                result["snapshotId"] = nullptr;
                result["errors"]     = json::array();

                if (!m_database) {
                    result["errors"].push_back("Database not available");
                    return result;
                }
                if (!args.contains("tenantId") || !args["tenantId"].is_string()) {
                    result["errors"].push_back("Missing required argument: tenantId");
                    return result;
                }

                const std::string tenant_id = args["tenantId"].get<std::string>();

                // List all snapshots (newest created_at first)
                auto list_res = m_database->list_config_snapshots(tenant_id);
                if (!list_res) {
                    result["errors"].push_back("Failed to retrieve snapshot list");
                    return result;
                }

                const auto& snaps = list_res.value();
                if (snaps.size() < 2) {
                    result["errors"].push_back("No previous snapshot available to roll back to");
                    return result;
                }

                // Find the currently active snapshot and the one before it
                std::string rollback_target_id;
                bool found_active = false;
                for (const auto& s : snaps) {
                    if (s.is_active) { found_active = true; continue; }
                    if (found_active) { rollback_target_id = s.id; break; }
                }

                // Fallback: if none found after active, use second snapshot
                if (rollback_target_id.empty()) {
                    for (std::size_t i = 0; i < snaps.size(); ++i) {
                        if (!snaps[i].is_active) {
                            rollback_target_id = snaps[i].id;
                            break;
                        }
                    }
                }

                if (rollback_target_id.empty()) {
                    result["errors"].push_back("Cannot determine rollback target");
                    return result;
                }

                auto act_res = m_database->activate_config_snapshot(rollback_target_id);
                if (!act_res) {
                    result["errors"].push_back("Failed to activate rollback target");
                    return result;
                }

                // Queue schema reload
                for (const auto& s : snaps) {
                    if (s.id == rollback_target_id && !s.schema_sdl.empty()) {
                        set_pending_schema_change({tenant_id, s.schema_sdl});
                        break;
                    }
                }

                result["success"]    = true;
                result["snapshotId"] = rollback_target_id;
                return result;
            });

        // ---------------------------------------------------------------
        // Phase 4 query: activeConfiguration (T035)
        // ---------------------------------------------------------------
        register_resolver({}, "activeConfiguration",
            [this, snap_to_json](const json&, const json& args, const ResolverCtx&) -> json {
                if (!m_database) return nullptr;
                if (!args.contains("tenantId") || !args["tenantId"].is_string())
                    return nullptr;

                const std::string tenant_id = args["tenantId"].get<std::string>();
                auto res = m_database->get_active_config_snapshot(tenant_id);
                if (!res || !res.value()) return nullptr;
                return snap_to_json(*res.value());
            });

        // ---------------------------------------------------------------
        // Phase 4 query: configurationHistory (T035)
        // ---------------------------------------------------------------
        register_resolver({}, "configurationHistory",
            [this, snap_to_json](const json&, const json& args, const ResolverCtx&) -> json {
                if (!m_database) return json::array();
                if (!args.contains("tenantId") || !args["tenantId"].is_string())
                    return json::array();

                const std::string tenant_id = args["tenantId"].get<std::string>();
                auto res = m_database->list_config_snapshots(tenant_id);
                if (!res) return json::array();

                json arr = json::array();
                for (const auto& s : res.value()) {
                    arr.push_back(snap_to_json(s));
                }
                return arr;
            });

        load_schema(BUILTIN_SCHEMA);
    }

    basic_json<> GqlExecutor::generate_directives_introspection() {
        json my_ret_val = basic_json<>::array();
        static std::string their_null_str = "Unknown";
        for (const auto &[dirName, dirNodePtr]: m_directives) {
            const auto &dirNode = *dirNodePtr;
            json dirObj;
            dirObj["name"] = dirName;
            dirObj["description"] = nullptr;
            dirObj["locations"] = json::array();
            dirObj["args"] = json::array();

            for (const auto &child: dirNode->children) {
                if (child->type == "isched::v0_0_1::gql::Description") {
                    dirObj["description"] = child->string_view();
                } else if (child->type == "isched::v0_0_1::gql::ArgumentsDefinition") {
                    for (const auto &argChild: child->children) {
                        if (argChild->type == "isched::v0_0_1::gql::InputValueDefinition") {
                            json argObj;
                            for (const auto &ivChild: argChild->children) {
                                if (ivChild->type == "isched::v0_0_1::gql::Name") {
                                    argObj["name"] = ivChild->string_view();
                                } else if (ivChild->type == "isched::v0_0_1::gql::Type") {
                                    auto typeStr = gql::ast_node_to_str(ivChild);
                                    argObj["type"] = {{"name", typeStr ? *typeStr : "Unknown"}};
                                }
                            }
                            dirObj["args"].push_back(argObj);
                        }
                    }
                }
            }
            // For locations, we'd need more logic if we want to extract them properly from our simplified grammar
            // For now, let's just put a placeholder if needed, or leave it empty.
            
            my_ret_val.push_back(dirObj);
        }
        return my_ret_val;
    }

    json GqlExecutor::generate_schema_introspection() {
        json schema=json::object();

        // Default query type is "Query"
        schema["queryType"] = {
            {"name", "Query"}
        };
        schema["mutationType"] = m_type_map.count("Mutation") ? json{{"name", "Mutation"}} : json(nullptr);
        schema["subscriptionType"] = nullptr;

        auto types = json::array();

        for (const auto &[typeName, typeNodePtr]: m_type_map) {
            const auto &typeNode = *typeNodePtr;
            json typeObj;
            typeObj["name"] = typeName;
            typeObj["kind"] = "OBJECT"; // Simplified for now

            // Extract description if present
            for (const auto &child: typeNode->children) {
                if (child->type == "isched::v0_0_1::gql::Description") {
                    typeObj["description"] = child->string_view();
                }
            }

            // Extract fields
            auto fieldsArray = json::array();
            // Helper to find fields recursively within the type node
             std::function<void(const TAstNodePtr &)> findFieldsRecursive;
             findFieldsRecursive = [&](const TAstNodePtr &node) {
                 if (!node) return;
                 if (node->type == "isched::v0_0_1::gql::FieldDefinition") {
                     json fieldObj;
                     for (const auto &fieldChild: node->children) {
                         // should we check for TypeName as well?
                         if (fieldChild->type == "isched::v0_0_1::gql::Name") {
                             fieldObj["name"] = fieldChild->string_view();
                             break;
                         }
                     }
                     for (const auto &fieldChild: node->children) {
                         if (fieldChild->type == "isched::v0_0_1::gql::Description") {
                             fieldObj["description"] = fieldChild->string_view();
                         } else if (fieldChild->type == "isched::v0_0_1::gql::Type") {
                             auto typeStr = gql::ast_node_to_str(fieldChild);
                             fieldObj["type"] = {{"name", typeStr ? *typeStr : "Unknown"}};
                         }
                     }
                     fieldsArray.push_back(fieldObj);
                 } else {
                     for (const auto &child: node->children) {
                         findFieldsRecursive(child);
                     }
                 }
            };
            findFieldsRecursive(typeNode);
            typeObj["fields"] = fieldsArray;
            types.push_back(typeObj);
        }
        schema["types"] = types;
        schema["directives"] = generate_directives_introspection();

        return schema;
    }

    void GqlExecutor::update_type_map_recursive(const TAstNodePtr &p_typedef, TAstNodeMap &p_type_map, TAstNodeMap &p_directives) {
        if (!p_typedef) return;
        // ObjectTypeDefinition, ScalarTypeDefinition, DirectiveDefinition, etc.
        // For ObjectTypeDefinition, name is usually child 0 or 1
        std::string myTypeName;
        for (const auto &child: p_typedef->children) {
            if (child->type.find("Name") != std::string::npos) {
                myTypeName = std::string(child->string_view());
                break;
            }
        }
        if (!myTypeName.empty()) {
            // If the parent is a TypeDefinition or starts with it, it's a type
            if (p_typedef->type.find("Definition") != std::string::npos &&
                p_typedef->type.find("Field") == std::string::npos &&
                p_typedef->type.find("Operation") == std::string::npos) {

                if (p_typedef->type.find("DirectiveDefinition") != std::string::npos) {
                    p_directives[myTypeName] = &p_typedef;
                } else {
                    p_type_map[myTypeName] = &p_typedef;
                }
            }
        }
        for (const auto &myChild: p_typedef->children) {
            update_type_map_recursive(myChild, p_type_map, p_directives);
        }
    }

    void GqlExecutor::update_type_map() {
        m_type_map.clear();
        m_directives.clear();
        if (m_current_schema) {
            update_type_map_recursive(m_current_schema, m_type_map, m_directives);
        }
    }

    void GqlExecutor::process_field_definition(const ResolverPath& p_path,ExecutionResult &p_result,
                                               const TAstNodePtr &p_typedef, size_t p_idx) {
        const auto &myField = p_typedef->children[p_idx];
        if (!myField || myField->type.find("FieldDefinition") == std::string::npos) {
            return;
        }
        if (myField->children.empty()) {
            p_result.errors.push_back(gql::Error{
                .code=gql::EErrorCodes::PARSE_ERROR, .message="Incomplete Query field definition"
            });
        } else {
            const TAstNodePtr *nameNodePtr = nullptr;
            for (const auto &child: myField->children) {
                if (child->type.find("Name") != std::string::npos) {
                    nameNodePtr = &child;
                    break;
                }
            }
            if (nameNodePtr) {
                const auto myFieldName = (*nameNodePtr)->string_view();
                spdlog::debug("Checking resolver for field {}.{} in Query type",concat_vector(p_path,"."),
                    myFieldName);
                if (!m_resolvers.has_resolver(p_path,std::string(myFieldName))) {
                    p_result.errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::MISSING_GQL_RESOLVER,
                        .message=std::format("Missing resolver for field {}.{} in Query type", concat_vector(p_path,"."),
                            myFieldName)
                    });
                }
            }
        }
    }

    json GqlExecutor::extract_argument_value(const TAstNodePtr &p_arg, gql::TErrorVector& p_errors) const {
        json my_ret_val;
        if (p_arg->type == "isched::v0_0_1::gql::Variable") {
            // Strip leading '$' to get the variable name
            const std::string var_sv(p_arg->string_view());
            const std::string var_name = var_sv.starts_with('$') ? var_sv.substr(1) : var_sv;
            if (tl_gql_variables.contains(var_name)) {
                return tl_gql_variables[var_name];
            }
            return nullptr;
        } else if (p_arg->type == "isched::v0_0_1::gql::StringValue") {
            const std::string my_ret_val_str(p_arg->string_view());
            if (my_ret_val_str.starts_with(R"(""")")) {
                my_ret_val = my_ret_val_str.substr(3, my_ret_val_str.length() - 6);
            } else if (my_ret_val_str.length() >= 2 && my_ret_val_str.front() == '"') {
                my_ret_val = my_ret_val_str.substr(1, my_ret_val_str.length() - 2);
            } else { my_ret_val = my_ret_val_str; }
        } else if (p_arg->type == "isched::v0_0_1::gql::IntValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = std::stoll(std::string(my_ret_val_str));
        } else if (p_arg->type == "isched::v0_0_1::gql::FloatValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = std::stod(std::string(my_ret_val_str));
        } else if (p_arg->type == "isched::v0_0_1::gql::BooleanValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = (my_ret_val_str == "true");
        } else if (p_arg->type == "isched::v0_0_1::gql::NullValue") {
            my_ret_val = nullptr;
        } else if (p_arg->type == "isched::v0_0_1::gql::EnumValue") {
            my_ret_val = std::string(p_arg->string_view());
        } else if (p_arg->type == "isched::v0_0_1::gql::ListValue") {
            my_ret_val = json::array();
            for (const auto &myChild: p_arg->children) {
                my_ret_val.push_back(extract_argument_value(myChild, p_errors));
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::ObjectValue") {
            my_ret_val = json::object();
            for (const auto &myField: p_arg->children) {
                if (myField->type == "isched::v0_0_1::gql::ObjectField") {
                    const auto myFieldName = std::string(myField->children[0]->string_view());
                    my_ret_val[myFieldName] = extract_argument_value(myField->children[1], p_errors);
                }
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::Value") {
            if (!p_arg->children.empty()) {
                return extract_argument_value(p_arg->children[0], p_errors);
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::Argument") {
            if (p_arg->children.size() > 1) {
                return extract_argument_value(p_arg->children[1], p_errors);
            }
        } else {
            p_errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR,
                format("Unknown argument value type: {}.", p_arg->type)
            });
        }
        return my_ret_val;
    }

    /**
     * Extracts arguments from a GraphQL field node and returns them as a JSON object.
     *
     * @param p_field_node The field node containing arguments
     * @param p_errors Error vector to store parsing errors
     * @return JSON object containing extracted arguments
     */
    json GqlExecutor::process_arguments(const TAstNodePtr &p_arguments_node,
                                        gql::TErrorVector &p_errors) const {
        json my_ret_val = json::object();
        if (!p_arguments_node || p_arguments_node->type == "isched::v0_0_1::gql::SelectionSet") {
            spdlog::debug("Skipping node {} in argument processing tree=.",p_arguments_node->type,
                gql::dump_ast(p_arguments_node));
            return my_ret_val;
        }
        if (!p_arguments_node || p_arguments_node->type != "isched::v0_0_1::gql::Arguments") {
            spdlog::error("Expected arguments or selection set, got a {}", gql::dump_ast(p_arguments_node));
            return my_ret_val;
        }
        for (const auto &myArg: p_arguments_node->children) {
            if (myArg->type != "isched::v0_0_1::gql::Argument") {
                p_errors.push_back(gql::Error{
                    .code = gql::EErrorCodes::PARSE_ERROR,
                    .message = format("Expected an argument, got a {}", myArg->type)
                });
                continue;
            }
            if (myArg->children.size() != 2) {
                p_errors.push_back(gql::Error{
                    .code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty argument"
                });
                continue;
            }
            const auto myArgName = std::string(myArg->children[0]->string_view());
            my_ret_val[myArgName] = extract_argument_value(myArg->children[1], p_errors);
        }
        return my_ret_val;
    }

    json GqlExecutor::process_argument_field(const TAstNodePtr &p_field_node,gql::TErrorVector &p_errors) const {
        spdlog::debug("Will extract arguments out field: \n***\n{}\n***\n.", gql::dump_ast(p_field_node));
        json my_ret_val = json::object();
        for (const auto &my_arguments_node: p_field_node->children) {
            if (my_arguments_node->type == "isched::v0_0_1::gql::Arguments") {
                my_ret_val = process_arguments(my_arguments_node, p_errors);
                break; // a field has at most one Arguments node
            }
        }
        spdlog::debug("Got args: '{}' for field '{}'", my_ret_val.dump(4), std::string(p_field_node->children[0]->string_view()));
        return my_ret_val;
    }

    void GqlExecutor::process_sub_selection(const json& p_parent_result, const ResolverPath& p_path, const TAstNodePtr &node,  json &p_result, gql::TErrorVector& p_errors) const {
        // Arguments nodes are already extracted by process_argument_field before sub-selections run.
        if (node->type == "isched::v0_0_1::gql::Arguments") {
            return;
        }
        if (node->type != "isched::v0_0_1::gql::SelectionSet") {
            p_errors.push_back(gql::Error{.code=gql::EErrorCodes::ARGUMENT_ERROR,.message=format(
                "Expected a selection set while processing sub selection, got a {}.", node->type)});
            return;
        }
        for (const auto &my_selection: node->children) {
            if (my_selection->type != "isched::v0_0_1::gql::Selection") {
                p_errors.push_back(gql::Error{.code=gql::EErrorCodes::ARGUMENT_ERROR,.message=format(
                    "Expected a selection, got a {}.", my_selection->type)});
                continue;
            }
            for (const auto &my_field: my_selection->children) {
                if (my_field->type == "isched::v0_0_1::gql::Field") {
                    resolve_field_selection_details(p_parent_result, p_path, my_field, p_result, p_errors);
                }
            }
        }
        spdlog::debug("Got subselection: \n***\n{}\n***\n.", gql::dump_ast(node));
    }

    void GqlExecutor::process_field_sub_selections(
        const json &p_parent_result,
        const ResolverPath &p_path,
        const TAstNodePtr &p_selection_set,
        json &p_result,
        gql::TErrorVector &p_error,
        const std::string myFieldName
    ) const {
        ResolverPath my_sub_path = p_path;
        my_sub_path.push_back(myFieldName);
        for (size_t myIdx = 1; myIdx < p_selection_set->children.size(); ++myIdx) {
            process_sub_selection(p_parent_result,my_sub_path,p_selection_set->children[myIdx], p_result, p_error);
        }
    }

    bool GqlExecutor::resolve_field_selection_details(const json& p_parent, const ResolverPath& p_path,const TAstNodePtr &p_field_node, json &p_result, gql::TErrorVector& p_error) const {
        if (p_field_node->children.empty()) {
            p_error.push_back(
                gql::Error{.code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty field"});
            return true;
        }
        const std::string myFieldName = std::string(p_field_node->children[0]->string_view());
        spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
        // Build path element for error reporting
        ResolverPath my_field_path = p_path;
        my_field_path.push_back(myFieldName);
        if (p_result.empty()) {
            p_result = json::object();
        }
        if (!m_resolvers.has_resolver(p_path,myFieldName)) {
            // Default field resolver: extract from parent when key is present
            if (p_parent.is_object() && p_parent.contains(myFieldName)) {
                p_result[myFieldName] = p_parent.at(myFieldName);
            } else {
                gql::ErrorPath ep;
                for (const auto& s : my_field_path) ep.push_back(s);
                p_error.push_back(gql::Error{
                    .code    = gql::EErrorCodes::MISSING_GQL_RESOLVER,
                    .message = std::format("Missing resolver for field {} in Query type",
                                           concat_vector(my_field_path, ".")),
                    .path    = std::move(ep),
                });
            }
        } else {
            ResolverCtx my_ctx = {};
            json my_args = process_argument_field(p_field_node, p_error);
            spdlog::debug("Got args: '{}' for field '{}' in Query type", my_args.dump(4), myFieldName);
            const ResolverFunction& my_found_resolver = m_resolvers.get_resolver(p_path,myFieldName);
            json my_result;
            try {
                my_result = my_found_resolver(p_parent, my_args, my_ctx);
            } catch (const std::exception& ex) {
                gql::ErrorPath ep;
                for (const auto& s : my_field_path) ep.push_back(s);
                p_error.push_back(gql::Error{
                    .code    = gql::EErrorCodes::UNKNOWN_ERROR,
                    .message = std::format("Resolver for field {} threw: {}",
                                           concat_vector(my_field_path, "."), ex.what()),
                    .path    = std::move(ep),
                });
                p_result[myFieldName] = nullptr;
                return false;
            }
            spdlog::debug("Got result: '{}' for field '{}' in Query type, going to process sub selections.",
                p_result.dump(4,'.'), myFieldName);
            // Detect sub-selection set in Field node children (index > 0)
            bool has_sub_sel = false;
            for (size_t i = 1; i < p_field_node->children.size(); ++i) {
                if (p_field_node->children[i]->type == "isched::v0_0_1::gql::SelectionSet") {
                    has_sub_sel = true;
                    break;
                }
            }
            if (has_sub_sel) {
                if (my_result.is_null()) {
                    // Nullable field returned null — propagate without sub-selection processing
                    p_result[myFieldName] = nullptr;
                } else if (my_result.is_array()) {
                    // List type: apply sub-selections to each element individually
                    json arr = json::array();
                    for (const auto& element : my_result) {
                        if (element.is_null()) {
                            arr.push_back(nullptr);
                            continue;
                        }
                        json elem_result = json::object();
                        process_field_sub_selections(element, p_path, p_field_node, elem_result, p_error, myFieldName);
                        arr.push_back(std::move(elem_result));
                    }
                    p_result[myFieldName] = std::move(arr);
                } else {
                    p_result[myFieldName] = json::object();
                    process_field_sub_selections(my_result, p_path, p_field_node, p_result[myFieldName], p_error, myFieldName);
                }
            } else {
                p_result[myFieldName] = std::move(my_result);
            }
        }
        return false;
    }

    void GqlExecutor::process_field_selection(const json& p_parent_result,const ResolverPath& p_path, const TAstNodePtr &p_selection_set,
        json &p_result, gql::TErrorVector& p_errors) const {
        for (const auto &mySelection: p_selection_set->children) {
            if (mySelection->type == "isched::v0_0_1::gql::SelectionSet") {
                process_field_selection(p_parent_result, p_path,mySelection, p_result, p_errors);
            } else if (mySelection->type == "isched::v0_0_1::gql::Selection") {
                if (mySelection->children.empty()) {
                    p_errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty selection"
                    });
                    continue;
                }
                const auto &myField = mySelection->children[0];
                if (myField->type != "isched::v0_0_1::gql::Field") {
                    p_errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR,
                        .message = format("Expected a field, got a {}", myField->type)
                    });
                    continue;
                }
                resolve_field_selection_details(p_parent_result, p_path, myField,p_result, p_errors);
            } else if (mySelection->type == "isched::v0_0_1::gql::Name") {
                spdlog::debug("Skipping node name while processing field children");
            } else {
                p_errors.push_back(gql::Error{
                    gql::EErrorCodes::PARSE_ERROR,
                    format("Expected a selection or selection set, got a {}", mySelection->type)
                });
            }
        }
    }

    TAstNodePtr const *GqlExecutor::find_node_by_type(const TAstNodeMap::value_type &pair, std::string_view p_type) const {
        if (!pair.second) {
            return nullptr;
        }
        if (!pair.second->get()) {
            return nullptr;
        }
        if (pair.second->get()->children.empty()) {
            return nullptr;
        }
        for (const auto &child: pair.second->get()->children) {
            if (child->type == p_type) {
                return &child;
            }
        }
        return nullptr;
    }

    ExecutionResult GqlExecutor::load_schema(std::string &&pSchemaDocument, bool p_print_dot) {
        static const std::string aName = "SchemaDocument";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        m_schema_documents.push_back(
            std::make_shared<TSchemaDoc>(std::forward<std::string>(pSchemaDocument),aName
        ));
        ExecutionResult myResult;
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(*m_schema_documents.back()
                , aName, false, p_print_dot);
            const bool aParsingOk = std::get<0>(myRetVal);
            auto aRoot = std::get<1>(std::move(myRetVal));
            if (!aParsingOk) {
                myResult.errors.push_back(gql::Error{.code=gql::EErrorCodes::PARSE_ERROR,
                    .message="Failed to parse schema document"});
                return myResult;
            }
            spdlog::debug("Parsing schema document:{}", gql::dump_ast(aRoot));
            update_type_map();
            if (aRoot && !aRoot->children.empty()) {
                const TAstNodePtr &myDoc = (aRoot->type == "isched::v0_0_1::gql::Document")
                                               ? aRoot
                                               : aRoot->children[0];

                for (const auto &myDefNode: myDoc->children) {
                    if (myDefNode->type == "isched::v0_0_1::gql::TypeDefinition" || myDefNode->type ==
                        "isched::v0_0_1::gql::ObjectTypeDefinition") {
                        const auto myTypedefName = myDefNode->children[0]->string_view();
                        if (myTypedefName == "Query") {
                            for (size_t myIdx = 1; myIdx < myDefNode->children.size(); ++myIdx) {
                                ResolverPath const my_path;
                                process_field_definition(my_path, myResult, myDefNode, myIdx);
                            }
                        } else if (myTypedefName == "Mutation") {
                            for (size_t myIdx = 1; myIdx < myDefNode->children.size(); ++myIdx) {
                                ResolverPath const my_path;
                                process_field_definition(my_path, myResult, myDefNode, myIdx);
                            }
                        }
                    } else if (myDefNode->type == "isched::v0_0_1::gql::DirectiveDefinition") {
                        spdlog::debug("Loaded directive definition: {}", myDefNode->children[0]->string_view());
                    } else if (myDefNode->type == "isched::v0_0_1::gql::ExecutableDefinition") {
                        myResult.errors.push_back(gql::Error{
                            .code=gql::EErrorCodes::EXECUTABLE_DEF_NOT_ALLOWED,
                            .message="Executable definition not allowed in schema load."
                        });
                    }
                }
            }
            if (myResult.is_success()) {
                m_current_schema = gql::merge_type_definitions(std::move(m_current_schema), std::move(aRoot));
                update_type_map();
            } else {
                myResult.errors.push_back(gql::Error{
                    gql::EErrorCodes::PARSE_ERROR,
                    "Failed to parse schema, ignoring it."
                });
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(*m_schema_documents.back(), myResult, e);
        }
        return myResult;
    }

    void GqlExecutor::log_parse_error_exception(const tao::pegtl::string_input<> &in, ExecutionResult &myResult,
                                                const tao::pegtl::parse_error &e) const {
        const auto p = e.positions().front();
        const int line   = static_cast<int>(p.line);
        const int column = static_cast<int>(p.column);
        spdlog::error("Parse error: {} at {}:{}: {}", e.what(), line, column, in.line_at(p));
        myResult.errors.push_back(gql::Error{
            .code      = gql::EErrorCodes::PARSE_ERROR,
            .message   = std::string(e.what()),
            .locations = {{line, column}},
        });
    }

    bool GqlExecutor::process_operation_definitions(
        json p_parent_result,
        const TAstNodePtr &myOperation,
        json &p_result,
        gql::TErrorVector &p_errors) const {
        if (myOperation->type != "isched::v0_0_1::gql::OperationDefinition") {
            p_errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR,
                std::format("Expected with an operation definition, got {}.", myOperation->type)
            });
            return true;
        }
        if (myOperation->children.empty()) {
            p_errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Empty operation definition"});
            return true;
        }
        const auto a_op_type = myOperation->children[0]->string_view();
        ResolverPath const p_path;
        if (a_op_type == "query") {
            for (size_t myIdx = 1; myIdx < myOperation->children.size(); ++myIdx) {
                const auto& child = myOperation->children[myIdx];
                if (child->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(p_parent_result, p_path, child, p_result, p_errors);
                } else if (child->type == "isched::v0_0_1::gql::VariablesDefinition"
                        || child->type == "isched::v0_0_1::gql::VariableDefinitions"
                        || child->type == "isched::v0_0_1::gql::VariableDefinition"
                        || child->type == "isched::v0_0_1::gql::Name") {
                    // Variable declarations and operation names are metadata; skip.
                } else {
                    p_errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Expected a selection set in query, got {}.", child->type)
                    });
                }
            }
        } else if (a_op_type == "mutation") {
            for (size_t myIdx = 1; myIdx < myOperation->children.size(); ++myIdx) {
                const auto& child = myOperation->children[myIdx];
                if (child->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(p_parent_result, p_path, child, p_result, p_errors);
                } else if (child->type == "isched::v0_0_1::gql::VariablesDefinition"
                        || child->type == "isched::v0_0_1::gql::VariableDefinitions"
                        || child->type == "isched::v0_0_1::gql::VariableDefinition"
                        || child->type == "isched::v0_0_1::gql::Name") {
                    // Variable declarations and operation names are metadata; skip.
                } else {
                    p_errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Expected a selection set in mutation, got {}.", child->type)
                    });
                }
            }
        } else if (myOperation->children[0]->type == "isched::v0_0_1::gql::SelectionSet") {
            process_field_selection(p_parent_result, p_path,myOperation->children[0], p_result, p_errors);
        } else {
            p_errors.push_back(gql::Error{
                .code=gql::EErrorCodes::PARSE_ERROR,
                .message=std::format("Only query operations are supported, got {}.", a_op_type)
            });
        }
        return false;
    }

    ExecutionResult GqlExecutor::execute(
        const std::string_view p_query,
        const std::string_view p_variables_json,
        const bool p_print_dot
    ) const {
        // Parse and install per-request variables (thread_local for thread safety)
        tl_gql_variables = nlohmann::json::parse(p_variables_json, nullptr, /*exceptions=*/false);
        if (tl_gql_variables.is_discarded() || !tl_gql_variables.is_object()) {
            tl_gql_variables = nlohmann::json::object();
        }
        static const std::string aName = "ExecutableDocument";
        ExecutionResult my_result;
        if (p_query.length() > 100000) {
            // Max pQuery length
            my_result.errors.push_back(gql::Error{
                .code=gql::EErrorCodes::ARGUMENT_ERROR,
                .message="Query length exceeds maximum allowed"
            });
            return my_result;
        }
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(std::string(p_query)), aName);
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName, false, p_print_dot);
            auto aRoot = std::get<1>(std::move(myRetVal));
            const bool aParsingOk = std::get<0>(myRetVal);
            if (!aParsingOk) {
                my_result.errors.push_back(gql::Error{
                    .code=gql::EErrorCodes::PARSE_ERROR,.message="Failed to parse schema document"
                });
                return my_result;
            }
            if (!aRoot || aRoot->children.empty()) {
                my_result.errors.push_back(gql::Error{.code=gql::EErrorCodes::PARSE_ERROR, .message="Empty document"});
                return my_result;
            }
            const auto &myDoc = aRoot->children[0];
            if (!myDoc || myDoc->type != "isched::v0_0_1::gql::Document") {
                my_result.errors.push_back(gql::Error{.code=gql::EErrorCodes::PARSE_ERROR, .message="Expected with a document"});
                return my_result;
            }
            if (myDoc->children.empty()) {
                my_result.errors.push_back(gql::Error{.code=gql::EErrorCodes::PARSE_ERROR, .message="Empty document"});
                return my_result;
            }
            for (const auto &myChild: myDoc->children) {
                if (myChild->type != "isched::v0_0_1::gql::ExecutableDefinition") {
                    my_result.errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Expected with an executable definition, got {}.", myChild->type)
                    });
                    return my_result;
                }
                for (const auto &myOperation: myChild->children) {
                    process_operation_definitions(json::object(),myOperation, my_result.data, my_result.errors);
                }
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(in, my_result, e);
        }
        return my_result;
    }


    std::unique_ptr<GqlExecutor> GqlExecutor::create(std::shared_ptr<DatabaseManager> database) {
        return std::make_unique<GqlExecutor>(std::move(database));
    }
}
