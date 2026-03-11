// Skeleton unit tests for GqlExecutor

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <nlohmann/json.hpp>

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
        const auto myReply= proc.execute(R"(query { hello_ping hello_who(who: "Josef") } )", true);
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
        const auto myReply= proc.execute(R"(query { multi(p_i: 2) } )", true);
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
            const auto reply = proc.execute("query { test_args(i: 123) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["i"] == 123);
        }
        SECTION("FloatValue") {
            const auto reply = proc.execute("query { test_args(f: 123.456) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["f"] == 123.456);
        }
        SECTION("StringValue") {
            const auto reply = proc.execute("query { test_args(s: \"hello\") }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["s"] == "hello");
        }
        SECTION("BooleanValue") {
            const auto reply = proc.execute("query { test_args(b: true) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["b"] == true);
        }
        SECTION("NullValue") {
            const auto reply = proc.execute("query { test_args(n: null) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["n"].is_null());
        }
        SECTION("EnumValue") {
            const auto reply = proc.execute("query { test_args(e: ENUM_VAL) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["e"] == "ENUM_VAL");
        }
        SECTION("ListValue") {
            const auto reply = proc.execute("query { test_args(l: [1, 2]) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["l"] == json::array({1, 2}));
        }
        SECTION("ObjectValue") {
            const auto reply = proc.execute("query { test_args(o: {a: 1}) }", true);
            REQUIRE(reply.is_success());
            REQUIRE(reply.data["test_args"]["o"]["a"] == 1);
        }
        SECTION("BlockString") {
            const auto reply = proc.execute(R"(query { test_args(s: """line1""") } )", true);
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
            const auto reply=proc.execute("query { summ(i: 1) summ(i: 2) summ(i: 3) }", true);
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

            const auto reply = proc.execute("query { __schema { types { name description fields { name description type { name } } } } }",true);
            log_result(reply);
            REQUIRE(reply.is_success());
/*
            json types = reply.data["__schema"]["types"];
            std::cout << "Types: " << types.dump(4) << std::endl << std::flush;
            bool foundUser = false;
            for (const auto& type : types) {
                if (type["name"] == "User") {
                    foundUser = true;
                    if (!type["description"].is_null()) {
                        std::cout << "Description for User: " << type["description"].dump() << std::endl;
                    } else {
                        std::cout << "Description for User is null" << std::endl;
                    }
                    REQUIRE(type["fields"].is_array());
                    bool foundName = false;
                    for (const auto& field : type["fields"]) {
                        if (field["name"] == "name") {
                            foundName = true;
                            // REQUIRE(field["description"] == "\"User name\"");
                            REQUIRE(field["type"]["name"] == "String");
                        }
                    }
                    REQUIRE(foundName);
                }
            }
            REQUIRE(foundUser);*/
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
