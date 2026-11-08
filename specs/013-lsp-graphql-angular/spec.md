# Feature Specification: LSP GraphQL Angular Integration

**Feature Branch**: `013-lsp-graphql-angular`  
**Created**: 2026-11-08  
**Status**: Draft  
**Input**: User description: "Run speckit.clarify for the current feature in this repository (branch 013-lsp-graphql-angular, spec at specs/013-lsp-graphql-angular/spec.md). Ask up to 5 highly targeted clarification questions for underspecified areas and encode answers back into the spec. Since this invocation is non-interactive from the user, proceed by resolving ambiguities using conservative, implementation-safe defaults grounded in the existing repository architecture and conventions where needed, and clearly document those assumptions in the spec. Update the spec accordingly and report what changed."

## Clarifications

### Session 2026-11-08

- Q: What is the canonical UI transport/API boundary for browser clients? → A: GraphQL endpoint `/graphql` only; no REST fallback for new UI flows.
- Q: What authentication storage policy applies in browser clients? → A: No persistent browser token storage; use in-memory tokens with secure transport controls.
- Q: What architecture style should new Angular UI pieces follow? → A: Standalone components with signal-first local state and typed reactive forms.
- Q: How should local development route API calls from Angular to backend? → A: Use dev-server proxy for `/graphql`, including WebSocket upgrade support.
- Q: What minimum quality gates define completion for this feature? → A: Add measurable latency, reliability, and test-pass criteria in Success Criteria and acceptance scenarios.

## User Scenarios & Testing *(mandatory)*

<!--
  IMPORTANT: User stories should be PRIORITIZED as user journeys ordered by importance.
  Each user story/journey must be INDEPENDENTLY TESTABLE - meaning if you implement just ONE of them,
  you should still have a viable MVP (Minimum Viable Product) that delivers value.
  
  Assign priorities (P1, P2, P3, etc.) to each story, where P1 is the most critical.
  Think of each story as a standalone slice of functionality that can be:
  - Developed independently
  - Tested independently
  - Deployed independently
  - Demonstrated to users independently
-->

### User Story 1 - Execute Core GraphQL User Flow from Angular (Priority: P1)

As a web user, I can complete the primary feature journey from the Angular UI using GraphQL operations so the workflow works end-to-end without REST dependencies.

**Why this priority**: Core user value depends on reliable end-to-end interaction between Angular and backend via the intended API boundary.

**Independent Test**: Can be fully tested by running the Angular UI against a backend exposing `/graphql`, executing the primary journey, and verifying expected UI and backend state outcomes.

**Acceptance Scenarios**:

1. **Given** the Angular app is configured with `/graphql`, **When** the user performs the primary feature action, **Then** the UI issues GraphQL operations and shows the successful result.
2. **Given** the GraphQL service returns a recoverable error, **When** the user submits the action, **Then** the UI presents actionable feedback without crashing and allows retry.

---

### User Story 2 - Maintain Secure Session Behavior in Browser (Priority: P2)

As a security-conscious operator, I need browser-side authentication handling that avoids persistent token storage while preserving user workflow continuity during an active session.

**Why this priority**: Security posture materially affects risk and compliance of any browser feature using JWT-based authentication.

**Independent Test**: Can be tested by authenticating in the UI, validating authorized GraphQL calls succeed, and confirming no JWT is written to localStorage/sessionStorage/IndexedDB.

**Acceptance Scenarios**:

1. **Given** a user authenticates successfully, **When** the app issues protected GraphQL operations, **Then** requests include valid auth context without persisting JWTs in browser storage.

---

### User Story 3 - Enable Predictable Local Development Workflow (Priority: P3)

As a developer, I can run the Angular app locally with proxy routing for GraphQL (including subscriptions/WebSocket upgrades) so local testing mirrors production API interaction patterns.

**Why this priority**: Reduces integration friction and prevents environment-specific defects.

**Independent Test**: Can be tested by starting local UI and backend services, verifying HTTP GraphQL requests and WebSocket upgrades pass through proxy, and confirming feature flows work without CORS workarounds.

**Acceptance Scenarios**:

1. **Given** local dev servers are running, **When** a developer executes a UI flow requiring GraphQL query/mutation and subscription behavior, **Then** proxy routing handles both HTTP and WebSocket traffic correctly.

---

[Add more user stories as needed, each with an assigned priority]

### Edge Cases

<!--
  ACTION REQUIRED: The content in this section represents placeholders.
  Fill them out with the right edge cases.
-->

- If `/graphql` is temporarily unavailable, the UI must show non-destructive error state, preserve user-entered form data, and allow retry.
- If auth context expires mid-session, protected operations must fail gracefully and trigger a clear re-authentication path.
- If WebSocket upgrade fails in development, the UI must continue supporting non-subscription flows and display a diagnostic message for subscription-dependent sections.

## Requirements *(mandatory)*

<!--
  ACTION REQUIRED: The content in this section represents placeholders.
  Fill them out with the right functional requirements.
-->

### Functional Requirements

- **FR-001**: System MUST provide the feature’s browser-facing API through GraphQL `/graphql` for all new Angular user flows in this scope.
- **FR-002**: Angular UI MUST execute required queries/mutations (and subscriptions where applicable) to complete the primary user journey end-to-end.
- **FR-003**: System MUST provide user-visible, actionable error handling for GraphQL validation, authorization, and transient transport failures.
- **FR-004**: Authentication handling in browser clients MUST avoid persistent token storage (`localStorage`, `sessionStorage`, IndexedDB) and use session-safe in-memory handling.
- **FR-005**: Local development setup MUST define proxy behavior for `/graphql`, including WebSocket upgrade support for subscription traffic.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: User input flows MUST use typed reactive forms with strict TypeScript and strict template type checking enabled.
- **FCR-004**: Browser API consumption MUST use GraphQL `/graphql` only (HTTP/WebSocket) with no REST fallback.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and define secure transport/storage controls.
- **FCR-006**: Local development MUST define Angular dev-server proxy behavior for `/graphql` (including WebSocket upgrades).

*Example of marking unclear requirements:*

- **FR-006**: System MUST authenticate users via [NEEDS CLARIFICATION: auth method not specified - email/password, SSO, OAuth?]
- **FR-007**: System MUST retain user data for [NEEDS CLARIFICATION: retention period not specified]

### Key Entities *(include if feature involves data)*

- **GraphQLOperation**: Client-initiated operation (`query`, `mutation`, or `subscription`) with operation name, variables payload, and typed result contract.
- **AuthSessionContext**: In-memory browser auth context containing current identity claims and token material used for request authorization during active session.
- **ProxyRouteConfig**: Local development routing rule mapping frontend `/graphql` traffic to backend endpoint with explicit WebSocket upgrade behavior.

## Success Criteria *(mandatory)*

<!--
  ACTION REQUIRED: Define measurable success criteria.
  These must be technology-agnostic and measurable.
-->

### Measurable Outcomes

- **SC-001**: 95% of primary user-flow GraphQL operations complete with end-user-visible response in ≤2 seconds in local and test environments under normal load.
- **SC-002**: 100% of new UI feature paths in scope use `/graphql` and no new REST client calls are introduced.
- **SC-003**: 100% of automated checks for changed frontend and integration tests pass, including at least one scenario covering GraphQL error handling.
- **SC-004**: Security verification confirms zero persisted JWT artifacts in `localStorage`, `sessionStorage`, and IndexedDB during and after normal feature usage.
