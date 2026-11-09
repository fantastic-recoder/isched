# Feature Specification: LSP GraphQL Angular Integration

**Feature Branch**: `013-lsp-graphql-angular`
**Created**: 2026-11-08
**Status**: Completed
**Completion evidence**: [quickstart.md](quickstart.md), including the backend constitution review and subscription-policy verification on 2026-11-09.
**Input**: Deliver schema-aware GraphQL editor assistance and complete Playground and organization/user administration journeys through the existing GraphQL-only browser API, with cookie-only authentication, local proxy support, security controls, and measurable UI performance.

## Clarifications

### Session 2026-11-08

- Q: What is the canonical UI transport/API boundary for browser clients? → A: GraphQL endpoint `/graphql` only; no REST fallback for new UI flows.
- Q: What authentication storage and transport policy applies in browser clients? → A: Do not persist JWTs; authenticate only with Secure, HttpOnly, SameSite=Strict cookies (no bearer-token alternative).
- Q: What architecture style should new Angular UI pieces follow? → A: Standalone components with signal-first local state and typed reactive forms.
- Q: How should local development route API calls from Angular to backend? → A: Use dev-server proxy for `/graphql`, including WebSocket upgrade support.
- Q: What minimum quality gates define completion for this feature? → A: Add measurable latency, reliability, and test-pass criteria in Success Criteria and acceptance scenarios.

Final decisions for this feature: The editor MUST provide schema-aware completion, hover documentation, and live syntax/type diagnostics using the active schema fetched through GraphQL `/graphql`; wire-level LSP JSON-RPC conformance is not required, and no new external LSP endpoint is allowed. Browser authentication MUST use Secure, HttpOnly, SameSite=Strict cookies only; there is no bearer-token alternative and JWTs MUST NOT be stored in localStorage, sessionStorage, or IndexedDB. Validate exact allowed origins for all GraphQL HTTP requests and WebSocket handshakes. Authenticated HTTP GraphQL mutations require a session-bound CSRF token. Login and first-run platform-admin bootstrap are pre-session operations: they remain Origin-validated and rate-limited, and neither requires an authenticated-session CSRF token when no session exists. Both successful operations create an authenticated cookie session and return its session-bound CSRF value through the GraphQL result, while the HTTP response layer sets the cookie and never returns the JWT. After reload, an authenticated read-only GraphQL session bootstrap query reacquires the CSRF value without a circular mutation prerequisite. No database migration is planned. The Playground and organization/user administration journeys are equally required. For each journey separately, run 10 concurrent users; each user performs 10 unmeasured warmup operations and then 100 measured operations. Denominator is 1,000 measured operations per journey; time submission through final rendered UI result; nearest-rank p95 must be strictly below 2 seconds. Detailed protocol is in [quickstart.md](quickstart.md).

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

### User Story 1 - Explore and Execute GraphQL in the Playground (Priority: P1)

As a web user, I can browse the active GraphQL schema and edit, validate, and run queries and mutations in the Angular GraphQL Playground, so I can use the schema effectively and see results and errors without REST dependencies.

**Why this priority**: The Playground is a core product journey and is equally required with the administration journey. Schema-aware editor assistance and execution must work together over the intended API boundary.

**Independent Test**: Run the Angular UI against a backend exposing `/graphql`; verify schema-aware completion, hover documentation, live syntax/type diagnostics, then edit and run both a query and a mutation and verify rendered results and errors.

**Acceptance Scenarios**:

1. **Given** the editor has loaded the active schema through `/graphql`, **When** the user edits a GraphQL document, **Then** the editor offers schema-aware completion, hover documentation, and live syntax/type diagnostics without contacting a separate LSP endpoint.
2. **Given** a valid query or mutation is entered, **When** the user submits it, **Then** the UI issues it through `/graphql` and renders the final result.
3. **Given** the GraphQL service returns a recoverable error, **When** the user submits an operation, **Then** the UI presents actionable feedback without crashing and allows retry while preserving the editor content.

---

### User Story 2 - Administer Organizations and Users (Priority: P1)

As an authorized administrator, I can inspect and manage organizations and users from the Angular administration screens using GraphQL, so routine administration is available through the same supported API boundary.

**Why this priority**: Administration is an equally required acceptance journey with the Playground; neither journey can be deferred as optional scope.

**Independent Test**: Authenticate using the secure cookie flow, then list and manage an organization and its users through the Admin UI; verify server state and user-visible validation/error feedback while asserting that all backend requests use `/graphql`.

**Acceptance Scenarios**:

1. **Given** an authorized administrator is in the Admin UI, **When** they view and create/update or change the status of an organization, **Then** GraphQL operations update the organization and the UI presents the final state.
2. **Given** an organization is selected, **When** the administrator views and creates/updates or changes the status of a user, **Then** GraphQL operations update the user and the UI presents the final state.
3. **Given** validation, authorization, conflict, or transport errors occur, **When** the administrator submits a change, **Then** actionable feedback is shown, unsafe changes are not presented as successful, and form data is retained where retry is safe.

---

