# Data Model: RATE_LIMITED + Auth Bootstrap Consistency

**Phase**: 1 (Design & Contracts)  
**Updated**: 2026-04-05  
**Feature**: `005-rate-limited-auth-bootstrap`

## Entity: AuthAttemptOutcome

**Purpose**: Canonical result of an authentication attempt, used for deterministic UI and retry guidance.

**Fields**:
- `status: Enum(Success, InvalidCredentials, RateLimited, TransportFailure)`
- `errorCode: String?` (`RATE_LIMITED`, `UNAUTHENTICATED`, `TRANSIENT_NETWORK`, ...)
- `message: String`
- `retryAfterMs: Int?`
- `guidanceText: String` (derived fallback/metadata-aware copy)
- `occurredAt: DateTime`

**Validation Rules**:
- `status = RateLimited` MUST map to `errorCode = RATE_LIMITED`.
- `guidanceText` MUST always be present for `RateLimited` outcomes.
- Repeated equivalent backend error categories MUST produce stable `guidanceText`.

## Entity: UserFacingAlert

**Purpose**: Standardized alert payload rendered by auth/bootstrap screens.

**Fields**:
- `kind: Enum(Info, Warning, Error, Success)`
- `category: Enum(AuthRateLimited, AuthFailure, BootstrapUnavailable, SessionExpired, Generic)`
- `title: String`
- `body: String`
- `retryAfterMs: Int?`
- `dismissible: Boolean`

**Validation Rules**:
- `category = AuthRateLimited` MUST include actionable retry guidance in `body`.
- `category = BootstrapUnavailable` MUST mention that bootstrap has already completed.

## Entity: SessionBootstrapState

**Purpose**: Represents app initialization state that drives first-route determination.

**Fields**:
- `seedModeActive: Boolean`
- `sessionKnown: Boolean`
- `sessionAuthenticated: Boolean`
- `initialRouteResolved: Boolean`
- `firstGuardRevalidationComplete: Boolean`

**Validation Rules**:
- If `seedModeActive = true`, first route MUST be bootstrap regardless of `sessionAuthenticated`.
- Protected-route decisions require `initialRouteResolved = true`.
- One-time guard revalidation flips `firstGuardRevalidationComplete` to `true`.

**State Transitions**:
- `Unresolved -> BootstrapRouted` when seed mode is active.
- `Unresolved -> SignInRouted` when seed mode is inactive and session unauthenticated.
- `Unresolved -> ProtectedRouted` when seed mode is inactive and session authenticated.

## Entity: BootstrapEligibilityState

**Purpose**: Snapshot of whether bootstrap actions are currently valid.

**Fields**:
- `isAvailable: Boolean`
- `checkedAt: DateTime`
- `source: Enum(StartupProbe, GuardProbe, ActionProbe)`

**Validation Rules**:
- If `isAvailable` changes from `true` to `false` while bootstrap page is active, UI MUST redirect to sign-in immediately.
- Bootstrap actions are invalid when `isAvailable = false`.

## Entity: AuthFlowFlightState

**Purpose**: Controls duplicate-submission suppression for login/bootstrap requests.

**Fields**:
- `flow: Enum(SignIn, BootstrapComplete)`
- `inFlight: Boolean`
- `requestId: String?`
- `startedAt: DateTime?`
- `lastResolvedAt: DateTime?`

**Validation Rules**:
- A new request for `flow` MUST NOT start when `inFlight = true` for the same `flow`.
- `inFlight` MUST return to `false` on both success and failure resolution paths.

## Derived Invariants

- No auth lockout outcome is shown as a generic auth failure when `errorCode = RATE_LIMITED`.
- Startup route decisions are deterministic for the same `(seedModeActive, sessionAuthenticated)` tuple.
- Guard revalidation runs exactly once on first guarded navigation after initialization.
- Bootstrap-unavailable redirects always include a user-facing explanatory notice.
- Duplicate submissions cannot create multiple concurrent requests for the same flow.

