// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 unit tests for `GqlExecutor`.
 *
 * Covers built-in resolver dispatch (hello, version, uptime, serverInfo,
 * health) and the `__schema` / `__type` introspection skeleton.
 * Tests are extended as Phase 5b introspection tasks are completed.
 */

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <variant>

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
        // Construction should not throw
        REQUIRE(true);
    }

    TEST_CASE("GqlExecutor executes on an empty Document and returns JSON object", "[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());

        // Minimal empty document for now; parser not required for skeleton
        json result = proc.execute("").data;
        // For a skeleton test, just assert it's a JSON value (object/array/etc.).
        // The current implementation returns default-constructed json which is null; accept that but ensure it is a json type.
        // Adjust expectations in future when load_schema() gains behavior.
        REQUIRE(result.is_null());
    }

    TEST_CASE("Test hello world query","[gql][processor][smoke]") {
        // load the Schema
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
            std::cout << "hello_who resolver called with args: " << args.dump(4) << std::endl;
            std::string my_who = args["who"].get<std::string>();
            std::string my_ret_val = std::format("Hello, {}!",my_who);
            return my_ret_val;
        });
        const auto myResult2=proc.load_schema(schema_str);
        REQUIRE(myResult2.is_success());
        const auto myReply= proc.execute(R"(query { hello_ping hello_who(who: "Josef") } )", "{}", true);
        for (int myIdx=0;auto& err : myReply.errors) {
            std::cerr << "error "<< std::setw(3) << ++myIdx <<"   |" << err.message << "| code="  << std::endl;
        }
        REQUIRE(myReply.is_success()) ;
        REQUIRE(myReply.data.is_object());
        std::cerr << "myReply.data: " << myReply.data.dump(4) << std::endl;
        std::cerr.flush();
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
        for (int myIdx=0;auto& err : myReply.errors) {
            std::cerr << "error "<< std::setw(3) << ++myIdx <<"   |" << err.message << "| code="  << std::endl << std::flush;
        }
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

        // Simple query with each argument type separately to verify they work
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
            log_result(reply);
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
            // Ensure the input actually matches the Document structure
            // e.g., " \"My Description\" type User { name: String } "
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
            if (!found) {
                std::cerr << "Expected description '" << expected_desc << "' not found in AST. Found " << descriptions.size() << " descriptions." << std::endl;
                for (const auto& d : descriptions) {
                    std::cerr << "  - |" << d << "|" << std::endl;
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

    TEST_CASE("GraphQL Introspection", "[gql][introspection]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        
        SECTION("Introspection without directives") {
            const std::string schema = R"(
                "User object"
                type User {
                    "User name"
                    name: String
                    age: Int
                }
                type Query {
                    me: User
                }
            )";
            proc.register_resolver({},"me", [](const json&, const json&, const ResolverCtx&)
                { return json::object(); });
            const auto loadResult = proc.load_schema(std::string(schema));
            log_result(loadResult);
            REQUIRE(loadResult.is_success());

            const auto reply = proc.execute("query { __schema { types { name description fields { name description type { name } } } } }","{}",true);
            log_result(reply);
            REQUIRE(reply.is_success());

            // T-INTRO-040: un-commented assertions
            json types = reply.data["__schema"]["types"];
            bool foundUser = false;
            for (const auto& type : types) {
                if (type["name"] == "User") {
                    foundUser = true;
                    REQUIRE(type["fields"].is_array());
                    bool foundName = false;
                    for (const auto& field : type["fields"]) {
                        if (field["name"] == "name") {
                            foundName = true;
                            REQUIRE(field["type"]["name"] == "String");
                        }
                    }
                    REQUIRE(foundName);
                }
            }
            REQUIRE(foundUser);
        }

        SECTION("Introspection with directives") {
            const std::string schema = R"(
                "Auth directive"
                directive @auth(role: String) on FIELD_DEFINITION
                
                type Query {
                    secret: String @auth(role: "admin")
                }
            )";
            proc.register_resolver({},"secret", [](const json&, const json&, const ResolverCtx&)
                { return "secret"; });
            const auto loadResult = proc.load_schema(schema,true);
            log_result(loadResult);
            REQUIRE(loadResult.is_success());

            const auto reply = proc.execute("query { __schema { directives { name args { name type { name } } } } }");
            log_result(reply);
            REQUIRE(reply.is_success());
            
            json directives = reply.data["__schema"]["directives"];
            bool foundAuth = false;
            for (const auto& dir : directives) {
                if (dir["name"] == "auth") {
                    foundAuth = true;
                    // REQUIRE(dir["description"] == "\"Auth directive\"");
                    REQUIRE(dir["args"].is_array());
                    REQUIRE(dir["args"].size() == 1);
                    REQUIRE(dir["args"][0]["name"] == "role");
                    REQUIRE(dir["args"][0]["type"]["name"] == "String");
                }
            }
            REQUIRE(foundAuth);
        }
    }
}

