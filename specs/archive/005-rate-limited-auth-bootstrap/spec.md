# Feature Specification: RATE_LIMITED + Auth Bootstrap Consistency

**Feature Branch**: `005-rate-limited-auth-bootstrap`  
**Created**: 2026-04-05  
**Status**: Implemented and Archived  
**Input**: User description: "Execute the speckit.specify workflow for the current feature in /home/groby/dev/isched. Use the repository's existing context and recent work (including RATE_LIMITED gap closure and WebUI auth/bootstrap behavior) to create or update the active feature specification according to Speckit conventions, and write changes to the appropriate spec artifacts."

## Clarifications

### Session 2026-04-05

- Q: If seed mode is active and a valid session exists at app startup, which route takes precedence? → A: Always route to bootstrap first.
- Q: How should repeated submissions be handled while auth/bootstrap requests are already pending? → A: Enforce single-flight behavior per flow: do not start duplicate requests while one is pending.
- Q: When session validity changes between app initialization and first guarded navigation, what behavior should guards enforce? → A: Revalidate session once at first guarded navigation; redirect to sign-in if invalid.
- Q: When bootstrap becomes unavailable while the bootstrap page is already open, what should the app do? → A: Immediately redirect to sign-in and show a clear "bootstrap already completed" notice.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Deterministic Lockout Guidance (Priority: P1)

As an operator signing in, I need clear and deterministic feedback when I hit request limits so I know when and how to retry instead of guessing.

**Why this priority**: Auth lockout and rate-limit failures block all protected workflows; unclear feedback causes repeated failures and support churn.

**Independent Test**: Can be fully tested by triggering repeated failed sign-in attempts and verifying over-limit responses produce deterministic user-facing retry guidance.

**Acceptance Scenarios**:

1. **Given** an operator exceeds the allowed auth attempt window, **When** they submit another sign-in request, **Then** the system returns a deterministic `RATE_LIMITED` outcome and the UI shows actionable retry guidance.
2. **Given** retry timing metadata is present, **When** a rate-limited response is shown, **Then** the UI includes the provided retry timing guidance.
3. **Given** retry timing metadata is absent, **When** a rate-limited response is shown, **Then** the UI still provides a deterministic fallback retry message.

---

### User Story 2 - Stable Session Bootstrap on App Start (Priority: P2)

As an authenticated or returning operator, I need the app to resolve session/bootstrap state consistently at startup so I am routed to the correct page without confusing redirects.

**Why this priority**: Incorrect startup routing creates broken first impressions and can strand valid users on the wrong route.

**Independent Test**: Can be fully tested by loading the app in seed-mode and non-seed-mode with authenticated and unauthenticated sessions, then verifying deterministic route outcomes.

**Acceptance Scenarios**:

1. **Given** seed mode is active and no admin session is valid, **When** the app loads, **Then** the operator is routed to bootstrap flow.
2. **Given** seed mode is inactive and no valid session exists, **When** the app loads, **Then** the operator is routed to sign-in flow.
3. **Given** seed mode is active and a valid authenticated session exists, **When** the app loads, **Then** bootstrap flow still takes precedence before protected-route access is allowed.
4. **Given** seed mode is inactive and a valid authenticated session exists, **When** the app loads, **Then** protected routes are accessible without transient misrouting.

---

### User Story 3 - Predictable Bootstrap-to-Auth Transition (Priority: P3)

As a first-time platform operator, I need bootstrap completion and immediate authentication behavior to be predictable so I can proceed directly to administration.

**Why this priority**: This is the first end-to-end operator journey; weak handoff between bootstrap and auth undermines trust.

**Independent Test**: Can be tested independently by completing bootstrap in a clean environment and validating post-submit navigation and fallback behavior.

**Acceptance Scenarios**:

1. **Given** bootstrap succeeds and follow-up authentication succeeds, **When** completion is submitted, **Then** the operator lands on the dashboard in one continuous flow.
2. **Given** bootstrap succeeds but follow-up authentication fails, **When** completion is submitted, **Then** the operator is routed to sign-in with a clear recovery path and without residual invalid session state.
3. **Given** an auth or bootstrap request is already pending, **When** the operator resubmits the same action, **Then** no duplicate request is sent and UI state remains tied to the original in-flight request.
4. **Given** an operator is on the bootstrap page and bootstrap becomes unavailable, **When** availability is rechecked or the next bootstrap action is attempted, **Then** the operator is immediately redirected to sign-in and shown a clear "bootstrap already completed" notice.

### Edge Cases

- If the backend returns `RATE_LIMITED` for auth without retry metadata, the UI MUST show deterministic fallback retry guidance without timing details.
- If session validity changes between app initialization and first guarded navigation, guards MUST revalidate once on that first guarded navigation and redirect to sign-in if invalid.
- If bootstrap becomes unavailable while the bootstrap page is open, the app MUST immediately redirect to sign-in and show a clear "bootstrap already completed" notice.
- Repeated submissions during a pending auth/bootstrap request use single-flight handling and MUST NOT create duplicate in-flight requests.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST return deterministic rate-limit outcomes for over-limit authentication attempts using GraphQL error code `RATE_LIMITED`.
- **FR-002**: The system MUST provide actionable retry guidance for every `RATE_LIMITED` auth outcome, using backend-provided timing metadata when present and deterministic fallback messaging when absent.
- **FR-003**: The WebUI MUST map `RATE_LIMITED` auth outcomes to a dedicated user-facing alert state distinct from generic authentication failures.
- **FR-004**: The WebUI MUST resolve session state before finalizing protected-route decisions during application initialization.
- **FR-005**: The routing behavior MUST enforce bootstrap gating rules consistently: if seed mode is active, routing always goes to bootstrap first even when a valid session exists; if seed mode is inactive, unauthenticated users route to sign-in.
- **FR-006**: Successful bootstrap completion MUST transition users into an authenticated post-bootstrap destination without requiring manual refresh.
- **FR-007**: If post-bootstrap automatic authentication fails, the system MUST route users to sign-in with explicit recovery guidance and without leaving stale authenticated state.
- **FR-008**: Sign-out MUST clear in-memory authentication/session indicators so subsequent guarded navigation behaves as unauthenticated until a new successful sign-in occurs.
- **FR-009**: The system MUST ensure auth/bootstrap error messaging is deterministic for the same backend error category across repeated attempts.
- **FR-010**: Test coverage MUST include automated verification for rate-limit handling, session bootstrap behavior, and bootstrap-to-auth transition outcomes in both success and failure paths.
- **FR-011**: During pending auth/bootstrap operations, the WebUI MUST enforce single-flight behavior per flow by suppressing duplicate submissions until the in-flight request resolves.
- **FR-012**: The WebUI route-guard flow MUST revalidate session state once at the first guarded navigation after initialization; if revalidation fails, the user MUST be redirected to sign-in.
- **FR-013**: If bootstrap availability changes to unavailable while the bootstrap page is active, the WebUI MUST immediately redirect the user to sign-in and display a clear "bootstrap already completed" notice.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: User input flows MUST use typed reactive forms with strict TypeScript and strict template type checking enabled.
- **FCR-004**: Browser API consumption MUST use GraphQL `/graphql` only (HTTP/WebSocket) with no REST fallback.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/`IndexedDB`) and define secure transport/storage controls.
- **FCR-006**: Local development MUST define Angular dev-server proxy behavior for `/graphql` (including WebSocket upgrades).

### Key Entities *(include if feature involves data)*

- **AuthAttemptOutcome**: User-visible result of an authentication submission, including success/failure category, deterministic error code, and optional retry guidance.
- **SessionBootstrapState**: Startup session resolution state used to determine initial route eligibility for protected pages.
- **BootstrapEligibilityState**: Current platform bootstrap availability status used to gate bootstrap and sign-in routes.
- **UserFacingAlert**: Standardized alert payload shown to users for auth/bootstrap outcomes, including message class and recovery instructions.

### Assumptions

- Existing backend auth and bootstrap operations remain the source of truth for session validity and seed-mode status.
- `RATE_LIMITED` remains the canonical over-limit auth error code.
- Retry timing metadata may be present for some rate-limit responses and absent for others.
- This feature targets deterministic behavior alignment and does not expand authorization scope beyond existing auth/bootstrap journeys.

### Dependencies

- Availability of backend rate-limit enforcement and GraphQL error metadata for auth flows.
- Existing bootstrap status query and bootstrap completion mutation behavior.
- Existing UI auth routing and guard behavior used during application startup and protected navigation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In automated auth lockout tests, 100% of validated over-limit authentication attempts return `RATE_LIMITED`.
- **SC-002**: In automated UI tests for auth lockout handling, 100% of `RATE_LIMITED` responses display actionable retry guidance, including timing when supplied.
- **SC-003**: In startup routing verification across seed-mode/normal-mode and authenticated/unauthenticated permutations, at least 95% of runs land on the correct first route without user intervention.
- **SC-004**: In bootstrap end-to-end verification runs, at least 95% of clean-environment bootstrap completions reach a post-bootstrap destination in under 30 seconds.
- **SC-005**: In bootstrap-followed-by-auth failure simulations, 100% of runs route users to sign-in with a recoverable state and without stale authenticated indicators.
- **SC-006**: In release candidate regression testing, support-reproducible incidents for ambiguous auth/bootstrap error messaging are reduced by at least 50% versus the prior baseline test cycle.
