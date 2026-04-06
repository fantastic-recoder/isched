# Implementation Plan: Typed PEGTL AST Node Dispatch

**Branch**: `010-ast-enum-dispatch` | **Date**: 2026-04-06 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/010-ast-enum-dispatch/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Replace string-based rule identification in PEGTL AST nodes with high-performance enum class dispatch. Implementation includes a CustomNode type with NodeType enum storage, NodeSelector template for compile-time rule mapping, and stack-based non-recursive visitor pattern for safe deep tree traversal. This optimization maintains PEGTL compatibility while improving performance and cache locality for GraphQL parsing in the isched backend.

## Technical Context

**Language/Version**: C++23 (required by constitution)  
**Primary Dependencies**: taocpp-pegtl, nlohmann_json, spdlog, Catch2 3.x  
**Storage**: N/A (in-memory AST processing)  
**Testing**: Catch2 3.x  
**Target Platform**: Linux primary, cross-platform (Conan-managed dependencies)  
**Project Type**: C++ library (AST processing component for GraphQL backend)  
**Performance Goals**: High-performance enum dispatch, cache-locality optimization for deep trees, non-recursive traversal for stack safety  
**Constraints**: Must integrate with existing PEGTL parse flows, maintain compatibility with tao::pegtl::parse_tree::parse API, support deep nesting (10,000+ levels) without recursion  
**Scale/Scope**: NEEDS CLARIFICATION - specific node type count, expected AST depth distributions, performance benchmarks vs baseline

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only) - N/A: Internal AST processing, no external interfaces
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja) - Confirmed: C++23, existing build system
- [x] WebUI changes (if any) follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, zoneless/`OnPush`-compatible patterns, and no async-pipe-driven template state for app-owned UI state - N/A: No WebUI changes
- [x] Frontend API calls (if any) use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced - N/A: No frontend changes
- [x] Browser JWT handling (if any) avoids persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and documents secure cookie or equivalent controls - N/A: No authentication changes
- [x] Local Angular development (if any) uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames in client source - N/A: No Angular development
- [ ] Test plan proves required coverage before each user story is marked complete - To be addressed in Phase 1: deep nesting tests, enum dispatch verification
- [x] Security-sensitive changes include both feature-scoped and project-level threat-model updates - N/A: Internal optimization, no security surface changes
- [ ] New code follows Clean Code Principles: functions are small and focused, polymorphism preferred over complex conditional chains, and any hot-path abstraction trade-offs are documented with profiling evidence - To be addressed in design: enum dispatch (polymorphism over conditionals), performance benchmarking required

## Project Structure

### Documentation (this feature)

```text
specs/010-ast-enum-dispatch/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/main/cpp/isched/
├── shared/
│   ├── ast/                    # NEW: AST node system
│   │   ├── isched_CustomNode.hpp
│   │   ├── isched_CustomNode.cpp
│   │   ├── isched_NodeType.hpp
│   │   ├── isched_NodeSelector.hpp
│   │   └── isched_NonRecursiveVisitor.hpp
│   ├── config/
│   ├── exceptions/
│   └── fs/
├── backend/                    # Existing GraphQL processing
│   ├── isched_Server.hpp
│   ├── isched_GqlExecutor.hpp
│   └── ...
└── runtime/

src/test/cpp/isched/
├── isched_ast_node_tests.cpp        # NEW: Unit tests for AST system
├── isched_custom_node_tests.cpp     # NEW: CustomNode specific tests
├── isched_visitor_tests.cpp         # NEW: Visitor pattern tests
├── isched_graphql_tests.cpp         # Existing: Integration with GraphQL parsing
└── ...

src/test/cpp/performance/
└── ast_benchmarks.cpp               # NEW: Performance validation tests
```

**Structure Decision**: AST components are placed in `src/main/cpp/isched/shared/ast/` as they provide foundational infrastructure for GraphQL parsing used across the backend. This follows the existing pattern where `shared/` contains cross-cutting utilities (config, exceptions, fs) that are consumed by higher-level components in `backend/`.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |
