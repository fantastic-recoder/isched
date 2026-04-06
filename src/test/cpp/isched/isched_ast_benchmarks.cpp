// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_ast_benchmarks.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Advisory performance benchmark comparing enum-dispatch vs. string-based
 *        parse-tree traversal.  Results are informational; no hard ratio gate.
 * @author isched Development Team
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <isched/shared/ast/isched_CustomNode.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>
#include <isched/shared/ast/isched_NodeSelector.hpp>
#include <isched/shared/ast/isched_NonRecursiveVisitor.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include <tao/pegtl/string_input.hpp>

#include <string>
#include <string_view>

using namespace isched::v0_0_1;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {
    // A moderately complex anonymous shorthand query used as benchmark input.
    // GqlQuery = opt<"query"> SelectionSet — use anonymous form (no name, no variables).
    constexpr std::string_view k_bench_query = R"(
        {
            user {
                id
                name
                email
                posts {
                    id
                    title
                    content
                    tags {
                        id
                        label
                    }
                }
                friends {
                    id
                    name
                }
            }
        }
    )";

    /// Count nodes in a typed tree using raw string comparison (legacy approach simulation).
    /// Simulates the old pattern: if (node->type == "isched::...::Field") { ... }
    std::size_t count_nodes_by_string(
        const tao::pegtl::parse_tree::node& root) {
        std::size_t count = 0;
        std::vector<const tao::pegtl::parse_tree::node*> q = {&root};
        while (!q.empty()) {
            const auto* n = q.back(); q.pop_back();
            ++count;
            // Simulate string-based type dispatch by doing a string comparison.
            if (n->type == "isched::v0_0_1::gql::Field") { /* handle Field */ }
            else if (n->type == "isched::v0_0_1::gql::SelectionSet") { /* handle SelectionSet */ }
            else if (n->type == "isched::v0_0_1::gql::OperationDefinition") { /* handle op */ }
            // (other rules would follow in real dispatch code)
            for (const auto& child : n->children) {
                q.push_back(child.get());
            }
        }
        return count;
    }

    /// Count nodes in a typed tree using NodeType enum dispatch.
    std::size_t count_nodes_by_enum(const ast::CustomNode& root) {
        ast::NonRecursiveVisitor visitor;
        visitor.traverse(root);
        return visitor.node_count();
    }
}

// ── Baseline advisory benchmarks ─────────────────────────────────────────────

TEST_CASE("ADVISORY: enum-dispatch vs string-dispatch parse-tree traversal", "[ast][benchmark][advisory]") {

    SECTION("baseline measurements (advisory only — no hard ratio gate)") {
        // Build both tree variants once outside the hot loop.

        // Typed (enum-dispatch) tree
        auto build_typed = [&]() -> std::unique_ptr<ast::CustomNode> {
            tao::pegtl::string_input<> si(std::string(k_bench_query), "bench");
            return tao::pegtl::parse_tree::parse<
                gql::GqlQuery, ast::CustomNode, ast::NodeSelector
            >(si);
        };

        // Legacy (string-dispatch) tree via GqlSelector / generate_ast_and_log
        auto build_legacy = [&]() -> std::unique_ptr<tao::pegtl::parse_tree::node> {
            tao::pegtl::string_input<> si(std::string(k_bench_query), "bench");
            auto [ok, root] = gql::generate_ast_and_log<gql::GqlQuery>(si, "bench");
            (void)ok;
            return std::move(root);
        };

        auto typed_root  = build_typed();
        auto legacy_root = build_legacy();
        REQUIRE(typed_root  != nullptr);
        REQUIRE(legacy_root != nullptr);
        REQUIRE(!typed_root->children.empty());
        REQUIRE(!legacy_root->children.empty());

        // Unwrap the PEGTL synthetic root: actual grammar node is children[0].
        // Both trees are CustomNode instances (generate_ast_and_log uses CustomNode internally).
        const auto& typed_node  = *static_cast<const ast::CustomNode*>(typed_root->children[0].get());
        const auto& legacy_node = *legacy_root->children[0].get();  // base node type for string dispatch

        // Sanity: both trees hold the same number of nodes.
        const std::size_t typed_count  = count_nodes_by_enum(typed_node);
        const std::size_t legacy_count = count_nodes_by_string(legacy_node);
        INFO("Typed tree nodes:  " << typed_count);
        INFO("Legacy tree nodes: " << legacy_count);
        // Both traversals should visit the same tree (same retention rules).
        REQUIRE(typed_count == legacy_count);

        BENCHMARK("enum-dispatch traversal") {
            return count_nodes_by_enum(typed_node);
        };

        BENCHMARK("string-dispatch traversal (legacy baseline)") {
            return count_nodes_by_string(legacy_node);
        };

        // Advisory message — no hard assertion on ratio.
        SUCCEED("Benchmark measurements completed. Inspect output for ratio advisory.");
    }
}
