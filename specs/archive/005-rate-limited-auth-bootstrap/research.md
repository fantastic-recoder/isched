# Research: RATE_LIMITED + Auth Bootstrap Consistency

**Phase**: 0 (Outline & Research)  
**Updated**: 2026-04-05  
**Feature**: `005-rate-limited-auth-bootstrap`

## Decision 1: Standardize auth lockout contract on `RATE_LIMITED` with optional retry metadata

- **Decision**: Treat `extensions.code = RATE_LIMITED` as the canonical lockout signal for authentication and include `extensions.retryAfterMs` when available.
- **Rationale**: The feature spec requires deterministic lockout outcomes and explicit retry guidance while preserving compatibility with responses that do not include timing metadata.
- **Alternatives considered**:
  - Parse lockout intent from free-form error messages: rejected as non-deterministic.
  - Introduce a separate REST lockout endpoint: rejected by GraphQL-only architecture.

## Decision 2: Enforce deterministic frontend alert mapping with fallback guidance

- **Decision**: Introduce a dedicated user-facing alert state for `RATE_LIMITED` that is distinct from generic credential failures, with stable fallback copy when `retryAfterMs` is absent.
- **Rationale**: Avoids ambiguous auth failure UX and directly satisfies FR-002/FR-003/FR-009.
- **Alternatives considered**:
  - Reuse generic auth error banner: rejected because it hides retry semantics.
  - Show timing-dependent message only: rejected because metadata is optional.

## Decision 3: Route initialization always resolves bootstrap eligibility before auth destination

- **Decision**: Startup navigation evaluates bootstrap eligibility first; if seed mode is active, route to bootstrap even when a valid session already exists.
- **Rationale**: Matches clarification rule from 2026-04-05 and eliminates startup misrouting churn.
- **Alternatives considered**:
  - Session-first routing with later bootstrap correction: rejected due to transient redirect flicker.
  - Mode-specific ad hoc checks per route: rejected for inconsistency risk.

## Decision 4: Revalidate session once at first guarded navigation

- **Decision**: Route-guard flow performs a one-time session revalidation at the first guarded navigation after initialization, then continues with normal guard checks.
- **Rationale**: Satisfies FR-012 and edge-case handling when session validity changes after app bootstrap.
- **Alternatives considered**:
  - Never revalidate after initialization: rejected (stale session risk).
  - Revalidate on every guarded navigation: rejected due to avoidable latency/noise.

## Decision 5: Single-flight behavior per auth/bootstrap flow

- **Decision**: While sign-in or bootstrap completion is pending, suppress duplicate submissions and bind UI state to the original in-flight request.
- **Rationale**: Required by FR-011 and acceptance scenarios for predictable behavior under rapid repeated clicks.
- **Alternatives considered**:
  - Allow parallel submissions and keep latest response: rejected due to race conditions.
  - Queue duplicate submissions: rejected as unnecessary for this flow.

## Decision 6: Immediate bootstrap-unavailable redirect behavior

- **Decision**: If bootstrap becomes unavailable while bootstrap UI is active, immediately route to sign-in and surface a clear "bootstrap already completed" notice.
- **Rationale**: Required by FR-013 and clarification session outcome.
- **Alternatives considered**:
  - Only block on next form submit: rejected (user remains on invalid screen).
  - Silent redirect with no notice: rejected as confusing.

## Decision 7: Preserve secure cookie-auth posture and no persistent JWT storage

- **Decision**: Keep browser auth/session indicators ephemeral in memory and rely on backend-managed secure cookie flows; do not persist access JWTs in `localStorage`, `sessionStorage`, or IndexedDB.
- **Rationale**: Required by constitution Security-First principle and frontend constitutional requirements.
- **Alternatives considered**:
  - Persist bearer tokens in browser storage: rejected by policy.
  - Long-lived script-readable in-memory token cache shared across tabs: rejected for larger exposure surface.

## Decision 8: Test strategy spans backend integration + frontend component/e2e checks

- **Decision**: Add/extend automated verification for lockout signaling, startup routing permutations, bootstrap-to-auth success/failure, and single-flight suppression.
- **Rationale**: FR-010 mandates complete behavioral coverage across success and failure paths.
- **Alternatives considered**:
  - Backend-only tests: rejected (cannot validate route/alert behavior).
  - Manual QA-only startup verification: rejected (non-repeatable).

## Decision 9: Security documentation updates are mandatory deliverables

- **Decision**: Include a feature-scoped threat model (`specs/archive/005-rate-limited-auth-bootstrap/threat-model.md`) and add a summary entry in `docs/security-threat-model.md`.
- **Rationale**: Constitution requires threat-model coverage for security-sensitive auth/session behavior changes.
- **Alternatives considered**:
  - Defer threat model updates until implementation review: rejected (gate violation risk).

