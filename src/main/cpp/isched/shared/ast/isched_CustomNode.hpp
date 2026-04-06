// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_CustomNode.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief PEGTL parse-tree node extended with a typed NodeType identity.
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-06
 */

#ifndef ISCHED_SHARED_AST_CUSTOM_NODE_HPP
#define ISCHED_SHARED_AST_CUSTOM_NODE_HPP

#include <tao/pegtl/contrib/parse_tree.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>

namespace isched::v0_0_1::ast {

/**
 * @class CustomNode
 * @brief PEGTL parse-tree node extended with a compact typed identity.
 *
 * @par Motivation
 * The default @c tao::pegtl::parse_tree::node carries rule identity only through
 * its @c type() string result, which requires a heap-allocated string and O(n)
 * comparison on every dispatch.  This subtype replaces that with a single
 * @c uint8_t-backed @ref NodeType enum, enabling O(1) jump-table dispatch in
 * @ref NonRecursiveVisitor and reducing per-node metadata footprint.
 *
 * @par CacheLocalityRationale
 * Adding @c node_type costs exactly one byte.  On 64-bit ABIs the parent @c node
 * struct has tail-padding after its @c bool member, so @c node_type occupies
 * existing alignment padding with zero struct-size increase in most cases.
 * More importantly, the dispatch key is now co-located with other hot fields in
 * the same cache line rather than requiring a separate pointer dereference to
 * a heap-allocated type-name string.  Deep-tree traversal workloads involving
 * 10 000+ nodes benefit measurably from this reduction in cache-line crossings
 * per dispatch.
 *
 * @pre @c tao::pegtl::parse_tree::node must have a virtual destructor (guaranteed
 *      by PEGTL ≥ 3.x) so that @c std::unique_ptr<node> holding a @c CustomNode*
 *      releases memory correctly.
 * @post Every @c CustomNode constructed by @ref NodeSelector::transform has
 *       @c node_type set to a valid @ref NodeType enumerator.
 *       Zero-initialised instances default to @c NodeType::Unknown.
 *
 * @see NodeType, NodeSelector, NonRecursiveVisitor
 *
 * @code
 * // Typical usage: tree returned by generate_typed_ast contains CustomNode nodes.
 * auto [ok, root] = isched::v0_0_1::gql::generate_ast_and_log<gql::Document>(in, "q");
 * if (ok) {
 *     auto& typed = static_cast<const isched::v0_0_1::ast::CustomNode&>(*root);
 *     assert(typed.node_type == isched::v0_0_1::ast::NodeType::Document);
 * }
 * @endcode
 */
struct CustomNode : tao::pegtl::parse_tree::node {
    /**
     * @brief Typed identity of this node.
     *
     * @par Motivation
     * Single-byte enum replaces string rule-name comparison.  Set by
     * @ref NodeSelector::transform immediately after node construction.
     *
     * @pre Initialised to @c NodeType::Unknown (safe default for any rule not
     *      explicitly mapped by @ref NodeSelector).
     * @post After @c NodeSelector::transform runs: holds the @ref NodeType
     *       corresponding to the matched grammar rule, or @c NodeType::Unknown
     *       for unmapped rules.
     */
    NodeType node_type = NodeType::Unknown;
};

} // namespace isched::v0_0_1::ast

#endif // ISCHED_SHARED_AST_CUSTOM_NODE_HPP
