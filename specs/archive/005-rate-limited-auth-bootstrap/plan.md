# Implementation Plan: RATE_LIMITED + Auth Bootstrap Consistency

**Branch**: `005-rate-limited-auth-bootstrap` | **Date**: 2026-04-05 | **Spec**: `/home/groby/dev/isched/specs/archive/005-rate-limited-auth-bootstrap/spec.md`
**Input**: Feature specification from `/home/groby/dev/isched/specs/archive/005-rate-limited-auth-bootstrap/spec.md`

## Summary

Align backend GraphQL auth rate-limit signaling and Angular startup/bootstrap/auth behavior so operators always get deterministic outcomes: `RATE_LIMITED` feedback with retry guidance, stable first-route decisions under seed/non-seed mode, single-flight request suppression, and immediate bootstrap-to-auth recovery handling. Implementation centers on error-contract normalization, route/bootstrap guard ordering, and explicit frontend state modeling with signal-backed UI alerts and pending-state gates.

## Technical Context

**Language/Version**: C++23 backend + Angular 21 / strict TypeScript frontend  
**Primary Dependencies**: `cpp-httplib`, `jwt-cpp`, `nlohmann_json`, `sqlite3`, `spdlog`, `boost` (backend); Angular Router, typed reactive forms, signals, RxJS interop (frontend)  
**Storage**: SQLite (`isched_system.db` + tenant databases), in-memory frontend auth/bootstrap state  
**Testing**: Catch2 (`src/test/cpp/isched`, `src/test/cpp/integration`), Angular unit tests, Playwright E2E under `src/ui/e2e`  
**Target Platform**: Linux server runtime + modern browsers for WebUI  
**Project Type**: Single-repo GraphQL backend + embedded Angular WebUI  
**Performance Goals**: deterministic startup route correctness >= 95% across mode/session permutations; bootstrap completion-to-dashboard path under 30s in clean env verification  
**Constraints**: GraphQL-only external access at `/graphql`; no persistent JWT storage in browser APIs; single-flight auth/bootstrap submissions; deterministic alert mapping for repeated backend error categories  
**Scale/Scope**: Auth/bootstrap entry flows, startup navigation, route-guard revalidation, and deterministic lockout UX for platform operators

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

**Gate result (pre-Phase 0)**: PASS  
**Gate result (post-Phase 1 design)**: PASS

## Project Structure

### Documentation (this feature)

```text
specs/archive/005-rate-limited-auth-bootstrap/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── threat-model.md
├── contracts/
│   └── graphql-auth-bootstrap-consistency.md
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── main/cpp/isched/
│   ├── backend/
│   │   ├── isched_GqlExecutor.cpp
│   │   ├── isched_Server.cpp
│   │   └── isched_RateLimiter.hpp
│   └── shared/
├── test/cpp/
│   ├── integration/
│   │   ├── test_rate_limiting.cpp
│   │   └── test_seed_mode.cpp
│   └── isched/
└── ui/
    ├── src/app/
    │   ├── app.routes.ts
    │   ├── guards/
    │   ├── pages/{bootstrap,login,seed}/
    │   └── services/{auth,bootstrap,graphql}.service.ts
    ├── e2e/
    └── proxy.conf.json
```

**Structure Decision**: Keep the existing single-repo backend + embedded Angular WebUI layout and scope implementation to auth/bootstrap contracts, route guards, and frontend deterministic state handling with corresponding backend/integration/frontend test layers.

## Complexity Tracking

No constitution violations or approved exceptions.
