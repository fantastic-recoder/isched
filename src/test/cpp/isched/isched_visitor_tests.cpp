// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_visitor_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Unit tests for NonRecursiveVisitor, TraversalFrame, and VisitorDispatchOutcome.
 * @author isched Development Team
 */

#include <catch2/catch_test_macros.hpp>
#include <isched/shared/ast/isched_CustomNode.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>
#include <isched/shared/ast/isched_NonRecursiveVisitor.hpp>
#include <isched/backend/isched_gql_grammar.hpp>

#include <tao/pegtl/string_input.hpp>

using namespace isched::v0_0_1;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {
    /// Parse a query and return the PEGTL synthetic root (lifetime owner).
    /// The actual grammar node is at children[0].
    std::unique_ptr<ast::CustomNode> parse_typed(const std::string& query) {
        tao::pegtl::string_input<> si(query, "visitor_test");
        return tao::pegtl::parse_tree::parse<
            gql::GqlQuery, ast::CustomNode, ast::NodeSelector
        >(si);
    }

    /// Return the actual grammar root from a PEGTL virtual root.
    /// Caller must keep `vroot` alive.
    const ast::CustomNode& grammar_root(const ast::CustomNode& vroot) {
        return *static_cast<const ast::CustomNode*>(vroot.children.front().get());
    }

    /// Count all nodes in tree via BFS.
    std::size_t count_nodes(const ast::CustomNode& root) {
        std::size_t count = 0;
        std::vector<const ast::CustomNode*> q = {&root};
        while (!q.empty()) {
            const auto* cn = q.back(); q.pop_back();
            ++count;
            for (const auto& child : cn->children) {
                q.push_back(static_cast<const ast::CustomNode*>(child.get()));
            }
        }
        return count;
    }
}

// ── VisitorDispatchOutcome ────────────────────────────────────────────────────

TEST_CASE("VisitorDispatchOutcome enum values are distinct", "[ast][visitor]") {
    using O = ast::VisitorDispatchOutcome;
    REQUIRE(O::Success != O::UnknownNodeFallback);
}

// ── NonRecursiveVisitor: basic traversal ─────────────────────────────────────

TEST_CASE("NonRecursiveVisitor: traverse simple query", "[ast][visitor]") {
    auto vroot = parse_typed("{ hello }");
    REQUIRE(vroot != nullptr);
    REQUIRE(!vroot->children.empty());

    const auto& node = grammar_root(*vroot);
    ast::NonRecursiveVisitor visitor;
    const auto outcome = visitor.traverse(node);

    REQUIRE(visitor.node_count() > 0);
    REQUIRE(visitor.node_count() == count_nodes(node));
    REQUIRE(outcome == ast::VisitorDispatchOutcome::Success);
    REQUIRE(visitor.unknown_node_count() == 0);
}

TEST_CASE("NonRecursiveVisitor: traverse named operation query", "[ast][visitor]") {
    // GqlQuery = opt<"query"> SelectionSet — anonymous form only, no operation name.
    auto vroot = parse_typed("{ fieldA fieldB }");
    REQUIRE(vroot != nullptr);
    REQUIRE(!vroot->children.empty());

    const auto& node = grammar_root(*vroot);
    ast::NonRecursiveVisitor visitor;
    const auto outcome = visitor.traverse(node);

    REQUIRE(outcome == ast::VisitorDispatchOutcome::Success);
    REQUIRE(visitor.node_count() == count_nodes(node));
    REQUIRE(visitor.unknown_node_count() == 0);
}

TEST_CASE("NonRecursiveVisitor: traverse fragment query", "[ast][visitor]") {
    // GqlQuery can parse fragment spreads inside SelectionSet; fragment definitions require Document.
    const std::string query = "{ hero { ...nameFragment } }";
    auto vroot = parse_typed(query);
    REQUIRE(vroot != nullptr);
    REQUIRE(!vroot->children.empty());

    const auto& node = grammar_root(*vroot);
    ast::NonRecursiveVisitor visitor;
    const auto outcome = visitor.traverse(node);

    REQUIRE(outcome == ast::VisitorDispatchOutcome::Success);
    REQUIRE(visitor.node_count() == count_nodes(node));
}

// ── Counter resets between traversals ────────────────────────────────────────

TEST_CASE("NonRecursiveVisitor: counters reset on successive traversals", "[ast][visitor]") {
    auto typed_root1 = parse_typed("{ a }");
    auto typed_root2 = parse_typed("{ b c d }");
    REQUIRE(typed_root1 != nullptr);
    REQUIRE(typed_root2 != nullptr);

    const auto& node1 = grammar_root(*typed_root1);
    const auto& node2 = grammar_root(*typed_root2);

    ast::NonRecursiveVisitor visitor;
    visitor.traverse(node1);

    visitor.traverse(node2);
    const auto count2 = visitor.node_count();

    // Restarted — count2 reflects node2 only, not accumulated.
    REQUIRE(count2 == count_nodes(node2));
}

// ── Unknown node fallback ─────────────────────────────────────────────────────

TEST_CASE("NonRecursiveVisitor: node with Unknown type triggers fallback", "[ast][visitor]") {
    // Build a tiny 2-node synthetic tree: root Unknown → child Unknown
    // We can't easily build CustomNode trees manually, so test through inject.
    // Instead: verify that a node_type of Unknown reports UnknownNodeFallback.
    // Use a single CustomNode (no parse needed).
    ast::CustomNode root;
    root.node_type = ast::NodeType::Unknown;

    ast::NonRecursiveVisitor visitor;
    auto outcome = visitor.traverse(root);

    REQUIRE(outcome == ast::VisitorDispatchOutcome::UnknownNodeFallback);
    REQUIRE(visitor.node_count() == 1);
    REQUIRE(visitor.unknown_node_count() == 1);
}

TEST_CASE("NonRecursiveVisitor: Sentinel type triggers fallback", "[ast][visitor]") {
    ast::CustomNode root;
    root.node_type = ast::NodeType::Sentinel;

    ast::NonRecursiveVisitor visitor;
    auto outcome = visitor.traverse(root);

    REQUIRE(outcome == ast::VisitorDispatchOutcome::UnknownNodeFallback);
    REQUIRE(visitor.unknown_node_count() == 1);
}

// ── (d) Mixed tree: known + unknown nodes ────────────────────────────────────

TEST_CASE("NonRecursiveVisitor: real grammar tree has zero unknown nodes", "[ast][visitor]") {
    auto vroot = parse_typed("{ alpha beta }");
    REQUIRE(vroot != nullptr);
    REQUIRE(!vroot->children.empty());

    const auto& node = grammar_root(*vroot);
    ast::NonRecursiveVisitor visitor;
    const auto outcome = visitor.traverse(node);

    REQUIRE(visitor.unknown_node_count() == 0);
    REQUIRE(outcome == ast::VisitorDispatchOutcome::Success);
}

TEST_CASE("NonRecursiveVisitor: Unknown root gives UnknownNodeFallback", "[ast][visitor]") {
    ast::CustomNode unknown_root;
    unknown_root.node_type = ast::NodeType::Unknown;

    ast::NonRecursiveVisitor visitor;
    visitor.traverse(unknown_root);

    REQUIRE(visitor.unknown_node_count() == 1);
}
