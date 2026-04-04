# Research: Isched WebUI

**Phase**: 0 (Outline & Research)  
**Created**: 2026-04-04  
**Feature**: `004-add-isched-webui`

## Decision 1: Keep GraphQL as the only WebUI backend interface

- **Decision**: WebUI data and auth state checks use only `/graphql` (HTTP for query/mutation, WebSocket for subscription where needed). No REST bootstrap/auth/admin endpoints are added.
- **Rationale**: This is required by FR-008 and constitutional GraphQL-only governance. It avoids split authorization logic and keeps backend contract consistent.
- **Alternatives considered**:
  - Add REST endpoints for bootstrap/admin convenience: rejected because it violates GraphQL-only constraints.
  - Hybrid GraphQL + REST fallback in UI: rejected because it increases drift and test matrix size.

## Decision 2: Browser auth model is secure cookie-based and token-opaque

- **Decision**: JWTs are delivered in secure HttpOnly SameSite cookies; Angular treats session credential as non-readable and never stores token strings in `localStorage`, `sessionStorage`, or IndexedDB.
- **Rationale**: Matches clarified spec, FR-010, and constitution security requirements. Reduces XSS token exfiltration risk.
- **Alternatives considered**:
  - Store bearer tokens in browser storage: rejected by requirement and governance.
  - In-memory bearer token handling in JS: rejected for same reason (still script-readable).

## Decision 3: CSRF protection contract for all state-changing mutations

- **Decision**: Mutations require both a double-submit CSRF token and strict `Origin`/`Referer` validation; failures return actionable GraphQL errors for retry/re-auth flows.
- **Rationale**: Matches FR-010a clarification and protects cookie-authenticated mutation traffic.
- **Alternatives considered**:
  - CSRF token only: rejected because Origin/Referer check is explicitly required.
  - Origin/Referer only: rejected because double-submit token is explicitly required.

## Decision 4: Angular architecture defaults to standalone + signals + typed forms

- **Decision**: New WebUI feature code uses standalone components/directives/pipes, signal-first state, modern control flow (`@if/@for/@switch`), typed reactive forms, and `OnPush`-compatible patterns.
- **Rationale**: Required by constitution section VI and spec Angular conventions section.
- **Alternatives considered**:
  - NgModule-centric architecture: rejected unless unavoidable third-party constraint appears.
  - Unstructured mutable state with imperative subscriptions: rejected due to maintainability and predictability concerns.

## Decision 5: Multi-organization scope is explicit and write-guarded

- **Decision**: Every org/user/role mutation is bound to explicit organization context in both UI state and GraphQL variables; context switches during dirty edit state require user confirmation and block accidental cross-org writes.
- **Rationale**: Required by FR-011 and edge-case scenarios; prevents stale-context errors.
- **Alternatives considered**:
  - Implicit current-org inferred server-side only: rejected due to ambiguity and higher cross-scope risk.
  - Best-effort warning without hard guard: rejected because it does not meet zero cross-org write target.

## Decision 6: Role assignment lifecycle preserves assignments across user deactivation

- **Decision**: User deactivation does not delete role assignments; assignments become ineffective while disabled and automatically effective again after re-enable.
- **Rationale**: Required by FR-006c and acceptance criteria.
- **Alternatives considered**:
  - Remove assignments on deactivate: rejected due to requirement conflict and restoration overhead.
  - Keep assignments effective while disabled: rejected due to security/least-privilege conflict.

## Decision 7: Local Angular development uses proxy for `/graphql` HTTP + WS

- **Decision**: Provide a documented proxy configuration for Angular dev server routing `/graphql` (HTTP and WebSocket) to local backend, with troubleshooting guidance and no hard-coded backend origin in app code.
- **Rationale**: Required by FR-013/FR-014 and Angular technical standards in constitution.
- **Alternatives considered**:
  - Hard-code `http://localhost:<port>` in Angular services: rejected by governance and portability concerns.
  - Disable WebSocket proxy in dev: rejected because subscriptions and parity need full `/graphql` transport behavior.

## Decision 8: Error surfacing model combines field-level and global feedback

- **Decision**: Validation errors map to typed form controls when possible; auth/authz/network/CSRF failures surface as global actionable alerts with retry or re-auth paths.
- **Rationale**: Required by FR-012 and edge-case expectations for actionable diagnostics.
- **Alternatives considered**:
  - Generic error toast for all failures: rejected due to poor remediation clarity.
  - Field-level only model: rejected because many errors are non-field (session expiry, network, authorization).

## Decision 9: Threat-model updates are mandatory deliverables for this feature

- **Decision**: Implementation tasks must include (1) feature-scoped threat model under `specs/004-add-isched-webui/` and (2) project-level summary update in `docs/security-threat-model.md`.
- **Rationale**: Constitution documentation standard explicitly requires both for security-sensitive changes.
- **Alternatives considered**:
  - Postpone threat-model update until after implementation: rejected as constitution gate failure.

