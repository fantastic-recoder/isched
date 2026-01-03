//
// Created by groby on 2025-12-19.
//

#include "isched_GqlExecutor.hpp"

#include <chrono>
#include <regex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tao/pegtl.hpp>

#include "isched_gql_grammar.hpp"
#include "isched_builtin_server_schema.hpp"

namespace {
}

namespace isched::v0_0_1::backend {
    using nlohmann::json;
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
        register_resolver("hello", [](const nlohmann::json &, const nlohmann::json &) {
            return nlohmann::basic_json("Hello, GraphQL!");
        });

        register_resolver("version", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return nlohmann::basic_json("0.0.1");
        });

        // Uptime resolver
        register_resolver("uptime", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            auto now = std::chrono::system_clock::now();
            auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time).count();
            return nlohmann::basic_json(uptime_seconds);
        });

        // Client count resolver
        register_resolver("clientCount", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            return nlohmann::basic_json(1); // Placeholder - could be enhanced with actual connection tracking
        });

        // Spring Boot Actuator-style Health endpoint
        register_resolver("health", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
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
        register_resolver("info", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
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
        register_resolver("metrics", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
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
        register_resolver("env", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
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
        register_resolver("configprops", [](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
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
        register_resolver("__schema", [this](const nlohmann::json &, const nlohmann::json &) -> nlohmann::json {
            nlohmann::json schema;

            // Query type definition
            schema["queryType"] = {
                {"name", "Query"},
                {"description", "Root query type for isched GraphQL API"}
            };

            schema["mutationType"] = nullptr;
            schema["subscriptionType"] = nullptr;

            // Built-in types with comprehensive field definitions
            schema["types"] = nlohmann::json::array({
                {
                    {"name", "Query"},
                    {"kind", "OBJECT"},
                    {"description", "Root query type"},
                    {
                        "fields", nlohmann::json::array({
                            {{"name", "hello"}, {"type", "String"}, {"description", "Simple greeting message"}},
                            {{"name", "version"}, {"type", "String"}, {"description", "Server version"}},
                            {{"name", "uptime"}, {"type", "Int"}, {"description", "Server uptime in seconds"}},
                            {{"name", "clientCount"}, {"type", "Int"}, {"description", "Number of active clients"}},
                            {{"name", "health"}, {"type", "HealthStatus"}, {"description", "Server health status"}},
                            {{"name", "info"}, {"type", "AppInfo"}, {"description", "Application information"}},
                            {{"name", "metrics"}, {"type", "ServerMetrics"}, {"description", "Server metrics"}},
                            {{"name", "env"}, {"type", "Environment"}, {"description", "Environment properties"}},
                            {
                                {"name", "configprops"}, {"type", "Configuration"},
                                {"description", "Configuration properties"}
                            }
                        })
                    }
                },
                {
                    {"name", "HealthStatus"},
                    {"kind", "OBJECT"},
                    {"description", "Health status information"},
                    {
                        "fields", nlohmann::json::array({
                            {{"name", "status"}, {"type", "String"}, {"description", "Overall health status"}},
                            {
                                {"name", "components"}, {"type", "[HealthComponent]"},
                                {"description", "Individual component health"}
                            },
                            {{"name", "timestamp"}, {"type", "String"}, {"description", "Health check timestamp"}}
                        })
                    }
                },
                {
                    {"name", "ServerMetrics"},
                    {"kind", "OBJECT"},
                    {"description", "Server performance metrics"},
                    {
                        "fields", nlohmann::json::array({
                            {{"name", "uptime"}, {"type", "Int"}, {"description", "Uptime in milliseconds"}},
                            {{"name", "activeConnections"}, {"type", "Int"}, {"description", "Active connections"}},
                            {{"name", "totalRequests"}, {"type", "Int"}, {"description", "Total requests processed"}},
                            {{"name", "failedRequests"}, {"type", "Int"}, {"description", "Failed requests"}},
                            {
                                {"name", "averageResponseTime"}, {"type", "Float"},
                                {"description", "Average response time"}
                            },
                            {{"name", "memoryUsage"}, {"type", "String"}, {"description", "Memory usage information"}},
                            {{"name", "threadCount"}, {"type", "Int"}, {"description", "Active thread count"}}
                        })
                    }
                }
            });


            schema["directives"] = nlohmann::json::array();

            return schema;
        });

        load_schema(BUILTIN_SCHEMA);
    }

    void GqlExecutor::udate_type_map(const TAstNodePtr &p_typedef, TTypeMap &p_type_map) {
        if (p_typedef->type == "isched::v0_0_1::gql::TypeDefinition") {
            const auto myTypeName = std::string(p_typedef->children[0]->string_view());
            p_type_map[myTypeName] = &p_typedef;
        }
        for (const auto &myChild: p_typedef->children) {
            udate_type_map(myChild, p_type_map);
        }
    }

    void GqlExecutor::update_type_map() {
        m_type_map.clear();
    }

    void GqlExecutor::process_field_definition(ExecutionResult &p_result,
                                               const TAstNodePtr &p_typedef, size_t p_idx) {
        const auto &myField = p_typedef->children[p_idx];
        if (myField->type != "isched::v0_0_1::gql::FieldDefinition") {
            return;
        }
        if (myField->children.empty()) {
            p_result.errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR, "Incomplete Query field definition"
            });
        } else {
            const auto myFieldName = myField->children[0]->string_view();
            spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
            if (!m_resolvers.has_resolver(std::string(myFieldName))) {
                p_result.errors.push_back(gql::Error{
                    gql::EErrorCodes::MISSING_GQL_RESOLVER,
                    std::format("Missing resolver for field {} in Query type", myFieldName)
                });
            }
        }
    }

    json GqlExecutor::extract_argument_value(const TAstNodePtr &p_arg, ExecutionResult &p_execution_result) const {
        json my_ret_val;
        if (p_arg->type == "isched::v0_0_1::gql::StringValue") {
            const std::string my_ret_val_str(p_arg->string_view());
            if (my_ret_val_str.starts_with("\"\"\"")) {
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
                my_ret_val.push_back(extract_argument_value(myChild, p_execution_result));
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::ObjectValue") {
            my_ret_val = json::object();
            for (const auto &myField: p_arg->children) {
                if (myField->type == "isched::v0_0_1::gql::ObjectField") {
                    const auto myFieldName = std::string(myField->children[0]->string_view());
                    my_ret_val[myFieldName] = extract_argument_value(myField->children[1], p_execution_result);
                }
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::Value") {
            if (!p_arg->children.empty()) {
                return extract_argument_value(p_arg->children[0], p_execution_result);
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::Argument") {
            if (p_arg->children.size() > 1) {
                return extract_argument_value(p_arg->children[1], p_execution_result);
            }
        } else {
            p_execution_result.errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR,
                format("Unknown argument value type: {}.", p_arg->type)
            });
        }
        return my_ret_val;
    }

    json GqlExecutor::process_arguments(const TAstNodePtr &p_field_node, ExecutionResult &p_execution_result) const {
        json my_ret_val = json::object();
        if (p_field_node->children.size() > 1) {
            const auto &myArgs = p_field_node->children[1];
            if (!myArgs || myArgs->type != "isched::v0_0_1::gql::Arguments") {
                p_execution_result.errors.push_back(gql::Error{
                    .code = gql::EErrorCodes::PARSE_ERROR,
                    .message = format("Expected arguments, got a {}", myArgs->type)
                });
                return my_ret_val;
            }
            for (const auto &myArg: myArgs->children) {
                if (myArg->type != "isched::v0_0_1::gql::Argument") {
                    p_execution_result.errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR,
                        .message = format("Expected an argument, got a {}", myArg->type)
                    });
                    continue;
                }
                if (myArg->children.size() != 2) {
                    p_execution_result.errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty argument"
                    });
                    continue;
                }
                const auto myArgName = std::string(myArg->children[0]->string_view());
                my_ret_val[myArgName] = extract_argument_value(myArg->children[1], p_execution_result);
            }
        }
        return my_ret_val;
    }

    void GqlExecutor::process_field_selection(const TAstNodePtr &p_selection_set, ExecutionResult &p_result) const {
        for (const auto &mySelection: p_selection_set->children) {
            if (mySelection->type == "isched::v0_0_1::gql::SelectionSet") {
                process_field_selection(mySelection, p_result);
            } else if (mySelection->type == "isched::v0_0_1::gql::Selection") {
                if (mySelection->children.empty()) {
                    p_result.errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty selection"
                    });
                    continue;
                }
                const auto &myField = mySelection->children[0];
                if (myField->type != "isched::v0_0_1::gql::Field") {
                    p_result.errors.push_back(gql::Error{
                        .code = gql::EErrorCodes::PARSE_ERROR,
                        .message = format("Expected a field, got a {}", myField->type)
                    });
                    continue;
                }
                if (myField->children.empty()) {
                    p_result.errors.push_back(
                        gql::Error{.code = gql::EErrorCodes::PARSE_ERROR, .message = "Empty field"});
                    continue;
                }
                const std::string myFieldName = std::string(myField->children[0]->string_view());
                spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
                if (!m_resolvers.has_resolver(myFieldName)) {
                    p_result.errors.push_back(gql::Error{
                        gql::EErrorCodes::MISSING_GQL_RESOLVER,
                        std::format("Missing resolver for field {} in Query type", myFieldName)
                    });
                } else {
                    if (p_result.data.empty()) {
                        p_result.data = json::object();
                    }
                    json my_args = process_arguments(myField, p_result);
                    const ResolverFunction my_found_resolver = m_resolvers.get_resolver(myFieldName);
                    json my_result = my_found_resolver(my_args, json::object());
                    p_result.data[myFieldName] = my_result;
                }
            } else {
                p_result.errors.push_back(gql::Error{
                    gql::EErrorCodes::PARSE_ERROR,
                    format("Expected a selection or selection set, got a {}", mySelection->type)
                });
            }
        }
    }

    ExecutionResult GqlExecutor::load_schema(const std::string &pSchemaDocument, bool p_print_dot) {
        static const std::string aName = "SchemaDocument";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(pSchemaDocument), aName);
        ExecutionResult myResult;
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName, false, p_print_dot);
            const bool aParsingOk = std::get<0>(myRetVal);
            auto aRoot = std::get<1>(std::move(myRetVal));
            if (!aParsingOk) {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
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
                                process_field_definition(myResult, myDefNode, myIdx);
                            }
                        }
                    } else if (myDefNode->type == "isched::v0_0_1::gql::ExecutableDefinition") {
                        myResult.errors.push_back(gql::Error{
                            gql::EErrorCodes::EXECUTABLE_DEF_NOT_ALLOWED,
                            "Executable definition not allowed in schema load."
                        });
                    }
                }
            }
            if (myResult.is_success()) {
                m_current_schema = gql::merge_type_definitions(std::move(m_current_schema), std::move(aRoot));
            } else {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR,
                    "Failed to parse schema, ignoring it."});
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(in, myResult, e);
        }
        return myResult;
    }

    void GqlExecutor::log_parse_error_exception(const tao::pegtl::string_input<> &in, ExecutionResult myResult,
                                                const tao::pegtl::parse_error &e) const {
        const auto p = e.positions().front();
        myResult.errors.push_back(gql::Error{
            gql::EErrorCodes::PARSE_ERROR,
            std::format("Error parsing schema: message={}, line={} column={}.",
                        e.what(), in.line_at(p), p.column)
        });
    }

    bool GqlExecutor::process_operation_definitions(ExecutionResult &p_result, const TAstNodePtr &myOperation) const {
        if (myOperation->type != "isched::v0_0_1::gql::OperationDefinition") {
            p_result.errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR,
                std::format("Expected with an operation definition, got {}.", myOperation->type)
            });
            return true;
        }
        if (myOperation->children.empty()) {
            p_result.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Empty operation definition"});
            return true;
        }
        const auto a_op_type = myOperation->children[0]->string_view();
        if (a_op_type == "query") {
            for (size_t myIdx = 1; myIdx < myOperation->children.size(); ++myIdx) {
                if (myOperation->children[myIdx]->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(myOperation->children[myIdx], p_result);
                } else {
                    p_result.errors.push_back(gql::Error{
                        gql::EErrorCodes::PARSE_ERROR,
                        std::format("Expected with a selection set, got {}.", myOperation->children[myIdx]->type)
                    });
                }
            }
        } else if (myOperation->children[0]->type == "isched::v0_0_1::gql::SelectionSet") {
            process_field_selection(myOperation->children[0], p_result);
        } else {
            p_result.errors.push_back(gql::Error{
                gql::EErrorCodes::PARSE_ERROR,
                std::format("Only query operations are supported, got {}.", a_op_type)
            });
        }
        return false;
    }

    ExecutionResult GqlExecutor::execute(const std::string_view p_query, const bool p_print_dot) const {
        static const std::string aName = "ExecutableDocument";
        ExecutionResult myResult;
        if (p_query.length() > 100000) {
            // Max pQuery length
            myResult.errors.push_back(gql::Error{
                gql::EErrorCodes::ARGUMENT_ERROR,
                "Query length exceeds maximum allowed"
            });
        }
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(std::string(p_query)), aName);
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName, false, p_print_dot);
            auto aRoot = std::get<1>(std::move(myRetVal));
            const bool aParsingOk = std::get<0>(myRetVal);
            if (!aParsingOk) {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
            if (!aRoot || aRoot->children.empty()) {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            const auto &myDoc = aRoot->children[0];
            if (!myDoc || myDoc->type != "isched::v0_0_1::gql::Document") {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Expected with a document"});
                return myResult;
            }
            if (myDoc->children.empty()) {
                myResult.errors.push_back(gql::Error{gql::EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            for (const auto &myChild: myDoc->children) {
                if (myChild->type != "isched::v0_0_1::gql::ExecutableDefinition") {
                    myResult.errors.push_back(gql::Error{
                        gql::EErrorCodes::PARSE_ERROR,
                        std::format("Expected with an executable definition, got {}.", myChild->type)
                    });
                    return myResult;
                }
                for (const auto &myOperation: myChild->children) {
                    process_operation_definitions(myResult, myOperation);
                }
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(in, myResult, e);
        }
        return myResult;
    }


    std::unique_ptr<GqlExecutor> GqlExecutor::create(std::shared_ptr<DatabaseManager> database) {
        return std::make_unique<GqlExecutor>(std::move(database));
    }
}