// ---------------------------------------------------------------------------
// T-EXEC-006: Sub-resolver dispatch correctness tests (Phase 1c)
// ---------------------------------------------------------------------------
namespace isched::v0_0_1::backend {

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
        for (const auto& e : reply.errors) std::cerr << "[sub-resolver] error: " << e.message << "\n";
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["player"]["name"] == "Alice");
        REQUIRE(reply.data["player"]["age"] == 30);
        // Parent forwarded correctly
        REQUIRE(captured_parent.value("name", "") == "Alice");
        REQUIRE(captured_parent.value("age", -1) == 30);
    }

    TEST_CASE("Sub-resolver: default field resolver extracts from parent", "[gql][executor][sub-resolver][default-resolver]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "srv_info_test", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"version", "1.0"}, {"name", "isched"}};
        });
        // No explicit resolver for {"srv_info_test","version"} or {"srv_info_test","name"} — default resolver used

        const auto load_res = proc.load_schema(
            "type Query { srv_info_test: InfoType } type InfoType { version: String name: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ srv_info_test { version name } }");
        for (const auto& e : reply.errors) std::cerr << "[default-resolver] error: " << e.message << "\n";
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["srv_info_test"]["version"] == "1.0");
        REQUIRE(reply.data["srv_info_test"]["name"] == "isched");
    }

    TEST_CASE("Sub-resolver: multi-level nesting produces correct JSON structure", "[gql][executor][sub-resolver][nesting]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        // Outer resolver returns nested object; default resolvers extract at every level
        proc.register_resolver({}, "a", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"b", {{"c", "deep_value"}}}};
        });

        const auto load_res = proc.load_schema(
            "type Query { a: AType } type AType { b: BType } type BType { c: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ a { b { c } } }");
        for (const auto& e : reply.errors) std::cerr << "[nesting] error: " << e.message << "\n";
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
        REQUIRE_FALSE(reply.is_success());           // has errors
        REQUIRE(reply.data["good"] == "good_value"); // sibling resolved
        REQUIRE(reply.data["bad"].is_null());         // failed field is null
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
        for (const auto& e : reply.errors) std::cerr << "[args] error: " << e.message << "\n";
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["container"]["greet"] == "Hello, World!");
    }

    TEST_CASE("Sub-resolver: missing resolver with absent parent key emits MISSING_GQL_RESOLVER", "[gql][executor][sub-resolver][missing]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());

        proc.register_resolver({}, "outer", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object(); // empty — no "x" key
        });
        // No resolver for {"outer","x"} and parent has no "x" key

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
            return json::object(); // no "missing" key
        });
        // No resolver for {"parent_field","missing"} and parent has no "missing" key

        const auto load_res = proc.load_schema(
            "type Query { parent_field: ParentType } type ParentType { missing: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ parent_field { missing } }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors) {
            if (e.code == EErrorCodes::MISSING_GQL_RESOLVER) {
                REQUIRE_FALSE(e.path.empty());
                // Last path element must be the string "missing"
                REQUIRE(std::holds_alternative<std::string>(e.path.back()));
                REQUIRE(std::get<std::string>(e.path.back()) == "missing");
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

} // namespace isched::v0_0_1::backend

// ---------------------------------------------------------------------------
// T010: Mutation operation dispatch tests
// ---------------------------------------------------------------------------
namespace isched::v0_0_1::backend {

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
        for (const auto& e : reply.errors) {
            std::cerr << "  error: " << e.message << std::endl;
        }
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["createItem"]["id"] == "item-1");
        REQUIRE(reply.data["createItem"]["name"] == "widget");
        REQUIRE(captured_name == "widget");
    }

    TEST_CASE("Schema introspection: mutationType name is present", "[gql][executor][introspection][mutation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute("query { __schema { mutationType { name } } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__schema"]["mutationType"]["name"] == "Mutation");
    }

    TEST_CASE("Variables: query with string variable", "[gql][executor][variables]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "greetVar", [](const json&, const json& args, const auto&) -> json {
            return "Hello, " + args.value("who", std::string{"world"}) + "!";
        });
        const auto load_res = proc.load_schema("type Query { greetVar(who: String): String }");
        REQUIRE(load_res.is_success());

        // Pass variable via the variables_json parameter
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

        // Variables JSON doesn't contain the variable used in query
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

