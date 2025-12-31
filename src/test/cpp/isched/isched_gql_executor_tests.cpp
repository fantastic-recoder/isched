// Skeleton unit tests for GqlExecutor

#include <catch2/catch_test_macros.hpp>
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
        // Adjust expectations in future when execute() gains behavior.
        REQUIRE(result.is_null());
    }

    TEST_CASE("Test hello world query","[gql][processor][smoke]") {
        // load the Schema
        std::string schema_str = fsutils::read_file("hello_world_schema.graphql");
        REQUIRE(!schema_str.empty());
        GqlExecutor proc(std::make_shared<backend::DatabaseManager>());
        const auto myResult=proc.load_schema(std::move(schema_str));
        REQUIRE(!myResult.is_success());
        REQUIRE(myResult.errors.size()==2);
        REQUIRE(myResult.errors[0].code==EErrorCodes::MISSING_GQL_RESOLVER);
        REQUIRE(myResult.errors[1].code==EErrorCodes::MISSING_GQL_RESOLVER);
    }
}
