# Implementation Plan: WebUI Navigation + Status Bars

**Branch**: `007-webui-nav-status-bars` | **Date**: 2026-04-06 | **Spec**: `/home/groby/dev/isched/specs/007-webui-nav-status-bars/spec.md`
**Input**: Feature specification from `/home/groby/dev/isched/specs/007-webui-nav-status-bars/spec.md`

## Summary

Add a shared authenticated app-shell frame in Angular WebUI with a top navigation bar (asset-based isched logo + menu with clear active destination) and a bottom status bar (latest operation digest + current user display name), implemented with signal-backed state and DaisyUI/Tailwind styling. The design introduces a dedicated shell state service so digest and identity updates are deterministic, race-safe for rapid operation updates, and testable through focused unit suites plus Playwright smoke checks.

## Technical Context

**Language/Version**: C++23 (repo baseline) + Angular 21 / strict TypeScript for `src/ui`  
**Primary Dependencies**: Angular standalone APIs/signals, Angular Router, RxJS interop (`toSignal`), Tailwind CSS 3.x, DaisyUI 4.x, existing GraphQL service (`/graphql`), Playwright, Jest/Karma test harness in `src/ui`  
**Storage**: N/A for new persistence; consumes existing in-memory auth/session state and GraphQL-backed user/session data  
**Testing**: Angular unit/component tests (`*.spec.ts`) and Playwright smoke specs in `src/ui/e2e/`  
**Target Platform**: Linux dev/CI runtime for build/test + modern browsers for WebUI  
**Project Type**: Monorepo GraphQL backend + embedded Angular WebUI  
**Performance Goals**: Shell chrome renders with no route flicker on authenticated screens; digest update latency is UI-immediate on operation lifecycle transitions; no stale digest after rapid update bursts  
**Constraints**: GraphQL-only transport, no browser-persistent JWT storage, signal-first app-owned template state, separate `templateUrl`/`styleUrl`, responsive shell without hiding critical nav/status info by default  
**Scale/Scope**: App-shell changes across authenticated routes (`/dashboard`, `/admin/*`) plus shared service plumbing and regression coverage (unit + smoke E2E)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms where forms are present, strict TypeScript, zoneless/`OnPush`-compatible patterns, and no async-pipe-driven template state for app-owned state
- [x] Frontend API calls remain GraphQL `/graphql` only; no REST endpoints or alternate transports introduced
- [x] Browser JWT handling remains ephemeral/in-memory; no new persistent token storage introduced
- [x] Local Angular development keeps proxy routing for `/graphql` (HTTP + WS) and avoids hard-coded backend origins
- [x] Test plan includes required coverage before each user story closes (unit shell coverage + Playwright smoke flow)
- [x] Security-sensitive change gate: no new auth boundary/credential transport change; feature-scoped threat-model update is not required beyond existing auth model

**Gate result (pre-Phase 0)**: PASS  
**Gate result (post-Phase 1 design)**: PASS

## Project Structure

### Documentation (this feature)

```text
specs/007-webui-nav-status-bars/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── webui-shell-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
src/ui/
├── src/app/
│   ├── app.ts
│   ├── app.html
│   ├── app.scss
│   ├── app.spec.ts
│   ├── services/
│   │   ├── auth.service.ts
│   │   ├── graphql.service.ts
│   │   └── (new) shell-status.service.ts
│   └── pages/
│       ├── dashboard/
│       └── admin/
└── e2e/
    └── (new) shell-smoke.spec.ts
```

**Structure Decision**: Implement as a shared app-shell enhancement centered in `src/ui/src/app/` so authenticated pages inherit common top/bottom bars from the root shell, while page-level operations publish digest updates through a dedicated signal-first service.

## Dependencies, Risks, and Rollout

### Dependencies

- Existing logo asset at `src/ui/src/assets/isched_logo.jpg` (or equivalent final asset path used by the shell component)
- Existing authenticated user/session GraphQL context (`currentUser`) expanded or reused for display name exposure
- Existing admin/user operations (especially organization-user fetch) wired to publish digest lifecycle events
- Existing Angular routing for authenticated screens and active-route highlighting

### Key Risks and Mitigations

- **Risk**: Inconsistent digest updates across pages if some flows do not emit shell status events.  
  **Mitigation**: Define a single digest publication API and require each tracked operation path to use it; test representative flow in unit + smoke tests.
- **Risk**: Long digest text collides with username on narrow viewports.  
  **Mitigation**: Use responsive DaisyUI/Tailwind layout with truncation/ellipsis and non-overlapping regions; include viewport-focused smoke assertion.
- **Risk**: Username temporarily unavailable at initial render causes blank identity label.  
  **Mitigation**: Provide required non-empty fallback label and reactive update when identity resolves.
- **Risk**: Route-level shell duplication (old page-local nav vs new global nav) confuses users.  
  **Mitigation**: Consolidate authenticated navigation into app shell and trim redundant page-local chrome during implementation.

### Migration and Rollout Notes

- Rollout is UI-only and backward compatible with backend GraphQL transport.
- No data migration required.
- Use feature-branch regression gates (unit + Playwright smoke) before merge.
- If needed, rollout can be staged by enabling shell only on authenticated routes first, then removing redundant page-local nav fragments.

### Test Strategy

- Add/extend Angular unit tests for root shell render, nav active-state behavior, digest state transitions (loading/success/failure/latest-wins), and identity fallback-to-resolved behavior.
- Add Playwright smoke scenario validating: top nav visible with logo, nav menu navigation, bottom status bar visibility, digest transition for organization-user fetch, and current user label persistence.
- Keep existing backend tests unchanged except where UI-facing operation wording contracts are validated via frontend mocks.
- Run full frontend gate (`pnpm run test`, `pnpm run e2e`) and repository regression gate (`ctest --output-on-failure`) before merge.

## Complexity Tracking

No constitution violations or approved exceptions.
