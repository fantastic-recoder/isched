# Tasks: Typed PEGTL AST Node Dispatch

**Input**: Design documents from `/specs/010-ast-enum-dispatch/`
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓

**Tests**: Required by FR-011 — feature acceptance tests must verify both functional parsing correctness and deep-nesting traversal safety.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story ([US1], [US2], [US3])
- Exact file paths are included in every task description

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the `shared/ast/` module structure and wire it into the build system so all subsequent tasks compile cleanly.

- [X] T001 Add `src/main/cpp/isched/shared/ast/` directory and register new AST source files (`isched_CustomNode.cpp`) in `src/main/cpp/isched/CMakeLists.txt` (add to `isched` library target)
- [X] T002 [P] Register new test executables (`isched_custom_node_tests`, `isched_visitor_tests`) and performance benchmark target (`ast_benchmarks`) in `src/test/cpp/isched/CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core type definitions that both US1 and US2 depend on. MUST be complete before any user story work starts.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T003 Define `NodeType` enum class with `uint8_t` underlying storage (including `Unknown` and `Sentinel` fallback values and all mapped GraphQL grammar rule types) in `src/main/cpp/isched/shared/ast/isched_NodeType.hpp`
- [X] T004 [P] Implement `CustomNode` struct inheriting from `tao::pegtl::parse_tree::node`, storing a `NodeType` member with minimal footprint, in `src/main/cpp/isched/shared/ast/isched_CustomNode.hpp` and `src/main/cpp/isched/shared/ast/isched_CustomNode.cpp`

**Checkpoint**: Foundation ready — user story implementation can begin

---

## Phase 3: User Story 1 — Build typed AST nodes during parse (Priority: P1) 🎯 MVP

**Goal**: Parse grammar input into AST nodes that carry a compact `NodeType` enum value so node identity checks no longer rely on rule-name strings.

**Independent Test**: Parse representative grammar inputs (mapped rules, unmapped rules, existing parse entry points) and verify every produced node has the expected typed identity while remaining compatible with `tao::pegtl::parse_tree::parse`.

### Tests for User Story 1

- [X] T005 [P] [US1] Write Catch2 acceptance tests covering: (a) mapped grammar rule → correct `NodeType` value, (b) unmapped rule → deterministic `NodeType::Unknown` fallback, (c) `tao::pegtl::parse_tree::parse` invocation with `NodeSelector` produces expected result-shape in `src/test/cpp/isched/isched_custom_node_tests.cpp`

### Implementation for User Story 1

- [X] T006 [US1] Implement `NodeSelector` template with `if constexpr` and `std::is_same_v` compile-time rule-to-`NodeType` mapping, including a `static transform` method compatible with `tao::pegtl::parse_tree::parse`, in `src/main/cpp/isched/shared/ast/isched_NodeSelector.hpp`
- [X] T007 [US1] Integrate `NodeSelector` and `CustomNode` with existing PEGTL parse entry points (replace or augment default node selector) in `src/main/cpp/isched/backend/isched_gql_grammar.hpp` while preserving existing `generate_ast_and_log` and `ast_node_to_str` behavior; all existing `isched_ast_node_tests` MUST pass without modification (backward-compatible drop-in)

**Checkpoint**: User Story 1 fully functional — typed node creation works and all `isched_custom_node_tests` pass

---

## Phase 4: User Story 2 — Traverse deeply nested trees safely and efficiently (Priority: P2)

**Goal**: Evaluate and traverse deeply nested AST trees through enum-driven dispatch and a non-recursive traversal pattern, resistant to stack overflow for inputs with 10,000+ nesting levels.

**Independent Test**: Construct synthetic trees with 10,000+ levels and validate traversal completes without recursion-related failure; confirm all dispatch branches use only `NodeType` enum values with zero `dynamic_cast` or `std::type_info` accesses.

### Tests for User Story 2

- [X] T008 [P] [US2] Write Catch2 acceptance tests in `src/test/cpp/isched/isched_visitor_tests.cpp` covering: (a) 10,000+ level nested tree traversal completes without stack overflow, (b) visitor dispatches exclusively on `NodeType` enum (assert no `dynamic_cast` code paths exist), (c) unknown/sentinel `NodeType` is handled without undefined behavior, (d) a tree containing both terminal leaf nodes and non-terminal nodes traverses in one pass and both node categories are processed correctly (covers spec edge case 4)

### Implementation for User Story 2

- [X] T009 [US2] Define `TraversalFrame` struct (current node pointer + traversal state) and `VisitorDispatchOutcome` enum (Success, UnknownNodeFallback) in `src/main/cpp/isched/shared/ast/isched_NonRecursiveVisitor.hpp`
- [X] T010 [US2] Implement stack-based non-recursive visitor (complete standalone implementation) using `std::stack<TraversalFrame>` — where `TraversalFrame` holds raw non-owning observer pointers to `CustomNode` — that dispatches behavior via `switch` on `NodeType`, with explicit defined behavior for `NodeType::Unknown` and `NodeType::Sentinel`, in `src/main/cpp/isched/shared/ast/isched_NonRecursiveVisitor.hpp`
- [X] T011 [P] [US2] Implement deep-nesting performance benchmark (synthetic 10,000-level tree construction and traversal timing) in `src/test/cpp/isched/isched_ast_benchmarks.cpp`; measure both enum-dispatch and a string-comparison baseline on the same synthetic trees and record the ratio; document measured node metadata footprint and cache-locality rationale in inline comments; register target in `src/test/cpp/isched/CMakeLists.txt` (covered by T002)

**Checkpoint**: User Stories 1 and 2 both independently functional and tested; deep-nesting traversal verified safe

---

## Phase 5: User Story 3 — Consume maintainable API contracts and docs (Priority: P3)

**Goal**: Complete Doxygen documentation on all public classes and methods introduced by this feature, including Motivation, Pre-conditions, and Post-conditions sections on every item.

**Independent Test**: Run `cmake --build cmake-build-debug --target docs` and confirm Doxygen output contains Motivation, Pre-conditions, and Post-conditions sections for all public classes and methods in the `shared/ast/` module.

### Verification for User Story 3

- [X] T012 [P] [US3] Run `cmake --build cmake-build-debug --target docs` and verify generated HTML/XML contains `@pre`, `@post`, and `@par Motivation` sections for `CustomNode`, `NodeType`, `NodeSelector`, `TraversalFrame`, `VisitorDispatchOutcome`, and the non-recursive visitor skeleton

### Documentation for User Story 3

- [X] T013 [US3] Add complete Doxygen documentation with `@brief`, `@par Motivation`, `@pre`, `@post`, `@param`, `@return`, `@throw`, and usage `@code` block to `CustomNode` and all public members in `src/main/cpp/isched/shared/ast/isched_CustomNode.hpp`
- [X] T014 [P] [US3] Add complete Doxygen documentation with `@brief`, `@par Motivation`, `@pre`, `@post` to `NodeType` enum and every enumerator in `src/main/cpp/isched/shared/ast/isched_NodeType.hpp`
- [X] T015 [P] [US3] Add complete Doxygen documentation with `@brief`, `@par Motivation`, `@pre`, `@post` to `NodeSelector` template and its `transform` static method in `src/main/cpp/isched/shared/ast/isched_NodeSelector.hpp`
- [X] T016 [P] [US3] Add complete Doxygen documentation with `@brief`, `@par Motivation`, `@pre`, `@post` to `TraversalFrame`, `VisitorDispatchOutcome`, and the non-recursive visitor class in `src/main/cpp/isched/shared/ast/isched_NonRecursiveVisitor.hpp`

**Checkpoint**: All user stories independently functional; documentation verified complete on all public APIs

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, cache-locality rationale, security check, and clean code review.

- [X] T017 [P] Document and justify node metadata footprint for cache locality in deep-tree workloads as a `@par CacheLocalityRationale` Doxygen section in `src/main/cpp/isched/shared/ast/isched_CustomNode.hpp` (per FR-007 and SC-003)
- [X] T018 Clean code review: (1) verify all new functions are small and focused; (2) document the polymorphism/`switch` trade-off — FR-006 prohibits `dynamic_cast`/`std::type_info`, ruling out virtual dispatch, therefore `switch` on `NodeType` is the only constitutionally compliant dispatch mechanism; record this justification as a `@par DesignRationale` Doxygen comment in `isched_NonRecursiveVisitor.hpp`; (3) confirm hot-path abstraction trade-offs are supported by profiling ratio from T011 benchmark results (constitution §"Balancing Clean Code with Performance")
- [X] T019 Run full test suite `cd cmake-build-debug && ctest --output-on-failure` and verify all success criteria are met: SC-001 (100% deterministic NodeType), SC-002 (10,000+ depth completes), SC-003 (0 dynamic_cast findings), SC-004 (100% documented APIs), SC-005 (maintainability confirmed), SC-006 (T011 benchmark ratio recorded in benchmark output)
- [X] T020 [P] Run security scan `cmake --build ./cmake-build-debug/ --target security_scan` and resolve any findings

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — **BLOCKS all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 — no dependency on US2 or US3
- **US2 (Phase 4)**: Depends on Phase 2 — no dependency on US3; may build on US1 `NodeType` and `CustomNode`
- **US3 (Phase 5)**: Depends on Phases 3 and 4 — documentation of APIs that must exist first
- **Polish (Phase 6)**: Depends on all user story phases complete

### User Story Dependencies

```
Phase 1 (Setup)
    └─► Phase 2 (Foundational: NodeType + CustomNode)
            ├─► Phase 3 (US1: NodeSelector + parse integration)
            │       └─► Phase 5 (US3: documentation — needs US1 APIs)
            └─► Phase 4 (US2: NonRecursiveVisitor + deep traversal)
                    └─► Phase 5 (US3: documentation — needs US2 APIs)
