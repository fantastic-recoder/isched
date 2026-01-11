//
// Created by groby on 2025-12-19.
//

#include "isched_GqlExecutor.hpp"

#include <chrono>
#include <regex>
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

namespace {
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
        register_resolver({},"hello", [](const nlohmann::json &, const nlohmann::json &) {
            return basic_json("Hello, GraphQL!");
        });

        register_resolver({},"version", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return basic_json("0.0.1");
        });

        // Uptime resolver
        register_resolver({},"uptime", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            auto now = std::chrono::system_clock::now();
            auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time).count();
            return basic_json(uptime_seconds);
        });

        // Client count resolver
        register_resolver({},"clientCount", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return nlohmann::basic_json(1); // Placeholder - could be enhanced with actual connection tracking
        });

        // Spring Boot Actuator-style Health endpoint
        register_resolver({},"health", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            nlohmann::json health;

            // Overall status check
            std::string overall_status = "UP";
            nlohmann::json components;

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

            health = {
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
        register_resolver({},"info", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return nlohmann::json{
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
        register_resolver({},"metrics", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            static auto start_time = std::chrono::system_clock::now();
            static std::atomic<int> request_counter{0};
            static std::atomic<int> error_counter{0};

            auto now = std::chrono::system_clock::now();
            auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

            request_counter++;

            return nlohmann::json{
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
        register_resolver({},"env", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            nlohmann::json env;

            // System properties
            env["systemProperties"] = {
                {"os.name", "Linux"},
                {"user.name", getenv("USER") ? getenv("USER") : "unknown"},
                {"user.home", getenv("HOME") ? getenv("HOME") : "/"},
                {"file.separator", "/"},
                {"path.separator", ":"}
            };

            // Environment variables (filtered for security)
            env["environmentVariables"] = nlohmann::json::object();
            const char *safe_vars[] = {"PATH", "HOME", "USER", "LANG", "TZ", nullptr};
            for (int i = 0; safe_vars[i]; ++i) {
                if (const char *value = getenv(safe_vars[i])) {
                    env["environmentVariables"][safe_vars[i]] = value;
                }
            }

            return env;
        });

        // Configuration properties endpoint
        register_resolver({},"configprops", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return nlohmann::json{
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
                {"features", nlohmann::json::array({"GraphQL", "Multi-tenant", "Health monitoring", "Metrics"})},
                {"version", "1.0.0"},
                {"environment", "development"}
            };
        });

        // Enhanced schema introspection resolver
        register_resolver({},"__schema", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            json my_ret_val = generate_schema_introspection();
            return my_ret_val;
        });
        register_resolver({"__schema"},"name", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res1");
        });
        register_resolver({"__schema"},"description", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res2");
        });
        register_resolver({"__schema"},"fields", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res3");
        });
        register_resolver({"__schema"},"args", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res-schema-args");
        });
        register_resolver({"__schema","args"},"name", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res-schema-args");
        });
        register_resolver({"__schema","fields"},"name", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res4");
        });
        register_resolver({"__schema","types","fields"},"description", [this](const nlohmann::json &p_args, const nlohmann::json &) -> nlohmann::json {
            return basic_json("res5");
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
            dirObj["locations"] = nlohmann::json::array();
            dirObj["args"] = nlohmann::json::array();

            for (const auto &child: dirNode->children) {
                if (child->type == "isched::v0_0_1::gql::Description") {
                    dirObj["description"] = child->string_view();
                } else if (child->type == "isched::v0_0_1::gql::ArgumentsDefinition") {
                    for (const auto &argChild: child->children) {
                        if (argChild->type == "isched::v0_0_1::gql::InputValueDefinition") {
                            nlohmann::json argObj;
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
        schema["mutationType"] = nullptr;
        schema["subscriptionType"] = nullptr;

        auto types = nlohmann::json::array();

        for (const auto &[typeName, typeNodePtr]: m_type_map) {
            const auto &typeNode = *typeNodePtr;
            nlohmann::json typeObj;
            typeObj["name"] = typeName;
            typeObj["kind"] = "OBJECT"; // Simplified for now

            // Extract description if present
            for (const auto &child: typeNode->children) {
                if (child->type == "isched::v0_0_1::gql::Description") {
                    typeObj["description"] = child->string_view();
                }
            }

            // Extract fields
            auto fieldsArray = nlohmann::json::array();
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
        if (p_arg->type == "isched::v0_0_1::gql::StringValue") {
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
            if (my_arguments_node->type == "isched::v0_0_1::gql::Name") {
                spdlog::debug("Skipping argument field: {}", std::string(my_arguments_node->string_view()));
                continue;
            }
            my_ret_val=process_arguments(my_arguments_node, p_errors);
        }
        spdlog::debug("Got args: '{}' for field '{}'", my_ret_val.dump(4), std::string(p_field_node->children[0]->string_view()));
        return my_ret_val;
    }

    void GqlExecutor::process_sub_selection(const ResolverPath& p_path, const TAstNodePtr &node,  json &p_result, gql::TErrorVector& p_errors) const {
        json my_args=json::object(); //<TODO
        if (node->type == "isched::v0_0_1::gql::Arguments") {
            my_args=process_arguments(node, p_errors);
        } else {
            if (node->type != "isched::v0_0_1::gql::SelectionSet") {
                p_errors.push_back(gql::Error{.code=gql::EErrorCodes::ARGUMENT_ERROR,.message=format(
                    "Expected a arguments or selection set while processing sub selection, got a {}.", node->type)});
                return;
            }
            for (const auto &my_selection_set: node->children) {
                if (my_selection_set->type != "isched::v0_0_1::gql::Selection") {
                    p_errors.push_back(gql::Error{.code=gql::EErrorCodes::ARGUMENT_ERROR,.message=format(
                        "Expected a selection, got a {}.", my_selection_set->type)});
                    continue;
                }
                for (const auto &my_field: my_selection_set->children) {
                    process_field_selection(p_path, my_field, p_result, p_errors);
                }
            }
        }
        spdlog::debug("Got subselection: \n***\n{}\n***\n.", gql::dump_ast(node));
    }

    void GqlExecutor::process_field_sub_selections(
        const ResolverPath &p_path,
        const TAstNodePtr &p_selection_set,
        json &p_result,
        gql::TErrorVector &p_error,
        const std::string myFieldName
    ) const {
        ResolverPath my_sub_path = p_path;
        my_sub_path.push_back(myFieldName);
        for (size_t myIdx = 1; myIdx < p_selection_set->children.size(); ++myIdx) {
            process_sub_selection(my_sub_path,p_selection_set->children[myIdx], p_result, p_error);
        }
    }

    bool GqlExecutor::resolve_field_selection_details(const ResolverPath& p_path,const TAstNodePtr &p_field_node, json &p_result, gql::TErrorVector& p_error) const {
        if (p_field_node->children.empty()) {
            p_error.push_back(
                gql::Error{.code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty field"});
            return true;
        }
        const std::string myFieldName = std::string(p_field_node->children[0]->string_view());
        spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
        if (!m_resolvers.has_resolver(p_path,myFieldName)) {
            p_error.push_back(gql::Error{
                .code=gql::EErrorCodes::MISSING_GQL_RESOLVER,
                .message = std::format("Missing resolver for field {}.{} in Query type", concat_vector(p_path, "."),
                                       myFieldName)
            });
        } else {
            if (p_result.empty()) {
                p_result = json::object();
            }
            json my_ctx ={basic_json<>::array()};
            json my_args = process_argument_field(p_field_node, p_error);
            spdlog::debug("Got args: '{}' for field '{}' in Query type", my_args.dump(4), myFieldName);
            const ResolverFunction& my_found_resolver = m_resolvers.get_resolver(p_path,myFieldName);
            json my_result = my_found_resolver(my_args, my_ctx);
            p_result[myFieldName] = my_result;
            spdlog::debug("Got result: '{}' for field '{}' in Query type, going to process sub selections.",
                p_result.dump(4,'.'), myFieldName);
            process_field_sub_selections(p_path, p_field_node, p_result, p_error, myFieldName);
        }
        return false;
    }

    void GqlExecutor::process_field_selection(const ResolverPath& p_path, const TAstNodePtr &p_selection_set,
        json &p_result, gql::TErrorVector& p_errors) const {
        for (const auto &mySelection: p_selection_set->children) {
            if (mySelection->type == "isched::v0_0_1::gql::SelectionSet") {
                process_field_selection(p_path, mySelection,p_result, p_errors);
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
                resolve_field_selection_details(p_path, myField,p_result, p_errors);
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
        spdlog::error("Parse error: {} at {}:{}", e.what(), in.line_at(p), p.column);
        myResult.errors.push_back(gql::Error{
            gql::EErrorCodes::PARSE_ERROR,
            std::format("Error parsing schema: message={}, line={} column={}.",
                        e.what(), in.line_at(p), p.column)
        });
    }

    bool GqlExecutor::process_operation_definitions(
        const TAstNodePtr &myOperation,
        json &p_result, gql::TErrorVector &p_errors) const {
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
                if (myOperation->children[myIdx]->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(p_path, myOperation->children[myIdx],p_result, p_errors);
                } else {
                    p_errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Expected with a selection set, got {}.", myOperation->children[myIdx]->type)
                    });
                }
            }
        } else if (myOperation->children[0]->type == "isched::v0_0_1::gql::SelectionSet") {
            process_field_selection(p_path, myOperation->children[0],p_result, p_errors);
        } else {
            p_errors.push_back(gql::Error{
                .code=gql::EErrorCodes::PARSE_ERROR,
                .message=std::format("Only query operations are supported, got {}.", a_op_type)
            });
        }
        return false;
    }

    ExecutionResult GqlExecutor::execute(const std::string_view p_query, const bool p_print_dot) const {
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
                    process_operation_definitions(myOperation,my_result.data, my_result.errors);
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
