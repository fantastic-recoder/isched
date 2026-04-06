# Research: WebUI Navigation + Status Bars

**Phase**: 0 (Outline & Research)  
**Updated**: 2026-04-06  
**Feature**: `007-webui-nav-status-bars`

## Decision 1: Implement shell chrome at app root for authenticated routes

- **Decision**: Render top navigation and bottom status bars in `src/ui/src/app/app.html` around the routed authenticated content instead of duplicating shell fragments inside each page.
- **Rationale**: The spec requires shell elements on all authenticated screens; root placement avoids per-page drift and keeps consistency/maintainability high.
- **Alternatives considered**:
  - Keep page-local nav/status elements: rejected due to duplicated behavior and inconsistent active-state handling.
  - Create a separate full-layout route subtree first: rejected as higher routing churn than needed for this scope.

## Decision 2: Use a dedicated signal-first shell state service

- **Decision**: Add a `ShellStatusService` with signal-backed state for `operationDigest` and `sessionIdentitySummary`, plus explicit update methods.
- **Rationale**: A dedicated shared state holder keeps app-owned template state signal-backed and testable, matching constitution and frontend requirements.
- **Alternatives considered**:
  - Store digest in per-page component state only: rejected because status bar must persist across authenticated screens.
  - Drive shell state via component-owned observables + async pipe: rejected by constitution unless third-party immutable streams require it.

## Decision 3: Standardize operation digest lifecycle wording

- **Decision**: Represent digest status with a small state model (`idle | loading | success | error`) and emit normalized user-facing text, including required phrases "Loading organization users" and "Organization users loaded" for the representative flow.
- **Rationale**: Deterministic wording is required by FR-006 and is needed for reliable tests.
- **Alternatives considered**:
  - Free-form page-specific messages only: rejected due to inconsistent UX and brittle assertions.
  - Backend-originated digest text only: rejected for this phase because shell digest is UI-owned presentation state.

## Decision 4: Resolve rapid digest updates with monotonic last-write-wins

- **Decision**: Include monotonic sequencing/timestamp handling in `ShellStatusService` so only the latest emitted digest is displayed.
- **Rationale**: Explicitly satisfies edge-case requirement that stale status text must not reappear under rapid emissions.
- **Alternatives considered**:
  - Debounce updates globally: rejected because it can hide legitimate intermediate state transitions.
  - Queue and display all digests: rejected because the requirement is to show the latest operation digest.

## Decision 5: Derive and display current user name with non-empty fallback

- **Decision**: Populate `SessionIdentitySummary.displayName` from existing authenticated context (`currentUser`) and show a fallback label (e.g., `Signed-in user`) until resolved.
- **Rationale**: Meets FR-007 and edge-case behavior while avoiding blank status identity.
- **Alternatives considered**:
  - Display only user ID: rejected as less user-friendly and not aligned with requirement wording.
  - Leave identity blank until fetch completes: rejected by edge-case requirement.

## Decision 6: Keep GraphQL and JWT handling unchanged

- **Decision**: Use existing `GraphQLService` endpoint `/graphql` and current ephemeral auth handling in `AuthService`; do not add REST calls or persistent token storage.
- **Rationale**: Preserves constitutional constraints and avoids introducing security regressions for a shell-UI feature.
- **Alternatives considered**:
  - Add auxiliary REST endpoint for lightweight status: rejected by GraphQL-only rule.
  - Cache identity in localStorage for quicker display: rejected by JWT/browser storage policy.

## Decision 7: Implement shell styling with DaisyUI components plus Tailwind utilities

- **Decision**: Use DaisyUI `navbar` for top bar and a lightweight fixed/flex footer/status container with Tailwind utility classes to keep digest and user label readable across viewport sizes.
- **Rationale**: Matches project styling conventions and FR-008/FR-009 requirements.
- **Alternatives considered**:
  - Custom CSS-only shell without DaisyUI primitives: rejected due to style inconsistency risk.
  - Hide digest on small screens by default: rejected because critical information must remain available.

## Decision 8: Define explicit navigation contract and active-state semantics

- **Decision**: The top menu will include scoped primary destinations already present in routing (`Dashboard`, `Organizations`, `Users`, `RBAC`) with active route highlighting.
- **Rationale**: Directly satisfies FR-003 and aligns with existing route map.
- **Alternatives considered**:
  - Use only page-local quick links from dashboard: rejected because non-dashboard pages need persistent navigation.
  - Add new destinations not in current route tree: rejected as out of scope and not required by the feature spec.

## Decision 9: Test strategy combines deterministic unit tests and smoke Playwright checks

- **Decision**: Add focused unit tests for shell rendering/state transitions and one or more Playwright smoke tests for global shell visibility + representative digest transition.
- **Rationale**: Meets FR-010/FR-011 and keeps confidence high for globally visible app shell behavior.
- **Alternatives considered**:
  - Unit tests only: rejected because end-to-end shell integration and routing behavior need browser-level confirmation.
  - Playwright-only checks: rejected because digest transition edge cases are more precise and cheaper to validate in unit tests.

## Decision 10: Rollout with minimal migration impact and explicit risk controls

- **Decision**: Treat this as a non-breaking UI shell rollout with no data migration; include implementation tasks to remove duplicated local nav where applicable and verify no layout regressions.
- **Rationale**: The feature changes presentation and shared state wiring, not backend schema or persistence.
- **Alternatives considered**:
  - Large route/layout refactor before shell delivery: rejected as higher-risk than required for this feature.
  - Keep old and new nav systems indefinitely: rejected due to user confusion and maintenance cost.

