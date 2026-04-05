# Threat Model: 005-rate-limited-auth-bootstrap

## Scope

This feature updates authentication lockout signaling and startup/bootstrap route behavior in the WebUI. Trust boundaries include browser UI state, GraphQL error metadata, route guards, and backend session/bootstrap state checks.

## Assets

- Authentication session state (server-managed, cookie-backed)
- Lockout/error metadata (`extensions.code`, `retryAfterMs`)
- Bootstrap eligibility state (`seedModeActive`)
- Guard-controlled route access decisions
- User-facing security guidance messages

## Threats and Mitigations

- **Ambiguous lockout responses causing repeated brute-force retries**
  - Mitigation: deterministic `RATE_LIMITED` mapping and explicit retry guidance fallback when timing metadata is absent.
- **Session state drift between app init and guarded navigation**
  - Mitigation: one-time guard revalidation using `currentUser`; redirect to sign-in on failure.
- **Duplicate auth/bootstrap submissions creating race conditions**
  - Mitigation: single-flight suppression for each flow while requests are pending.
- **Bootstrap page exposed after bootstrap is no longer allowed**
  - Mitigation: immediate redirect from bootstrap page to sign-in with clear completion notice.
- **Credential/token exposure in browser storage**
  - Mitigation: no persistent JWT storage in `localStorage`, `sessionStorage`, or IndexedDB; keep app-owned auth indicators ephemeral.

## Residual Risks

- Inconsistent backend error metadata across all auth resolvers can still degrade UX until contract normalization is complete everywhere.
- Guard revalidation correctness depends on reliable and low-latency `currentUser` responses in degraded network conditions.

