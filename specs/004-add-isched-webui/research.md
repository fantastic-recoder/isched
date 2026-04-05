# Research: Isched WebUI

**Phase**: 0 (Outline & Research)  
**Updated**: 2026-04-05  
**Feature**: `004-add-isched-webui`

## Decision 1: Preserve GraphQL-only integration for all WebUI flows

- **Decision**: WebUI bootstrap, auth, organization, user, and RBAC flows use only `/graphql` over HTTP and WebSocket.
- **Rationale**: Required by constitution Principle II and FR-008/architecture constraints; avoids split auth and duplicate contracts.
- **Alternatives considered**:
  - Add REST admin endpoints: rejected as constitutional violation.
  - Hybrid GraphQL + REST fallback: rejected due to drift and larger test matrix.

## Decision 2: Use secure cookie, token-opaque JWT handling in browser

- **Decision**: JWT session credentials are sent via secure HttpOnly SameSite cookie(s); frontend never reads token values.
- **Rationale**: Required by FR-010 and constitution Security-First guidance.
- **Alternatives considered**:
  - `localStorage`/`sessionStorage`/IndexedDB token persistence: rejected by explicit requirements.
  - JS in-memory bearer token model: rejected because token remains script-readable.

## Decision 3: Enforce CSRF with double-submit token plus strict origin checks

- **Decision**: State-changing mutations require both CSRF double-submit validation and strict `Origin`/`Referer` validation.
- **Rationale**: Required by FR-010a and clarification session 2026-04-04.
- **Alternatives considered**:
  - Token-only CSRF defense: rejected as incomplete against spec constraints.
  - Origin-only defense: rejected as incomplete against spec constraints.

## Decision 4: Angular implementation follows modern standalone conventions

- **Decision**: Use standalone components, signal-first state, typed reactive forms, strict templates, and `@if/@for/@switch`; prefer `OnPush`/zoneless-compatible patterns.
- **Rationale**: Required by constitution Principle VI and spec Angular conventions.
- **Alternatives considered**:
  - NgModule-centric composition: rejected unless a third-party boundary forces it.
  - RxJS-first global mutable stores for all state: rejected where signal-first is sufficient.

## Decision 5: Explicit organization context guards all scoped writes

- **Decision**: Org-scoped mutations require explicit organization context and must fail with `CONTEXT_MISMATCH` when stale or mismatched.
- **Rationale**: Required by FR-011 and edge-case requirements (context switch safety).
- **Alternatives considered**:
  - Server-only inferred org context: rejected because it obscures client safety semantics.
  - Soft warning only on context switch: rejected because it cannot guarantee zero cross-org writes.

## Decision 6: Use optimistic concurrency for admin edit mutations

- **Decision**: Organization/user/custom-role edits include a version/revision field; stale writes fail atomically with `CONFLICT`.
- **Rationale**: Required by FR-006d and clarifications from 2026-04-05.
- **Alternatives considered**:
  - Last-write-wins: rejected due to silent overwrite risk.
  - Pessimistic locking for all edits: rejected due to higher contention and UX overhead.

## Decision 7: Preserve role assignments across deactivate/reactivate

- **Decision**: User deactivation keeps stored assignments but marks them ineffective until re-enabled.
- **Rationale**: Required by FR-006c and acceptance scenarios.
- **Alternatives considered**:
  - Delete assignments on deactivate: rejected due to requirement conflict.
  - Keep assignments effective for disabled users: rejected by least-privilege posture.

## Decision 8: Admin lists must be server-driven for scale baseline

- **Decision**: Organization, user, role, and assignment listing APIs mandate server-side pagination/filter/sort arguments and bounded result pages.
- **Rationale**: Required by FR-006f and FR-016a for 10,000 users / 1,000 roles baseline.
- **Alternatives considered**:
  - Client-side full dataset loading: rejected as non-compliant and non-scalable.

## Decision 9: Embedded static hosting is default runtime delivery

- **Decision**: In non-development mode, backend serves built WebUI assets with SPA fallback to `index.html`.
- **Rationale**: Required by FR-001a and keeps product delivery single-process.
- **Alternatives considered**:
  - Separate UI hosting service: rejected for this feature scope and deployment complexity.

## Decision 10: Local development uses Angular proxy for `/graphql` HTTP + WS

- **Decision**: Dev server routes `/graphql` and WebSocket upgrades through proxy config, not hard-coded origins.
- **Rationale**: Required by FR-013/FR-013a/FR-014 and constitution Technical Standards.
- **Alternatives considered**:
  - Hard-coded backend URL in frontend source: rejected by governance and environment portability concerns.

## Decision 11: Testing strategy includes auth helpers and real-backend Playwright

- **Decision**: Introduce shared GraphQL auth/CSRF test helpers and require Playwright bootstrap integration using real backend with temporary `--data-dir`.
- **Rationale**: Required by FR-010c, FR-010d, FR-017, and FR-018.
- **Alternatives considered**:
  - Per-test ad hoc auth wiring: rejected as brittle and duplicative.
  - Mock backend for bootstrap integration only: rejected because FR-018 requires real backend process.

## Decision 12: Audit and reliability baselines become explicit non-functional gates

- **Decision**: All admin mutations emit immutable audit events (success and failure) with 90-day retention, and operations docs include 99.5% monthly availability target with RTO <= 60 minutes.
- **Rationale**: Required by FR-019/FR-019a and FR-016b.
- **Alternatives considered**:
  - Success-only audit logging: rejected because failed-attempt traceability is required.
  - Best-effort retention with no minimum window: rejected by compliance baseline.

## Decision 13: Canonical embedded WebUI entry URL is `/isched`

- **Decision**: Treat `/isched` as the canonical browser URL for embedded runtime. HTTP `GET /` and browser `GET /graphql` are redirected to `/isched`.
- **Rationale**: Aligns embedded serving behavior with the earlier seed-mode admin UI artifact and removes ambiguity between API and browser entry paths.
- **Alternatives considered**:
  - Keep `/` as a second direct WebUI entry: rejected to avoid dual-entry ambiguity.
  - Serve content directly on `GET /graphql`: rejected because `/graphql` remains API-first (`POST`) and should not be a distinct WebUI surface.

