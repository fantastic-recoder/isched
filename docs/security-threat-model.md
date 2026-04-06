# Security Threat Model Summary

**Updated**: 2026-04-06

## Purpose

This document summarizes the security posture and recurring mitigations for Isched security-sensitive features. Feature-specific threat analysis remains in each feature directory and is referenced here.

## Feature Threat Models

- `specs/001-universal-backend/threat-model.md` — GraphQL transport, bootstrap flow, JWT auth, RBAC, tenant isolation, session revocation, WebSocket auth, and outbound HTTP secret handling
- `specs/004-add-isched-webui/threat-model.md` — Embedded WebUI serving, proxy-backed local GraphQL flow, CSRF/cookie auth behavior, and organization-context write boundaries
- `specs/005-rate-limited-auth-bootstrap/threat-model.md` — Deterministic auth lockout signaling, startup guard revalidation, bootstrap route gating transitions, and single-flight auth/bootstrap submission handling
- `specs/006-upload-schema/threat-model.md` — Tenant-admin schema upload authorization, overwrite conflict controls, SDL validation, and cross-tenant read isolation for schema documents

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

## Common Security Themes

- **Secure bootstrap**: any unauthenticated bootstrap path must be narrow, explicitly documented, and automatically disabled after first-use conditions are satisfied.
- **JWT-first authentication**: GraphQL operations require JWT validation, with scope-aware authorization for platform and tenant operations.
- **RBAC with scoped roles**: platform and tenant permissions must remain separated; custom roles must be constrained to their owning scope.
- **Tenant isolation**: tenant boundaries apply to authorization, runtime state, storage, metrics, and integration configuration.
- **Session revocation**: revoked sessions must be enforced at request time and propagated to long-lived WebSocket connections.
- **Secret protection**: external integration secrets must not be stored in plaintext and must be protected against accidental disclosure in logs or responses.
- **WebUI boundary controls**: browser-facing state must avoid persistent token storage and enforce explicit organization context for admin mutations.

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

