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

    TExpectedStr ast_node_to_str(const TAstNodePtr &p_node) {
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
            
            // We might need to add spaces between some nodes if they are not captured in the AST
            // However, the grammar seems to include some separators or tokens that might be in the AST.
            // Let's check GqlSelector again.
        }

        return result;
    }

}
