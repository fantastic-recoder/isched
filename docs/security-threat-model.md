# Security Threat Model Summary

**Updated**: 2026-11-09

## Purpose

This document summarizes the security posture and recurring mitigations for Isched security-sensitive features. Feature-specific threat analysis remains in each feature directory and is referenced here.

## Feature Threat Models

- `specs/archive/001-universal-backend/threat-model.md` — GraphQL transport, bootstrap flow, JWT auth, RBAC, tenant isolation, session revocation, WebSocket auth, and outbound HTTP secret handling
- `specs/archive/004-add-isched-webui/threat-model.md` — Embedded WebUI serving, proxy-backed local GraphQL flow, CSRF/cookie auth behavior, and organization-context write boundaries
- `specs/archive/005-rate-limited-auth-bootstrap/threat-model.md` — Deterministic auth lockout signaling, startup guard revalidation, bootstrap route gating transitions, and single-flight auth/bootstrap submission handling
- `specs/archive/006-upload-schema/threat-model.md` — Tenant-admin schema upload authorization, overwrite conflict controls, SDL validation, and cross-tenant read isolation for schema documents
- `specs/011-graphql-playground/threat-model.md` — Authenticated schema browsing, query execution, safe editor/result rendering, layout preference persistence, and subscription advisory-only behavior
- `specs/archive/013-lsp-graphql-angular/threat-model.md` — Implemented Secure, HttpOnly, SameSite=Strict cookie-only auth; exact allowed-Origin checks for all GraphQL HTTP requests and WebSocket handshakes; session-bound CSRF for authenticated HTTP mutations, with GraphQL login/first-run bootstrap and read-only reacquisition; GraphQL-only Playground/Admin journeys and browser-local editor language features. Verified evidence and residual risks are recorded in the feature model and quickstart.

## Feature 013 Security Closeout Snapshot (2026-11-09)

- **Scope**: `013-lsp-graphql-angular` (cookie-backed browser session, exact-Origin and CSRF enforcement, GraphQL-only Playground/Admin UI, local proxy, and browser-local schema assistance).
- **Validated mitigations**:
  - Secure, HttpOnly, SameSite=Strict cookie is issued and used by the browser; JWT is not returned to JavaScript, persisted, placed in Authorization, URL, or `connection_init`.
  - HTTP Origin tests cover valid, missing, `null`, malformed, and unlisted values; authenticated mutation CSRF rejection is verified before organization-creation side effects.
  - Login/bootstrap establish the session cookie and expose only the CSRF value via GraphQL; read-only session CSRF reacquisition, logout/revocation, and cookie-authenticated WS behavior are covered by backend tests.
  - Public anonymous subscriptions remain intentional when Origin is allowed and no cookie is supplied. Invalid supplied cookies fail closed; protected fields are rejected before resolver/broker work. Both listener routes have registration-count regressions.
  - Admin, Playground and proxy browser journeys use only application `/graphql` HTTP/WS traffic. Local proxy acceptance verifies HTTP forwarding and authenticated `graphql-transport-ws` upgrade/ack.
  - Browser-local schema assistance covers completion, hover, diagnostics, and explicit error/retry behavior; descriptions render as text.
- **Validation evidence**: CTest **58/58**; Angular **33 suites / 238 tests**; strict production build and Playwright TypeScript check passed. Browser Admin/security/Playground acceptance: **9 passed, 1 skipped** (no subscription field in the acceptance schema); proxy acceptance: **2/2 passed**. Separate 10-user/1,000-operation benchmarks passed with aggregate rank-950 p95 **450.39 ms** (Playground) and **629.33 ms** (Admin), with zero failed or timed-out samples. Detailed class counts and commands are in `specs/archive/013-lsp-graphql-angular/quickstart.md`.
- **Residual risks**: same-origin XSS can operate within an authenticated session; Origin allowlists and RBAC assignments remain deployment responsibilities; browser Secure-cookie behavior and benchmark performance can differ by environment. This closeout is regression evidence, not a penetration test.
- **Backend review follow-up**: changed C++ ownership, threading, error handling, and focused-function responsibilities were reviewed; CSRF-map synchronization is documented and shared cookie/parsed-operation helpers replace duplicated/nested policy code. Fresh backend validation passed **58/58** after this refactor. Revocation does not roll back already admitted work, and process restart requires reauthentication for process-local CSRF state. Full review scope is in the feature quickstart.

