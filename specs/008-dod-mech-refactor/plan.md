# Implementation Plan: Data-Oriented Refactor Passes

**Branch**: `008-dod-mech-refactor` | **Date**: 2026-04-06 | **Spec**: `/home/groby/dev/isched/specs/008-dod-mech-refactor/spec.md`
**Input**: Feature specification from `/home/groby/dev/isched/specs/008-dod-mech-refactor/spec.md`

## Summary

Execute iterative, performance-first refactor passes that optimize measured hot paths, migrate selected hot-loop data to SoA-style contiguous/index-addressable layouts, and preserve externally visible behavior. Each pass is accepted only with green tests, affected-scope coverage gates (>=80% line and branch), and complete artifacts (source/test updates, performance summary, docs/spec updates).

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: C++23  
**Primary Dependencies**: Catch2 3.x, Boost 1.84, cpp-httplib, nlohmann_json, spdlog, jwt-cpp, sqlite3, taocpp-pegtl  
**Storage**: SQLite3 (embedded, per-tenant)  
**Testing**: Catch2 + `ctest --output-on-failure`; coverage gates via `gcovr` for affected scope  
**Target Platform**: Linux (primary), CMake/Conan portable build pipeline  
**Project Type**: Monorepo C++ backend/library + Angular WebUI  
**Performance Goals**: No selected hot-path regressions per pass; branch-cost reduction or no-regression when branch elimination is applied; measurable gain in at least one selected metric in most passes  
**Constraints**: Performance over clean-code preference on hot paths, SoA transition where feasible, pointer-to-index replacement for selected structures, preserve behavior, green tests, >=80% line/branch affected-scope coverage, per-pass docs/spec updates  
**Scale/Scope**: Incremental backend subsystem refactors, pass-by-pass, with rollback-friendly boundaries

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only); this feature changes internals only.
- [x] Language/build baseline preserved (C++23, Conan dependencies, CMake + Ninja).
- [x] WebUI changes are out of scope; Angular standards remain unaffected.
- [x] Frontend API transport remains GraphQL-only; no REST additions.
- [x] Browser JWT handling unchanged; no persistent token storage introduced.
- [x] Angular dev proxy behavior unchanged; no client hard-coded backend hosts added.
- [x] Test/coverage plan includes per-pass green tests and affected-scope >=80% line/branch gates.
- [x] No new security-sensitive interface/auth flow is introduced; threat-model impact is none.
- [x] Clean code compliance is maintained with performance-first hot-path tradeoffs explicitly documented in pass summaries.

**Post-Design Re-check**: PASS. Phase 1 artifacts (`research.md`, `data-model.md`, `contracts/refactor-pass-artifact.schema.json`, `quickstart.md`) preserve all constitution gates.

## Project Structure

### Documentation (this feature)

```text
specs/008-dod-mech-refactor/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── refactor-pass-artifact.schema.json
└── tasks.md             # Created by /speckit.tasks
```

### Source Code (repository root)
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. Delete unused options and expand the chosen structure with
  real paths (e.g., apps/admin, packages/something). The delivered plan must
  not include Option labels.
-->

```text
src/
├── main/cpp/isched/
│   ├── backend/
│   └── shared/
├── test/cpp/isched/
└── ui/

tests/
specs/
docs/
tools/
```

**Structure Decision**: Keep existing monorepo structure. Refactor code changes target `src/main/cpp/isched/` and `src/test/cpp/isched/`; planning and acceptance artifacts live under `specs/008-dod-mech-refactor/`.

## Phase Plan

### Phase 0 - Research

- Resolve measurement, coverage-gate, and migration-pattern decisions for repeatable pass execution.
- Define branch-elimination options and when to keep original logic if no measurable gain exists.

### Phase 1 - Design & Contracts

- Define entities and state transitions for pass lifecycle and quality gates.
- Define a machine-readable artifact contract schema for pass acceptance evidence.
- Publish quickstart steps for one complete pass from baseline capture through documentation closure.

### Phase 2 - Task Planning (for /speckit.tasks)

- Slice implementation into independent passes per subsystem hot path.
- For each pass, sequence work as: baseline -> data layout -> logic refactor -> tests/coverage -> perf/docs.
- Enforce no-merge policy until tests, coverage gates, and artifact contract are all satisfied.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| None | N/A | N/A |
