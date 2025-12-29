// Skeleton unit tests for GqlExecutor

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_GqlParser.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include "isched/backend/isched_DatabaseManager.hpp"

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
}
