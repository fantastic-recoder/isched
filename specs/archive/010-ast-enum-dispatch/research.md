# Research: Typed PEGTL AST Node Dispatch

**Date**: 2026-04-06  
**Feature**: 010-ast-enum-dispatch  
**Status**: Resolved via direct codebase inspection — full findings encoded in spec.md §Key Entities

## Research Tasks

### Task 1: Scale and Scope Analysis

**Question**: What are the specific node type count, expected AST depth distributions, and performance benchmarks vs baseline for the isched GraphQL parsing system?

**Resolution**: Inspected `isched_gql_grammar.hpp` directly. The grammar defines 39 meaningful rule structs that warrant typed dispatch; the remainder (lexical primitives, ignored tokens, punctuators) fall back to `NodeType::Unknown`. Full enumeration is in `spec.md §Key Entities → NodeType mapping table`. Baseline performance comparison is deferred to T011 benchmark implementation (synthetic 10,000-level trees, enum-dispatch vs string-comparison measurement).

### Task 2: PEGTL CustomNode Integration Patterns

**Question**: What are the best practices for integrating custom node types with PEGTL parse_tree::parse while maintaining API compatibility?

**Resolution**: PEGTL's `parse_tree::parse` accepts a `NodeSelector` template parameter. A valid selector must expose a `transform` static method that receives a constructed `node_up` and may mutate it before insertion into the tree. The existing `isched_ast_node_tests.cpp` exercises this path already via `generate_ast_and_log`. The new `NodeSelector` must preserve this call shape.

### Task 3: Enum Dispatch Performance Optimization

**Question**: What are the proven patterns for high-performance enum-based dispatch in C++23, particularly for AST traversal workloads?

**Resolution**: `switch` on `uint8_t` produces a jump table on all mainstream compilers (GCC, Clang) with O2+. FR-006 prohibits `dynamic_cast`/`std::type_info`, ruling out virtual dispatch alternatives. Performance trade-off vs. the constitution's "Prefer Polymorphism" principle is documented as a `@par DesignRationale` in `isched_NonRecursiveVisitor.hpp` (T018) and justified by baseline profiling from T011.