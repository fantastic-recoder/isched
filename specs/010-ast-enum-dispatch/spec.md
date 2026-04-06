# Feature Specification: Typed PEGTL AST Node Dispatch

**Feature Branch**: `010-ast-enum-dispatch`  
**Created**: 2026-04-06  
**Status**: Completed  
**Input**: User description: "Provide a complete C++ implementation for a custom AST node system in PEGTL replacing string-based rule identification with high-performance enum class dispatch."

## Clarifications

### Session 2026-04-06

- Q: Is the non-recursive visitor a complete standalone implementation or an abstract base/CRTP extension point? → A: Complete standalone implementation — no extension contract needed; contributors modify in-place (YAGNI).
- Q: What memory ownership model does TraversalFrame use for node pointers? → A: Raw non-owning observer pointers — PEGTL owns the parse tree via `unique_ptr` chains; TraversalFrame must never take ownership.
- Q: Must existing `isched_ast_node_tests.cpp` pass unchanged after T007 integrates NodeSelector? → A: Yes — T007 is a drop-in; existing tests must pass without modification.
- Q: Is `NodeType::Unknown` sufficient for partial or failed-parse scenarios? → A: Yes — `parse_tree::parse` never calls `transform` on failed parses; partial trees are unreachable through the PEGTL API.
- Q: Is a hard performance ratio gate required for the T011 benchmark? → A: No — advisory benchmark only; records improvement evidence but does not gate acceptance.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build typed AST nodes during parse (Priority: P1)

As a backend developer, I can parse grammar input into AST nodes that carry a compact node type value so node identity checks do not rely on rule-name strings.

**Why this priority**: This is the core value of the feature and must exist before traversal, optimization, or documentation work is useful.

**Independent Test**: Can be fully tested by parsing representative grammar inputs and verifying every produced node has the expected typed identity and remains compatible with existing parse-tree construction flow.

**Acceptance Scenarios**:

1. **Given** grammar rules that are mapped to node types, **When** parsing valid input into a parse tree, **Then** each mapped node is created as a custom node with the correct enum value.
2. **Given** an unmapped grammar rule, **When** parsing valid input, **Then** the node is still created safely with a defined fallback node type behavior.
3. **Given** existing parse entry points, **When** the typed node selector is enabled, **Then** parsing remains compatible with `tao::pegtl::parse_tree::parse` behavior and result-shape expectations.

---

### User Story 2 - Traverse deeply nested trees safely and efficiently (Priority: P2)

As a runtime maintainer, I can evaluate and traverse deeply nested AST trees through enum-driven dispatch and a non-recursive traversal pattern so traversal is predictable and resistant to stack overflow.

**Why this priority**: Runtime safety and performance are critical once typed node creation is in place, especially for adversarial or large nested input.

**Independent Test**: Can be fully tested by constructing very deep nested trees and validating traversal completes without recursive call-stack growth while dispatching only by node type enum.

**Acceptance Scenarios**:

1. **Given** an AST with deep nesting, **When** the non-recursive visitor skeleton traverses it, **Then** traversal completes without recursion-related failure.
2. **Given** node dispatch requirements, **When** visitor logic executes, **Then** dispatch branches exclusively on node type enum values and does not use runtime type inspection (`dynamic_cast` or `std::type_info`).
3. **Given** node size optimization goals, **When** custom node definitions are reviewed and benchmarked, **Then** node metadata footprint is minimized and documented for cache-locality rationale.

---

### User Story 3 - Consume maintainable API contracts and docs (Priority: P3)

As a contributor, I can read complete API documentation for the custom node and selector system, including motivation and behavioral contracts, so implementation intent and usage expectations are unambiguous.

**Why this priority**: Good documentation prevents regressions and makes the optimization-focused design maintainable over time.

**Independent Test**: Can be fully tested by generating documentation and confirming required sections (Motivation, Pre-conditions, Post-conditions) exist on all specified public classes and methods.

**Acceptance Scenarios**:

