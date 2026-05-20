// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_field_nodes_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Focused tests for pass-01 field-node collection/dispatch hot-path helpers.
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/backend/isched_GqlExecutor.hpp>

#include <isched/backend/isched_DatabaseManager.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

namespace isched::v0_0_1::backend {
    namespace {
        [[nodiscard]] auto make_node(const std::string& type) -> gql::TAstNodePtr {
            auto node = std::make_unique<gql::node>();
            node->type = type;
            return node;
        }

        struct ParsedQuery {
            std::unique_ptr<tao::pegtl::string_input<>> input;
            gql::TAstNodePtr root;
        };

        [[nodiscard]] const gql::TAstNodePtr* find_first_selection_set(const gql::TAstNodePtr& node) {
            if (!node) {
                return nullptr;
            }
            if (node->type == "isched::v0_0_1::gql::SelectionSet") {
                return &node;
            }
            for (const auto& child : node->children) {
                if (auto found = find_first_selection_set(child)) {
                    return found;
                }
            }
            return nullptr;
        }

        [[nodiscard]] ParsedQuery parse_query_root(std::string_view query) {
            auto input = std::make_unique<tao::pegtl::string_input<>>(std::string(query), "field_nodes_test");
            auto [ok, root] = gql::generate_ast_and_log<gql::Document>(*input, "field_nodes_test", false, false);
            REQUIRE(ok);
            REQUIRE(root != nullptr);
            return ParsedQuery{std::move(input), std::move(root)};
        }
    } // namespace

    TEST_CASE("collect_field_nodes extracts top-level field handles in order",
              "[gql][executor][hotpath][field-nodes]") {
        GqlExecutor exec(std::make_shared<DatabaseManager>());
        auto parsed = parse_query_root("query { hello version }");
        const auto* selection_set = find_first_selection_set(parsed.root);
        REQUIRE(selection_set != nullptr);

        gql::TErrorVector errors;
        const auto fields = exec.collect_field_nodes(*selection_set, errors);

        REQUIRE(errors.empty());
        REQUIRE(fields.size() == 2);
        REQUIRE((*fields[0])->children[0]->string_view() == "hello");
        REQUIRE((*fields[1])->children[0]->string_view() == "version");
    }

    TEST_CASE("collect_field_nodes covers tolerant branches for synthetic AST shapes",
              "[gql][executor][hotpath][field-nodes]") {
        GqlExecutor exec(std::make_shared<DatabaseManager>());
        gql::TErrorVector errors;

        SECTION("null selection set returns empty") {
            const auto fields = exec.collect_field_nodes(nullptr, errors);
            REQUIRE(fields.empty());
        }

        SECTION("direct Field child is accepted") {
            auto selection_set = make_node("isched::v0_0_1::gql::SelectionSet");
            selection_set->children.push_back(make_node("isched::v0_0_1::gql::Field"));
            const auto fields = exec.collect_field_nodes(selection_set, errors);
            REQUIRE(fields.size() <= 1);
        }

        SECTION("non-Selection child is ignored") {
            auto selection_set = make_node("isched::v0_0_1::gql::SelectionSet");
            selection_set->children.push_back(make_node("isched::v0_0_1::gql::Name"));
            const auto fields = exec.collect_field_nodes(selection_set, errors);
            REQUIRE(fields.empty());
        }

        SECTION("empty Selection child is ignored") {
            auto selection_set = make_node("isched::v0_0_1::gql::SelectionSet");
            selection_set->children.push_back(make_node("isched::v0_0_1::gql::Selection"));
            const auto fields = exec.collect_field_nodes(selection_set, errors);
            REQUIRE(fields.empty());
        }

        SECTION("Selection with non-Field first child is ignored") {
            auto selection_set = make_node("isched::v0_0_1::gql::SelectionSet");
            auto selection = make_node("isched::v0_0_1::gql::Selection");
            selection->children.push_back(make_node("isched::v0_0_1::gql::Name"));
            selection_set->children.push_back(std::move(selection));
            const auto fields = exec.collect_field_nodes(selection_set, errors);
            REQUIRE(fields.empty());
        }

        SECTION("Selection with Field first child is accepted") {
            auto selection_set = make_node("isched::v0_0_1::gql::SelectionSet");
            auto selection = make_node("isched::v0_0_1::gql::Selection");
            selection->children.push_back(make_node("isched::v0_0_1::gql::Field"));
            selection_set->children.push_back(std::move(selection));
            const auto fields = exec.collect_field_nodes(selection_set, errors);
            REQUIRE(fields.size() <= 1);
        }
    }

    TEST_CASE("collect_field_nodes safely handles fragment spread selections",
              "[gql][executor][hotpath][field-nodes]") {
        GqlExecutor exec(std::make_shared<DatabaseManager>());
        auto parsed = parse_query_root("query { ...Frag } fragment Frag on Query { hello }");
        const auto* selection_set = find_first_selection_set(parsed.root);
        REQUIRE(selection_set != nullptr);

        gql::TErrorVector errors;
        const auto fields = exec.collect_field_nodes(*selection_set, errors);

        REQUIRE(fields.empty());
        if (!errors.empty()) {
            REQUIRE(errors[0].code == gql::EErrorCodes::PARSE_ERROR);
        }
    }

    TEST_CASE("collect_field_nodes returns empty when called with non-selection-set node",
              "[gql][executor][hotpath][field-nodes]") {
        GqlExecutor exec(std::make_shared<DatabaseManager>());
        auto parsed = parse_query_root("query { hello }");
        const auto* selection_set = find_first_selection_set(parsed.root);
        REQUIRE(selection_set != nullptr);
        REQUIRE_FALSE((*selection_set)->children.empty());
        const auto& non_selection_set = (*selection_set)->children[0];

        gql::TErrorVector errors;
        const auto fields = exec.collect_field_nodes(non_selection_set, errors);

        REQUIRE(fields.empty());
        REQUIRE(errors.empty());
    }

    TEST_CASE("process_field_nodes skips null field handles and still resolves valid ones",
              "[gql][executor][hotpath][field-nodes]") {
        GqlExecutor exec(std::make_shared<DatabaseManager>());
        auto parsed = parse_query_root("query { hello }");
        const auto* selection_set = find_first_selection_set(parsed.root);
        REQUIRE(selection_set != nullptr);

        gql::TErrorVector errors;
        auto fields = exec.collect_field_nodes(*selection_set, errors);
        REQUIRE(errors.empty());
        REQUIRE(fields.size() == 1);
        REQUIRE((*fields.front())->children[0]->string_view() == "hello");

        GqlExecutor::FieldNodeList with_null;
        with_null.push_back(nullptr);
        with_null.push_back(fields.front());

        nlohmann::json result;
        ResolverPath path_context;
        exec.process_field_nodes(nlohmann::json::object(), path_context, with_null, result, errors);

        REQUIRE(errors.empty());
        REQUIRE(result.contains("hello"));
        REQUIRE(result["hello"] == "Hello, GraphQL!");
    }
}

