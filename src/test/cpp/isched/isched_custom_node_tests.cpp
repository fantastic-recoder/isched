// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_custom_node_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for CustomNode, NodeType enum, and NodeSelector.
 * @author isched Development Team
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/shared/ast/isched_CustomNode.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include <tao/pegtl/string_input.hpp>

using namespace isched::v0_0_1;

// ── NodeType enum ─────────────────────────────────────────────────────────────

TEST_CASE("NodeType enum zero-initialises to Unknown", "[ast][nodetype]") {
    ast::NodeType nt{};
    REQUIRE(nt == ast::NodeType::Unknown);
}

TEST_CASE("NodeType enum values are distinct", "[ast][nodetype]") {
    REQUIRE(ast::NodeType::Unknown     != ast::NodeType::Document);
    REQUIRE(ast::NodeType::Document    != ast::NodeType::Field);
    REQUIRE(ast::NodeType::Sentinel    != ast::NodeType::Unknown);
}

// ── CustomNode struct ─────────────────────────────────────────────────────────

TEST_CASE("CustomNode default-constructs with NodeType::Unknown", "[ast][customnode]") {
    ast::CustomNode node;
    REQUIRE(node.node_type == ast::NodeType::Unknown);
}

TEST_CASE("CustomNode is assignable to node_type", "[ast][customnode]") {
    ast::CustomNode node;
    node.node_type = ast::NodeType::Field;
    REQUIRE(node.node_type == ast::NodeType::Field);
}

// ── Parse tree integration: NodeSelector assigns NodeType ────────────────────

TEST_CASE("NodeSelector: first child has NodeType::GqlQuery for simple query", "[ast][nodeselector]") {
    // parse_tree::parse returns a synthetic virtual root; the actual grammar node
    // is children[0].
    tao::pegtl::string_input<> si("{ hello }", "test");
    auto typed_root = tao::pegtl::parse_tree::parse<
        gql::GqlQuery, ast::CustomNode, ast::NodeSelector
    >(si);
    REQUIRE(typed_root != nullptr);
    REQUIRE(!typed_root->children.empty());
    const auto* gql_node = static_cast<const ast::CustomNode*>(typed_root->children[0].get());
    REQUIRE(gql_node->node_type == ast::NodeType::GqlQuery);
}

TEST_CASE("NodeSelector: children inherit correct NodeType", "[ast][nodeselector]") {
    // GqlQuery = opt<"query"> GqlSubQuery (GqlSubQuery : SelectionSet).
    // GqlSubQuery is not retained (not in selector), so Field nodes float up to GqlQuery.
    tao::pegtl::string_input<> si("{ hello }", "test");
    auto typed_root = tao::pegtl::parse_tree::parse<
        gql::GqlQuery, ast::CustomNode, ast::NodeSelector
    >(si);
    REQUIRE(typed_root != nullptr);

    bool found_field = false;
    bool found_name = false;

    // children[0] of the virtual root is the GqlQuery node.
    // GqlQuery directly contains Field nodes (SelectionSet/GqlSubQuery is not retained).
    REQUIRE(!typed_root->children.empty());
    const auto& gql_node = *static_cast<const ast::CustomNode*>(typed_root->children[0].get());
    for (const auto& child : gql_node.children) {
        const auto* cn = static_cast<const ast::CustomNode*>(child.get());
        if (cn->node_type == ast::NodeType::Field) {
            found_field = true;
            for (const auto& grandchild : cn->children) {
                const auto* gcn = static_cast<const ast::CustomNode*>(grandchild.get());
                if (gcn->node_type == ast::NodeType::Name) {
                    found_name = true;
                }
            }
        }
    }

    REQUIRE(found_field);
    REQUIRE(found_name);
}

TEST_CASE("NodeSelector: no retained node has NodeType::Sentinel", "[ast][nodeselector]") {
    tao::pegtl::string_input<> si("{ hello world }", "test");
    auto typed_root = tao::pegtl::parse_tree::parse<
        gql::GqlQuery, ast::CustomNode, ast::NodeSelector
    >(si);
    REQUIRE(typed_root != nullptr);

    // BFS over all nodes: none should be Sentinel
    std::vector<const ast::CustomNode*> queue;
    queue.push_back(typed_root.get());
    while (!queue.empty()) {
        const auto* current = queue.back();
        queue.pop_back();
        CHECK(current->node_type != ast::NodeType::Sentinel);
        for (const auto& child : current->children) {
            queue.push_back(static_cast<const ast::CustomNode*>(child.get()));
        }
    }
}

TEST_CASE("generate_ast_and_log returns CustomNode tree (backward compat)", "[ast][grammar]") {
    // The public API returns std::unique_ptr<node>; verify the root is actually a CustomNode.
    tao::pegtl::string_input<> si("{ ping }", "test");
    auto [ok, root] = gql::generate_ast_and_log<gql::GqlQuery>(si, "test");
    REQUIRE(ok);
    REQUIRE(root != nullptr);
    // root is the virtual container; children[0] is the actual GqlQuery node.
    REQUIRE(!root->children.empty());
    const auto* cn = static_cast<const ast::CustomNode*>(root->children[0].get());
    REQUIRE(cn->node_type == ast::NodeType::GqlQuery);
}
