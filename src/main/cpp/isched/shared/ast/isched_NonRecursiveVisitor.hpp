// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_NonRecursiveVisitor.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Stack-based non-recursive AST visitor with enum-driven dispatch.
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-06
 */

#ifndef ISCHED_SHARED_AST_NON_RECURSIVE_VISITOR_HPP
#define ISCHED_SHARED_AST_NON_RECURSIVE_VISITOR_HPP

#include <cstddef>
#include <cstdint>
#include <stack>
#include <isched/shared/ast/isched_CustomNode.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>

namespace isched::v0_0_1::ast {

// ── VisitorDispatchOutcome ───────────────────────────────────────────────────

/**
 * @brief Result of dispatching a single node in the visitor.
 *
 * @par Motivation
 * Provides a typed, non-boolean outcome so callers can distinguish between
 * nodes that were handled by a specific dispatch arm and nodes that fell
 * through to the unknown-node handler.  Using an enum (rather than bool)
 * makes call-site intent self-documenting.
 *
 * @pre Returned by @ref NonRecursiveVisitor::dispatch_node and aggregated
 *      across the full traversal by @ref NonRecursiveVisitor::traverse.
 * @post A return value of @c Success means every node encountered a specific
 *       dispatch arm.  @c UnknownNodeFallback means at least one node had an
 *       unmapped or sentinel @ref NodeType; the fallback code still ran without
 *       undefined behaviour.
 */
enum class VisitorDispatchOutcome : uint8_t {
    /// @brief Node was handled by a specific named dispatch arm.
    Success,
    /// @brief Node type was @c Unknown or @c Sentinel; fallback handler ran.
    UnknownNodeFallback,
};

// ── TraversalFrame ───────────────────────────────────────────────────────────

/**
 * @brief One entry on the traversal stack representing a partially-visited node.
 *
 * @par Motivation
 * Captures the minimum state needed to continue iterating over a node's
 * children without recursion.  Using an explicit stack of @c TraversalFrame
 * entries replaces the implicit call-stack frame that a recursive visitor would
 * consume, enabling safe traversal of trees with 10 000+ nesting levels.
 *
 * @pre @c node is a non-owning raw observer pointer.  The PEGTL parse tree
 *      owns all nodes via @c std::unique_ptr chains; @c TraversalFrame must
 *      never take or transfer ownership.
 * @post @c child_index advances monotonically from 0 to @c node->children.size()
 *       as children are pushed onto the traversal stack.
 */
struct TraversalFrame {
    /// @brief Non-owning observer pointer to the node being traversed.
    const CustomNode* node;
    /// @brief Index of the next child to push onto the traversal stack.
    std::size_t child_index = 0;
};

// ── NonRecursiveVisitor ──────────────────────────────────────────────────────

/**
 * @class NonRecursiveVisitor
 * @brief Complete standalone stack-based visitor that traverses a CustomNode
 *        tree in pre-order without recursion.
 *
 * @par Motivation
 * Recursive tree visitors accumulate one stack frame per nesting level.  For
 * adversarial or large GraphQL inputs this leads to stack overflow.  This
 * visitor replaces the implicit call stack with an explicit @c std::stack of
 * @ref TraversalFrame entries, bounding stack growth to O(depth) heap
 * allocations instead of O(depth) OS stack frames.
 *
 * @par DesignRationale
 * The visitor dispatches node handling via a @c switch statement on @ref NodeType.
 * The project constitution (§Clean Code) prefers polymorphism over complex
 * conditionals; however, FR-006 explicitly prohibits @c dynamic_cast and
 * @c std::type_info, which rule out virtual dispatch as the primary dispatch
 * mechanism for per-node-type behaviour.  A @c switch on a @c uint8_t enum
 * produces a hardware jump table at O2 optimisation — neutral-to-positive
 * performance characteristics per the constitution's own trade-off table.
 * This justification is supported by the baseline ratio measured in
 * @c isched_ast_benchmarks.cpp (T011).
 *
 * @pre The tree passed to @ref traverse must remain valid (non-modified,
 *      non-deleted) for the lifetime of the traversal call.
 * @post @ref node_count() returns the total number of nodes visited.
 *       @ref unknown_node_count() returns nodes with @c NodeType::Unknown
 *       or @c NodeType::Sentinel that fell through to the fallback handler.
 *
 * @see TraversalFrame, VisitorDispatchOutcome, NodeType, CustomNode
 *
 * @code
 * // Traverse a typed parse tree and check statistics.
 * NonRecursiveVisitor visitor;
 * auto outcome = visitor.traverse(root);
 * std::cout << "visited=" << visitor.node_count()
 *           << " unknown=" << visitor.unknown_node_count() << '\n';
 * @endcode
 */
class NonRecursiveVisitor {
public:
    NonRecursiveVisitor() = default;

