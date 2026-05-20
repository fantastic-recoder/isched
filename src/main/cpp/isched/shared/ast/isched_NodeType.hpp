// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_NodeType.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Compact enum identifying the grammar rule that produced an AST node.
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-06
 */

#ifndef ISCHED_SHARED_AST_NODE_TYPE_HPP
#define ISCHED_SHARED_AST_NODE_TYPE_HPP

#include <cstdint>

namespace isched::v0_0_1::ast {

/**
 * @brief Compact enum identifying the grammar rule that produced an AST node.
 *
 * @par Motivation
 * Replaces string-based PEGTL rule identification with a single-byte enum value,
 * eliminating per-node heap-allocated string comparisons in all dispatch paths and
 * reducing the per-node type-identity footprint from a pointer-width string to
 * one byte. This enables O(1) jump-table dispatch in @ref NonRecursiveVisitor.
 *
 * @pre Underlying type is @c uint8_t; maximum 256 distinct enumerators.
 *      Current mapping uses 41 values, well within the ceiling.
 * @post All enumerator values are distinct and representable in @c uint8_t.
 *       Zero-initialised @ref CustomNode instances always hold @c Unknown.
 *
 * @see CustomNode, NodeSelector, NonRecursiveVisitor
 */
enum class NodeType : uint8_t {
    // ---- Fallback / sentinel (0 so zero-initialised nodes are safe) ----

    /// @brief Default value for zero-initialised CustomNode — not a real rule node.
    Unknown = 0,

    /// @brief Explicit sentinel for dispatch table arming — never produced by NodeSelector.
    Sentinel,

    // ---- Document ----

    /// @brief Top-level GraphQL Document rule.
    Document,

    /// @brief Executable or type-system definition inside a Document.
    ExecutableDefinition,

    // ---- Operations ----

    /// @brief A single operation (query / mutation / subscription) or shorthand SelectionSet.
    OperationDefinition,

    /// @brief Braced field selection `{ … }`.
    SelectionSet,

    /// @brief A single queried field, optionally aliased.
    Field,

    /// @brief Alias prefix for a field: `alias:`.
    Alias,

    /// @brief Argument list `(name: value, …)` on a field or directive.
    Arguments,

    /// @brief Single `name: value` argument.
    Argument,

    // ---- Fragments ----

    /// @brief Named fragment definition `fragment Name on Type { … }`.
    FragmentDefinition,

    /// @brief Fragment spread `…FragmentName`.
    FragmentSpread,

    /// @brief Inline fragment `… on TypeCondition { … }`.
    InlineFragment,

    /// @brief `on TypeName` type restriction inside a fragment.
    TypeCondition,

    // ---- Variables ----

    /// @brief Variable reference `$Name`.
    Variable,

    /// @brief Single variable definition inside `(…)`.
    VariableDefinition,

    /// @brief Variable definitions list `(…)` on an operation.
    VariableDefinitions,

    // ---- Values ----

    /// @brief Any GraphQL value (variable | int | float | string | bool | null | enum | list | object).
    Value,

    /// @brief String value literal (quoted or block string).
    StringValue,

    /// @brief Integer value literal.
    IntValue,

    /// @brief Floating-point value literal.
    FloatValue,

    /// @brief Boolean value literal (`true` or `false`).
    BooleanValue,

    /// @brief Null value literal.
    NullValue,

    /// @brief Enum value (a Name that is not a built-in keyword).
    EnumValue,

    /// @brief Object literal value `{field: value, …}`.
    ObjectValue,

    // ---- Names and types ----

    /// @brief GraphQL name token.
    Name,

    /// @brief Named type reference.
    NamedType,

    /// @brief List type `[Type]`.
    ListType,

    /// @brief Non-null type `Type!`.
    NonNullType,

    // ---- Type system definitions ----

    /// @brief Schema definition block `schema { … }`.
    SchemaDefinition,

    /// @brief Object type definition `type Name { … }`.
    ObjectTypeDefinition,

    /// @brief Interface type definition `interface Name { … }`.
    InterfaceTypeDefinition,

    /// @brief Union type definition `union Name = A | B`.
    UnionTypeDefinition,

    /// @brief Enum type definition `enum Name { … }`.
    EnumTypeDefinition,

    /// @brief Input object type definition `input Name { … }`.
    InputObjectTypeDefinition,

    /// @brief Scalar type definition `scalar Name`.
    ScalarTypeDefinition,

    /// @brief Field definition inside an object or interface type.
    FieldDefinition,

    /// @brief Input value / argument definition.
    InputValueDefinition,

    /// @brief Directive definition `directive @Name on …`.
    DirectiveDefinition,

    /// @brief GraphQL type definition wrapper rule.
    TypeDefinition,

    /// @brief GraphQL Type rule wrapper.
    Type,

    /// @brief Constant directives list.
    DirectivesConst,

    /// @brief Constant single directive.
    DirectiveConst,

    /// @brief Constant arguments list.
    ArgumentsConst,

    /// @brief Constant single argument.
    ArgumentConst,

    /// @brief Constant value wrapper.
    ValueConst,

    /// @brief Default value wrapper.
    DefaultValue,

    /// @brief Variables definition list.
    VariablesDefinition,

    /// @brief Enum values definition list.
    EnumValuesDefinition,

    /// @brief Input fields definition list.
    InputFieldsDefinition,

    /// @brief Type name reference.
    TypeName,

    /// @brief Arguments definition list.
    ArgumentsDefinition,

    /// @brief Union member types list.
    UnionMemberTypes,

    /// @brief Enum value definition.
    EnumValueDefinition,

    /// @brief List value literal.
    ListValue,

    /// @brief Object field name-value pair.
    ObjectField,

    // ---- Miscellaneous ----

    /// @brief String description preceding a type or field definition.
    Description,

    /// @brief Shorthand `query { … }` rule (legacy helper rule).
    GqlQuery,
};

} // namespace isched::v0_0_1::ast

#endif // ISCHED_SHARED_AST_NODE_TYPE_HPP