### User Story 3 - Maintain Secure Session Behavior in Browser (Priority: P1)

As a security-conscious operator, I need browser authentication to use Secure, HttpOnly, SameSite=Strict cookies without persistent or in-memory bearer-token storage, while preserving a clear re-authentication path.

**Why this priority**: Security posture materially affects risk and compliance of any browser feature using JWT-based authentication.

**Independent Test**: Authenticate in the UI, verify protected GraphQL HTTP and WebSocket operations succeed using browser-managed cookies, expire/revoke the session, and confirm the re-authentication path works with no JWT in browser storage, application memory, Authorization headers, or WebSocket payloads.

**Acceptance Scenarios**:

1. **Given** a user authenticates successfully, **When** the app issues protected GraphQL HTTP or WebSocket operations, **Then** the browser sends a secure HttpOnly, SameSite cookie and JavaScript does not handle a bearer token.
2. **Given** the session expires or is revoked, **When** a protected operation is attempted, **Then** the UI clears non-cookie session state and directs the user to re-authenticate without a bearer-token fallback.
3. **Given** an allowed-Origin WebSocket handshake without a session cookie, **When** a public subscription such as `healthChanged` is requested, **Then** anonymous delivery remains supported; a protected subscription is rejected before its resolver or broker registration runs. This policy applies to both the same-port and dedicated WebSocket listeners.

---

### User Story 4 - Enable Predictable Local Development Workflow (Priority: P2)

As a developer, I can run the Angular app locally with proxy routing for GraphQL (including subscriptions/WebSocket upgrades) so local testing mirrors production API interaction patterns.

**Why this priority**: Reduces integration friction and prevents environment-specific defects.

**Independent Test**: Can be tested by starting local UI and backend services, verifying HTTP GraphQL requests and WebSocket upgrades pass through proxy, and confirming feature flows work without CORS workarounds.

**Acceptance Scenarios**:

1. **Given** local dev servers are running, **When** a developer executes a UI flow requiring GraphQL query/mutation and subscription behavior, **Then** proxy routing handles both HTTP and WebSocket traffic correctly.

---

### Edge Cases

- If `/graphql` is temporarily unavailable, the UI must show non-destructive error state, preserve user-entered form data, and allow retry.
- If a cookie is missing, invalid, expired, or revoked, protected HTTP and WebSocket operations must fail safely and trigger a clear re-authentication path; no bearer-token alternative is attempted.
- A missing cookie permits anonymous public subscriptions after Origin validation. A supplied invalid, empty, malformed, duplicate, expired, or revoked session cookie is rejected rather than downgraded to anonymous access.
- If WebSocket upgrade fails in development, the UI must continue supporting non-subscription flows and display a diagnostic message for subscription-dependent sections.
- If introspection is unavailable or lacks the active schema, the editor must show a clear language-service-unavailable diagnostic and must not imply that schema-aware validation is active.
- If schema documentation is absent, hover must not invent descriptions; completion and diagnostics should still reflect available schema data.
- Requests with missing, `null`, malformed, or unlisted Origin values must be rejected for GraphQL HTTP and WebSocket handshakes; Referer is not a bypass.
- Authenticated HTTP mutations must reject missing, stale, or cross-session CSRF tokens before resolver side effects. Login and first-run bootstrap require no session-bound CSRF token only before a session exists; both remain Origin-checked and rate-limited, and both establish the cookie session plus its CSRF value on success.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide the feature’s external browser-facing backend API through GraphQL `/graphql` only for all new Angular user flows in this scope; it MUST NOT add REST endpoints or a separate external LSP endpoint. This is the canonical GraphQL-only boundary.
- **FR-002**: Angular UI MUST execute the GraphQL queries/mutations (and subscriptions where applicable) needed to complete both the Playground and organization/user administration journeys end-to-end.
- **FR-003**: System MUST provide user-visible, actionable error handling for GraphQL validation, authorization, and transient transport failures.
- **FR-004**: Browser clients MUST NOT persist JWTs in `localStorage`, `sessionStorage`, or IndexedDB. This storage prohibition is independent of, and does not permit substitution for, the cookie transport required by FCR-005.
- **FR-005**: Local development setup MUST define proxy behavior for `/graphql` over HTTP and WebSocket, including WebSocket upgrade support for subscription traffic. This is the canonical proxy HTTP/WS requirement.
- **FR-006**: The browser GraphQL editor MUST provide schema-aware completion, hover documentation, and live syntax/type diagnostics against the active GraphQL schema. Schema and documentation data MUST be obtained through `/graphql`; the language service MUST NOT introduce another external backend endpoint.
- **FR-007**: The Angular administration journey MUST support organization and user management through GraphQL `/graphql`, with user-visible outcomes and actionable validation/authorization/conflict/transport errors.
- **FR-008**: Browser authentication MUST use a Secure, HttpOnly, SameSite=Strict cookie for `/graphql` HTTP and WebSocket session authentication. Successful login and first-run platform-admin bootstrap responses MUST set the cookie through the HTTP response layer and MUST NOT return the JWT in their JSON payloads. The browser MUST NOT use an Authorization bearer header, JWT in-memory bearer state, URL token, or WebSocket bearer payload. FR-004's persistent-storage prohibition is a distinct requirement and MUST NOT be treated as a substitute for this transport rule.
  Anonymous public subscriptions remain supported only when no session cookie is supplied and the Origin is allowed. Protected subscriptions require a valid cookie session and the existing role/tenant authorization before resolver execution or broker registration. Invalid supplied cookies MUST NOT fall back to anonymous access; neither `connection_init` credentials nor an Authorization header may authenticate the WebSocket.
