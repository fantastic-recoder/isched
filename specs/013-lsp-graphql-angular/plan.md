# Implementation Plan: LSP GraphQL Angular Integration

**Branch**: `013-lsp-graphql-angular` | **Date**: 2026-11-08 | **Spec**: `/specs/013-lsp-graphql-angular/spec.md`
**Input**: Feature specification from `/specs/013-lsp-graphql-angular/spec.md`

## Summary

Implement end-to-end Angular WebUI integration over GraphQL-only transport (`/graphql` via HTTP/WebSocket), with secure browser auth handling (no persistent token storage), robust error/retry UX, and local dev proxy parity. Work spans Angular client integration, GraphQL operation contracts, auth/session handling, and dev/prod transport alignment under existing C++23 backend + GraphQL architecture.

## Technical Context

**Language/Version**: C++23 backend, TypeScript (Angular 21) frontend  
**Primary Dependencies**: GraphQL endpoint `/graphql`, Angular standalone APIs, Angular signals, typed reactive forms, RxJS (only where stream semantics required), WebSocket transport for subscriptions, Conan-managed C++ dependencies, CMake/Ninja build toolchain  
**Storage**: Backend existing ACID data store (no storage model changes required for this feature); browser-side auth context in-memory only  
**Testing**: C++ tests via existing CMake/CTest suites; frontend unit/component tests (Angular test stack); frontend/backend integration tests for GraphQL HTTP + WebSocket and auth/error flows  
**Target Platform**: Linux-hosted backend services and modern browsers for Angular WebUI; local developer environments using Angular dev server proxy  
**Project Type**: Full-stack web application (C++ backend + Angular frontend)  
**Performance Goals**: Meet SC-001: 95% of primary user-flow GraphQL operations render user-visible response in ≤2s in local/test under normal load; avoid regressions on backend core paths  
**Constraints**: GraphQL-only transport; no REST fallback; strict Angular modern patterns; no JWT persistence in localStorage/sessionStorage/IndexedDB; secure transport controls; proxy-based local routing with WS upgrades; constitution-driven threat-model and test coverage gates  
**Scale/Scope**: Feature-scoped integration affecting one prioritized end-to-end flow plus auth/session behavior and local dev routing; includes query/mutation path and subscription-capable transport handling

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

## Project Structure

### Documentation (this feature)

```text
specs/013-lsp-graphql-angular/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
└── tasks.md  # created later by /speckit.tasks
```

### Source Code (repository root)

```text
src/
├── auth/                    # JWT/session/auth pipeline (backend)
├── graphql/                 # GraphQL schema/resolvers/transport (backend)
├── lsp/                     # Existing LSP/domain services
└── ...

src/ui/
├── src/
│   ├── app/
│   │   ├── features/        # Standalone feature components
│   │   ├── core/graphql/    # GraphQL client, auth link/interceptors, transport setup
│   │   ├── core/auth/       # In-memory auth/session context
│   │   └── shared/
│   └── ...
└── proxy*.json              # Angular dev proxy config for /graphql + WS

tests/
├── integration/             # GraphQL transport/auth integration
├── contract/                # GraphQL contract/schema checks
└── ...
```

**Structure Decision**: Use existing monorepo layout with backend in `src/` and Angular frontend in `src/ui/`; add/modify feature-scoped modules under these roots rather than introducing new top-level projects.

## Phase 0: Research & Decisions

### Decision 1: Frontend GraphQL transport architecture
- **Decision**: Use a single GraphQL client abstraction configured for `/graphql`, splitting HTTP for queries/mutations and WebSocket for subscriptions, with centralized error mapping.
- **Rationale**: Enforces GraphQL-only boundary and keeps transport concerns isolated from feature components.
- **Alternatives considered**:
  - Direct per-component fetch/WebSocket calls (rejected: duplicates transport logic, weak consistency)
  - REST fallback adapters (rejected: violates constitutional/API constraints)

### Decision 2: Browser auth/session handling
- **Decision**: Keep auth context in memory only; prefer secure cookie-backed server session transport where available, or ephemeral in-memory bearer material with strict lifecycle controls; never persist JWT to browser storage.
- **Rationale**: Satisfies constitution security requirement and reduces token exfiltration persistence risk.
- **Alternatives considered**:
  - localStorage/sessionStorage token persistence (rejected: constitution violation)
  - IndexedDB token vault (rejected: constitution violation)

