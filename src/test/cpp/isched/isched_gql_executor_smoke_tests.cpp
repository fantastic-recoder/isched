// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_smoke_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for basic GqlExecutor resolver and variable execution.
 */

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <variant>
#include <iostream>
#include <iomanip>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_gql_grammar.hpp>
#include <nlohmann/json_fwd.hpp>
#include <tao/pegtl/string_input.hpp>
#include <functional>
#include <vector>

#include "isched/backend/isched_log_result.hpp"
#include "isched/backend/isched_DatabaseManager.hpp"
#include "isched/shared/fs/isched_fs_utils.hpp"

using nlohmann::json;
using isched::v0_0_1::gql::EErrorCodes;

namespace isched::v0_0_1::backend {

    TEST_CASE("GqlExecutor can be constructed", "[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        REQUIRE(true);
    }

    TEST_CASE("GqlExecutor executes on an empty Document and returns JSON object", "[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        json result = proc.execute("").data;
        REQUIRE(result.is_null());
    }

    TEST_CASE("Test hello world query","[gql][processor][smoke]") {
        const std::string schema_str = fsutils::read_file("hello_world_schema.graphql");
        REQUIRE(!schema_str.empty());
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        const auto myResult=proc.load_schema(std::string(schema_str));
        REQUIRE(myResult.is_success()==false);
        REQUIRE(myResult.errors.size()==3);
        REQUIRE(myResult.errors[0].code==EErrorCodes::MISSING_GQL_RESOLVER);
        REQUIRE(myResult.errors[1].code==EErrorCodes::MISSING_GQL_RESOLVER);
        proc.register_resolver({},"hello_ping", [](const json&,const json &, const ResolverCtx&)->json
        {
            return "Hello, World!";
        });
        proc.register_resolver({},"hello_who", [](const json& parent,const json& args, const ResolverCtx&)->json {
            REQUIRE(parent.is_object());
            REQUIRE(args.is_object());
            REQUIRE(args.contains("who"));
            std::string my_who = args["who"].get<std::string>();
            return std::format("Hello, {}!",my_who);
        });
        const auto myResult2=proc.load_schema(schema_str);
        REQUIRE(myResult2.is_success());
        const auto myReply= proc.execute(R"(query { hello_ping hello_who(who: "Josef") } )", "{}", true);
        REQUIRE(myReply.is_success()) ;
        REQUIRE(myReply.data.is_object());
        REQUIRE(myReply.data["hello_ping"].is_string());
        REQUIRE(myReply.data["hello_ping"] == "Hello, World!");
        REQUIRE(myReply.data["hello_who"].is_string());
        REQUIRE(myReply.data["hello_who"] == "Hello, Josef!");
    }