1. **Given** public classes and methods for the custom node system, **When** documentation is generated, **Then** each item includes Motivation, Pre-conditions, and Post-conditions.
2. **Given** parser and visitor extension points, **When** another contributor reads the docs, **Then** they can identify required inputs, guaranteed outputs, and failure behavior without reading implementation files.

### Edge Cases

- Parsing produces nodes for grammar rules intentionally not included in the selector mapping.
- Extremely deep nesting (for example, 10,000+ nested constructs) is provided in a single input payload.
- Unknown or sentinel node types are encountered during visitor dispatch.
- Mixed trees containing both terminal and non-terminal nodes appear in one traversal pass.
- Documentation generation succeeds but omits required contract sections unless explicitly validated.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST define a `CustomNode` type that inherits from `tao::pegtl::parse_tree::node`.
- **FR-002**: `CustomNode` MUST store node identity in a `NodeType` enum class with `uint8_t` underlying storage. The complete enumerator set is defined in the NodeType mapping table in §Key Entities above (39 named grammar rules + `Unknown` + `Sentinel`). `uint8_t` is sufficient as this is well below the 256-value ceiling.
- **FR-003**: The system MUST provide a `NodeSelector` template with static transform behavior that maps grammar rules to `NodeType` values using `std::is_same_v` checks.
- **FR-004**: Rule-to-node mapping logic MUST support compile-time branching with `if constexpr` and require C++23-or-newer compilation.
- **FR-005**: Typed node construction MUST remain compatible with `tao::pegtl::parse_tree::parse` integration and existing parse flows.
- **FR-006**: Node identification and dispatch MUST NOT use `dynamic_cast` or `std::type_info`.
- **FR-007**: The feature MUST document and justify node-size minimization expectations for cache locality in deep-tree workloads.
- **FR-008**: The feature MUST include a stack-based, non-recursive visitor that is a **complete standalone implementation** (not an abstract base or CRTP extension point) and dispatches behavior by `NodeType` via a `switch` statement.
- **FR-009**: The non-recursive visitor skeleton MUST define behavior for unknown or sentinel node types without undefined behavior.
- **FR-010**: Public classes and methods introduced for this feature MUST include Doxygen documentation sections for Motivation, Pre-conditions, and Post-conditions.
- **FR-011**: Feature acceptance tests MUST verify both functional parsing correctness and deep-nesting traversal safety.

### Key Entities *(include if feature involves data)*

- **CustomNode**: Parse tree node entity carrying typed identity (`NodeType`) and parse-tree payload inherited from PEGTL.
- **NodeType**: Compact `enum class` with `uint8_t` underlying storage defining supported AST node categories and sentinel/default categories. The following grammar rules from `isched_gql_grammar.hpp` MUST map to typed `NodeType` values (all others fall back to `NodeType::Unknown`):

  | Grammar Rule | NodeType enumerator |
  |---|---|
  | `gql::Document` | `Document` |
  | `gql::OperationDefinition` | `OperationDefinition` |
  | `gql::SelectionSet` | `SelectionSet` |
  | `gql::Field` | `Field` |
  | `gql::Alias` | `Alias` |
  | `gql::Arguments` | `Arguments` |
  | `gql::Argument` | `Argument` |
  | `gql::FragmentDefinition` | `FragmentDefinition` |
  | `gql::FragmentSpread` | `FragmentSpread` |
  | `gql::InlineFragment` | `InlineFragment` |
  | `gql::TypeCondition` | `TypeCondition` |
  | `gql::Variable` | `Variable` |
  | `gql::VariableDefinition` | `VariableDefinition` |
  | `gql::VariableDefinitions` | `VariableDefinitions` |
  | `gql::Value` | `Value` |
  | `gql::Name` | `Name` |
  | `gql::NamedType` | `NamedType` |
  | `gql::ListType` | `ListType` |
  | `gql::NonNullType` | `NonNullType` |
  | `gql::SchemaDefinition` | `SchemaDefinition` |
  | `gql::InterfaceTypeDefinition` | `InterfaceTypeDefinition` |
  | `gql::UnionTypeDefinition` | `UnionTypeDefinition` |
  | `gql::EnumTypeDefinition` | `EnumTypeDefinition` |
  | `gql::InputObjectTypeDefinition` | `InputObjectTypeDefinition` |
  | `gql::ScalarTypeDefinition` | `ScalarTypeDefinition` |
  | `gql::FieldDefinition` | `FieldDefinition` |
  | `gql::InputValueDefinition` | `InputValueDefinition` |

  > **Note — intentionally excluded transparent wrappers** (children bubble up to parent; retained as enum values and `transform` arms for future use, but not added to the node-retention list):
  > - `gql::ObjectTypeDefinition` — transparent `sor<>` wrapper; `Name` and `FieldDefinition` children lift to parent `TypeDefinition`. Retaining it would break existing tests expecting flat child layout.
  > - `gql::FieldsDefinition` — transparent braced-list wrapper; `FieldDefinition` children lift to parent. Same rationale as `gql::Selection` (already excluded in legacy `GqlSelector`).
  | `gql::DirectiveDefinition` | `DirectiveDefinition` |
  | `gql::Description` | `Description` |
  | `gql::StringValue` | `StringValue` |
  | `gql::IntValue` | `IntValue` |
  | `gql::FloatValue` | `FloatValue` |
  | `gql::BooleanValue` | `BooleanValue` |
  | `gql::NullValue` | `NullValue` |
  | `gql::EnumValue` | `EnumValue` |
  | `gql::ObjectValue` | `ObjectValue` |
  | `gql::GqlQuery` | `GqlQuery` |
  | *(sentinel)* | `Unknown` |
  | *(fallback/default)* | `Sentinel` |

