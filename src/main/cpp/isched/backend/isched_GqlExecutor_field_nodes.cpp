// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_GqlExecutor_field_nodes.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Field-collection hot-path helpers for GraphQL selection processing.
 */

#include "isched_GqlExecutor.hpp"
#include <unordered_set>
#include "isched/shared/ast/isched_CustomNode.hpp"

#include <nlohmann/json.hpp>

namespace isched::v0_0_1::backend {
    using nlohmann::json;
    using gql::TAstNodePtr;

    static void collect_field_nodes_impl(
        const TAstNodePtr& p_selection_set,
        const GqlExecutor::TFragmentMap& p_fragments,
        std::unordered_set<std::string_view>& p_visited_fragments,
        GqlExecutor::FieldNodeList& p_fields_out,
        gql::TErrorVector& p_errors) {
        if (!p_selection_set) return;

        for (const auto& child : p_selection_set->children) {
            const auto ct = ast::get_node_type(child);
            if (ct == ast::NodeType::Field) {
                p_fields_out.push_back(&child);
            } else if (ct == ast::NodeType::FragmentSpread) {
                std::string_view fragName = child->children[0]->string_view();
                if (p_visited_fragments.contains(fragName)) {
                    continue; // Cycle detected
                }
                p_visited_fragments.insert(fragName);
                if (auto it = p_fragments.find(fragName); it != p_fragments.end()) {
                    const TAstNodePtr& fragDef = *(it->second);
                    for (const auto& fragChild : fragDef->children) {
                        if (ast::get_node_type(fragChild) == ast::NodeType::SelectionSet) {
                            collect_field_nodes_impl(fragChild, p_fragments, p_visited_fragments, p_fields_out, p_errors);
                        }
                    }
                } else {
                    p_errors.push_back(gql::Error{
                        .code=gql::EErrorCodes::PARSE_ERROR,
                        .message=std::format("Unknown fragment '{}'", fragName)
                    });
                }
            } else if (ct == ast::NodeType::InlineFragment) {
                for (const auto& inlineChild : child->children) {
                    if (ast::get_node_type(inlineChild) == ast::NodeType::SelectionSet) {
                        collect_field_nodes_impl(inlineChild, p_fragments, p_visited_fragments, p_fields_out, p_errors);
                    }
                }
            }
        }
    }

    GqlExecutor::FieldNodeList GqlExecutor::collect_field_nodes(
        const TAstNodePtr& p_selection_set,
        const TFragmentMap& p_fragments,
        gql::TErrorVector& p_errors) const {
        FieldNodeList fields;
        std::unordered_set<std::string_view> visited;
        collect_field_nodes_impl(p_selection_set, p_fragments, visited, fields, p_errors);
        return fields;
    }

    void GqlExecutor::process_field_nodes(
        const json& p_parent_result,
        ResolverPath& p_path,
        const FieldNodeList& p_fields,
        const TFragmentMap& p_fragments,
        json& p_result,
        gql::TErrorVector& p_errors) const {
        for (const auto& field : p_fields) {
            // GCOVR_EXCL_START -- tolerant skip for malformed field handles
            if (field) {
                resolve_field_selection_details(p_parent_result, p_path, *field, p_fragments, p_result, p_errors);
            }
            // GCOVR_EXCL_STOP
        }
    }
}

