// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_NodeSelector.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Compile-time rule-to-NodeType mapping for PEGTL parse-tree construction.
 * @author isched Development Team
 * @version 1.0.0
 * @date 2026-04-06
 */

#ifndef ISCHED_SHARED_AST_NODE_SELECTOR_HPP
#define ISCHED_SHARED_AST_NODE_SELECTOR_HPP

#include <type_traits>
#include <memory>
#include <isched/shared/ast/isched_CustomNode.hpp>
#include <isched/shared/ast/isched_NodeType.hpp>

// Forward-declare grammar rules.  Full definitions live in isched_gql_grammar.hpp,
// which includes this header after all rules are defined.  std::is_same_v only
// requires type identity — it does not require complete types.
namespace isched::v0_0_1::gql {
    struct GqlQuery;
    struct Document;
    struct ExecutableDefinition;
    struct OperationDefinition;
    struct OperationType;
    struct SelectionSet;
    struct Field;
    struct Alias;
    struct Selection;
    struct Arguments;
    struct Argument;
    struct FragmentDefinition;
    struct FragmentSpread;
    struct InlineFragment;
    struct TypeCondition;
    struct Variable;
    struct VariableDefinition;
    struct VariableDefinitions;
    struct Value;
    struct StringValue;
    struct IntValue;
    struct FloatValue;
    struct BooleanValue;
    struct NullValue;
    struct EnumValue;
    struct ListValue;
    struct ObjectValue;
    struct ObjectField;
    struct ValueConst;
    struct TrueKeyword;
    struct FalseKeyword;
    struct NullKeyword;
    struct Token;
    struct Name;
    struct NamedType;
    struct ListType;
    struct NonNullType;
    struct Type;
    struct TypeName;
    struct GqlNonNullType;
    struct BuiltInType;
    struct String;
    struct Int;
    struct Float;
    struct Boolean;
    struct ID;
    struct TypeDefinition;
    struct SchemaDefinition;
    struct RootOperationTypeDefinition;
    struct ObjectTypeDefinition;
    struct InterfaceTypeDefinition;
    struct UnionTypeDefinition;
    struct UnionMemberTypes;
    struct EnumTypeDefinition;
    struct EnumValueDefinition;
    struct EnumValuesDefinition;
    struct InputObjectTypeDefinition;
    struct InputFieldsDefinition;
    struct ScalarTypeDefinition;
    struct FieldDefinition;
    struct FieldsDefinition;
    struct ArgumentsDefinition;
    struct InputValueDefinition;
    struct DirectiveDefinition;
    struct DirectiveLocation;
    struct DirectiveLocations;
    struct DirectiveConst;
    struct DirectivesConst;
    struct ArgumentsConst;
    struct ArgumentConst;
    struct Description;
    struct ObjectTypeExtension;
    struct InterfaceTypeExtension;
    struct UnionTypeExtension;
    struct EnumTypeExtension;
    struct InputObjectTypeExtension;
    struct ScalarTypeExtension;
    struct SchemaExtension;
    struct TypeSystemExtension;
} // namespace isched::v0_0_1::gql

