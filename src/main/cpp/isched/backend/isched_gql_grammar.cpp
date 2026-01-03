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
        return p_schema_node;
    }
}