## Feature 005 Security Closeout Snapshot (2026-04-05)

- **Scope**: `005-rate-limited-auth-bootstrap` (auth lockout signaling, startup/guard session consistency, bootstrap transition controls)
- **Validated mitigations**:
  - Deterministic lockout classification through `extensions.code = RATE_LIMITED` with fallback guidance when `retryAfterMs` is absent
  - One-time guarded-route session revalidation to reduce stale-session drift after startup
  - Single-flight suppression on login/bootstrap submit paths to prevent race-condition amplification
  - Immediate bootstrap-unavailable redirect to sign-in with explicit operator notice
  - Browser posture remains no persistent JWT storage for app-owned state
- **Validation evidence**:
  - Backend gate: `ctest --output-on-failure` PASS (39/39)
  - Focused frontend unit gates: PASS (`test:login-lockout`, `test:startup-routing`, `test:auth-bootstrap`, full `pnpm test`)
  - Focused bootstrap E2E: PASS (`pnpm run e2e:bootstrap`)
  - Lockout/combined/full Playwright E2E gates: PASS (`pnpm run e2e:rate-limiting`, `pnpm run e2e:auth-bootstrap`, `pnpm e2e`)
- **Open blocker**:
  - None for Feature 005 closeout gates after lockout selector/classification alignment and bootstrap/login precondition stabilization in `src/ui/e2e/rate-limiting.spec.ts`.
- **Risk posture**: No new critical auth/session exposure identified from Feature 005 closeout validation; lockout E2E release-confidence blocker is resolved.

## Feature 006 Security Closeout Snapshot (2026-04-06)

- **Scope**: `006-upload-schema` (tenant-admin schema document upload, list, fetch; overwrite conflict controls; SDL validation; cross-tenant isolation)
- **Validated mitigations**:
  - Upload gated by `require_roles({"role_tenant_admin"})` — unauthenticated and non-admin callers receive `FORBIDDEN` before resolver runs
  - All list/fetch operations derive tenant scope from auth context only — no caller-supplied `organizationId` accepted
  - Default `overwrite=false` prevents accidental destructive writes; explicit opt-in required
  - SDL content validated with existing PEGTL parser before persistence — malformed SDL returns `VALIDATION_FAILED`
  - Content size capped at 1 MB (configurable) with early rejection before parsing
  - Schema name validated against `[A-Za-z0-9._-]{1,128}` before any DB interaction
  - All SQL queries use parameterized binding — no string interpolation of user-supplied values
  - Atomic `BEGIN IMMEDIATE` transaction for overwrite path — last-successful-write-wins
- **Validation evidence**:
  - Backend gate: `ctest --output-on-failure` PASS (40/40)
  - Dedicated integration test suite: `test_graphql_schema_upload` PASS (13 test cases, 1561 assertions)
  - Unit tests in `isched_gql_executor_tests`: PASS (new schema upload test cases)
- **Open blocker**: None — all security controls implemented and verified.
- **Risk posture**: No new critical auth/session exposure introduced by spec-006; all identified threats addressed with controls consistent with existing RBAC and tenant-isolation patterns.

## Feature 011 Security Planning Snapshot (2026-05-23)

