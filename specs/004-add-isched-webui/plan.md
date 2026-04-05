# Implementation Plan: Isched WebUI

**Branch**: `004-add-isched-webui` | **Date**: 2026-04-05 | **Spec**: `/home/groby/dev/isched/specs/004-add-isched-webui/spec.md`
**Input**: Feature specification from `/home/groby/dev/isched/specs/004-add-isched-webui/spec.md`

## Summary

Deliver an embedded Angular 21 WebUI for bootstrap, organization/user administration, and RBAC management with strict GraphQL-only backend integration at `/graphql`. The design uses secure cookie-based JWT auth (token-opaque frontend), temporary auth lockout (5 failed attempts in 15 minutes -> 15-minute auto-unlock), CSRF double-submit plus Origin/Referer checks, explicit organization context guards, optimistic concurrency for admin edits, server-side pagination/filter/sort for large datasets, and canonical runtime WebUI routing at `/isched` with browser `GET /` and `GET /graphql` redirects to `/isched`.

## Technical Context

**Language/Version**: C++23 backend + TypeScript (strict) / Angular 21 frontend  
**Primary Dependencies**: `cpp-httplib`, `nlohmann_json`, `sqlite3`, `jwt-cpp`, `spdlog`, `boost` (backend); Angular standalone APIs, signals, Tailwind CSS, DaisyUI (frontend)  
**Storage**: SQLite per-tenant data + immutable audit-event persistence (minimum 90-day retention)  
**Testing**: Catch2 backend tests, GraphQL integration/contract tests, Playwright bootstrap integration, Angular unit/component tests  
**Target Platform**: Linux backend runtime + modern browser admin UI  
**Project Type**: Single-repo GraphQL backend with embedded SPA frontend  
**Performance Goals**: static asset GET p95 <= 200 ms; representative admin GraphQL p95 <= 300 ms at 50 VUs for 5 minutes; non-intentional error rate < 1%  
**Constraints**: GraphQL-only transport; secure HttpOnly SameSite cookie JWT model; temporary auth lockout after 5 failed attempts in 15 minutes with 15-minute auto-unlock; CSRF double-submit + Origin/Referer validation; no JWT persistence in script-readable storage; optimistic concurrency with `CONFLICT`; no hard delete/archive in scope  
**Scale/Scope**: 10,000 users + 1,000 roles per organization baseline; server-side pagination/filter/sort required for admin listings

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, and zoneless/`OnPush`-compatible patterns
- [x] App-owned template state is signal-backed; async-pipe template state from component-owned observables is not used except explicitly documented immutable third-party bridges
- [x] Frontend API calls use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced
- [x] Browser JWT handling avoids persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and uses secure cookie controls
- [x] Local Angular development uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames in client source
- [x] Test plan defines required coverage before each user story is marked complete
- [x] Security-sensitive changes include both feature-scoped and project-level threat-model updates

**Gate result (pre-Phase 0)**: PASS  
**Gate result (post-Phase 1 design)**: PASS

## Project Structure

### Documentation (this feature)

```text
specs/004-add-isched-webui/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── main/cpp/isched/
│   ├── backend/
│   └── shared/
├── test/cpp/
│   ├── isched/
│   └── integration/
└── ui/
    ├── src/
    ├── proxy.conf.json
    └── e2e/              ← Playwright specs + global setup/teardown
```

**Structure Decision**: Keep the existing single-repository backend + embedded Angular WebUI layout. Add WebUI contracts and design artifacts under `specs/004-add-isched-webui/` and validate flows through both backend and frontend automated test layers.

## Complexity Tracking

No constitution violations or approved exceptions in this plan.
