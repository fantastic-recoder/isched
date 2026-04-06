# Implementation Plan: Improvement-Ratio Recovery Passes

**Branch**: `009-raise-pass-improvement-ratio` | **Date**: 2026-04-06 | **Spec**: `/home/groby/dev/isched/specs/009-raise-pass-improvement-ratio/spec.md`
**Input**: Feature specification from `/specs/009-raise-pass-improvement-ratio/spec.md`

## Summary

Recover SC-002 by executing additional auditable recovery passes that raise cumulative improving-pass ratio from baseline `1/2` to at least `>=90%` (minimum target `9/10`) while preserving all existing pass gates from feature 008: green tests, affected-scope line/branch coverage `>=80%`, artifact schema validation, and docs/spec updates. The technical approach formalizes a cumulative ratio ledger contract, per-pass artifact contract, and deterministic validation flow so acceptance decisions are reproducible from recorded evidence only.

## Technical Context

**Language/Version**: C++23 for product/test code; Python 3 for artifact-validation helpers and workflow scripts  
**Primary Dependencies**: Catch2 3.x, CMake + Ninja, Conan 2.x, existing `tools/refactor_pass/*` shell/Python tooling, JSON Schema Draft 2020-12 contracts  
**Storage**: File-based evidence in `specs/009-raise-pass-improvement-ratio/artifacts/` (JSON/Markdown/text gate outputs)  
**Testing**: CTest (`ctest --output-on-failure` + pass-relevant suite filters), affected-scope coverage via `tools/refactor_pass/run_affected_coverage.sh`, artifact validation via `tools/refactor_pass/validate_pass_artifact.py`  
**Target Platform**: Linux (primary supported platform in current build/test workflow)  
**Project Type**: C++ backend project with spec-driven artifact workflow and validation scripts  
**Performance Goals**: Increase cumulative improving-pass ratio to `>=90%` from baseline `1/2`; each counted pass improves at least one selected hot-path metric under repeatable measurement conditions  
**Constraints**: Keep all existing 008 quality gates unchanged; no denominator cherry-picking; cumulative ledger must include every completed pass and remain independently auditable  
**Scale/Scope**: Minimum eight additional improving passes from current baseline; typical evidence set is 10+ completed passes with per-pass metric/gate proofs

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes (if any) follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, zoneless/`OnPush`-compatible patterns, and no async-pipe-driven template state for app-owned UI state
- [x] Frontend API calls (if any) use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced
- [x] Browser JWT handling (if any) avoids persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and documents secure cookie or equivalent controls
- [x] Local Angular development (if any) uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames in client source
- [x] Test plan proves required coverage before each user story is marked complete
- [x] Security-sensitive changes include both feature-scoped and project-level threat-model updates
- [x] New code follows Clean Code Principles: functions are small and focused, polymorphism preferred over complex conditional chains, and any hot-path abstraction trade-offs are documented with profiling evidence

Constitution gate result (pre-research): **PASS**

Post-design re-check (after Phase 1 artifacts): **PASS**

## Project Structure

### Documentation (this feature)

```text
specs/009-raise-pass-improvement-ratio/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   |-- recovery-pass-artifact.schema.json
|   |-- improvement-ratio-ledger.schema.json
|   `-- compliance-decision-record.schema.json
`-- tasks.md  # Created later by /speckit.tasks
```

### Source Code (repository root)

```text
src/
|-- main/cpp/isched/
|   |-- backend/
|   `-- shared/
`-- test/cpp/isched/

tools/refactor_pass/
|-- verify_pass_gates.sh
|-- validate_pass_artifact.py
|-- run_affected_coverage.sh
`-- collect_perf.sh

specs/008-dod-mech-refactor/artifacts/
`-- passes-rollup.md  # Baseline 1/2 evidence source
```

**Structure Decision**: Use the existing single-project C++ repository layout and feature-scoped spec artifacts under `specs/009-raise-pass-improvement-ratio/`; no new top-level projects or transport surfaces are introduced.

## Complexity Tracking

No constitution violations or exceptions are required for this plan.