namespace isched::v0_0_1::ast {

// ── Helper: is T one of the listed types? ────────────────────────────────────

/// @cond INTERNAL
template<typename T, typename... Ts>
inline constexpr bool is_any_of_v = (std::is_same_v<T, Ts> || ...);
/// @endcond

// ── NodeSelector ─────────────────────────────────────────────────────────────

/**
 * @brief PEGTL parse-tree selector that maps grammar rules to @ref NodeType values.
 *
 * @par Motivation
 * Replaces @c GqlSelector (which uses string-based PEGTL rule identity) with a
 * compile-time @c if constexpr dispatch table.  Every matched rule receives a
 * @ref NodeType enumerator at node-construction time with no runtime overhead
 * beyond writing a single byte.  Unmapped rules that are retained in the tree
 * receive @c NodeType::Unknown, which is deterministic and safe.
 *
 * @par Usage with @c tao::pegtl::parse_tree::parse
 * @code
 * auto root = tao::pegtl::parse_tree::parse<
 *     gql::Document, CustomNode, NodeSelector>(input);
 * @endcode
 *
 * @tparam TRule The PEGTL grammar rule being considered for inclusion in the tree.
 *
 * @pre @c TRule is a complete type visible at instantiation site.
 * @pre @c CustomNode is the node type passed to @c parse_tree::parse.
 * @post For every rule where @c value == true, @c transform sets @c node_type
 *       to the corresponding @ref NodeType enumerator, or @c NodeType::Unknown
 *       if the rule has no explicit mapping.
 *
 * @see CustomNode, NodeType
 */
template<typename... Args>
struct NodeSelector;

template<typename TRule>
struct NodeSelector<TRule> {
    /// @brief True for all rules retained in the parse tree (same set as the existing GqlSelector).
    static constexpr bool value = is_any_of_v<TRule,
        // Executable / operations
        gql::GqlQuery,
        gql::Document,
        gql::ExecutableDefinition,
        gql::OperationDefinition,
        gql::OperationType,
        gql::SelectionSet,
        gql::Field,
        gql::Alias,
        // gql::Selection is intentionally excluded: it is a transparent sor<>
        // wrapper with no NodeType equivalent.  Its retained child (Field /
        // FragmentSpread / InlineFragment) floats up to the parent.
        gql::Arguments,
        gql::Argument,
        // Fragments
        gql::FragmentDefinition,
        gql::FragmentSpread,
        gql::InlineFragment,
        gql::TypeCondition,
        // Variables
        gql::Variable,
        gql::VariableDefinition,
        gql::VariableDefinitions,
        // Values
        gql::Value,
        gql::StringValue,
        gql::IntValue,
        gql::FloatValue,
        gql::BooleanValue,
        gql::NullValue,
        gql::EnumValue,
        gql::ListValue,
        gql::ObjectValue,
        gql::ObjectField,
        gql::ValueConst,
        gql::TrueKeyword,
        gql::FalseKeyword,
        gql::NullKeyword,
        gql::Token,
        // Names and types
        gql::Name,
        gql::NamedType,
        gql::ListType,
        gql::NonNullType,
        gql::Type,
        gql::TypeName,
        gql::GqlNonNullType,
        gql::BuiltInType,
        gql::String,
        gql::Int,
        gql::Float,
        gql::Boolean,
        gql::ID,
        // Type system definitions
        gql::TypeDefinition,
        gql::SchemaDefinition,
        gql::RootOperationTypeDefinition,
        gql::ObjectTypeDefinition,
        gql::InterfaceTypeDefinition,
        gql::UnionTypeDefinition,
        gql::UnionMemberTypes,
        gql::EnumTypeDefinition,
        gql::EnumValueDefinition,
        gql::EnumValuesDefinition,
        gql::InputObjectTypeDefinition,
        gql::InputFieldsDefinition,
        gql::ScalarTypeDefinition,
        gql::FieldDefinition,
        gql::FieldsDefinition,
        gql::ArgumentsDefinition,
        gql::InputValueDefinition,
        gql::DirectiveDefinition,
        gql::DirectiveLocation,
        gql::DirectiveLocations,
        gql::DirectiveConst,
        gql::DirectivesConst,
        gql::ArgumentsConst,
        gql::ArgumentConst,
        gql::Description,
        // Extensions
        gql::ObjectTypeExtension,
        gql::InterfaceTypeExtension,
        gql::UnionTypeExtension,
        gql::EnumTypeExtension,
        gql::InputObjectTypeExtension,
        gql::ScalarTypeExtension,
        gql::SchemaExtension,
        gql::TypeSystemExtension
    >;

