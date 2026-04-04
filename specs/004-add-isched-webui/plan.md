# Implementation Plan: Isched WebUI

**Branch**: `004-add-isched-webui` | **Date**: 2026-04-04 | **Spec**: [`spec.md`](spec.md)
**Input**: Feature specification from `/specs/004-add-isched-webui/spec.md`

## Summary

Add and harden the embedded Angular 21 WebUI for one-time bootstrap, organization/user administration, and RBAC management, while preserving the GraphQL-only `/graphql` integration contract. The plan enforces secure cookie-based JWT handling (token opaque to browser JavaScript), CSRF protection for mutations, strict multi-organization scope boundaries, backend-served static WebUI integration in non-development runtime, and a standalone local Angular development workflow via proxy (HTTP + WebSocket) with no hard-coded backend origins.

## Technical Context

Language/Version: C++23 backend + Angular 21 / TypeScript 5.9 (strict)  
Primary Dependencies: Angular standalone APIs, RxJS 7.8, Tailwind CSS 3.4, DaisyUI 4.12, GraphQL-over-HTTP + `graphql-transport-ws` semantics, existing backend stack (`boost`, `jwt-cpp`, `sqlite3`, `cpp-httplib` transport)  
Storage: Existing SQLite model (`isched_system.db` + organization DBs); WebUI holds transient view/form state in memory only  
Testing: Catch2 + `ctest` (backend), Angular unit/component tests (`ng test`/Jest), GraphQL integration coverage for auth/RBAC/scope/security flows  
Target Platform: Linux-hosted isched backend + modern browsers for Angular WebUI + local dev on Linux with Angular dev server proxy  
Project Type: Multi-tenant GraphQL backend with embedded web application  
Performance Goals: Bootstrap completion <5 minutes for first-time operator; admin create/edit flows first-attempt completion >=95%; no added REST round-trips; verify p95 static asset GET <=200 ms and p95 representative admin GraphQL <=300 ms at 50 concurrent virtual users for 5 minutes with <1% non-intentional errors  
Constraints: GraphQL-only external API, secure HttpOnly SameSite JWT cookies, no token persistence in script-readable storage, double-submit CSRF + strict Origin/Referer validation, strict organization scoping for admin mutations  
Scale/Scope: Platform bootstrap + organization CRUD scope controls + organization-scoped user lifecycle + built-in/custom role management + local proxy-backed developer workflow

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Initial Gate Review (Pre-Research)

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, zoneless/`OnPush`-compatible patterns
- [x] Frontend API calls use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced
- [x] Browser JWT handling avoids persistent token storage and uses secure cookie-based controls
- [x] Local Angular development uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames
- [x] Test plan includes required coverage before each user story is complete
- [x] Security-sensitive scope includes both feature-scoped and project-level threat-model updates
- [x] Plan includes explicit non-dev embedded WebUI serving verification and measurable performance/scalability evidence tasks aligned to constitution quality gates

### Post-Design Gate Review (After Phase 1)

- [x] `research.md`, `data-model.md`, `contracts/`, and `quickstart.md` keep GraphQL-only, Angular governance, and browser JWT constraints explicit
- [x] Contract design keeps mutation CSRF requirements (`double-submit` + `Origin`/`Referer`) and error surfacing rules testable
- [x] Design artifacts include role/scope boundaries and organization-context write guards to prevent cross-organization writes
- [x] Threat-model updates are planned as mandatory implementation tasks in Phase 2 generation

## Project Structure

### Documentation (this feature)

```text
specs/004-add-isched-webui/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- webui-graphql-contract.md
`-- tasks.md               # Generated later by /speckit.tasks
```

### Source Code (repository root)

```text
src/
|-- main/cpp/isched/
|   |-- backend/           # GraphQL resolvers, auth/csrf enforcement, server integration
|   `-- shared/            # shared config/utilities
|-- test/cpp/
|   |-- isched/            # unit tests
|   `-- integration/       # integration tests
`-- ui/
    |-- src/app/           # Angular standalone feature areas
    |-- src/environments/  # environment + proxy-aligned runtime config
    `-- ...                # Angular/Tailwind/DaisyUI project files

docs/
`-- security-threat-model.md
```

**Structure Decision**: Use the existing monorepo layout with backend and Angular code in-place (`src/main/cpp/isched` + `src/ui`). No new service boundary is introduced; WebUI remains embedded/product-integrated and talks only to `/graphql`.

## Phase Plan

### Phase 0 - Research

- Resolve implementation decisions for GraphQL client usage, CSRF token propagation model, and organization context protection UX.
- Define security and governance-aligned defaults for Angular 21 patterns (signals, standalone, typed forms, strict templates).
- Capture local proxy workflow and failure-diagnostics expectations.

### Phase 1 - Design & Contracts

- Define data model entities, validation rules, and state transitions for bootstrap, organization, user, role, assignment, and auth-session UI concerns.
- Specify GraphQL operation contracts for bootstrap/auth/org/user/rbac flows including authorization and CSRF error semantics.
- Produce quickstart workflow for backend-in-background + Angular proxy dev mode with verification steps.
- Refresh agent context for Copilot via update script.

### Phase 2 - Task Generation Readiness

- Inputs complete when `plan.md`, `research.md`, `data-model.md`, `contracts/`, and `quickstart.md` are current and constitution checks remain passing.

## Complexity Tracking

No constitution violations identified; complexity justification table not required.
