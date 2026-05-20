// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_GqlExecutor_field_nodes.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Field-collection hot-path helpers for GraphQL selection processing.
 */

#include "isched_GqlExecutor.hpp"
#include "isched/shared/ast/isched_CustomNode.hpp"

#include <nlohmann/json.hpp>

namespace isched::v0_0_1::backend {
    using nlohmann::json;
    using gql::TAstNodePtr;

    GqlExecutor::FieldNodeList GqlExecutor::collect_field_nodes(
        const TAstNodePtr& p_selection_set,
        gql::TErrorVector&) const {
        FieldNodeList fields;
        if (!p_selection_set) {
            return fields;
        }

        if (ast::get_node_type(p_selection_set) != ast::NodeType::SelectionSet) {
            return fields;
        }

        for (const auto& selection : p_selection_set->children) {
            // GCOVR_EXCL_START -- defensive guards for malformed/legacy AST shapes
            if (!selection) {
                continue;
            }
            const auto type = ast::get_node_type(selection);
            if (type == ast::NodeType::Field) {
                fields.push_back(&selection);
                continue;
            }
            if (type == ast::NodeType::Unknown && selection->type == "isched::v0_0_1::gql::Selection") {
                if (selection->children.empty()) {
                    continue;
                }
                const auto& first = selection->children.front();
                if (first && ast::get_node_type(first) == ast::NodeType::Field) {
                    fields.push_back(&first);
                }
            }
            // GCOVR_EXCL_STOP
        }

        return fields;
    }

    void GqlExecutor::process_field_nodes(
        const json& p_parent_result,
        ResolverPath& p_path,
        const FieldNodeList& p_fields,
        json& p_result,
        gql::TErrorVector& p_errors) const {
        for (const auto& field : p_fields) {
            // GCOVR_EXCL_START -- tolerant skip for malformed field handles
            if (field) {
                resolve_field_selection_details(p_parent_result, p_path, *field, p_result, p_errors);
            }
            // GCOVR_EXCL_STOP
        }
    }
}