- **FR-009**: The server MUST validate an exact configured allowed Origin for every GraphQL HTTP request and WebSocket handshake. Missing, `null`, malformed, wildcard, or unlisted origins MUST fail closed; Referer MUST NOT substitute for Origin. Credentialed CORS MUST NOT use wildcard `Access-Control-Allow-Origin`.
- **FR-010**: Every authenticated HTTP GraphQL mutation MUST require a session-bound CSRF token and validate it before executing mutation side effects. Login and first-run platform-admin bootstrap are explicit pre-session exceptions only when no authenticated session exists; they remain Origin-validated and rate-limited. Repository behavior creates an authenticated session during first-run bootstrap. Successful login and bootstrap MUST return that session's CSRF value through GraphQL, and a read-only existing-session GraphQL bootstrap query MUST provide/reacquire it. The pre-session exception MUST NOT apply when a valid session cookie is already present. This MUST NOT introduce a non-GraphQL endpoint or database migration.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: User input flows MUST use typed reactive forms; TypeScript strict mode and strict Angular template type checking MUST remain enabled and pass.
- **FCR-004**: Frontend API consumption MUST enforce FR-001: GraphQL `/graphql` only (HTTP/WebSocket), with no REST fallback or separate external LSP endpoint.
- **FCR-005**: Authentication transport MUST enforce FR-008 and FR-009: Secure, HttpOnly, SameSite=Strict cookies for GraphQL HTTP and WebSocket operations; no bearer-token alternative; no JWT exposed to JavaScript or persisted in browser storage; exact allowed-Origin validation and session-bound CSRF for authenticated HTTP mutations. Pre-session login/bootstrap MUST follow FR-010. These transport/security controls are distinct from FR-004's storage prohibition and MUST be verified.
- **FCR-006**: Local Angular development MUST configure the dev-server proxy required by FR-005 for `/graphql` HTTP and WebSocket upgrades.
- **FCR-007**: Before any required journey or the feature is marked complete, a documented constitution-compliance review MUST be completed and any violation resolved or approved as an explicit exception.

### Key Entities *(include if feature involves data)*

- **GraphQLOperation**: Client-initiated operation (`query`, `mutation`, or `subscription`) with operation name, variables payload, and typed result contract.
- **AuthSessionContext**: Non-secret browser UI session state; JWT credentials are transported only in a secure HttpOnly SameSite cookie that application JavaScript cannot read or copy.
- **ProxyRouteConfig**: Local development routing rule mapping frontend `/graphql` traffic to backend endpoint with explicit WebSocket upgrade behavior.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Independently for the Playground and administration journeys, run 10 concurrent users; each user performs 10 unmeasured warmup operations followed by 100 measured operations. The denominator is exactly 1,000 measured operations per journey. Measure from immediately before dispatch on submit through the final expected result rendered and visible in the UI, after loading/submitting settles and an Angular render frame occurs. Failed/time-out operations fail the run and MUST NOT be dropped. Compute nearest-rank p95 (rank 950 of 1,000); p95 MUST be strictly less than 2,000 ms for each journey separately. The deterministic workload and reproducible protocol are defined in `quickstart.md`.
- **SC-002**: 100% of new UI feature paths in scope use `/graphql` and no new REST client calls are introduced.
- **SC-003**: 100% of automated checks for changed frontend and integration tests pass, including success and error handling for both required journeys.
- **SC-004**: Security verification confirms zero JWT artifacts in `localStorage`, `sessionStorage`, IndexedDB, application-managed in-memory bearer state, HTTP Authorization headers, URLs, and WebSocket initialization payloads; authenticated HTTP and WebSocket operations use only the Secure, HttpOnly, SameSite=Strict cookie. Verification also confirms exact Origin enforcement for all GraphQL HTTP requests and WebSocket handshakes and session-bound CSRF validation for authenticated HTTP mutations, with the pre-session login/bootstrap path separately verified.
- **SC-005**: Acceptance evidence demonstrates both P1 journeys: (a) GraphQL Playground schema browsing, editing/running queries and mutations, hover docs, completions, diagnostics, and errors; and (b) administration of organizations and users. Neither journey may be waived as optional.
- **SC-006**: Frontend verification passes strict TypeScript and strict template compilation and explicitly verifies typed reactive forms, standalone component use, signal-backed application-owned template state, and modern Angular template control flow.
- **SC-007**: A constitution-compliance review is recorded as complete before feature closeout; a pending or failed review blocks completion.