// ---------------------------------------------------------------------------
// Phase 5b: Introspection completeness tests (T-INTRO-041 … T-INTRO-051)
// ---------------------------------------------------------------------------
namespace isched::v0_0_1::backend {

    // T-INTRO-041: __schema { types } contains all five built-in scalars
    TEST_CASE("Introspection: __schema types contains all built-in scalars",
              "[gql][introspection][T-INTRO-041]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(
            "query { __schema { types { name kind } } }");
        REQUIRE(reply.is_success());
        const json& types = reply.data["__schema"]["types"];
        REQUIRE(types.is_array());
        const std::vector<std::string> builtins{"String","Int","Float","Boolean","ID"};
        for (const auto& bname : builtins) {
            bool found = false;
            for (const auto& t : types) {
                if (t["name"] == bname) {
                    found = true;
                    REQUIRE(t["kind"] == "SCALAR");
                    break;
                }
            }
            INFO("Built-in scalar not found: " << bname);
            REQUIRE(found);
        }
    }

    // T-INTRO-042: __schema types contains user-defined OBJECT with fields
    TEST_CASE("Introspection: user-defined OBJECT type in __schema types",
              "[gql][introspection][T-INTRO-042]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "me", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object();
        });
        const auto load_res = proc.load_schema(R"(
            "A user in the system"
            type User { id: ID name: String }
            type Query { me: User }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            "query { __schema { types { name kind description fields { name type { name kind } } } } }");
        REQUIRE(reply.is_success());

        const json& types = reply.data["__schema"]["types"];
        bool found = false;
        for (const auto& t : types) {
            if (t["name"] == "User") {
                found = true;
                REQUIRE(t["kind"] == "OBJECT");
                REQUIRE(t["fields"].is_array());
                bool found_id = false, found_name = false;
                for (const auto& f : t["fields"]) {
                    if (f["name"] == "id")   { found_id = true;   REQUIRE(f["type"]["name"] == "ID"); }
                    if (f["name"] == "name") { found_name = true; REQUIRE(f["type"]["name"] == "String"); }
                }
                REQUIRE(found_id);
                REQUIRE(found_name);
            }
        }
        REQUIRE(found);
    }

    // T-INTRO-043: __schema types contains user-defined INPUT_OBJECT with inputFields
    TEST_CASE("Introspection: INPUT_OBJECT type has inputFields in __schema",
              "[gql][introspection][T-INTRO-043]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "addUser", [](const json&, const json&, const ResolverCtx&) -> json {
            return "ok";
        });
        const auto load_res = proc.load_schema(R"(
            input CreateUserInput { name: String email: String }
            type Query  { hello: String }
            type Mutation { addUser(input: CreateUserInput): String }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            "query { __schema { types { name kind inputFields { name type { name } } } } }");
        REQUIRE(reply.is_success());

        bool found = false;
        for (const auto& t : reply.data["__schema"]["types"]) {
            if (t["name"] == "CreateUserInput") {
                found = true;
                REQUIRE(t["kind"] == "INPUT_OBJECT");
                REQUIRE(t["inputFields"].is_array());
                bool found_name = false, found_email = false;
                for (const auto& f : t["inputFields"]) {
                    if (f["name"] == "name")  found_name  = true;
                    if (f["name"] == "email") found_email = true;
                }
                REQUIRE(found_name);
                REQUIRE(found_email);
            }
        }
        REQUIRE(found);
    }

    // T-INTRO-044: __schema types contains user-defined ENUM with enumValues
    TEST_CASE("Introspection: ENUM type has enumValues in __schema",
              "[gql][introspection][T-INTRO-044]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "status", [](const json&, const json&, const ResolverCtx&) -> json {
            return "ACTIVE";
        });
        const auto load_res = proc.load_schema(R"(
            enum UserStatus { ACTIVE INACTIVE SUSPENDED }
            type Query { status: UserStatus }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            "query { __schema { types { name kind enumValues { name } } } }");
        REQUIRE(reply.is_success());

        bool found = false;
        for (const auto& t : reply.data["__schema"]["types"]) {
            if (t["name"] == "UserStatus") {
                found = true;
                REQUIRE(t["kind"] == "ENUM");
                REQUIRE(t["enumValues"].is_array());
                std::vector<std::string> vals;
                for (const auto& ev : t["enumValues"]) vals.push_back(ev["name"].get<std::string>());
                REQUIRE(std::find(vals.begin(), vals.end(), "ACTIVE")    != vals.end());
                REQUIRE(std::find(vals.begin(), vals.end(), "INACTIVE")  != vals.end());
                REQUIRE(std::find(vals.begin(), vals.end(), "SUSPENDED") != vals.end());
            }
        }
        REQUIRE(found);
    }

    // T-INTRO-045: __type(name: "User") returns correct __Type
    TEST_CASE("Introspection: __type(name:) returns correct type for known object",
              "[gql][introspection][T-INTRO-045]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "me", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::object();
        });
        const auto load_res = proc.load_schema(R"(
            type User { id: ID name: String }
            type Query { me: User }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(
            R"(query { __type(name: "User") { name kind fields { name } } })");
        REQUIRE(reply.is_success());
        REQUIRE(!reply.data["__type"].is_null());
        REQUIRE(reply.data["__type"]["name"] == "User");
        REQUIRE(reply.data["__type"]["kind"] == "OBJECT");
        REQUIRE(reply.data["__type"]["fields"].is_array());
    }

    // T-INTRO-046: __type(name: "NonExistent") returns null without error
    TEST_CASE("Introspection: __type(name:) returns null for unknown type",
              "[gql][introspection][T-INTRO-046]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(
            R"(query { __type(name: "DoesNotExist") { name kind } })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__type"].is_null());
    }

    // T-INTRO-047: field typed [String!]! produces NON_NULL → LIST → NON_NULL → SCALAR chain
    TEST_CASE("Introspection: list-of-non-null field produces correct ofType chain",
              "[gql][introspection][T-INTRO-047]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "tags", [](const json&, const json&, const ResolverCtx&) -> json {
            return json::array({"a","b"});
        });
        const auto load_res = proc.load_schema(R"(
            type Query { tags: [String!]! }
        )");
        REQUIRE(load_res.is_success());

        // Introspect to get the field type chain for Query.tags
        const auto reply = proc.execute(R"(
            query {
                __type(name: "Query") {
                    fields {
                        name
                        type {
                            kind name
                            ofType { kind name
                                ofType { kind name
                                    ofType { kind name ofType { kind name } }
                                }
                            }
                        }
                    }
                }
            }
        )");
        REQUIRE(reply.is_success());
        REQUIRE(!reply.data["__type"].is_null());

        // Find the "tags" field
        const json& fields = reply.data["__type"]["fields"];
        bool found = false;
        for (const auto& f : fields) {
            if (f["name"] == "tags") {
                found = true;
                // Outermost: NON_NULL
                REQUIRE(f["type"]["kind"] == "NON_NULL");
                // Next: LIST
                REQUIRE(f["type"]["ofType"]["kind"] == "LIST");
                // Next: NON_NULL (the String! part)
                REQUIRE(f["type"]["ofType"]["ofType"]["kind"] == "NON_NULL");
                // Innermost: SCALAR String
                REQUIRE(f["type"]["ofType"]["ofType"]["ofType"]["kind"] == "SCALAR");
                REQUIRE(f["type"]["ofType"]["ofType"]["ofType"]["name"] == "String");
            }
        }
        REQUIRE(found);
    }

    // T-INTRO-048: __typename in nested selection set returns runtime type name
    TEST_CASE("Introspection: __typename in nested selection set returns type name",
              "[gql][introspection][T-INTRO-048]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "player", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"name","Alice"}};
        });
        const auto load_res = proc.load_schema(R"(
            type PlayerType { name: String }
            type Query { player: PlayerType }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ player { __typename name } }");
        for (const auto& e : reply.errors) std::cerr << "[__typename] error: " << e.message << "\n";
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["player"]["__typename"] == "PlayerType");
        REQUIRE(reply.data["player"]["name"] == "Alice");
    }

    // T-INTRO-049: __schema { directives } contains @skip, @include, @deprecated
    TEST_CASE("Introspection: __schema directives include skip, include, deprecated",
              "[gql][introspection][T-INTRO-049]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(R"(
            query { __schema { directives { name locations args { name type { kind name ofType { kind name } } } } } }
        )");
        REQUIRE(reply.is_success());
        const json& directives = reply.data["__schema"]["directives"];
        REQUIRE(directives.is_array());

        auto find_dir = [&](const std::string& name) -> const json* {
            for (const auto& d : directives)
                if (d["name"] == name) return &d;
            return nullptr;
        };

        // @skip
        const json* skip = find_dir("skip");
        REQUIRE(skip != nullptr);
        REQUIRE((*skip)["locations"].is_array());
        bool has_field_loc = false;
        for (const auto& loc : (*skip)["locations"])
            if (loc == "FIELD") has_field_loc = true;
        REQUIRE(has_field_loc);
        REQUIRE((*skip)["args"].is_array());
        REQUIRE((*skip)["args"].size() >= 1);
        REQUIRE((*skip)["args"][0]["name"] == "if");

        // @include
        REQUIRE(find_dir("include") != nullptr);

        // @deprecated
        const json* depr = find_dir("deprecated");
        REQUIRE(depr != nullptr);
        REQUIRE((*depr)["args"].is_array());
        REQUIRE((*depr)["args"].size() >= 1);
        REQUIRE((*depr)["args"][0]["name"] == "reason");
    }

    // T-INTRO-050: @deprecated on a field sets isDeprecated and deprecationReason
    TEST_CASE("Introspection: @deprecated directive sets isDeprecated and reason",
              "[gql][introspection][T-INTRO-050]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        proc.register_resolver({}, "me", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"old","x"},{"newField","y"}};
        });
        const auto load_res = proc.load_schema(R"(
            type User {
                old:      String @deprecated(reason: "Use newField instead")
                newField: String
            }
            type Query { me: User }
        )");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute(R"(
            query { __schema { types { name fields { name isDeprecated deprecationReason } } } }
        )");
        REQUIRE(reply.is_success());

        for (const auto& t : reply.data["__schema"]["types"]) {
            if (t["name"] == "User") {
                for (const auto& f : t["fields"]) {
                    if (f["name"] == "old") {
                        REQUIRE(f["isDeprecated"] == true);
                        REQUIRE(!f["deprecationReason"].is_null());
                        std::string reason = f["deprecationReason"].get<std::string>();
                        REQUIRE(reason.find("newField") != std::string::npos);
                    }
                    if (f["name"] == "newField") {
                        REQUIRE(f["isDeprecated"] == false);
                    }
                }
            }
        }
    }

    // T-INTRO-051: __schema { queryType { name } } returns "Query"
    TEST_CASE("Introspection: __schema queryType name is Query",
              "[gql][introspection][T-INTRO-051]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(
            "query { __schema { queryType { name } } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__schema"]["queryType"]["name"] == "Query");
    }

} // namespace isched::v0_0_1::backend

