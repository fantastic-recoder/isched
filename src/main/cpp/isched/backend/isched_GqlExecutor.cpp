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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <nlohmann/json.hpp>
#include <thread>
#include <spdlog/spdlog.h>
#include <string>
#include <tao/pegtl.hpp>
#include <tao/pegtl/parse_error.hpp>
#include <tao/pegtl/string_input.hpp>

#include "isched_AuthenticationMiddleware.hpp"
#include "isched_CryptoUtils.hpp"
#include "isched_RateLimiter.hpp"
#include "isched_ExecutionResult.hpp"
#include "isched_gql_error.hpp"
#include "isched_gql_grammar.hpp"
#include "isched_builtin_server_schema.hpp"
#include "isched_log_result.hpp"
#include "isched_DatabaseManager.hpp"
#include "isched_RestDataSource.hpp"
#include "isched_SubscriptionBroker.hpp"
#include "isched_MetricsCollector.hpp"

namespace {
// Per-request GraphQL variables context (thread-safe: one entry per executing thread)
// NOLINTNEXTLINE(cert-err58-cpp) -- json::object() is functionally noexcept; only theoretical std::bad_alloc risk
thread_local nlohmann::json tl_gql_variables = nlohmann::json::object();

// Per-request resolver context (auth info populated by execute() overload that accepts ctx)
thread_local isched::v0_0_1::backend::ResolverCtx tl_resolver_ctx;

// ---------------------------------------------------------------------------
// Phase 5b: GraphQL Introspection helpers (T-INTRO-001 … T-INTRO-031)
// ---------------------------------------------------------------------------
using nlohmann::json;
using isched::v0_0_1::gql::TAstNodePtr;
using TAstNodeMap = std::map<std::string, const TAstNodePtr*>;

/// Strip surrounding ("…") or ("""…""") from a Description raw string.
static std::string strip_description(std::string_view raw) {
    if (raw.size() >= 6 && raw.substr(0, 3) == R"(""")" && raw.substr(raw.size()-3) == R"(""")") {
        return std::string(raw.substr(3, raw.size()-6));
    }
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return std::string(raw.substr(1, raw.size()-2));
    }
    return std::string(raw);
}

/// Return the description JSON value (null if absent) from the immediate
/// children of the given AST node.
static json extract_description(const TAstNodePtr& node) {
    if (!node) return nullptr;
    for (const auto& c : node->children) {
        if (c->type.ends_with("gql::Description")) {
            return strip_description(c->string_view());
        }
    }
    return nullptr;
}

/// Return true if `name` is one of the five built-in scalar types.
static bool is_builtin_scalar(const std::string& name) {
    return name == "String" || name == "Int" || name == "Float" ||
           name == "Boolean" || name == "ID";
}

/// Determine the __TypeKind string for the type stored in the type-map.
/// The stored node may be TypeDefinition (for OBJECT) or a specific
/// definition node (SCALAR, INTERFACE, UNION, ENUM, INPUT_OBJECT).
static std::string get_type_kind_for_node(const TAstNodePtr& node) {
    const auto t = node->type;
    if (t.ends_with("ScalarTypeDefinition"))      return "SCALAR";
    if (t.ends_with("InterfaceTypeDefinition"))   return "INTERFACE";
    if (t.ends_with("UnionTypeDefinition"))       return "UNION";
    if (t.ends_with("EnumTypeDefinition"))        return "ENUM";
    if (t.ends_with("InputObjectTypeDefinition")) return "INPUT_OBJECT";
    // TypeDefinition without a specific child  → ObjectTypeDefinition bubbled up
    if (t.ends_with("gql::TypeDefinition")) {
        for (const auto& c : node->children) {
            const auto ct = c->type;
            if (ct.ends_with("ScalarTypeDefinition"))      return "SCALAR";
            if (ct.ends_with("InterfaceTypeDefinition"))   return "INTERFACE";
            if (ct.ends_with("UnionTypeDefinition"))       return "UNION";
            if (ct.ends_with("EnumTypeDefinition"))        return "ENUM";
            if (ct.ends_with("InputObjectTypeDefinition")) return "INPUT_OBJECT";
        }
        return "OBJECT";
    }
    return "OBJECT";
}

/// Resolve the __TypeKind for a named type.
static std::string named_type_kind(const std::string& name, const TAstNodeMap& type_map) {
    if (is_builtin_scalar(name)) return "SCALAR";
    auto it = type_map.find(name);
    if (it != type_map.end()) return get_type_kind_for_node(*it->second);
    return "OBJECT";
}

/// Recursively build the { kind, name, ofType } chain for a Type AST node.
static json build_type_ref_json(const TAstNodePtr& node, const TAstNodeMap& type_map) {
    if (!node) return nullptr;
    const auto t = node->type;

    if (t.ends_with("NonNullType")) {
        json inner = nullptr;
        for (const auto& c : node->children) inner = build_type_ref_json(c, type_map);
        return json{{"kind","NON_NULL"},{"name",nullptr},{"ofType",inner}};
    }
    if (t.ends_with("ListType")) {
        json inner = nullptr;
        for (const auto& c : node->children) {
            const auto& ct = c->type;
            if (ct.ends_with("NonNullType") || ct.ends_with("NamedType") ||
                ct.ends_with("ListType") || ct.ends_with("gql::Type")) {
                inner = build_type_ref_json(c, type_map);
                break;
            }
        }
        return json{{"kind","LIST"},{"name",nullptr},{"ofType",inner}};
    }
    if (t.ends_with("NamedType")) {
        std::string name;
        if (node->has_content()) {
            name = std::string(node->string_view());
        } else {
            for (const auto& c : node->children)
                if (c->has_content()) { name = std::string(c->string_view()); break; }
        }
        return json{{"kind", named_type_kind(name, type_map)},{"name",name},{"ofType",nullptr}};
    }
    if (t.ends_with("gql::Type")) {
        // Generic Type wrapper — recurse into its first relevant child
        for (const auto& c : node->children) return build_type_ref_json(c, type_map);
    }
    return nullptr;
}

/// Return { isDeprecated, deprecationReason } by scanning DirectivesConst children.
static std::pair<bool, json> check_deprecated(const TAstNodePtr& node) {
    for (const auto& c : node->children) {
        const auto& ct = c->type;
        if (!ct.ends_with("DirectivesConst") && !ct.ends_with("DirectiveConst")) continue;
        for (const auto& dir : c->children) {
            const auto& dt = dir->type;
            if (!dt.ends_with("DirectiveConst") && !dt.ends_with("gql::Directive")) continue;
            // First Name child = directive name
            std::string dir_name;
            for (const auto& dc : dir->children) {
                if (dc->type.ends_with("gql::Name") && dc->has_content()) {
                    dir_name = std::string(dc->string_view()); break;
                }
            }
            if (dir_name != "deprecated") continue;
            // Found @deprecated — look for reason argument
            json reason = nullptr;
            for (const auto& ac : dir->children) {
                const auto& act = ac->type;
                if (!act.ends_with("ArgumentsConst") && !act.ends_with("gql::Arguments")) continue;
                for (const auto& arg : ac->children) {
                    std::string arg_name;
                    json arg_val = nullptr;
                    for (const auto& av : arg->children) {
                        if (av->type.ends_with("gql::Name") && av->has_content())
                            arg_name = std::string(av->string_view());
                        else if (av->type.ends_with("StringValue") || av->type.ends_with("ValueConst")) {
                            auto s = std::string(av->string_view());
                            arg_val = (s.size() >= 2 && s.front() == '"')
                                      ? json(s.substr(1, s.size()-2)) : json(s);
                        }
                    }
                    if (arg_name == "reason") reason = arg_val;
                }
            }
            return {true, reason};
        }
    }
    return {false, nullptr};
}

/// Build a __InputValue JSON object from an InputValueDefinition node.
static json build_input_value_json(const TAstNodePtr& node, const TAstNodeMap& type_map) {
    json iv{{"name",nullptr},{"description",nullptr},{"type",nullptr},
             {"defaultValue",nullptr},{"isDeprecated",false},{"deprecationReason",nullptr}};
    for (const auto& c : node->children) {
        const auto& ct = c->type;
        if (ct.ends_with("gql::Description")) {
            iv["description"] = strip_description(c->string_view());
        } else if (ct.ends_with("gql::Name") && c->has_content()) {
            if (iv["name"].is_null()) iv["name"] = std::string(c->string_view());
        } else if (ct.ends_with("gql::Type") || ct.ends_with("NamedType") ||
                   ct.ends_with("NonNullType") || ct.ends_with("ListType")) {
            iv["type"] = build_type_ref_json(c, type_map);
        } else if (ct.ends_with("DefaultValue")) {
            auto s = isched::v0_0_1::gql::ast_node_to_str(c);
            iv["defaultValue"] = s ? json(*s) : json(nullptr);
        }
    }
    auto [depr, reason] = check_deprecated(node);
    iv["isDeprecated"] = depr;
    iv["deprecationReason"] = reason;
    return iv;
}

/// Build a __EnumValue JSON object from an EnumValueDefinition node.
static json build_enum_value_json(const TAstNodePtr& node) {
    json ev{{"name",nullptr},{"description",nullptr},
             {"isDeprecated",false},{"deprecationReason",nullptr}};
    for (const auto& c : node->children) {
        const auto& ct = c->type;
        if (ct.ends_with("gql::Description")) {
            ev["description"] = strip_description(c->string_view());
        } else if (ct.ends_with("gql::Name") && c->has_content()) {
            if (ev["name"].is_null()) ev["name"] = std::string(c->string_view());
        }
    }
    auto [depr, reason] = check_deprecated(node);
    ev["isDeprecated"] = depr;
    ev["deprecationReason"] = reason;
    return ev;
}

/// Build a __Field JSON object from a FieldDefinition node.
static json build_field_json(const TAstNodePtr& node, const TAstNodeMap& type_map) {
    json f{{"name",nullptr},{"description",nullptr},{"args",json::array()},
            {"type",nullptr},{"isDeprecated",false},{"deprecationReason",nullptr}};
    for (const auto& c : node->children) {
        const auto& ct = c->type;
        if (ct.ends_with("gql::Description")) {
            f["description"] = strip_description(c->string_view());
        } else if (ct.ends_with("gql::Name") && c->has_content()) {
            if (f["name"].is_null()) f["name"] = std::string(c->string_view());
        } else if (ct.ends_with("gql::Type") || ct.ends_with("NamedType") ||
                   ct.ends_with("NonNullType") || ct.ends_with("ListType")) {
            f["type"] = build_type_ref_json(c, type_map);
        } else if (ct.ends_with("ArgumentsDefinition")) {
            for (const auto& arg : c->children)
                if (arg->type.ends_with("InputValueDefinition"))
                    f["args"].push_back(build_input_value_json(arg, type_map));
        }
    }
    auto [depr, reason] = check_deprecated(node);
    f["isDeprecated"] = depr;
    f["deprecationReason"] = reason;
    return f;
}

/// Collect all FieldDefinition descendants of a type node.
static void collect_fields_walk(const TAstNodePtr& n, nlohmann::json& fields, const TAstNodeMap& type_map) {
    if (!n) return;
    if (n->type.ends_with("FieldDefinition")) {
        fields.push_back(build_field_json(n, type_map));
        return; // don't recurse inside a FieldDefinition
    }
    for (const auto& c : n->children) collect_fields_walk(c, fields, type_map);
}

static nlohmann::json collect_fields(const TAstNodePtr& type_node, const TAstNodeMap& type_map) {
    nlohmann::json fields = nlohmann::json::array();
    // Walk only the type-specific sub-node (not the TypeDefinition wrapper)
    // to avoid picking up nested type definitions' fields.
    if (type_node->type.ends_with("gql::TypeDefinition")) {
        for (const auto& c : type_node->children) collect_fields_walk(c, fields, type_map);
    } else {
        collect_fields_walk(type_node, fields, type_map);
    }
    return fields;
}

/// Resolve the base type name from a Type/NonNullType/ListType/NamedType node.
static std::string base_type_name(const TAstNodePtr& node) {
    if (!node) return {};
    if (node->type.ends_with("NamedType")) {
        if (node->has_content()) return std::string(node->string_view());
        for (const auto& c : node->children)
            if (c->has_content()) return std::string(c->string_view());
    }
    for (const auto& c : node->children) {
        auto n = base_type_name(c);
        if (!n.empty()) return n;
    }
    return {};
}

/// Get the declared field return-type base name from the type node.
/// Walks FieldDefinition children looking for a field whose Name matches.
static void field_return_type_walk(const TAstNodePtr& n, const std::string& field_name,
                                    std::string& result) {
    if (!n || !result.empty()) return;
    if (n->type.ends_with("FieldDefinition")) {
        // Find the field's name
        std::string fname;
        const TAstNodePtr* type_ptr = nullptr;
        for (const auto& c : n->children) {
            if (c->type.ends_with("gql::Name") && c->has_content() && fname.empty())
                fname = std::string(c->string_view());
            if (c->type.ends_with("gql::Type") || c->type.ends_with("NamedType") ||
                c->type.ends_with("NonNullType") || c->type.ends_with("ListType"))
                type_ptr = &c;
        }
        if (fname == field_name && type_ptr)
            result = base_type_name(*type_ptr);
        return; // don't recurse into FieldDefinition
    }
    for (const auto& c : n->children) field_return_type_walk(c, field_name, result);
}

static std::string field_return_type(const TAstNodePtr& type_node,
                                      const std::string& field_name) {
    std::string result;
    field_return_type_walk(type_node, field_name, result);
    return result;
}

/// Build a single __Type JSON object for use in __type(name:) responses.
static json build_type_json(const std::string& type_name, const TAstNodePtr& node,
                             const TAstNodeMap& type_map) {
    json t;
    std::string kind = get_type_kind_for_node(node);
    t["kind"]        = kind;
    t["name"]        = type_name;
    t["description"] = extract_description(node);
    t["ofType"]      = nullptr;

    // fields (OBJECT, INTERFACE)
    if (kind == "OBJECT" || kind == "INTERFACE") {
        t["fields"] = collect_fields(node, type_map);
    } else {
        t["fields"] = nullptr;
    }

    // interfaces (OBJECT)
    if (kind == "OBJECT") {
        json ifaces = json::array();
        auto& n = node;
        // Direct NamedType children of TypeDefinition are interface names
        // (they bubble up from ImplementsInterfaces which is not selected)
        for (const auto& c : n->children) {
            if (c->type.ends_with("NamedType")) {
                std::string iname;
                if (c->has_content()) iname = std::string(c->string_view());
                else if (!c->children.empty() && c->children[0]->has_content())
                    iname = std::string(c->children[0]->string_view());
                if (!iname.empty()) ifaces.push_back({{"name", iname}});
            }
        }
        t["interfaces"] = std::move(ifaces);
    } else {
        t["interfaces"] = nullptr;
    }

    // possibleTypes (INTERFACE: objects implementing it; UNION: member types)
    if (kind == "UNION" || kind == "INTERFACE") {
        json possible = json::array();
        if (kind == "UNION") {
            std::function<void(const TAstNodePtr&)> find_members = [&](const TAstNodePtr& n) {
                if (n->type.ends_with("UnionMemberTypes")) {
                    for (const auto& c : n->children)
                        if (c->type.ends_with("NamedType")) {
                            std::string nm;
                            if (c->has_content()) nm = std::string(c->string_view());
                            else if (!c->children.empty()) nm = std::string(c->children[0]->string_view());
                            if (!nm.empty()) possible.push_back({{"name", nm}});
                        }
                    return;
                }
                for (const auto& c : n->children) find_members(c);
            };
            find_members(node);
        } else {
            // INTERFACE: scan all OBJECT types for ones that implement this interface
            for (const auto& [oname, optr] : type_map) {
                if (get_type_kind_for_node(*optr) != "OBJECT") continue;
                for (const auto& c : (*optr)->children) {
                    if (c->type.ends_with("NamedType")) {
                        std::string iname;
                        if (c->has_content()) iname = std::string(c->string_view());
                        if (iname == type_name) { possible.push_back({{"name", oname}}); break; }
                    }
                }
            }
        }
        t["possibleTypes"] = std::move(possible);
    } else {
        t["possibleTypes"] = nullptr;
    }

    // enumValues (ENUM)
    if (kind == "ENUM") {
        json ev = json::array();
        std::function<void(const TAstNodePtr&)> walk_enum = [&](const TAstNodePtr& n) {
            if (n->type.ends_with("EnumValueDefinition")) {
                ev.push_back(build_enum_value_json(n)); return;
            }
            for (const auto& c : n->children) walk_enum(c);
        };
        walk_enum(node);
        t["enumValues"] = std::move(ev);
    } else {
        t["enumValues"] = nullptr;
    }

    // inputFields (INPUT_OBJECT)
    if (kind == "INPUT_OBJECT") {
        json iv = json::array();
        std::function<void(const TAstNodePtr&)> walk_iv = [&](const TAstNodePtr& n) {
            if (n->type.ends_with("InputValueDefinition")) {
                iv.push_back(build_input_value_json(n, type_map)); return;
            }
            for (const auto& c : n->children) walk_iv(c);
        };
        walk_iv(node);
        t["inputFields"] = std::move(iv);
    } else {
        t["inputFields"] = nullptr;
    }

    return t;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// T041: Query complexity and depth analysis
// ---------------------------------------------------------------------------
namespace {

/// Walk the parsed query AST and compute the total number of Field nodes
/// (complexity) and the maximum SelectionSet nesting depth.
struct QueryAnalysis {
    uint32_t depth      = 0;
    uint32_t complexity = 0;
};

static QueryAnalysis analyse_query_complexity(const isched::v0_0_1::gql::TAstNodePtr& root) {
    QueryAnalysis result{};
    if (!root) return result;

    // Plain recursive functor: no std::function overhead, no lambda captures.
    struct Walker {
        QueryAnalysis& r;
        void operator()(const isched::v0_0_1::gql::TAstNodePtr& node,
                        uint32_t cur_depth) const {
            if (!node) return;
            const auto& t = node->type;
            if (t.ends_with("gql::Field")) ++r.complexity;
            if (t.ends_with("gql::SelectionSet")) {
                const uint32_t next = cur_depth + 1;
                if (next > r.depth) r.depth = next;
                for (const auto& child : node->children) (*this)(child, next);
                return;
            }
            for (const auto& child : node->children) (*this)(child, cur_depth);
        }
    };
    Walker{result}(root, 0);
    return result;
}

} // anonymous namespace

namespace isched::v0_0_1::backend {
    using nlohmann::json;
    using nlohmann::basic_json;
    using gql::TAstNodePtr;

    GqlExecutor::GqlExecutor(std::shared_ptr<DatabaseManager> p_database, Config config)
        : m_database(std::move(p_database)), m_config(std::move(config)) {
        setup_builtin_resolvers();
    }

    GqlExecutor::GqlExecutor(std::shared_ptr<DatabaseManager> database)
        : GqlExecutor(std::move(database), Config{}) {
    }

    void GqlExecutor::setup_builtin_resolvers() {
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
                {"host", "localhost"},
                {"port", 8080},
                {"status", "RUNNING"},
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

        // ---------------------------------------------------------------
        // T-UI-D-002: systemState query — unauthenticated; reports seed mode.
        // Seed mode is active when no active platform-admin accounts exist.
        // ---------------------------------------------------------------
        register_resolver({}, "systemState",
            [this](const json&, const json&, const ResolverCtx&) -> json {
                bool seed_active = true;
                if (m_database) {
                    const auto result = m_database->list_platform_admins();
                    if (result) {
                        seed_active = std::ranges::none_of(
                            result.value(),
                            [](const PlatformAdminRecord& r) { return r.is_active; });
                    }
                }
                return json{{"seedModeActive", seed_active}};
            });
        // systemState is intentionally NOT gated by require_roles().

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

        // Legacy metrics endpoint (no auth required; delegates to MetricsCollector if wired)
        register_resolver({}, "metrics", [this](const json &, const json &, const ResolverCtx&) -> json {
            if (m_metrics) {
                // Use live data from the MetricsCollector (T051)
                return m_metrics->get_server_metrics(0, 0, 0);
            }
            // Fallback stub for environments where the collector is not wired
            return json{
                {"requestsInInterval", 0},
                {"errorsInInterval", 0},
                {"totalRequestsSinceStartup", 0},
                {"totalErrorsSinceStartup", 0},
                {"activeConnections", 0},
                {"activeSubscriptions", 0},
                {"avgResponseTimeMs", 0.0},
                {"tenantCount", 0}
            };
        });

        // Server-wide metrics (platform_admin only) — T051-004
        register_resolver({}, "serverMetrics", [this](const json &, const json &, const ResolverCtx&) -> json {
            if (m_metrics) {
                // active_connections and active_subscriptions are provided by the
                // Server layer via the MetricsCollector; tenant_count is approximated
                // as the number of tenants that have recorded at least one request.
                return m_metrics->get_server_metrics(0, 0, 0);
            }
            return json{
                {"requestsInInterval", 0},
                {"errorsInInterval", 0},
                {"totalRequestsSinceStartup", 0},
                {"totalErrorsSinceStartup", 0},
                {"activeConnections", 0},
                {"activeSubscriptions", 0},
                {"avgResponseTimeMs", 0.0},
                {"tenantCount", 0}
            };
        });
        require_roles("serverMetrics", {std::string(Role::PLATFORM_ADMIN)});

        // Per-tenant metrics — T051-005
        // platform_admin can pass any organizationId; tenant_admin sees own org.
        register_resolver({}, "tenantMetrics", [this](const json &, const json & args, const ResolverCtx& ctx) -> json {
            std::string org_id = ctx.tenant_id;
            if (args.contains("organizationId") && !args["organizationId"].is_null()) {
                const std::string requested = args["organizationId"].get<std::string>();
                // Non-platform-admins may only see their own org
                bool is_platform_admin = false;
                for (const auto& r : ctx.roles) {
                    if (r == Role::PLATFORM_ADMIN) { is_platform_admin = true; break; }
                }
                if (!is_platform_admin && requested != org_id) {
                    return json{{"errors", json::array({json{{"message", "Access denied: cannot view metrics for another organization"}}})}};
                }
                org_id = requested;
            }
            if (org_id.empty()) {
                return json{{"errors", json::array({json{{"message", "organizationId is required for unauthenticated requests"}}})}};
            }
            if (m_metrics) {
                return m_metrics->get_tenant_metrics(org_id);
            }
            return json{
                {"organizationId", org_id},
                {"requestsInInterval", 0},
                {"errorsInInterval", 0},
                {"totalRequestsSinceStartup", 0},
                {"totalErrorsSinceStartup", 0},
                {"avgResponseTimeMs", 0.0}
            };
        });
        require_roles("tenantMetrics", {std::string(Role::TENANT_ADMIN), std::string(Role::PLATFORM_ADMIN)});

        // Subscription: serverMetricsUpdated — T051-006
        // Published by Server's background publisher thread; this resolver handles
        // the initial subscribe request (returns empty data; actual payloads come
        // from broker publishes on topic "__metrics/server").
        register_resolver({}, "serverMetricsUpdated", [this](const json &, const json &, const ResolverCtx&) -> json {
            if (m_broker) {
                // Subscribing is managed by the WebSocket layer; return current snapshot
                if (m_metrics) {
                    return m_metrics->get_server_metrics(0, 0, 0);
                }
            }
            return json{
                {"requestsInInterval", 0}, {"errorsInInterval", 0},
                {"totalRequestsSinceStartup", 0}, {"totalErrorsSinceStartup", 0},
                {"activeConnections", 0}, {"activeSubscriptions", 0},
                {"avgResponseTimeMs", 0.0}, {"tenantCount", 0}
            };
        });
        require_roles("serverMetricsUpdated", {std::string(Role::PLATFORM_ADMIN)});

        // Subscription: tenantMetricsUpdated — T051-007
        register_resolver({}, "tenantMetricsUpdated", [this](const json &, const json & args, const ResolverCtx& ctx) -> json {
            std::string org_id = ctx.tenant_id;
            if (args.contains("organizationId") && !args["organizationId"].is_null()) {
                org_id = args["organizationId"].get<std::string>();
            }
            if (m_metrics && !org_id.empty()) {
                return m_metrics->get_tenant_metrics(org_id);
            }
            return json{
                {"organizationId", org_id},
                {"requestsInInterval", 0}, {"errorsInInterval", 0},
                {"totalRequestsSinceStartup", 0}, {"totalErrorsSinceStartup", 0},
                {"avgResponseTimeMs", 0.0}
            };
        });
        require_roles("tenantMetricsUpdated", {std::string(Role::TENANT_ADMIN), std::string(Role::PLATFORM_ADMIN)});

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

        // Enhanced schema introspection resolver (T-INTRO-010 … T-INTRO-018)
        register_resolver({},"__schema", [this](const json &, const json &, const ResolverCtx&) -> json {
            return generate_schema_introspection();
        });

        // __type(name:) root field (T-INTRO-020, T-INTRO-021)
        register_resolver({},"__type", [this](const json &, const json & args, const ResolverCtx&) -> json {
            if (!args.contains("name") || !args["name"].is_string()) return nullptr;
            const std::string name = args["name"].get<std::string>();
            // Built-in scalars
            if (is_builtin_scalar(name)) {
                return json{{"kind","SCALAR"},{"name",name},{"description",nullptr},
                            {"fields",nullptr},{"interfaces",nullptr},{"possibleTypes",nullptr},
                            {"enumValues",nullptr},{"inputFields",nullptr},{"ofType",nullptr}};
            }
            auto it = m_type_map.find(name);
            if (it == m_type_map.end()) return nullptr; // T-INTRO-021: null for unknown names
            return build_type_json(name, *it->second, m_type_map);
        });

        // Built-in mutation resolvers
        register_resolver({}, "echo", [](const json&, const json& args, const ResolverCtx&) -> json {
            if (args.contains("message") && args["message"].is_string()) {
                return args["message"];
            }
            return nullptr;
        });

        // ---------------------------------------------------------------
        // T047-000b: bootstrapPlatformAdmin (one-time, unauthenticated)
        // ---------------------------------------------------------------
        register_resolver({}, "bootstrapPlatformAdmin",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                if (!m_database) {
                    throw std::runtime_error("bootstrapPlatformAdmin: database not available");
                }
                if (!m_auth) {
                    throw std::runtime_error("bootstrapPlatformAdmin: authentication middleware not configured");
                }
                if (!args.contains("input") || !args["input"].is_object()) {
                    throw std::invalid_argument("bootstrapPlatformAdmin: input is required");
                }

                const auto& input = args["input"];
                const std::string email = input.value("email", "");
                const std::string password = input.value("password", "");
                const std::string display_name = input.value("displayName", "");
                if (email.empty() || password.empty()) {
                    throw std::invalid_argument("bootstrapPlatformAdmin: email and password are required");
                }

                if (auto init_res = m_database->ensure_system_db(); !init_res) {
                    throw std::runtime_error("bootstrapPlatformAdmin: failed to initialize system database");
                }

                auto admins = m_database->list_platform_admins();
                if (!admins) {
                    throw std::runtime_error("bootstrapPlatformAdmin: failed to query platform admins");
                }
                if (!admins.value().empty()) {
                    throw std::runtime_error("bootstrapPlatformAdmin is no longer available");
                }

                static std::atomic<uint64_t> admin_counter{0};
                const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const std::string admin_id = "platform_admin_" + std::to_string(now_ms)
                    + "_" + std::to_string(++admin_counter);

                const std::string password_hash = hash_password(password);
                if (auto create_res = m_database->create_platform_admin(
                        admin_id, email, password_hash, display_name);
                    !create_res)
                {
                    if (create_res.error() == DatabaseError::DuplicateKey) {
                        throw std::runtime_error("bootstrapPlatformAdmin: platform admin already exists");
                    }
                    throw std::runtime_error("bootstrapPlatformAdmin: failed to create platform admin");
                }

                const LoginSession sess = m_auth->create_session(
                    *m_database,
                    admin_id,
                    display_name.empty() ? email : display_name,
                    "platform",
                    {std::string(Role::PLATFORM_ADMIN)});

                return json{{"token", sess.token}, {"expiresAt", sess.expires_at}};
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
            std::ignore = std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
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

                // Optional optimistic concurrency gate used by closeout conflict tests.
                // If expectedVersion is supplied, it must match the currently active
                // snapshot version for the tenant before we persist a new snapshot.
                if (inp.contains("expectedVersion") && !inp["expectedVersion"].is_null()) {
                    if (!inp["expectedVersion"].is_string()) {
                        result["errors"].push_back("input.expectedVersion must be a string");
                        return result;
                    }
                    const std::string tenant_id = inp["tenantId"].get<std::string>();
                    const std::string expected_version = inp["expectedVersion"].get<std::string>();
                    const auto active_res = m_database->get_active_config_snapshot(tenant_id);
                    if (!active_res) {
                        result["errors"].push_back("Failed to read active configuration for expectedVersion check");
                        return result;
                    }
                    const std::string active_version = active_res.value().has_value()
                        ? active_res.value()->version
                        : std::string("0");
                    if (expected_version != active_version) {
                        result["errors"].push_back(
                            "Configuration conflict: expectedVersion '" + expected_version
                            + "' does not match active version '" + active_version + "'");
                        return result;
                    }
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

                // Persist optional resolver bindings (T048-007)
                if (inp.contains("resolverBindings") && inp["resolverBindings"].is_array())
                    snap.resolver_bindings = inp["resolverBindings"].dump();

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
            [this, fmt_iso8601](const json&, const json& args, const ResolverCtx&) -> json {
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
                const std::string tenant_id = get_res.value()->tenant_id;
                if (!new_sdl.empty()) {
                    set_pending_schema_change({tenant_id, new_sdl});
                }

                // Register outbound_http resolver bindings from the snapshot (T048-007)
                const std::string bindings_raw = get_res.value()->resolver_bindings;
                if (!bindings_raw.empty() && bindings_raw != "[]") {
                    try {
                        const json bindings = json::parse(bindings_raw);
                        for (const auto& b : bindings) {
                            if (!b.is_object()) continue;
                            const std::string kind    = b.value("resolverKind", "");
                            if (kind != "outbound_http") continue;
                            const std::string ds_id   = b.value("dataSourceId", "");
                            const std::string field   = b.value("fieldName",    "");
                            const std::string path_p  = b.value("pathPattern",  "/");
                            const std::string http_m  = b.value("httpMethod",   "GET");
                            if (field.empty() || ds_id.empty()) continue;

                            // Capture by value to avoid dangling references
                            register_resolver({}, field,
                                [this, tenant_id, ds_id, path_p, http_m]
                                (const json&, const json&, const ResolverCtx& ctx) -> json {
                                    auto ds_res = m_database->get_data_source_by_id(
                                        ctx.tenant_id.empty() ? tenant_id : ctx.tenant_id, ds_id);
                                    if (!ds_res)
                                        return json{{"statusCode", 0},
                                                    {"message", "Data source '" + ds_id + "' not found"},
                                                    {"url", ""}};

                                    const auto& ds = ds_res.value();
                                    backend::DataSourceConfig cfg;
                                    cfg.base_url        = ds.base_url;
                                    cfg.auth_kind       = ds.auth_kind;
                                    cfg.api_key_header  = ds.api_key_header;
                                    cfg.timeout_ms      = ds.timeout_ms;
                                    // Decrypt API key if present
                                    if (!ds.api_key_value_encrypted.empty() &&
                                        !m_master_secret.empty())
                                    {
                                        try {
                                            cfg.api_key_value = backend::decrypt_secret(
                                                ds.api_key_value_encrypted,
                                                ctx.tenant_id.empty() ? tenant_id : ctx.tenant_id,
                                                m_master_secret);
                                        } catch (const std::exception& ex) {
                                            spdlog::error("activateSnapshot binding: "
                                                "decrypt_secret failed for ds '{}': {}",
                                                ds_id, ex.what());
                                        }
                                    }
                                    return backend::RestDataSource::fetch(
                                        cfg, path_p, http_m, json{}, ctx.bearer_token);
                                });
                        }
                    } catch (const json::parse_error& e) {
                        spdlog::warn("activateSnapshot: failed to parse resolver_bindings: {}", e.what());
                    }
                }

                // Publish configuration activation event to SubscriptionBroker (T046)
                if (m_broker) {
                    json evt;
                    evt["tenantId"]   = tenant_id;
                    evt["snapshotId"] = snap_id;
                    evt["schemaSdl"]  = new_sdl.empty() ? json(nullptr) : json(new_sdl);
                    evt["activatedAt"] = fmt_iso8601(std::chrono::system_clock::now());
                    m_broker->publish("config:" + tenant_id, "configurationActivated", evt);
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

        // ---------------------------------------------------------------
        // Phase 5 subscription: healthChanged (T046)
        // Returns the current health snapshot on subscribe; ongoing events
        // are published by the Server via SubscriptionBroker.
        // ---------------------------------------------------------------
        register_resolver({}, "healthChanged",
            [fmt_iso8601](const json&, const json&, const ResolverCtx&) -> json {
                json evt;
                evt["status"]    = "UP";
                evt["timestamp"] = fmt_iso8601(std::chrono::system_clock::now());
                return evt;
            });

        // ---------------------------------------------------------------
        // Phase 5 subscription: configurationActivated (T046)
        // Returns the current active snapshot for the tenant (initial value);
        // activation events are published by activateSnapshot and rollback.
        // ---------------------------------------------------------------
        register_resolver({}, "configurationActivated",
            [this, fmt_iso8601](const json&, const json& args, const ResolverCtx&) -> json {
                if (!m_database) return nullptr;
                if (!args.contains("tenantId") || !args["tenantId"].is_string())
                    return nullptr;
                const std::string tenant_id = args["tenantId"].get<std::string>();
                auto res = m_database->get_active_config_snapshot(tenant_id);
                if (!res || !res.value()) return nullptr;
                const auto& s = *res.value();
                json evt;
                evt["tenantId"]   = s.tenant_id;
                evt["snapshotId"] = s.id;
                evt["schemaSdl"]  = s.schema_sdl.empty() ? json(nullptr) : json(s.schema_sdl);
                evt["activatedAt"] = s.activated_at
                    ? json(fmt_iso8601(*s.activated_at))
                    : json(fmt_iso8601(s.created_at));
                return evt;
            });

        // ---------------------------------------------------------------
        // Phase 6 stub resolvers (T047-000a)
        // Full implementations added in T047-009, T047-015, T047-016.
        // ---------------------------------------------------------------
        register_resolver({}, "currentUser", [](const json&, const json&, const ResolverCtx&) -> json {
            return nullptr; // not yet authenticated — implemented in T047-016
        });
        // ---------------------------------------------------------------
        // T047-015: user / users query resolvers
        // ---------------------------------------------------------------
        register_resolver({}, "user",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const std::string id     = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("user: id is required");
                }
                auto res = m_database->get_user_by_id(org_id, id);
                if (!res) {
                    if (res.error() == DatabaseError::NotFound) { return nullptr; }
                    throw std::runtime_error("Failed to fetch user");
                }
                const auto& r = res.value();
                json roles_arr = json::array();
                for (const auto& role : r.roles) { roles_arr.push_back(role); }
                return json{
                    {"id",          r.id},
                    {"email",       r.email},
                    {"displayName", r.display_name},
                    {"roles",       roles_arr},
                    {"isActive",    r.is_active},
                    {"createdAt",   r.created_at},
                    {"lastLogin",   r.last_login.empty() ? json(nullptr) : json(r.last_login)}
                };
            });
        require_roles("user", {std::string(Role::PLATFORM_ADMIN),
                               std::string(Role::TENANT_ADMIN)});

        register_resolver({}, "users",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                auto res = m_database->list_users(org_id);
                if (!res) {
                    throw std::runtime_error("Failed to list users");
                }
                json arr = json::array();
                for (const auto& r : res.value()) {
                    json roles_arr = json::array();
                    for (const auto& role : r.roles) { roles_arr.push_back(role); }
                    arr.push_back(json{
                        {"id",          r.id},
                        {"email",       r.email},
                        {"displayName", r.display_name},
                        {"roles",       roles_arr},
                        {"isActive",    r.is_active},
                        {"createdAt",   r.created_at},
                        {"lastLogin",   r.last_login.empty() ? json(nullptr) : json(r.last_login)}
                    });
                }
                return arr;
            });
        require_roles("users", {std::string(Role::PLATFORM_ADMIN),
                                std::string(Role::TENANT_ADMIN)});
        // organization / organizations — real implementations added in T047-009 below

        // ---------------------------------------------------------------
        // T049-003: logout mutation — revoke the caller's current session
        // ---------------------------------------------------------------
        register_resolver({}, "logout",
            [this](const json&, const json&, const ResolverCtx& ctx) -> json
            {
                if (ctx.session_id.empty())
                    return true; // unauthenticated caller — no session to revoke

                auto db_ptr = ctx.db.lock();
                if (!db_ptr)
                    throw std::runtime_error("logout: database not available");

                std::ignore = db_ptr->revoke_session(ctx.tenant_id, ctx.session_id);

                // Signal subscription broker to close any open WebSocket for this session.
                if (m_broker)
                    m_broker->revoke_auth_session(ctx.session_id);

                return true;
            });
        // logout is available to any authenticated caller — no require_roles gate.

        // ---------------------------------------------------------------
        // T049-004: revokeSession mutation — tenant_admin only
        // ---------------------------------------------------------------
        register_resolver({}, "revokeSession",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json
            {
                const std::string session_id = args.value("sessionId", "");
                if (session_id.empty())
                    throw std::invalid_argument("revokeSession: sessionId is required");

                auto db_ptr = ctx.db.lock();
                if (!db_ptr)
                    throw std::runtime_error("revokeSession: database not available");

                if (auto r = db_ptr->revoke_session(ctx.tenant_id, session_id); !r) {
                    if (r.error() == DatabaseError::NotFound)
                        throw std::runtime_error("Session not found");
                    throw std::runtime_error("revokeSession: database error");
                }

                if (m_broker)
                    m_broker->revoke_auth_session(session_id);

                return true;
            });
        require_roles("revokeSession", {std::string(Role::TENANT_ADMIN),
                                        std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // T049-005: revokeAllSessions mutation — tenant_admin only
        // ---------------------------------------------------------------
        register_resolver({}, "revokeAllSessions",
            [](const json&, const json& args, const ResolverCtx& ctx) -> json
            {
                const std::string user_id = args.value("userId", "");
                if (user_id.empty())
                    throw std::invalid_argument("revokeAllSessions: userId is required");

                auto db_ptr = ctx.db.lock();
                if (!db_ptr)
                    throw std::runtime_error("revokeAllSessions: database not available");

                std::ignore = db_ptr->revoke_all_sessions_for_user(ctx.tenant_id, user_id);
                // We don't have individual session IDs here; the broker's revocation is
                // best-effort for auth sessions that were registered.  A later poll by
                // validate_token will reject revoked sessions anyway.
                return true;
            });
        require_roles("revokeAllSessions", {std::string(Role::TENANT_ADMIN),
                                            std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // T049-006: terminateAllSessions mutation — platform_admin only
        // ---------------------------------------------------------------
        register_resolver({}, "terminateAllSessions",
            [](const json&, const json& args, const ResolverCtx& ctx) -> json
            {
                const std::string org_id = args.value("organizationId", "");
                if (org_id.empty())
                    throw std::invalid_argument("terminateAllSessions: organizationId is required");

                auto db_ptr = ctx.db.lock();
                if (!db_ptr)
                    throw std::runtime_error("terminateAllSessions: database not available");

                // Revoke all sessions except platform_admin sessions (RISK-003).
                std::ignore = db_ptr->revoke_all_sessions_for_org(
                    org_id, std::string(Role::PLATFORM_ADMIN));
                return true;
            });
        require_roles("terminateAllSessions", {std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // T047-004: createRole / deleteRole mutations
        // ---------------------------------------------------------------
        register_resolver({}, "createRole",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const auto& input = args.value("input", json::object());
                const std::string id    = input.value("id",          "");
                const std::string name  = input.value("name",        "");
                const std::string desc  = input.value("description", "");
                const std::string scope = input.value("scope",       "");

                if (id.empty() || name.empty() || scope.empty()) {
                    throw std::invalid_argument("createRole: id, name, and scope are required");
                }
                if (scope == "platform") {
                    if (auto res = m_database->create_platform_role(id, name, desc); !res) {
                        switch (res.error()) {
                            case DatabaseError::DuplicateKey:
                                throw std::runtime_error("Role '" + id + "' already exists");
                            default:
                                throw std::runtime_error("Failed to create platform role");
                        }
                    }
                }
                // tenant-scope roles will be implemented in T047-010 (per-tenant users table)
                return true;
            });
        require_roles("createRole", {std::string(Role::PLATFORM_ADMIN),
                                     std::string(Role::TENANT_ADMIN)});

        register_resolver({}, "deleteRole",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const std::string id = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("deleteRole: id is required");
                }
                if (auto res = m_database->delete_platform_role(id); !res) {
                    switch (res.error()) {
                        case DatabaseError::NotFound:
                            throw std::runtime_error("Role '" + id + "' not found");
                        case DatabaseError::AccessDenied:
                            throw std::runtime_error("Built-in roles cannot be deleted");
                        default:
                            throw std::runtime_error("Failed to delete role");
                    }
                }
                return true;
            });
        require_roles("deleteRole", {std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // T047-006: createOrganization mutation
        // ---------------------------------------------------------------
        register_resolver({}, "createOrganization",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const auto& input = args.value("input", json::object());
                const std::string id   = input.value("id",   "");
                const std::string name = input.value("name", "");
                if (name.empty()) {
                    throw std::invalid_argument("createOrganization: name is required");
                }
                // Generate id from name if not provided
                const std::string org_id = id.empty()
                    ? "org_" + std::to_string(std::hash<std::string>{}(name))
                    : id;
                const std::string domain           = input.value("domain",           "");
                const std::string subscription_tier = input.value("subscriptionTier", "free");
                const int         user_limit        = input.value("userLimit",    10);
                const int         storage_limit     = input.value("storageLimit", 1073741824);

                if (auto res = m_database->create_organization(
                        org_id, name, domain, subscription_tier, user_limit, storage_limit);
                    !res)
                {
                    switch (res.error()) {
                        case DatabaseError::DuplicateKey:
                            throw std::runtime_error("Organization '" + org_id + "' already exists");
                        default:
                            throw std::runtime_error("Failed to create organization");
                    }
                }
                // Provision the tenant SQLite file (idempotent)
                std::ignore = m_database->initialize_tenant(org_id);

                // Return the created record
                auto rec_result = m_database->get_organization(org_id);
                if (!rec_result) {
                    throw std::runtime_error("Organization created but could not be fetched");
                }
                const auto& r = rec_result.value();
                return json{
                    {"id",               r.id},
                    {"name",             r.name},
                    {"domain",           r.domain.empty() ? json(nullptr) : json(r.domain)},
                    {"subscriptionTier", r.subscription_tier},
                    {"userLimit",        r.user_limit},
                    {"storageLimit",     r.storage_limit},
                    {"createdAt",        r.created_at}
                };
            });
        require_roles("createOrganization", {std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // T047-007: updateOrganization mutation
        // ---------------------------------------------------------------
        register_resolver({}, "updateOrganization",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const std::string id = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("updateOrganization: id is required");
                }
                const auto& input = args.value("input", json::object());

                std::optional<std::string> name;
                std::optional<std::string> domain;
                std::optional<std::string> subscription_tier;
                std::optional<int>         user_limit;
                std::optional<int>         storage_limit;

                if (input.contains("name")             && !input["name"].is_null())
                    name = input["name"].get<std::string>();
                if (input.contains("domain")           && !input["domain"].is_null())
                    domain = input["domain"].get<std::string>();
                if (input.contains("subscriptionTier") && !input["subscriptionTier"].is_null())
                    subscription_tier = input["subscriptionTier"].get<std::string>();
                if (input.contains("userLimit")        && !input["userLimit"].is_null())
                    user_limit = input["userLimit"].get<int>();
                if (input.contains("storageLimit")     && !input["storageLimit"].is_null())
                    storage_limit = input["storageLimit"].get<int>();

                if (auto res = m_database->update_organization(
                        id, name, domain, subscription_tier, user_limit, storage_limit);
                    !res)
                {
                    switch (res.error()) {
                        case DatabaseError::NotFound:
                            throw std::runtime_error("Organization '" + id + "' not found");
                        default:
                            throw std::runtime_error("Failed to update organization");
                    }
                }
                auto rec_result = m_database->get_organization(id);
                if (!rec_result) {
                    throw std::runtime_error("Organization updated but could not be fetched");
                }
                const auto& r = rec_result.value();
                return json{
                    {"id",               r.id},
                    {"name",             r.name},
                    {"domain",           r.domain.empty() ? json(nullptr) : json(r.domain)},
                    {"subscriptionTier", r.subscription_tier},
                    {"userLimit",        r.user_limit},
                    {"storageLimit",     r.storage_limit},
                    {"createdAt",        r.created_at}
                };
            });
        require_roles("updateOrganization", {std::string(Role::PLATFORM_ADMIN),
                                             std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-008: deleteOrganization mutation
        // ---------------------------------------------------------------
        register_resolver({}, "deleteOrganization",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const std::string id = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("deleteOrganization: id is required");
                }
                if (auto res = m_database->delete_organization(id); !res) {
                    switch (res.error()) {
                        case DatabaseError::NotFound:
                            throw std::runtime_error("Organization '" + id + "' not found");
                        default:
                            throw std::runtime_error("Failed to delete organization");
                    }
                }
                return true;
            });
        require_roles("deleteOrganization", {std::string(Role::PLATFORM_ADMIN),
                                             std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-009: organization / organizations query resolvers
        // ---------------------------------------------------------------
        register_resolver({}, "organization",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const std::string id = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("organization: id is required");
                }
                auto res = m_database->get_organization(id);
                if (!res) {
                    if (res.error() == DatabaseError::NotFound) {
                        return nullptr;
                    }
                    throw std::runtime_error("Failed to fetch organization");
                }
                const auto& r = res.value();
                return json{
                    {"id",               r.id},
                    {"name",             r.name},
                    {"domain",           r.domain.empty() ? json(nullptr) : json(r.domain)},
                    {"subscriptionTier", r.subscription_tier},
                    {"userLimit",        r.user_limit},
                    {"storageLimit",     r.storage_limit},
                    {"createdAt",        r.created_at}
                };
            });
        require_roles("organization", {std::string(Role::PLATFORM_ADMIN),
                                       std::string(Role::TENANT_ADMIN)});

        register_resolver({}, "organizations",
            [this](const json&, const json&, const ResolverCtx& ctx) -> json {
                // platform_admin sees all; tenant_admin sees only their own org
                const bool is_platform_admin =
                    std::find(ctx.roles.begin(), ctx.roles.end(),
                              std::string(Role::PLATFORM_ADMIN)) != ctx.roles.end();

                auto res = m_database->list_organizations();
                if (!res) {
                    throw std::runtime_error("Failed to list organizations");
                }
                json arr = json::array();
                for (const auto& r : res.value()) {
                    if (!is_platform_admin && r.id != ctx.tenant_id) {
                        continue;  // tenant_admin: skip orgs that are not theirs
                    }
                    arr.push_back(json{
                        {"id",               r.id},
                        {"name",             r.name},
                        {"domain",           r.domain.empty() ? json(nullptr) : json(r.domain)},
                        {"subscriptionTier", r.subscription_tier},
                        {"userLimit",        r.user_limit},
                        {"storageLimit",     r.storage_limit},
                        {"createdAt",        r.created_at}
                    });
                }
                return arr;
            });
        require_roles("organizations", {std::string(Role::PLATFORM_ADMIN),
                                        std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-012: createUser mutation
        // ---------------------------------------------------------------
        register_resolver({}, "createUser",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const auto& input = args.value("input", json::object());
                const std::string email    = input.value("email",    "");
                const std::string password = input.value("password", "");
                if (email.empty() || password.empty()) {
                    throw std::invalid_argument("createUser: email and password are required");
                }
                if (password.size() < 12) {
                    throw std::invalid_argument("password must be at least 12 characters");
                }
                const std::string display_name = input.value("displayName", "");

                // Build roles JSON array
                json roles_arr = json::array();
                if (input.contains("roles") && input["roles"].is_array()) {
                    roles_arr = input["roles"];
                }
                const std::string roles_json = roles_arr.dump();

                const std::string hash = hash_password(password);

                // Generate a user id
                const std::string uid = "usr_" + std::to_string(
                    std::hash<std::string>{}(org_id + email));

                if (auto res = m_database->create_user(
                        org_id, uid, email, hash, display_name, roles_json);
                    !res)
                {
                    switch (res.error()) {
                        case DatabaseError::DuplicateKey:
                            throw std::runtime_error("User with email '" + email + "' already exists");
                        default:
                            throw std::runtime_error("Failed to create user");
                    }
                }
                auto rec_result = m_database->get_user_by_id(org_id, uid);
                if (!rec_result) {
                    throw std::runtime_error("User created but could not be fetched");
                }
                const auto& r = rec_result.value();
                json out_roles = json::array();
                for (const auto& role : r.roles) { out_roles.push_back(role); }
                return json{
                    {"id",          r.id},
                    {"email",       r.email},
                    {"displayName", r.display_name},
                    {"roles",       out_roles},
                    {"isActive",    r.is_active},
                    {"createdAt",   r.created_at},
                    {"lastLogin",   json(nullptr)}
                };
            });
        require_roles("createUser", {std::string(Role::PLATFORM_ADMIN),
                                     std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-013: updateUser mutation
        // ---------------------------------------------------------------
        register_resolver({}, "updateUser",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const std::string id     = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("updateUser: id is required");
                }
                const auto& input = args.value("input", json::object());

                std::optional<std::string> display_name;
                std::optional<std::string> roles_json;
                std::optional<bool>        is_active;

                if (input.contains("displayName") && !input["displayName"].is_null())
                    display_name = input["displayName"].get<std::string>();
                if (input.contains("roles") && input["roles"].is_array())
                    roles_json = input["roles"].dump();
                if (input.contains("isActive") && !input["isActive"].is_null())
                    is_active = input["isActive"].get<bool>();

                if (auto res = m_database->update_user(org_id, id, display_name, roles_json, is_active);
                    !res)
                {
                    if (res.error() == DatabaseError::NotFound)
                        throw std::runtime_error("User '" + id + "' not found");
                    throw std::runtime_error("Failed to update user");
                }
                auto rec_result = m_database->get_user_by_id(org_id, id);
                if (!rec_result) {
                    throw std::runtime_error("User updated but could not be fetched");
                }
                const auto& r = rec_result.value();
                json out_roles = json::array();
                for (const auto& role : r.roles) { out_roles.push_back(role); }
                return json{
                    {"id",          r.id},
                    {"email",       r.email},
                    {"displayName", r.display_name},
                    {"roles",       out_roles},
                    {"isActive",    r.is_active},
                    {"createdAt",   r.created_at},
                    {"lastLogin",   r.last_login.empty() ? json(nullptr) : json(r.last_login)}
                };
            });
        require_roles("updateUser", {std::string(Role::PLATFORM_ADMIN),
                                     std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-014: deleteUser mutation
        // ---------------------------------------------------------------
        register_resolver({}, "deleteUser",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const std::string id     = args.value("id", "");
                if (id.empty()) {
                    throw std::invalid_argument("deleteUser: id is required");
                }
                if (auto res = m_database->delete_user(org_id, id); !res) {
                    if (res.error() == DatabaseError::NotFound)
                        throw std::runtime_error("User '" + id + "' not found");
                    throw std::runtime_error("Failed to delete user");
                }
                return true;
            });
        require_roles("deleteUser", {std::string(Role::PLATFORM_ADMIN),
                                     std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T048-002: createDataSource mutation
        // ---------------------------------------------------------------
        register_resolver({}, "createDataSource",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const auto& input        = args.value("input", json::object());
                const std::string name     = input.value("name",    "");
                const std::string base_url = input.value("baseUrl", "");
                if (name.empty())     throw std::invalid_argument("createDataSource: name is required");
                if (base_url.empty()) throw std::invalid_argument("createDataSource: baseUrl is required");

                const std::string auth_kind       = input.value("authKind",       "none");
                const std::string api_key_header  = input.value("apiKeyHeader",   "");
                const int         timeout_ms_val  = input.value("timeoutMs",      5000);

                // Encrypt the plain-text API key value if provided
                std::string api_key_enc;
                if (input.contains("apiKeyValue") && !input["apiKeyValue"].is_null()) {
                    const std::string plain = input["apiKeyValue"].get<std::string>();
                    if (!plain.empty()) {
                        if (m_master_secret.empty())
                            throw std::runtime_error("createDataSource: master_secret not configured");
                        api_key_enc = backend::encrypt_secret(plain, org_id, m_master_secret);
                    }
                }

                // Generate a stable ID
                const std::string ds_id = "ds_" + std::to_string(
                    std::hash<std::string>{}(org_id + name + base_url));

                if (auto res = m_database->create_data_source(
                        org_id, ds_id, name, base_url, auth_kind,
                        api_key_header, api_key_enc, timeout_ms_val);
                    !res)
                {
                    if (res.error() == DatabaseError::DuplicateKey)
                        throw std::runtime_error("Data source already exists");
                    throw std::runtime_error("Failed to create data source");
                }
                auto rec = m_database->get_data_source_by_id(org_id, ds_id);
                if (!rec) throw std::runtime_error("Data source created but could not be fetched");
                const auto& r = rec.value();
                return json{
                    {"id",           r.id},
                    {"name",         r.name},
                    {"baseUrl",      r.base_url},
                    {"authKind",     r.auth_kind},
                    {"apiKeyHeader", r.api_key_header.empty() ? json(nullptr) : json(r.api_key_header)},
                    {"timeoutMs",    r.timeout_ms},
                    {"createdAt",    r.created_at}
                };
            });
        require_roles("createDataSource", {std::string(Role::PLATFORM_ADMIN),
                                           std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T048-003: updateDataSource mutation
        // ---------------------------------------------------------------
        register_resolver({}, "updateDataSource",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const std::string id     = args.value("id", "");
                if (id.empty()) throw std::invalid_argument("updateDataSource: id is required");

                const auto& input = args.value("input", json::object());

                std::optional<std::string> name, base_url, auth_kind, api_key_header, api_key_enc;
                std::optional<int>         timeout_ms_opt;

                if (input.contains("name")      && !input["name"].is_null())
                    name = input["name"].get<std::string>();
                if (input.contains("baseUrl")   && !input["baseUrl"].is_null())
                    base_url = input["baseUrl"].get<std::string>();
                if (input.contains("authKind")  && !input["authKind"].is_null())
                    auth_kind = input["authKind"].get<std::string>();
                if (input.contains("apiKeyHeader") && !input["apiKeyHeader"].is_null())
                    api_key_header = input["apiKeyHeader"].get<std::string>();
                if (input.contains("apiKeyValue")  && !input["apiKeyValue"].is_null()) {
                    const std::string plain = input["apiKeyValue"].get<std::string>();
                    if (!plain.empty()) {
                        if (m_master_secret.empty())
                            throw std::runtime_error("updateDataSource: master_secret not configured");
                        api_key_enc = backend::encrypt_secret(plain, org_id, m_master_secret);
                    }
                }
                if (input.contains("timeoutMs") && !input["timeoutMs"].is_null())
                    timeout_ms_opt = input["timeoutMs"].get<int>();

                if (auto res = m_database->update_data_source(
                        org_id, id, name, base_url, auth_kind, api_key_header, api_key_enc, timeout_ms_opt);
                    !res)
                {
                    if (res.error() == DatabaseError::NotFound)
                        throw std::runtime_error("Data source '" + id + "' not found");
                    throw std::runtime_error("Failed to update data source");
                }
                auto rec = m_database->get_data_source_by_id(org_id, id);
                if (!rec) throw std::runtime_error("Data source updated but could not be fetched");
                const auto& r = rec.value();
                return json{
                    {"id",           r.id},
                    {"name",         r.name},
                    {"baseUrl",      r.base_url},
                    {"authKind",     r.auth_kind},
                    {"apiKeyHeader", r.api_key_header.empty() ? json(nullptr) : json(r.api_key_header)},
                    {"timeoutMs",    r.timeout_ms},
                    {"createdAt",    r.created_at}
                };
            });
        require_roles("updateDataSource", {std::string(Role::PLATFORM_ADMIN),
                                           std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T048-003: deleteDataSource mutation
        // ---------------------------------------------------------------
        register_resolver({}, "deleteDataSource",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                const std::string id     = args.value("id", "");
                if (id.empty()) throw std::invalid_argument("deleteDataSource: id is required");

                if (auto res = m_database->delete_data_source(org_id, id); !res) {
                    if (res.error() == DatabaseError::NotFound)
                        throw std::runtime_error("Data source '" + id + "' not found");
                    throw std::runtime_error("Failed to delete data source");
                }
                return true;
            });
        require_roles("deleteDataSource", {std::string(Role::PLATFORM_ADMIN),
                                           std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T048-004: dataSources query
        // ---------------------------------------------------------------
        register_resolver({}, "dataSources",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                const std::string org_id = args.value("organizationId", ctx.tenant_id);
                auto result = m_database->list_data_sources(org_id);
                if (!result) {
                    throw std::runtime_error("Failed to list data sources");
                }
                json arr = json::array();
                for (const auto& r : result.value()) {
                    arr.push_back(json{
                        {"id",           r.id},
                        {"name",         r.name},
                        {"baseUrl",      r.base_url},
                        {"authKind",     r.auth_kind},
                        {"apiKeyHeader", r.api_key_header.empty() ? json(nullptr) : json(r.api_key_header)},
                        {"timeoutMs",    r.timeout_ms},
                        {"createdAt",    r.created_at}
                    });
                }
                return arr;
            });
        require_roles("dataSources", {std::string(Role::PLATFORM_ADMIN),
                                      std::string(Role::TENANT_ADMIN)});

        // ---------------------------------------------------------------
        // T047-016: login mutation
        // Unauthenticated — any caller may attempt; no require_roles() gate.
        // ---------------------------------------------------------------
        register_resolver({}, "login",
            [this](const json&, const json& args, const ResolverCtx&) -> json
            {
                const std::string email    = args.value("email", "");
                const std::string password = args.value("password", "");
                // organizationId may be JSON null or absent
                std::string org_id;
                if (args.contains("organizationId") && !args["organizationId"].is_null())
                    org_id = args["organizationId"].get<std::string>();

                if (email.empty() || password.empty())
                    throw std::invalid_argument("login: email and password are required");
                if (!m_auth)
                    throw std::runtime_error("login: authentication middleware not configured");

                auto& db = *m_database;

                // --- locate user and stored hash ----------------------------
                std::string user_id, user_name, stored_hash, tenant_id;
                std::vector<std::string> roles;

                if (org_id.empty()) {
                    // Platform-level login — look up in isched_system.db
                    auto res = db.get_platform_admin_by_email(email);
                    if (!res)
                        throw std::runtime_error("Invalid credentials");
                    const auto& admin = res.value();
                    if (!admin.is_active)
                        throw std::runtime_error("Account is disabled");
                    user_id     = admin.id;
                    user_name   = admin.display_name;
                    stored_hash = admin.password_hash;
                    roles       = {std::string(Role::PLATFORM_ADMIN)};
                    tenant_id   = "platform";
                } else {
                    // Tenant user login — look up in per-org DB
                    auto res = db.get_user_by_email(org_id, email);
                    if (!res)
                        throw std::runtime_error("Invalid credentials");
                    const auto& u = res.value();
                    if (!u.is_active)
                        throw std::runtime_error("Account is disabled");
                    user_id     = u.id;
                    user_name   = u.display_name;
                    stored_hash = u.password_hash;
                    roles       = u.roles;
                    tenant_id   = org_id;
                }

                // --- verify Argon2id password hash --------------------------
                if (!verify_password(password, stored_hash))
                    throw std::runtime_error("Invalid credentials");

                // --- issue JWT + persist session ----------------------------
                // Session creation is owned by AuthenticationMiddleware (T049-002).
                // Failures are non-fatal (token is still valid without a DB record).
                const LoginSession sess = m_auth->create_session(
                    db, user_id, user_name, tenant_id, roles);

                return json{{"token", sess.token}, {"expiresAt", sess.expires_at}};
            });
        // login is intentionally NOT gated by require_roles().

        // ---------------------------------------------------------------
        // T-UI-D-003: createPlatformAdmin mutation — unauthenticated (seed only).
        // Rate-limited per source IP; only allowed when in seed mode.
        // ---------------------------------------------------------------
        register_resolver({}, "createPlatformAdmin",
            [this](const json&, const json& args, const ResolverCtx& ctx) -> json {
                // --- Resolve rate limit from env / default -----------------
                static const int seed_rate_limit = []() -> int {
                    if (const char* ev = std::getenv("ISCHED_SEED_RATE_LIMIT"); ev) {
                        try { return std::stoi(ev); } catch (...) {}
                    }
                    return 5;
                }();
                static RateLimiter s_limiter;
                if (!s_limiter.allow(ctx.remote_ip, seed_rate_limit)) {
                    throw std::runtime_error{"RATE_LIMITED: too many createPlatformAdmin attempts"};
                }

                // --- Seed-mode gate ----------------------------------------
                bool seed_active = true;
                if (m_database) {
                    const auto result = m_database->list_platform_admins();
                    if (result) {
                        seed_active = std::ranges::none_of(
                            result.value(),
                            [](const PlatformAdminRecord& r) { return r.is_active; });
                    }
                }
                if (!seed_active) {
                    throw std::runtime_error(
                        "createPlatformAdmin is only allowed during seed mode");
                }

                // --- Input validation --------------------------------------
                const std::string email    = args.value("email",    "");
                const std::string password = args.value("password", "");
                if (email.empty() || password.empty()) {
                    throw std::invalid_argument(
                        "createPlatformAdmin: email and password are required");
                }
                if (password.size() < 12) {
                    throw std::invalid_argument(
                        "password must be at least 12 characters");
                }

                // --- Hash + persist ----------------------------------------
                const std::string hash = hash_password(password);
                const std::string admin_id =
                    "pa_" + std::to_string(
                        std::hash<std::string>{}(email));
                const std::string display_name = email;

                if (auto res = m_database->create_platform_admin(
                        admin_id, email, hash, display_name);
                    !res)
                {
                    if (res.error() == DatabaseError::DuplicateKey)
                        throw std::runtime_error(
                            "Platform admin with email '" + email + "' already exists");
                    throw std::runtime_error("Failed to create platform admin");
                }

                // --- Return User JSON --------------------------------------
                auto rec = m_database->get_platform_admin_by_id(admin_id);
                if (!rec)
                    throw std::runtime_error(
                        "Platform admin created but could not be fetched");
                const auto& r = rec.value();
                return json{
                    {"id",          r.id},
                    {"email",       r.email},
                    {"displayName", r.display_name},
                    {"roles",       json::array({std::string(Role::PLATFORM_ADMIN)})},
                    {"isActive",    r.is_active},
                    {"createdAt",   r.created_at},
                    {"lastLogin",   json(nullptr)}
                };
            });
        // createPlatformAdmin is intentionally NOT gated by require_roles().

        // ---------------------------------------------------------------
        // T050-001 / T051-003: updateTenantConfig mutation (platform_admin only)
        // ---------------------------------------------------------------
        register_resolver({}, "updateTenantConfig",
            [this](const json&, const json& args, const ResolverCtx&) -> json {
                const std::string org_id = args.value("organizationId", "");
                if (org_id.empty())
                    throw std::invalid_argument("updateTenantConfig: organizationId is required");

                // Retrieve existing values as defaults
                int min_t    = 4;
                int max_t    = 16;
                int metrics_interval = 60;
                if (auto r = m_database->get_tenant_setting(org_id, "min_threads"); r)
                    min_t = std::stoi(r.value());
                if (auto r = m_database->get_tenant_setting(org_id, "max_threads"); r)
                    max_t = std::stoi(r.value());
                if (auto r = m_database->get_tenant_setting(org_id, "metrics_interval_minutes"); r)
                    metrics_interval = std::stoi(r.value());

                if (args.contains("minThreads") && !args["minThreads"].is_null())
                    min_t = args["minThreads"].get<int>();
                if (args.contains("maxThreads") && !args["maxThreads"].is_null())
                    max_t = args["maxThreads"].get<int>();
                if (args.contains("metricsInterval") && !args["metricsInterval"].is_null()) {
                    metrics_interval = args["metricsInterval"].get<int>();
                    if (metrics_interval < 1) metrics_interval = 1;
                }

                if (min_t < 1) min_t = 1;
                if (max_t < min_t) max_t = min_t;

                std::ignore = m_database->set_tenant_setting(org_id, "min_threads", std::to_string(min_t));
                std::ignore = m_database->set_tenant_setting(org_id, "max_threads", std::to_string(max_t));
                std::ignore = m_database->set_tenant_setting(org_id, "metrics_interval_minutes", std::to_string(metrics_interval));

                // Propagate new interval to MetricsCollector if available (T051-003)
                if (m_metrics) {
                    m_metrics->set_interval_minutes(org_id, metrics_interval);
                }

                return json{
                    {"organizationId",         org_id},
                    {"minThreads",             min_t},
                    {"maxThreads",             max_t},
                    {"metricsIntervalMinutes", metrics_interval}
                };
            });
        require_roles("updateTenantConfig", {std::string(Role::PLATFORM_ADMIN)});

        // ---------------------------------------------------------------
        // Shutdown mutation: platform_admin role OR matching ISCHED_SHUTDOWN_TOKEN.
        // The actual stop is fired on a short-lived detached thread so the
        // HTTP response is flushed before the io_context is torn down.
        // ---------------------------------------------------------------
        register_resolver({}, "shutdown",
            [this](const json&, const json&, const ResolverCtx& ctx) -> json
            {
                // Accept a static bearer token from the environment (automation / benchmark use).
                const char* env_token = std::getenv("ISCHED_SHUTDOWN_TOKEN");
                const bool token_ok   = (env_token != nullptr &&
                                         *env_token != '\0' &&
                                         ctx.bearer_token == std::string_view(env_token));

                if (!token_ok) {
                    const auto& r = ctx.roles;
                    const bool is_admin = std::find(
                        r.begin(), r.end(), std::string(Role::PLATFORM_ADMIN)) != r.end();
                    if (!is_admin) {
                        throw std::runtime_error(
                            "shutdown: requires platform_admin role or valid ISCHED_SHUTDOWN_TOKEN");
                    }
                }

                spdlog::info("Server: graceful shutdown requested via GraphQL mutation");
                if (m_shutdown_callback) {
                    // Detach so the resolver returns (and the response is written)
                    // before the io_context threads are joined.
                    std::thread([cb = m_shutdown_callback]() {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(200ms);
                        cb();
                    }).detach();
                }
                return true;
            });

        load_schema(BUILTIN_SCHEMA);
    }

    // -------------------------------------------------------------------------
    // generate_directives_introspection  (T-INTRO-030, T-INTRO-031)
    // Returns the directives array for __schema { directives }.
    // Includes the four built-in directives plus any user-defined ones from
    // the loaded schema.
    // -------------------------------------------------------------------------
    basic_json<> GqlExecutor::generate_directives_introspection() {
        json result = json::array();

        // Helper: build one directive entry
        auto make_directive = [](const std::string& name,
                                  const std::string& description,
                                  json locations,
                                  json args,
                                  bool isRepeatable = false) -> json {
            return json{{"name", name}, {"description", description},
                        {"locations", std::move(locations)},
                        {"args", std::move(args)},
                        {"isRepeatable", isRepeatable}};
        };

        // Built-in scalar type ref helper
        auto scalar_ref = [](const std::string& n) -> json {
            return json{{"kind","SCALAR"},{"name",n},{"ofType",nullptr}};
        };

        // @skip(if: Boolean!)
        result.push_back(make_directive(
            "skip",
            "Directs the executor to skip this field or fragment when the 'if' argument is true.",
            json::array({"FIELD","FRAGMENT_SPREAD","INLINE_FRAGMENT"}),
            json::array({json{{"name","if"},{"description","Skipped when true."},
                              {"type", json{{"kind","NON_NULL"},{"name",nullptr},
                                            {"ofType", scalar_ref("Boolean")}}},
                              {"defaultValue",nullptr},
                              {"isDeprecated",false},{"deprecationReason",nullptr}}})
        ));

        // @include(if: Boolean!)
        result.push_back(make_directive(
            "include",
            "Directs the executor to include this field or fragment only when the 'if' argument is true.",
            json::array({"FIELD","FRAGMENT_SPREAD","INLINE_FRAGMENT"}),
            json::array({json{{"name","if"},{"description","Included when true."},
                              {"type", json{{"kind","NON_NULL"},{"name",nullptr},
                                            {"ofType", scalar_ref("Boolean")}}},
                              {"defaultValue",nullptr},
                              {"isDeprecated",false},{"deprecationReason",nullptr}}})
        ));

        // @deprecated(reason: String)
        result.push_back(make_directive(
            "deprecated",
            "Marks an element of a GraphQL schema as no longer supported.",
            json::array({"FIELD_DEFINITION","ARGUMENT_DEFINITION",
                         "INPUT_FIELD_DEFINITION","ENUM_VALUE"}),
            json::array({json{{"name","reason"},
                              {"description","Explains why this element was deprecated, usually also including a suggestion for how to access supported similar data. Formatted using the Markdown syntax, as specified by [CommonMark](https://commonmark.org/)."},
                              {"type", scalar_ref("String")},
                              {"defaultValue","\"No longer supported\""},
                              {"isDeprecated",false},{"deprecationReason",nullptr}}})
        ));

        // @specifiedBy(url: String!)
        result.push_back(make_directive(
            "specifiedBy",
            "Exposes a URL that specifies the behavior of this scalar.",
            json::array({"SCALAR"}),
            json::array({json{{"name","url"},
                              {"description","The URL that specifies the behavior of this scalar."},
                              {"type", json{{"kind","NON_NULL"},{"name",nullptr},
                                            {"ofType", scalar_ref("String")}}},
                              {"defaultValue",nullptr},
                              {"isDeprecated",false},{"deprecationReason",nullptr}}})
        ));

        // User-defined directives from the loaded schema
        for (const auto& [dirName, dirNodePtr] : m_directives) {
            const auto& dirNode = *dirNodePtr;
            json dirObj;
            dirObj["name"]        = dirName;
            dirObj["description"] = extract_description(dirNode);
            dirObj["locations"]   = json::array();
            dirObj["isRepeatable"] = false;
            dirObj["args"]        = json::array();

            for (const auto& child : dirNode->children) {
                if (child->type.ends_with("ArgumentsDefinition")) {
                    for (const auto& arg : child->children)
                        if (arg->type.ends_with("InputValueDefinition"))
                            dirObj["args"].push_back(build_input_value_json(arg, m_type_map));
                }
                // TODO: extract DirectiveLocations if grammar captures them
            }
            result.push_back(std::move(dirObj));
        }
        return result;
    }

    // -------------------------------------------------------------------------
    // generate_schema_introspection  (T-INTRO-002 … T-INTRO-018)
    // Returns the full __schema response object.
    // -------------------------------------------------------------------------
    json GqlExecutor::generate_schema_introspection() {
        json schema = json::object();

        // --- queryType / mutationType / subscriptionType (T-INTRO-010) ---
        schema["queryType"]        = m_type_map.count("Query")
                                     ? json{{"name","Query"}} : json(nullptr);
        schema["mutationType"]     = m_type_map.count("Mutation")
                                     ? json{{"name","Mutation"}} : json(nullptr);
        schema["subscriptionType"] = m_type_map.count("Subscription")
                                     ? json{{"name","Subscription"}} : json(nullptr);

        json all_types = json::array();

        // Built-in scalar types (T-INTRO-002)
        for (const char* name : {"String","Int","Float","Boolean","ID"}) {
            all_types.push_back(json{{"kind","SCALAR"},{"name",name},
                {"description",nullptr},{"fields",nullptr},{"interfaces",nullptr},
                {"possibleTypes",nullptr},{"enumValues",nullptr},
                {"inputFields",nullptr},{"ofType",nullptr}});
        }

        // Introspection meta-types (T-INTRO-004)
        static const char* const k_meta_objects[] = {
            "__Schema","__Type","__Field","__InputValue","__EnumValue","__Directive"};
        for (const char* name : k_meta_objects) {
            all_types.push_back(json{{"kind","OBJECT"},{"name",name},
                {"description",nullptr},{"fields",nullptr},{"interfaces",json::array()},
                {"possibleTypes",nullptr},{"enumValues",nullptr},
                {"inputFields",nullptr},{"ofType",nullptr}});
        }
        static const char* const k_meta_enums[] = {"__TypeKind","__DirectiveLocation"};
        for (const char* name : k_meta_enums) {
            all_types.push_back(json{{"kind","ENUM"},{"name",name},
                {"description",nullptr},{"fields",nullptr},{"interfaces",nullptr},
                {"possibleTypes",nullptr},{"enumValues",json::array()},
                {"inputFields",nullptr},{"ofType",nullptr}});
        }

        // User-defined types (T-INTRO-011 … T-INTRO-018)
        for (const auto& [type_name, type_node_ptr] : m_type_map) {
            all_types.push_back(build_type_json(type_name, *type_node_ptr, m_type_map));
        }

        schema["types"]      = std::move(all_types);
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

        // ---- __typename meta-field (T-INTRO-025, T-INTRO-026) ----
        if (myFieldName == "__typename") {
            // Derive the declared type at the current path by walking the schema.
            // Walk: root="Query"/"Mutation"/"Subscription", then follow each
            // element of p_path through FieldDefinition return types.
            std::string current_type;
            // Determine root operation type from p_path context (best-effort: use "Query")
            if (m_type_map.count("Query"))      current_type = "Query";
            else if (!m_type_map.empty())       current_type = m_type_map.begin()->first;

            for (const auto& seg : p_path) {
                auto it = m_type_map.find(current_type);
                if (it == m_type_map.end()) { current_type.clear(); break; }
                current_type = field_return_type(*it->second, seg);
                if (current_type.empty()) break;
            }
            if (current_type.empty() && p_parent.is_object() && p_parent.contains("__typename"))
                current_type = p_parent["__typename"].get<std::string>();
            p_result[myFieldName] = current_type.empty() ? json(nullptr) : json(current_type);
            return false;
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
            ResolverCtx my_ctx = tl_resolver_ctx;  // copy thread-local auth context for this field

            // T047-003: RBAC gate — check if the caller holds at least one required role.
            // Only enforced at the top level (p_path is empty).
            if (p_path.empty()) {
                auto gate_it = m_required_roles.find(myFieldName);
                if (gate_it != m_required_roles.end() && !gate_it->second.empty()) {
                    const auto& required = gate_it->second;
                    const auto& caller_roles = my_ctx.roles;
                    const bool has_role = std::any_of(required.begin(), required.end(),
                        [&caller_roles](const std::string& r) {
                            return std::find(caller_roles.begin(), caller_roles.end(), r)
                                   != caller_roles.end();
                        });
                    if (!has_role) {
                        gql::ErrorPath ep;
                        for (const auto& s : my_field_path) ep.push_back(s);
                        p_error.push_back(gql::Error{
                            .code    = gql::EErrorCodes::FORBIDDEN,
                            .message = std::format(
                                "Access denied: field '{}' requires one of [{}]",
                                myFieldName,
                                [&]() {
                                    std::string joined;
                                    for (std::size_t i = 0; i < required.size(); ++i) {
                                        if (i) joined += ", ";
                                        joined += required[i];
                                    }
                                    return joined;
                                }()),
                            .path = std::move(ep),
                        });
                        p_result[myFieldName] = nullptr;
                        return false;
                    }
                }
            }

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
        } else if (a_op_type == "subscription") {
            // Subscription operations: execute like a query to deliver the initial value.
            // Ongoing event delivery is handled by the WebSocket server / SubscriptionBroker.
            for (size_t myIdx = 1; myIdx < myOperation->children.size(); ++myIdx) {
                const auto& child = myOperation->children[myIdx];
                if (child->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(p_parent_result, p_path, child, p_result, p_errors);
                } else if (child->type == "isched::v0_0_1::gql::VariablesDefinition"
                        || child->type == "isched::v0_0_1::gql::VariableDefinitions"
                        || child->type == "isched::v0_0_1::gql::VariableDefinition"
                        || child->type == "isched::v0_0_1::gql::Name") {
                    // skip metadata
                } else {
                    p_errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Expected a selection set in subscription, got {}.", child->type)
                    });
                }
            }
        } else if (myOperation->children[0]->type == "isched::v0_0_1::gql::SelectionSet") {
            process_field_selection(p_parent_result, p_path,myOperation->children[0], p_result, p_errors);
        } else {
            p_errors.push_back(gql::Error{
                .code=gql::EErrorCodes::PARSE_ERROR,
                .message=std::format("Only query/mutation/subscription operations are supported, got {}.", a_op_type)
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

        // ------------------------------------------------------------------
        // Query-parse cache: look up the pre-parsed AST for this query text.
        // ------------------------------------------------------------------
        std::shared_ptr<CachedParse> exec_entry;
        {
            std::shared_lock lock(m_query_cache_mutex);
            if (const auto it = m_query_cache.find(p_query); it != m_query_cache.end()) {
                exec_entry = it->second;   // copy shared_ptr; release lock immediately
            }
        }

        if (!exec_entry) {
            // Cache miss: allocate a stable CachedParse so the PEGTL nodes'
            // char* iterators into the string_input buffer remain valid.
            auto new_entry = std::make_shared<CachedParse>();
            new_entry->input = std::make_unique<tao::pegtl::string_input<>>(
                std::string(p_query), aName);
            try {
                auto [ok, root] = gql::generate_ast_and_log<gql::Document>(
                    *new_entry->input, aName, false, p_print_dot);
                if (!ok) {
                    my_result.errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message="Failed to parse schema document"
                    });
                    return my_result;
                }
                new_entry->root = std::move(root);
            } catch (const tao::pegtl::parse_error& e) {
                log_parse_error_exception(*new_entry->input, my_result, e);
                return my_result;
            }

            // Insert under exclusive lock (double-check in case a concurrent
            // thread parsed the same query between our lookup and now).
            {
                std::unique_lock lock(m_query_cache_mutex);
                if (!m_query_cache.contains(p_query)) {
                    if (m_query_cache.size() >= k_query_cache_max) {
                        m_query_cache.erase(m_query_cache.begin());
                    }
                    m_query_cache.emplace(std::string(p_query), new_entry);
                }
            }
            exec_entry = std::move(new_entry);
        }

        // Execute from the (possibly cached) parse tree.
        const auto& aRoot = exec_entry->root;
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

        // T041: query complexity and depth enforcement
        if (m_config.max_depth > 0 || m_config.max_complexity > 0) {
            const auto qa = analyse_query_complexity(aRoot);
            if (m_config.max_depth > 0 && qa.depth > m_config.max_depth) {
                my_result.errors.push_back(gql::Error{
                    .code    = gql::EErrorCodes::ARGUMENT_ERROR,
                    .message = std::format(
                        "Query depth {} exceeds maximum allowed depth {}",
                        qa.depth, m_config.max_depth)
                });
                return my_result;
            }
            if (m_config.max_complexity > 0 && qa.complexity > m_config.max_complexity) {
                my_result.errors.push_back(gql::Error{
                    .code    = gql::EErrorCodes::ARGUMENT_ERROR,
                    .message = std::format(
                        "Query complexity {} exceeds maximum allowed complexity {}",
                        qa.complexity, m_config.max_complexity)
                });
                return my_result;
            }
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
        return my_result;
    }

    ExecutionResult GqlExecutor::execute(
        const std::string_view p_query,
        const std::string_view p_variables_json,
        ResolverCtx p_ctx,
        const bool p_print_dot
    ) const {
        // Install the caller-supplied context into the thread-local slot so that
        // all resolvers dispatched by the inner execute() call can read it.
        tl_resolver_ctx = std::move(p_ctx);
        // Delegate to the base overload; tl_resolver_ctx will be picked up in
        // resolve_field_selection_details().
        return execute(p_query, p_variables_json, p_print_dot);
    }


    std::unique_ptr<GqlExecutor> GqlExecutor::create(std::shared_ptr<DatabaseManager> database) {
        return std::make_unique<GqlExecutor>(std::move(database));
    }
}