- **NodeSelector**: Rule-to-node mapping policy that transforms grammar-rule matches into typed node-assignment decisions.
- **TraversalFrame**: Stack entry representation used by the non-recursive visitor to track current node and traversal state. Holds **raw non-owning observer pointers** to `CustomNode`; PEGTL owns the parse tree via `unique_ptr` chains and `TraversalFrame` must never take or transfer ownership.
- **VisitorDispatchOutcome**: Result `enum` capturing dispatch handling status values: `Success` (node processed by a mapped arm) and `UnknownNodeFallback` (node type had no mapped arm; fallback behavior executed without UB).

### Assumptions

- Existing grammar behavior and parse output semantics remain functionally equivalent after replacing string-based node identification.
- A fallback node type is acceptable for unmapped grammar rules as long as behavior is deterministic and test-covered.
- Deep nesting validation is performed with synthetic and representative grammar samples sufficient to demonstrate non-recursive safety.
- Documentation quality checks are part of feature acceptance, not optional follow-up work.
- `NodeSelector::transform` is only invoked on successfully parsed nodes within a valid parse tree; `parse_tree::parse` returns a null `node_up` on failure and never calls `transform`. Therefore `NodeType::Unknown` is sufficient for all unmapped-rule scenarios and no `ParseError` node type is required.
- The existing `isched_ast_node_tests.cpp` suite MUST continue to pass without modification after `NodeSelector` integration (T007 is a backward-compatible drop-in).
- The T011 performance benchmark is advisory — it records the improvement ratio for review evidence but does not constitute a hard numeric acceptance gate.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of grammar rules designated for typed mapping resolve to deterministic `NodeType` values in acceptance tests.
- **SC-002**: Deep-nesting traversal acceptance tests complete successfully for inputs at or above 10,000 nested levels without recursion-related failure.
- **SC-003**: 100% of node-dispatch code paths in this feature use enum-based dispatch and 0 acceptance-test findings show runtime type inspection (`dynamic_cast` or `std::type_info`).
- **SC-004**: 100% of public APIs introduced by this feature include generated documentation with Motivation, Pre-conditions, and Post-conditions sections.
- **SC-005**: In code review sign-off, maintainers confirm the specification and tests demonstrate improved maintainability over string-based node identification for the targeted AST flow.
- **SC-006**: The T011 benchmark documents a measured performance ratio (enum-dispatch vs. string-comparison baseline) for review evidence; no minimum ratio is required as a gate.