- **Scope**: `011-graphql-playground` (authenticated playground route, introspection browsing, query execution, panel layout persistence)
- **Validated mitigations planned**:
  - `/playground` remains behind existing authentication routing
  - All schema/query/result content is treated as text, not HTML
  - Layout persistence stores only non-sensitive UI preferences
  - GraphQL access stays on `/graphql` only
  - Subscription operations remain generate-only with explicit advisory output
- **Residual risk**:
  - Authenticated users can inspect schema metadata and query responses by design
  - Browser storage may retain harmless layout preferences across sessions

## Feature SP-012 Security Closeout Snapshot (2026-05-26)

- **Scope**: Plugin loading and runtime execution (`loadPlugin`, `unloadPlugin`, `plugins`, `plugin`) plus lifecycle subscription streaming (`pluginLifecycleEvents`) for Boost.DLL request/resolver plugins
- **Validated mitigations**:
  - Plugin GraphQL administration fields are restricted to `platform_admin`
  - FR-025 pre-stream authorization is enforced for `pluginLifecycleEvents` (unauthorized callers rejected before stream creation)
  - Plugin manifests carry `abi_version`; load rejects semver-major mismatches
  - Request-plugin `openApiFragmentJson` is validated as strict JSON at load time
  - FR-022 canonical path policy blocks relative/outside-root/symlink-escape plugin paths before dynamic load
  - Request route matching supports templates while blocking deterministic overlap collisions
  - Request and resolver plugin exceptions are contained and trigger automatic unload
  - FR-023/FR-024 lifecycle events are emitted exactly once only after successful load/unload state transitions
- **Validation evidence**:
  - Full backend regression: `ctest --output-on-failure` PASS (55/55)
  - Plugin suites: `isched_plugin_system_tests`, `isched_plugin_system_subscription_integration_tests`, and `isched_gql_executor_auth_resolvers_tests` PASS
- **Residual risk**:
  - A privileged operator can still intentionally load malicious native code
  - Crash containment does not sandbox arbitrary plugin side effects outside resolver/request callback boundaries

## Common Security Themes

- **Secure bootstrap**: any unauthenticated bootstrap path must be narrow, explicitly documented, and automatically disabled after first-use conditions are satisfied.
- **Session authentication and revocation**: GraphQL operations require validated session credentials, with scope-aware authorization for platform and tenant operations. Browser JWT credentials, where used by the backend, remain inside the HttpOnly cookie boundary.
- **RBAC with scoped roles**: platform and tenant permissions must remain separated; custom roles must be constrained to their owning scope.
- **Tenant isolation**: tenant boundaries apply to authorization, runtime state, storage, metrics, and integration configuration.
- **Session revocation**: revoked sessions must be enforced at request time and propagated to long-lived WebSocket connections.
- **Secret protection**: external integration secrets must not be stored in plaintext and must be protected against accidental disclosure in logs or responses.
- **WebUI boundary controls**: browser-facing state must avoid persistent JWT storage or in-memory bearer handling, use Secure, HttpOnly, SameSite=Strict cookie transport, validate exact allowed origins on all GraphQL HTTP requests and WebSocket handshakes, require session-bound CSRF on authenticated HTTP mutations (with GraphQL-only pre-session login/bootstrap handling), and enforce explicit organization context for admin mutations.
- **Playground safety**: authenticated-only playground features must render untrusted schema/query content as text and keep non-sensitive UI preferences separate from credentials.

## Reusable Mitigation Checklist

- validate authentication before resolver execution
- enforce platform-vs-tenant scope explicitly in authorization checks
- persist and check revocation state for active sessions
- encrypt stored secrets at rest
- document trust boundaries and residual risks for every security-sensitive feature
- include threat-model updates when auth, RBAC, session, transport, or secret-handling behavior changes

## Operational Follow-Ups

- maintain signing-key rotation guidance
- review logging for secret redaction and safe error reporting
- review denial-of-service protections for GraphQL queries, subscriptions, and outbound integrations
- re-run threat-model review when adding new authentication flows or privileged mutations
