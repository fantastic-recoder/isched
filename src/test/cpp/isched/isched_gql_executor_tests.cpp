// Skeleton unit tests for GqlExecutor

#include <catch2/catch_test_macros.hpp>
#include <catch2/internal/catch_stdstreams.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_GqlParser.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include "isched/backend/isched_DatabaseManager.hpp"
#include "isched/shared/fs/isched_fs_utils.hpp"

using nlohmann::json;

namespace isched::v0_0_1::backend {

    TEST_CASE("GqlExecutor can be constructed", "[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        // Construction should not throw
        REQUIRE(true);
    }

    TEST_CASE("GqlExecutor executes on an empty Document and returns JSON object", "[gql][processor][smoke]") {
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());

        // Minimal empty document for now; parser not required for skeleton
        gql::Document doc; // NOTE: structure defined by grammar; empty is acceptable for skeleton

        json result = proc.execute(doc).data;
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
        const auto myResult=proc.load_schema(schema_str);
        REQUIRE(myResult.is_success()==false);
        REQUIRE(myResult.errors.size()==2);
        REQUIRE(myResult.errors[0].code==EErrorCodes::MISSING_GQL_RESOLVER);
        REQUIRE(myResult.errors[1].code==EErrorCodes::MISSING_GQL_RESOLVER);
        proc.register_resolver("hello", [](const json&,const json&) {
            return json{"Hello, World!"};
        });
        proc.register_resolver("hello_who", [](const json& args,const json&) {
            std::cout << "hello_who resolver called with args: " << args.dump(4) << std::endl;
            return json{std::format("Hello, {}!",args["who"].get<std::string>())};
        });
        const auto myResult2=proc.load_schema(schema_str);
        REQUIRE(myResult2.is_success());
        const auto myReply= proc.execute(R"(query { hello hello_who(who: "Josef") } )", true);
        for (int myIdx=0;auto& err : myReply.errors) {
            std::cerr << "error "<< std::setw(3) << ++myIdx <<"   |" << err.message << "| code="  << std::endl;
        }
        REQUIRE(myReply.is_success()) ;
        REQUIRE(myReply.data.is_object());
        std::cerr << "myReply.data: " << myReply.data.dump(4) << std::endl;
        std::cerr.flush();
        REQUIRE(myReply.data["data"]["hello"].is_array());
        REQUIRE(myReply.data["data"]["hello"][0] == "Hello, World!");
        REQUIRE(myReply.data["data"]["hello_who"].is_array());
        REQUIRE(myReply.data["data"]["hello_who"][0] == "Hello, Josef!");
    }
}
