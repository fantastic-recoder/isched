// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_grammar.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Helper implementations for the PEGTL GraphQL grammar.
 *
 * Contains `ast_node_to_str()` and related utilities that convert PEGTL
 * parse-tree nodes to string representations, used in tests and debugging.
 */

#include "isched_gql_grammar.hpp"

#include <expected>
#include <string>
#include <vector>

#include "isched_gql_error.hpp"
#include "isched_gql_grammar.hpp"

namespace isched::v0_0_1::gql {

    auto ast_node_to_str(const TAstNodePtr &p_node) -> TExpectedStr {
        if (!p_node) {
            return "";
        }

        std::string result;

        // Nodes that have their content stored (leaf nodes in the selector)
        if (p_node->has_content()) {
            return std::string(p_node->string_view());
        }

        // For non-leaf nodes, we recursively build the string from children
        for (const auto& child : p_node->children) {
            auto child_str = ast_node_to_str(child);
            if (!child_str) {
                return child_str;
            }
            result += *child_str;
        }

        return result;
    }

    TAstNodePtr merge_type_definitions(TAstNodePtr &&p_schema_node, TAstNodePtr &&p_type_defs_node) {
        if (!p_schema_node) {
            return std::move(p_type_defs_node);
        }
        if (!p_type_defs_node) {
            return std::move(p_schema_node);
        }

        // If both are Documents, we merge their children.
        // In our grammar, Document -> seq<IgnoredMany, plus<seq<Definition, IgnoredMany>>, eof>
        // Depending on how PEGTL's parse_tree stores it, we might have Children directly under Document.

        // Actually, let's just append children from p_type_defs_node to p_schema_node
        // if they are both the same type, or if it makes sense to merge them.
        
        // Move children from p_type_defs_node to p_schema_node
        for (auto& child : p_type_defs_node->children) {
            p_schema_node->children.push_back(std::move(child));
        }

        return std::move(p_schema_node);
    }

    void dump_ast_recursive(const TAstNodePtr& p_node, std::string& p_result, int p_indent) {
        if (!p_node) return;

        for (int i = 0; i < p_indent; ++i) p_result += (i==0 ? "|--" : "---" );
        
        p_result += p_node->type;
        if (p_node->has_content()) {
            p_result += " (\"" + std::string(p_node->string_view()) + "\")";
        }
        p_result += "|\n";

        for (const auto& child : p_node->children) {
            dump_ast_recursive(child, p_result, p_indent + 1);
        }
    }

    std::string dump_ast(const TAstNodePtr& ast) {
        std::string result;
        dump_ast_recursive(ast, result, 1);
        return result;
    }
}