    /**
     * @brief Traverse the entire tree rooted at @p root without recursion.
     *
     * @par Motivation
     * Replaces the per-level OS stack frame of a recursive visitor with a
     * heap-allocated @c std::stack<TraversalFrame>.  Safe for trees of
     * 10 000+ nesting levels.
     *
     * @pre @p root is a valid, fully constructed @ref CustomNode tree.
     * @post @ref node_count() and @ref unknown_node_count() reflect all nodes
     *       encountered during this traversal.  Internal counters are reset at
     *       the beginning of each call to @c traverse.
     *
     * @param root  Root of the tree to traverse.  Not owned; must remain valid.
     * @return @c VisitorDispatchOutcome::Success if every node was handled by
     *         a specific dispatch arm;
     *         @c VisitorDispatchOutcome::UnknownNodeFallback if any node had
     *         an unrecognised @ref NodeType but was handled without UB.
     */
    VisitorDispatchOutcome traverse(const CustomNode& root) {
        visited_count_ = 0;
        unknown_count_ = 0;

        std::stack<TraversalFrame> stack;
        stack.push({&root, 0});

        auto overall = VisitorDispatchOutcome::Success;

        while (!stack.empty()) {
            auto& top = stack.top();
            const CustomNode* current = top.node;

            if (top.child_index == 0) {
                // First encounter of this node: dispatch it.
                if (dispatch_node(*current) == VisitorDispatchOutcome::UnknownNodeFallback) {
                    overall = VisitorDispatchOutcome::UnknownNodeFallback;
                }
            }

            // Push the next unvisited child, or pop this frame if done.
            if (top.child_index < current->children.size()) {
                const auto* child = static_cast<const CustomNode*>(
                    current->children[top.child_index].get());
                ++top.child_index;
                stack.push({child, 0});
            } else {
                stack.pop();
            }
        }

        return overall;
    }

    /**
     * @brief Total number of nodes visited in the last @ref traverse call.
     *
     * @pre @ref traverse has been called at least once.
     * @post Returns 0 if @ref traverse has never been called.
     *
     * @return Count of nodes dispatched.
     */
    [[nodiscard]] std::size_t node_count() const noexcept { return visited_count_; }

    /**
     * @brief Number of nodes that fell through to the unknown-node fallback.
     *
     * @pre @ref traverse has been called at least once.
     * @post Returns 0 if every node was handled by a named dispatch arm.
     *
     * @return Count of nodes with @c NodeType::Unknown or @c NodeType::Sentinel.
     */
    [[nodiscard]] std::size_t unknown_node_count() const noexcept { return unknown_count_; }

private:
    /**
     * @brief Dispatch a single node by its @ref NodeType.
     *
     * @pre @p n is a valid @ref CustomNode with a correct @c node_type.
     * @post @c visited_count_ is incremented.  @c unknown_count_ is incremented
     *       for @c NodeType::Unknown and @c NodeType::Sentinel.
     *
     * @param n  Node to dispatch.
     * @return @c Success unless @c node_type is @c Unknown or @c Sentinel.
     */
    VisitorDispatchOutcome dispatch_node(const CustomNode& n) noexcept {
        ++visited_count_;

        switch (n.node_type) {
            // ---- Document ----
            case NodeType::Document:           [[fallthrough]];
            case NodeType::ExecutableDefinition: break;
            // ---- Operations ----
            case NodeType::OperationDefinition: [[fallthrough]];
            case NodeType::SelectionSet:        [[fallthrough]];
            case NodeType::Field:               [[fallthrough]];
            case NodeType::Alias:               [[fallthrough]];
            case NodeType::Arguments:           [[fallthrough]];
            case NodeType::Argument:            break;
            // ---- Fragments ----
            case NodeType::FragmentDefinition:  [[fallthrough]];
            case NodeType::FragmentSpread:      [[fallthrough]];
            case NodeType::InlineFragment:      [[fallthrough]];
            case NodeType::TypeCondition:       break;
            // ---- Variables ----
            case NodeType::Variable:            [[fallthrough]];
            case NodeType::VariableDefinition:  [[fallthrough]];
            case NodeType::VariableDefinitions: break;
            // ---- Values ----
            case NodeType::Value:               [[fallthrough]];
            case NodeType::StringValue:         [[fallthrough]];
            case NodeType::IntValue:            [[fallthrough]];
            case NodeType::FloatValue:          [[fallthrough]];
            case NodeType::BooleanValue:        [[fallthrough]];
            case NodeType::NullValue:           [[fallthrough]];
            case NodeType::EnumValue:           [[fallthrough]];
            case NodeType::ObjectValue:         break;
            // ---- Names and types ----
            case NodeType::Name:                [[fallthrough]];
            case NodeType::NamedType:           [[fallthrough]];
            case NodeType::ListType:            [[fallthrough]];
            case NodeType::NonNullType:         break;
            // ---- Type system definitions ----
            case NodeType::SchemaDefinition:          [[fallthrough]];
            case NodeType::ObjectTypeDefinition:      [[fallthrough]];
            case NodeType::InterfaceTypeDefinition:   [[fallthrough]];
            case NodeType::UnionTypeDefinition:       [[fallthrough]];
            case NodeType::EnumTypeDefinition:        [[fallthrough]];
            case NodeType::InputObjectTypeDefinition: [[fallthrough]];
            case NodeType::ScalarTypeDefinition:      [[fallthrough]];
            case NodeType::FieldDefinition:           [[fallthrough]];
            case NodeType::InputValueDefinition:      [[fallthrough]];
            case NodeType::DirectiveDefinition:       [[fallthrough]];
            case NodeType::Description:               [[fallthrough]];
            case NodeType::GqlQuery:                  break;
            // ---- Unknown / Sentinel: defined fallback — no UB ----
            case NodeType::Unknown:    [[fallthrough]];
            case NodeType::Sentinel:   [[fallthrough]];
            default:
                ++unknown_count_;
                return VisitorDispatchOutcome::UnknownNodeFallback;
        }

        return VisitorDispatchOutcome::Success;
    }

    std::size_t visited_count_ = 0;
    std::size_t unknown_count_ = 0;
};

} // namespace isched::v0_0_1::ast

#endif // ISCHED_SHARED_AST_NON_RECURSIVE_VISITOR_HPP