    /**
     * @brief Set @c node_type on the freshly constructed node.
     *
     * @par Motivation
     * Called by PEGTL after a successful rule match for every rule where
     * @c value == true.  Uses @c if constexpr chains so the mapping is resolved
     * entirely at compile time — the resulting machine code is a simple
     * assignment, not a string lookup.
     *
     * @pre  @p n is non-null and points to a valid @ref CustomNode.
     * @post @c n->node_type holds the @ref NodeType for @c TRule, or
     *       @c NodeType::Unknown for rules that have no specific mapping.
     *       The node's content (string_view) is already retained by PEGTL
     *       before @c transform is called.
     *
     * @param n  Unique pointer to the node being finalised.
     */
    static void transform(std::unique_ptr<CustomNode>& n) noexcept {
        using NT = NodeType;
        using namespace gql;

        // ---- Document ----
        if constexpr      (std::is_same_v<TRule, gql::Document>)
            n->node_type = NT::Document;
        else if constexpr (std::is_same_v<TRule, gql::ExecutableDefinition>)
            n->node_type = NT::ExecutableDefinition;
        // ---- Operations ----
        else if constexpr (std::is_same_v<TRule, gql::OperationDefinition>)
            n->node_type = NT::OperationDefinition;
        else if constexpr (std::is_same_v<TRule, gql::SelectionSet>)
            n->node_type = NT::SelectionSet;
        else if constexpr (std::is_same_v<TRule, gql::Field>)
            n->node_type = NT::Field;
        else if constexpr (std::is_same_v<TRule, gql::Alias>)
            n->node_type = NT::Alias;
        else if constexpr (std::is_same_v<TRule, gql::Arguments>)
            n->node_type = NT::Arguments;
        else if constexpr (std::is_same_v<TRule, gql::Argument>)
            n->node_type = NT::Argument;
        // ---- Fragments ----
        else if constexpr (std::is_same_v<TRule, gql::FragmentDefinition>)
            n->node_type = NT::FragmentDefinition;
        else if constexpr (std::is_same_v<TRule, gql::FragmentSpread>)
            n->node_type = NT::FragmentSpread;
        else if constexpr (std::is_same_v<TRule, gql::InlineFragment>)
            n->node_type = NT::InlineFragment;
        else if constexpr (std::is_same_v<TRule, gql::TypeCondition>)
            n->node_type = NT::TypeCondition;
        // ---- Variables ----
        else if constexpr (std::is_same_v<TRule, gql::Variable>)
            n->node_type = NT::Variable;
        else if constexpr (std::is_same_v<TRule, gql::VariableDefinition>)
            n->node_type = NT::VariableDefinition;
        else if constexpr (std::is_same_v<TRule, gql::VariableDefinitions>)
            n->node_type = NT::VariableDefinitions;
        // ---- Values ----
        else if constexpr (std::is_same_v<TRule, gql::Value>)
            n->node_type = NT::Value;
        else if constexpr (std::is_same_v<TRule, gql::StringValue>)
            n->node_type = NT::StringValue;
        else if constexpr (std::is_same_v<TRule, gql::IntValue>)
            n->node_type = NT::IntValue;
        else if constexpr (std::is_same_v<TRule, gql::FloatValue>)
            n->node_type = NT::FloatValue;
        else if constexpr (std::is_same_v<TRule, gql::BooleanValue>)
            n->node_type = NT::BooleanValue;
        else if constexpr (std::is_same_v<TRule, gql::NullValue>)
            n->node_type = NT::NullValue;
        else if constexpr (std::is_same_v<TRule, gql::EnumValue>)
            n->node_type = NT::EnumValue;
        else if constexpr (std::is_same_v<TRule, gql::ObjectValue>)
            n->node_type = NT::ObjectValue;
        // ---- Names and types ----
        else if constexpr (std::is_same_v<TRule, gql::Name>)
            n->node_type = NT::Name;
        else if constexpr (std::is_same_v<TRule, gql::NamedType>)
            n->node_type = NT::NamedType;
        else if constexpr (std::is_same_v<TRule, gql::ListType>)
            n->node_type = NT::ListType;
        else if constexpr (std::is_same_v<TRule, gql::NonNullType>)
            n->node_type = NT::NonNullType;
        // ---- Schema definitions ----
        else if constexpr (std::is_same_v<TRule, gql::SchemaDefinition>)
            n->node_type = NT::SchemaDefinition;
        else if constexpr (std::is_same_v<TRule, gql::ObjectTypeDefinition>)
            n->node_type = NT::ObjectTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::InterfaceTypeDefinition>)
            n->node_type = NT::InterfaceTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::UnionTypeDefinition>)
            n->node_type = NT::UnionTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::EnumTypeDefinition>)
            n->node_type = NT::EnumTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::InputObjectTypeDefinition>)
            n->node_type = NT::InputObjectTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::ScalarTypeDefinition>)
            n->node_type = NT::ScalarTypeDefinition;
        else if constexpr (std::is_same_v<TRule, gql::FieldDefinition>)
            n->node_type = NT::FieldDefinition;
        else if constexpr (std::is_same_v<TRule, gql::InputValueDefinition>)
            n->node_type = NT::InputValueDefinition;
        else if constexpr (std::is_same_v<TRule, gql::DirectiveDefinition>)
            n->node_type = NT::DirectiveDefinition;
        else if constexpr (std::is_same_v<TRule, gql::Description>)
            n->node_type = NT::Description;
        else if constexpr (std::is_same_v<TRule, gql::GqlQuery>)
            n->node_type = NT::GqlQuery;
        // ---- All other retained rules: deterministic fallback ----
        // (node_type is already NodeType::Unknown from CustomNode's constructor)
    }
};

} // namespace isched::v0_0_1::ast

#endif // ISCHED_SHARED_AST_NODE_SELECTOR_HPP
