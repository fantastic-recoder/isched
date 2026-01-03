//
// Created by groby on 2026-01-03.
//

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
}
