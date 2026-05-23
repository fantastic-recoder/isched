// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_introspection_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for GraphQL introspection (types, fields, directives, UNION, and __schema queries).
 */

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <nlohmann/json.hpp>
#include <variant>
#include <vector>
#include <algorithm>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_DatabaseManager.hpp>
#include "isched/backend/isched_log_result.hpp"

using nlohmann::json;

namespace isched::v0_0_1::backend {

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
            REQUIRE(loadResult.is_success());

            const auto reply = proc.execute("query { __schema { types { name description fields { name description type { name } } } } }","{}",true);
            REQUIRE(reply.is_success());

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
            REQUIRE(loadResult.is_success());

            const auto reply = proc.execute("query { __schema { directives { name args { name type { name } } } } }");
            REQUIRE(reply.is_success());
            
            json directives = reply.data["__schema"]["directives"];
            bool foundAuth = false;
            for (const auto& dir : directives) {
                if (dir["name"] == "auth") {
                    foundAuth = true;
                    REQUIRE(dir["args"].is_array());
                    REQUIRE(dir["args"].size() == 1);
                    REQUIRE(dir["args"][0]["name"] == "role");
                    REQUIRE(dir["args"][0]["type"]["name"] == "String");
                }
            }
            REQUIRE(foundAuth);
        }
    }

    TEST_CASE("Schema introspection: mutationType name is present", "[gql][executor][introspection][mutation]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute("query { __schema { mutationType { name } } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__schema"]["mutationType"]["name"] == "Mutation");
    }

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

    TEST_CASE("Introspection: __type(name:) returns null for unknown type",
              "[gql][introspection][T-INTRO-046]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(
            R"(query { __type(name: "DoesNotExist") { name kind } })");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__type"].is_null());
    }

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

        const json& fields = reply.data["__type"]["fields"];
        bool found = false;
        for (const auto& f : fields) {
            if (f["name"] == "tags") {
                found = true;
                REQUIRE(f["type"]["kind"] == "NON_NULL");
                REQUIRE(f["type"]["ofType"]["kind"] == "LIST");
                REQUIRE(f["type"]["ofType"]["ofType"]["kind"] == "NON_NULL");
                REQUIRE(f["type"]["ofType"]["ofType"]["ofType"]["kind"] == "SCALAR");
                REQUIRE(f["type"]["ofType"]["ofType"]["ofType"]["name"] == "String");
            }
        }
        REQUIRE(found);
    }

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
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["player"]["__typename"] == "PlayerType");
        REQUIRE(reply.data["player"]["name"] == "Alice");
    }

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

        REQUIRE(find_dir("include") != nullptr);

        const json* depr = find_dir("deprecated");
        REQUIRE(depr != nullptr);
        REQUIRE((*depr)["args"].is_array());
        REQUIRE((*depr)["args"].size() >= 1);
        REQUIRE((*depr)["args"][0]["name"] == "reason");
    }

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

    TEST_CASE("Introspection: __schema queryType name is Query",
              "[gql][introspection][T-INTRO-051]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute(
            "query { __schema { queryType { name } } }");
        REQUIRE(reply.is_success());
        REQUIRE(reply.data["__schema"]["queryType"]["name"] == "Query");
    }

    TEST_CASE("Union type introspection", "[gql][introspection][union]") {
        GqlExecutor union_proc(std::make_shared<backend::DatabaseManager>());
        const std::string schema = R"(
            type Cat {
                name: String
            }
            type Dog {
                name: String
            }
            union Pet = Cat | Dog
            type Query {
                myPet: Pet
            }
        )";
        union_proc.register_resolver({}, "myPet", [](const json&, const json&, const ResolverCtx&) {
            return json::object();
        });
        auto loadResult = union_proc.load_schema(schema);
        REQUIRE(loadResult.is_success());

        auto res = union_proc.execute(R"(
            query {
                __type(name: "Pet") {
                    name
                    kind
                    possibleTypes {
                      name
                    }
                }
            }
        )");
        REQUIRE(res.is_success());
        auto t = res.data["__type"];
        REQUIRE(t["name"] == "Pet");
        REQUIRE(t["kind"] == "UNION");
        REQUIRE(t["possibleTypes"].is_array());
        REQUIRE(t["possibleTypes"].size() == 2);
        bool foundCat = false, foundDog = false;
        for (const auto& pt : t["possibleTypes"]) {
            if (pt["name"] == "Cat") foundCat = true;
            if (pt["name"] == "Dog") foundDog = true;
        }
        REQUIRE(foundCat);
        REQUIRE(foundDog);
    }

} // namespace isched::v0_0_1::backend