```

### Parallel Opportunities Per Phase

**Phase 1**: T001 → T002 [P] (after T001: different files)

**Phase 2**: T003 → T004 [P] (different header files)

**Phase 3 (US1)**: T005 [P] can start alongside T006 [P] (test file and header are independent); T007 depends on T005+T006 complete

**Phase 4 (US2)**: T008 [P] can start alongside T009 (test stub and type definitions are independent); T010 depends on T009; T011 [P] can run alongside T010

**Phase 5 (US3)**: T012 runs after T013–T016; T013–T016 are all [P] (separate files)

**Phase 6**: T017 [P] and T020 [P] can run alongside T018; T019 must be last

---

## Implementation Strategy

**MVP Scope**: Phase 3 (User Story 1) alone delivers the core value — typed node creation replaces string-based identification. US2 and US3 can follow incrementally.

**Recommended Execution Order**:
1. Phase 1 (T001, T002) — ~30 min
2. Phase 2 (T003, T004) — ~1 hour  
3. Phase 3 / US1 MVP (T005, T006, T007) — ~2 hours; run `ctest` to confirm green
4. Phase 4 / US2 (T008–T011) — ~2–3 hours; run `ctest` to confirm green
5. Phase 5 / US3 (T012–T016) — ~2 hours; generate docs to confirm green
6. Phase 6 Polish (T017–T020) — ~1 hour

**Commit cadence**: Commit after each user story phase once `ctest --output-on-failure` passes 100%.
