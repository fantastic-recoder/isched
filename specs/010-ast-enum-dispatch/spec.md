# Feature Specification: Typed PEGTL AST Node Dispatch

**Feature Branch**: `010-ast-enum-dispatch`  
**Created**: 2026-04-06  
**Status**: Draft  
**Input**: User description: "Provide a complete C++ implementation for a custom AST node system in PEGTL replacing string-based rule identification with high-performance enum class dispatch."

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
- **FR-002**: `CustomNode` MUST store node identity in a `NodeType` enum class with `uint8_t` underlying storage.
- **FR-003**: The system MUST provide a `NodeSelector` template with static transform behavior that maps grammar rules to `NodeType` values using `std::is_same_v` checks.
- **FR-004**: Rule-to-node mapping logic MUST support compile-time branching with `if constexpr` and require C++23-or-newer compilation.
- **FR-005**: Typed node construction MUST remain compatible with `tao::pegtl::parse_tree::parse` integration and existing parse flows.
- **FR-006**: Node identification and dispatch MUST NOT use `dynamic_cast` or `std::type_info`.
- **FR-007**: The feature MUST document and justify node-size minimization expectations for cache locality in deep-tree workloads.
- **FR-008**: The feature MUST include a stack-based, non-recursive visitor skeleton that dispatches behavior by `NodeType`.
- **FR-009**: The non-recursive visitor skeleton MUST define behavior for unknown or sentinel node types without undefined behavior.
- **FR-010**: Public classes and methods introduced for this feature MUST include Doxygen documentation sections for Motivation, Pre-conditions, and Post-conditions.
- **FR-011**: Feature acceptance tests MUST verify both functional parsing correctness and deep-nesting traversal safety.

### Key Entities *(include if feature involves data)*

- **CustomNode**: Parse tree node entity carrying typed identity (`NodeType`) and parse-tree payload inherited from PEGTL.
- **NodeType**: Compact enum identity set defining supported AST node categories and sentinel/default categories.
- **NodeSelector**: Rule-to-node mapping policy that transforms grammar-rule matches into typed node-assignment decisions.
- **TraversalFrame**: Stack entry representation used by the non-recursive visitor to track current node and traversal state.
- **VisitorDispatchOutcome**: Result state capturing dispatch handling status, including successful processing and unknown-node fallback handling.

### Assumptions

- Existing grammar behavior and parse output semantics remain functionally equivalent after replacing string-based node identification.
- A fallback node type is acceptable for unmapped grammar rules as long as behavior is deterministic and test-covered.
- Deep nesting validation is performed with synthetic and representative grammar samples sufficient to demonstrate non-recursive safety.
- Documentation quality checks are part of feature acceptance, not optional follow-up work.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of grammar rules designated for typed mapping resolve to deterministic `NodeType` values in acceptance tests.
- **SC-002**: Deep-nesting traversal acceptance tests complete successfully for inputs at or above 10,000 nested levels without recursion-related failure.
- **SC-003**: 100% of node-dispatch code paths in this feature use enum-based dispatch and 0 acceptance-test findings show runtime type inspection (`dynamic_cast` or `std::type_info`).
- **SC-004**: 100% of public APIs introduced by this feature include generated documentation with Motivation, Pre-conditions, and Post-conditions sections.
- **SC-005**: In code review sign-off, maintainers confirm the specification and tests demonstrate improved maintainability over string-based node identification for the targeted AST flow.