### Decision 3: Angular UI architecture patterns
- **Decision**: Implement/modify UI using standalone components, signal-first state containers, typed reactive forms, modern template control flow, and OnPush-compatible update paths.
- **Rationale**: Required by constitution and aligns with Angular 21 best practices/performance model.
- **Alternatives considered**:
  - NgModule-centric feature packaging (rejected: legacy pattern)
  - Observable-first template state with async pipes for app-owned state (rejected: constitutional prohibition)

### Decision 4: Local development API routing
- **Decision**: Configure Angular dev-server proxy for `/graphql` HTTP and WebSocket upgrade forwarding to backend origin, with no hard-coded backend hostnames in app code.
- **Rationale**: Ensures local behavior mirrors deployment transport and avoids CORS/workflow drift.
- **Alternatives considered**:
  - Hard-coded environment URLs in frontend source (rejected: violates constraints)
  - Enabling broad CORS for direct browser-to-backend in dev (rejected: weaker parity and security posture)

### Decision 5: Validation and performance strategy
- **Decision**: Validate through layered tests: frontend unit/component, GraphQL contract checks, integration tests covering success/error/auth expiry/retry + proxy WS path; add latency assertions for key flow in test/local environment.
- **Rationale**: Directly traces to acceptance scenarios and measurable success criteria.
- **Alternatives considered**:
  - Manual QA-only verification (rejected: insufficient repeatability)
  - Backend-only testing without frontend integration (rejected: misses user-visible behavior)

## Phase 1: Design & Contracts

### Data Model

See `/specs/013-lsp-graphql-angular/data-model.md` for full entity definitions. Core entities include:
- `GraphQLOperation`
- `AuthSessionContext`
- `ProxyRouteConfig`
- `UiFlowState`
- `GraphQLErrorPresentation`

### Interface Contracts

Contracts are defined under `/specs/013-lsp-graphql-angular/contracts/`:
- `graphql-operations.md`: Operation-level contract for primary flow query/mutation/subscription shapes, variables, and error envelopes.
- `auth-session-contract.md`: Browser/session auth behavioral contract and storage prohibitions.
- `dev-proxy-contract.md`: Local proxy behavior contract for HTTP + WebSocket.

### Component Boundaries

- **Backend GraphQL boundary (`src/graphql`, `src/auth`)**
  - Exposes schema/resolvers over `/graphql`
  - Enforces authn/authz and normalized GraphQL error categories
- **Frontend GraphQL transport (`src/ui/.../core/graphql`)**
  - Owns client setup, link splitting, retry/error mapping, auth context injection
- **Frontend auth context (`src/ui/.../core/auth`)**
  - Owns in-memory session state and auth lifecycle events
- **Feature UI components (`src/ui/.../features`)**
  - Own user interaction, typed forms, and signal-driven view models
  - Consume transport abstractions; never call raw network APIs directly
- **Dev-experience config (`src/ui/proxy*.json`)**
  - Owns local API route forwarding and WS upgrade compatibility

### Dependency Considerations

- Preserve Conan/CMake invariants for backend dependencies.
- Reuse existing GraphQL/auth infrastructure before adding new libs.
- Any frontend dependency addition must support strict typing, tree-shaking, and Angular 21 compatibility.
- Prefer no new runtime dependencies unless justified by measurable simplification or reliability gains.

### Testing & Validation Strategy

- **Frontend unit/component**: signal state transitions, typed form validation, error-state rendering, retry affordances.
- **Integration (frontend↔backend)**: primary happy path, GraphQL recoverable error, auth expiry mid-session, subscription transport path.
- **Contract checks**: operation shape/schema compatibility and error envelope expectations.
- **Security checks**: automated assertions that JWT is not persisted to localStorage/sessionStorage/IndexedDB; verify secure cookie/equivalent controls in documented flows.
- **Performance checks**: capture latency metrics for primary operations against SC-001 threshold.

### Rollout Notes

- Roll out behind feature flag/config toggle if cross-flow impact is possible.
- Deploy backend GraphQL contract updates before enabling dependent UI behavior.
- Keep fallback behavior as graceful degradation within GraphQL (error handling/retry), not transport fallback to REST.
- Update documentation and threat-model artifacts before marking stories complete.

## Phase 1 Artifacts Status

- [x] `research.md` generated
- [x] `data-model.md` generated
- [x] `contracts/` generated
- [x] `quickstart.md` generated
- [x] Agent context update script run

## Post-Design Constitution Check

- [x] All constitutional gates remain satisfied after design.
- [x] No unresolved clarifications remain.
- [x] No gate violations requiring Complexity Tracking.

## Complexity Tracking

No constitutional violations or exceptions identified.