    TEST_CASE("Test int resolver","[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({},"multi", [](const json& ,const json& args, const ResolverCtx&)->json {
            long long my_retval = args["p_i"].get<long long>();
            return json{my_retval*2}[0];
        });
        const auto myResult=proc.load_schema("type Query { multi(p_i: Int): Int }");
        REQUIRE(myResult.is_success()==true);
        const auto myReply= proc.execute(R"(query { multi(p_i: 2) } )", "{}", true);
        REQUIRE(myReply.is_success()) ;
        REQUIRE(myReply.data["multi"].is_number_integer());
        REQUIRE(myReply.data["multi"].get<int>()==4);
    }

    TEST_CASE("Test all argument types", "[gql][processor][arguments]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        
        proc.register_resolver({},"test_args", [](const json&, const json& args, const ResolverCtx) -> json {
            return args;
        });

        const std::string schema = "type Query { test_args: String }";
        const auto loadResult = proc.load_schema(std::string(schema));
        REQUIRE(loadResult.is_success());

        SECTION("IntValue") {
            const auto reply = proc.execute("query { test_args(i: 123) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["i"] == 123);
        }
        SECTION("FloatValue") {
            const auto reply = proc.execute("query { test_args(f: 123.456) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["f"] == 123.456);
        }
        SECTION("StringValue") {
            const auto reply = proc.execute("query { test_args(s: \"hello\") }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["s"] == "hello");
        }
        SECTION("BooleanValue") {
            const auto reply = proc.execute("query { test_args(b: true) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["b"] == true);
        }
        SECTION("NullValue") {
            const auto reply = proc.execute("query { test_args(n: null) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["n"].is_null());
        }
        SECTION("EnumValue") {
            const auto reply = proc.execute("query { test_args(e: ENUM_VAL) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["e"] == "ENUM_VAL");
        }
        SECTION("ListValue") {
            const auto reply = proc.execute("query { test_args(l: [1, 2]) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["l"] == json::array({1, 2}));
        }
        SECTION("ObjectValue") {
            const auto reply = proc.execute("query { test_args(o: {a: 1}) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["o"]["a"] == 1);
        }
        SECTION("BlockString") {
            const auto reply = proc.execute(R"(query { test_args(s: """line1""") } )", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["s"].get<std::string>() == "line1");
        }
    }

    TEST_CASE("Test statefull resolver","[isched_gql_executor_tests]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        struct Summator {
            double sum = 0;
        } summator;
        proc.register_resolver(
            {}, "summ", [&summator](const json &, const json &args, const ResolverCtx &) -> json {
            summator.sum += args["i"].get<double>();
            return json{summator.sum}[0];
        });
        SECTION("Test stateful resolver") {
            const auto reply=proc.execute("query { summ(i: 1) summ(i: 2) summ(i: 3) }", "{}", true);
            REQUIRE(reply.is_success());
            REQUIRE(summator.sum == double{6.0});
            REQUIRE(reply.data["summ"].get<double>() == double{6.0});
        }
    }

    TEST_CASE("AST Descriptions in Type Definitions", "[gql][ast][descriptions]") {
        using namespace isched::v0_0_1;
        using tao::pegtl::string_input;

        auto verify_description = [](const std::string& input, const std::string& expected_desc) {
            string_input in(input, "DescriptionTest");
            auto [ok, root] = gql::generate_ast_and_log<gql::Document>(in, "Description Test");
            if (!ok) {
                REQUIRE(ok);
                return;
            }
            REQUIRE(root != nullptr);

            std::vector<std::string> descriptions;
            std::function<void(const gql::TAstNodePtr&)> find_desc = [&](const gql::TAstNodePtr& node) {
                if (node->is_type<gql::Description>()) {
                    descriptions.push_back(std::string(node->string_view()));
                }
                for (const auto& child : node->children) {
                    find_desc(child);
                }
            };
            find_desc(root);

            bool found = false;
            for (const auto& d : descriptions) {
                if (d.find(expected_desc) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        };

        SECTION("Type description") {
            verify_description(R"("Description of MyType" type MyType { field: String })", "Description of MyType");
        }
        SECTION("Field description") {
            verify_description(R"(type MyType { "Description of field" field: String })", "Description of field");
        }
        SECTION("Scalar description") {
            verify_description(R"("Description of MyScalar" scalar MyScalar)", "Description of MyScalar");
        }
        SECTION("Schema description") {
            verify_description(R"("Description of Schema" schema { query: MyQuery })", "Description of Schema");
        }
        SECTION("Operation description") {
            verify_description(R"("Description of Query" query MyQuery { field })", "Description of Query");
        }
        SECTION("Input value description") {
            verify_description(R"(type MyType { field("Arg desc" arg: Int): String })", "Arg desc");
        }
        SECTION("Block string description") {
            verify_description(R"("""Description with
multiple lines"""
type MyType { field: String })", "Description with");
        }
    }

    TEST_CASE("Sub-resolver: explicit resolver receives parent value", "[gql][executor][sub-resolver]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        json captured_parent;
        proc.register_resolver({}, "player", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"name", "Alice"}, {"age", 30}};
        });
        proc.register_resolver({"player"}, "name", [&captured_parent](const json& parent, const json&, const ResolverCtx&) -> json {
            captured_parent = parent;
            return parent.at("name");
        });
        proc.register_resolver({"player"}, "age", [](const json& parent, const json&, const ResolverCtx&) -> json {
            return parent.at("age");
        });
        const auto load_res = proc.load_schema(
            "type Query { player: PlayerType } type PlayerType { name: String age: Int }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ player { name age } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["player"]["name"] == "Alice");
        REQUIRE(reply.data["player"]["age"] == 30);
        REQUIRE(captured_parent.value("name", "") == "Alice");
        REQUIRE(captured_parent.value("age", -1) == 30);
    }

    TEST_CASE("Sub-resolver: default field resolver extracts from parent", "[gql][executor][sub-resolver][default-resolver]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "srv_info_test", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"version", "1.0"}, {"name", "isched"}};
        });

        const auto load_res = proc.load_schema(
            "type Query { srv_info_test: InfoType } type InfoType { version: String name: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ srv_info_test { version name } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["srv_info_test"]["version"] == "1.0");
        REQUIRE(reply.data["srv_info_test"]["name"] == "isched");
    }

    TEST_CASE("Sub-resolver: multi-level nesting produces correct JSON structure", "[gql][executor][sub-resolver][nesting]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "a", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"b", {{"c", "deep_value"}}}};
        });

        const auto load_res = proc.load_schema(
            "type Query { a: AType } type AType { b: BType } type BType { c: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ a { b { c } } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["a"]["b"]["c"] == "deep_value");
    }

    TEST_CASE("Sub-resolver: failing resolver nullifies field, siblings still resolve", "[gql][executor][sub-resolver][error-propagation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "good", [](const json&, const json&, const ResolverCtx&) -> json {
            return "good_value";
        });
        proc.register_resolver({}, "bad", [](const json&, const json&, const ResolverCtx&) -> json {
            throw std::runtime_error("resolver failure");
        });

        const auto load_res = proc.load_schema(
            "type Query { good: String bad: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ good bad }");
        REQUIRE_FALSE(reply.is_success());
        REQUIRE(reply.data["good"] == "good_value");
        REQUIRE(reply.data["bad"].is_null());
        REQUIRE_FALSE(reply.errors.empty());
        bool found = false;
        for (const auto& e : reply.errors) {
            if (e.message.find("threw") != std::string::npos) { found = true; break; }
        }
        REQUIRE(found);
    }

    TEST_CASE("Sub-resolver: arguments reach sub-resolver p_args", "[gql][executor][sub-resolver][arguments]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "container", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object();
        });
        proc.register_resolver({"container"}, "greet", [](const json&, const json& args, const ResolverCtx&) -> json {
            return "Hello, " + args.at("who").get<std::string>() + "!";
        });

        const auto load_res = proc.load_schema(
            "type Query { container: ContainerType } type ContainerType { greet(who: String): String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(R"({ container { greet(who: "World") } })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["container"]["greet"] == "Hello, World!");
    }

    TEST_CASE("Sub-resolver: missing resolver with absent parent key emits MISSING_GQL_RESOLVER", "[gql][executor][sub-resolver][missing]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "outer", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object();
        });

        const auto load_res = proc.load_schema(
            "type Query { outer: OuterType } type OuterType { x: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ outer { x } }");
        REQUIRE_FALSE(reply.is_success());
        bool found_missing = false;
        for (const auto& e : reply.errors) {
            if (e.code == EErrorCodes::MISSING_GQL_RESOLVER) { found_missing = true; break; }
        }
        REQUIRE(found_missing);
    }

    TEST_CASE("Sub-resolver: error path contains field names as strings", "[gql][executor][sub-resolver][error-path]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "parent_field", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object();
        });

        const auto load_res = proc.load_schema(
            "type Query { parent_field: ParentType } type ParentType { missing: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ parent_field { missing } }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors) {
            if (e.code == EErrorCodes::MISSING_GQL_RESOLVER) {
                REQUIRE_FALSE(e.path.empty());
                REQUIRE(std::holds_alternative<std::string>(e.path.back()));
                REQUIRE(std::get<std::string>(e.path.back()) == "missing");
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    TEST_CASE("Sub-resolver: list element sub-selection preserves order and null entries",
              "[gql][executor][sub-resolver][list][parity]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "users_list_parity", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::array({
                json{{"name", "Alice"}, {"age", 30}},
                nullptr,
                json{{"name", "Bob"}, {"age", 28}}
            });
        });

        const auto load_res = proc.load_schema(
            "type Query { users_list_parity: [UserType] } type UserType { name: String age: Int }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ users_list_parity { name age } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["users_list_parity"].is_array());
        REQUIRE(reply.data["users_list_parity"].size() == 3);
        REQUIRE(reply.data["users_list_parity"][0]["name"] == "Alice");
        REQUIRE(reply.data["users_list_parity"][1].is_null());
        REQUIRE(reply.data["users_list_parity"][2]["age"] == 28);
    }

    TEST_CASE("Sub-resolver: repeated nested field reads stay deterministic across executions",
              "[gql][executor][sub-resolver][determinism]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "viewer", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"profile", {{"displayName", "isched-user"}}}};
        });

        const auto load_res = proc.load_schema(
            "type Query { viewer: ViewerType } type ViewerType { profile: ProfileType } type ProfileType { displayName: String }");
        REQUIRE(load_res.is_success());

        for (int run = 0; run < 5; ++run) {
            const auto reply = proc.execute("{ viewer { profile { displayName } } }");
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["viewer"]["profile"]["displayName"] == "isched-user");
        }
    }

    TEST_CASE("Mutation operation: echo built-in mutation dispatches correctly", "[gql][executor][mutation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(R"(mutation { echo(message: "hello") })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["echo"].is_string());
        REQUIRE(reply.data["echo"] == "hello");
    }

    TEST_CASE("Mutation operation: echo with null arg returns null", "[gql][executor][mutation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(R"(mutation { echo })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["echo"].is_null());
    }

    TEST_CASE("Mutation operation: custom mutation resolver", "[gql][executor][mutation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        std::string captured_name;
        proc.register_resolver({}, "createItem", [&](const json&, const json& args, const ResolverCtx&) -> json {
            captured_name = args.value("name", std::string{});
            return json{{"id", "item-1"}, {"name", captured_name}};
        });
        const auto load_res = proc.load_schema("type Query { hello: String } type Mutation { createItem(name: String): ItemResult } type ItemResult { id: String name: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(R"(mutation { createItem(name: "widget") { id name } })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["createItem"]["id"] == "item-1");
        REQUIRE(reply.data["createItem"]["name"] == "widget");
        REQUIRE(captured_name == "widget");
    }

    TEST_CASE("Variables: query with string variable", "[gql][executor][variables]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "greetVar", [](const json&, const json& args, const auto&) -> json {
            return "Hello, " + args.value("who", std::string{"world"}) + "!";
        });
        const auto load_res = proc.load_schema("type Query { greetVar(who: String): String }");
        REQUIRE(load_res.is_success());

        const std::string variables = R"({"name":"Alice"})";
        const auto reply = proc.execute(
            R"(query($name: String) { greetVar(who: $name) })",
            variables);
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["greetVar"] == "Hello, Alice!");
    }

    TEST_CASE("Variables: missing variable resolves to null", "[gql][executor][variables]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "echoVar", [](const json&, const json& args, const auto&) -> json {
            if (args.contains("msg") && !args["msg"].is_null()) {
                return args["msg"].get<std::string>();
            }
            return nullptr;
        });
        const auto load_res = proc.load_schema("type Query { echoVar(msg: String): String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            R"(query($msg: String) { echoVar(msg: $msg) })",
            "{}");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["echoVar"].is_null());
    }

    TEST_CASE("Variables: integer variable", "[gql][executor][variables]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "doubly", [](const json&, const json& args, const auto&) -> json {
            return args.value("n", 0) * 2;
        });
        const auto load_res = proc.load_schema("type Query { doubly(n: Int): Int }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            R"(query($n: Int) { doubly(n: $n) })",
            R"({"n":21})");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["doubly"] == 42);
    }

} // namespace isched::v0_0_1::backend