// ---------------------------------------------------------------------------
// T041: Query complexity and depth analysis tests
// ---------------------------------------------------------------------------
namespace isched::v0_0_1::backend {

    TEST_CASE("T041: simple query within limits succeeds", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_depth       = 5;
        cfg.max_complexity  = 20;
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);
        const auto reply = proc.execute("{ hello }");
        REQUIRE(reply.is_success());
    }

    TEST_CASE("T041: query exceeding max_depth is rejected", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_depth = 1; // depth-1: only root-level SelectionSet allowed
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);
        proc.register_resolver({}, "a", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"b","v"}};
        });
        const auto load_res = proc.load_schema(
            "type Query { a: AType } type AType { b: String }");
        REQUIRE(load_res.is_success());

        // Depth-2 query: { a { b } } — exceeds max_depth=1
        const auto reply = proc.execute("{ a { b } }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors)
            if (e.message.find("depth") != std::string::npos) { found = true; break; }
        REQUIRE(found);
    }

    TEST_CASE("T041: query exceeding max_complexity is rejected", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_complexity = 2; // only 2 fields allowed
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);

        // Three-field query — exceeds max_complexity=2
        const auto reply = proc.execute("{ hello version uptime }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors)
            if (e.message.find("complexity") != std::string::npos) { found = true; break; }
        REQUIRE(found);
    }

    TEST_CASE("T041: unlimited (0) depth/complexity imposes no restriction", "[gql][executor][T041][complexity]") {
        // Default config has max_depth=0, max_complexity=0 (unlimited)
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute("{ hello version uptime }");
        REQUIRE(reply.is_success());
    }

} // namespace isched::v0_0_1::backend
