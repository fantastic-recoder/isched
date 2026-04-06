# Feature Specification: Isched WebUI

**Feature Branch**: `004-add-isched-webui`  
**Created**: 2026-04-04  
**Status**: Archived Historical Implementation Record  
**Input**: User description: "Isched WebUI. Requirements: Add an embedded Angular 21 based Web UI to isched backend. UI capabilities must include: one-time platform bootstrap flow, create/edit organizations, create/edit users, and RBAC management (built-in roles + custom role definitions and assignments). UI stack must use Tailwind CSS and DaisyUI. Angular app must support standalone local development with proxy to locally running isched server (background), documenting proxy setup and dev workflow. Include acceptance criteria and measurable success criteria for these flows. Ensure architecture remains GraphQL-only integration with backend /graphql endpoint and JWT-based auth model. Include constraints for security (token handling), multi-organization context, and error surfacing.

Also include a section that references modern Angular coding conventions expected in this feature implementation: signal-first state management, standalone components, `@if/@for/@switch` template control flow, typed reactive forms, and no NgModule-centric architecture unless required by third-party constraints."

## Clarifications

### Session 2026-04-04

- Q: Which JWT handling model is required for WebUI auth state? → A: JWT is carried in secure HttpOnly SameSite cookie(s); the WebUI must never read token values directly.
- Q: What is the admin responsibility split for organization management? → A: Platform admins create organizations; organization admins can edit only their own organization profile and manage users/RBAC within their organization scope.
- Q: What is the uniqueness scope for user login identifiers? → A: Login identifier must be unique within an organization; the same identifier may exist in different organizations.
- Q: Which CSRF protection model is required for state-changing GraphQL mutations? → A: Use double-submit CSRF token validation plus strict Origin/Referer validation for state-changing mutations.
- Q: What happens to role assignments when a user is deactivated and later re-enabled? → A: Deactivated users keep role assignments; assignments are inactive while user is disabled and auto-reactivate when user is re-enabled.

### Session 2026-04-05

- Q: Which concurrency model is required for admin edit mutations? → A: Use optimistic concurrency with version check; stale version submissions are rejected with `CONFLICT`.
- Q: What is the required lifecycle operation scope for organization/user/RBAC management in this feature? → A: Scope is create/edit/assign/deactivate only; delete/archive operations are out of scope.
- Q: What scale baseline must WebUI administration flows support in this feature? → A: Use a medium-scale baseline of 10,000 users and 1,000 roles per organization, with mandatory server-side pagination, filtering, and sorting for admin listings.
- Q: What baseline reliability target applies to admin WebUI/GraphQL operations for this feature? → A: Baseline reliability is 99.5% monthly availability for admin WebUI/GraphQL operations, with documented RTO <= 60 minutes.
- Q: What auditability baseline is required for admin mutations in this feature? → A: Record immutable audit events for all admin mutations (organization/user/role/assignment/bootstrap) including actor, organization scope, action, target, outcome, and timestamp, with minimum 90-day retention.

### Session 2026-04-05 (continued)

- Q: What minimum content must the dashboard display after bootstrap auto-login to make SC-001 verifiable? → A: System health summary card (health badge displaying server UP/DOWN status) plus quicklinks to the three admin flows (Organizations, Users, RBAC); this is the minimum verifiable surface for SC-001 and matches the current dashboard implementation.
- Q: What happens if the Playwright e2e global-setup harness cannot find or execute the server binary? → A: Fail immediately (throw) with an error message that includes the fully-resolved binary path and a build-command remediation hint (`cmake --build ./cmake-build-debug`); no retry or fallback launch attempt.
- Q: What timeout and retry policy applies to the `waitForServerReady` probe in Playwright global-setup? → A: 20-second total timeout (configurable via the existing `timeoutMs` parameter), 250 ms polling interval, single probe per poll cycle (no per-attempt retry); on expiry throw with the server URL and a remediation hint.
- Q: Is in-memory JWT storage (private class field in `AuthService`) acceptable as an interim implementation while the full cookie-based JWT flow is built? → A: In-memory storage (private class field, non-persistent, non-`localStorage`/`sessionStorage`/IndexedDB) is acceptable as an interim implementation satisfying FR-010's prohibition on persistent script-readable storage; a cookie-based JWT model must be enforced before the feature is marked complete.
- Q: Does FR-013b (Angular unit tests as build gate) apply to the CMake `isched_ui_build` target, the CI pipeline, or both? → A: Both; the `isched_ui_build` CMake custom target already enforces test-before-build (`pnpm run test` runs before `pnpm run build` in the same target), and the CI pipeline must equally treat a failing Angular unit-test step as a blocking gate.
- Q: How must the interim `AuthService.accessToken` implementation be retired before release? → A: Add one explicit blocking task that removes all `AuthService.accessToken` usage, enforces a cookie-only session flow, and requires related test updates as a pre-release gate.
- Q: What accessibility conformance baseline is required for the initial WebUI delivery scope? → A: Require WCAG 2.1 AA conformance for bootstrap/auth/admin CRUD/RBAC flows, including keyboard-only navigation and screen-reader labels for interactive controls.
- Q: What temporary lockout policy is required for repeated failed authentication attempts? → A: Apply temporary lockout after 5 failed attempts within 15 minutes; lockout lasts 15 minutes, then auto-unlocks.
- Q: How should FR-010f be represented in execution planning? → A: Option A (`RECOMMENDED`) - define exactly one dedicated blocking FR-010f task in `tasks.md` for retiring `AuthService.accessToken` and gating release on passing related test updates.
- Q: What is the canonical browser entry URL for the embedded WebUI, and how should plain HTTP GET requests to `/` or `/graphql` behave? → A: Canonical WebUI URL is `/isched`; HTTP `GET /` and `GET /graphql` MUST redirect to `/isched`.

### Session 2026-04-05 (003->004 gap-closure)

- Q: What startup observability contract is required for embedded Admin UI serving? → A: Server startup logs MUST emit one deterministic Admin UI status line that records whether embedded assets are active and the canonical entry path (`/isched`) for operator diagnostics.
- Q: How must bootstrap/auth rate-limit policy be configured when both global and auth-specific controls exist? → A: Auth/bootstrap lockout and rate-limit behavior MUST resolve through an explicit configuration chain with deterministic precedence: bootstrap/auth-specific policy first, then feature defaults, then global fallback.
- Q: How must embedded routing distinguish missing assets from SPA route fallback? → A: Requests that target missing static asset paths (for example under `/isched/assets/` or filename-like paths) MUST return `404` JSON error envelopes; SPA fallback to `index.html` applies only to route-style navigation paths.
- Q: Which HTTP hardening headers are mandatory for embedded WebUI responses? → A: Embedded WebUI responses MUST enforce a CSP baseline and MUST include `X-Content-Type-Options: nosniff` and `X-Frame-Options: DENY`.
- Q: What cache-validation contract is required for embedded assets? → A: Embedded static assets MUST expose stable ETag values and honor `If-None-Match` by returning `304 Not Modified` with no body when tags match.

### Implementation Evidence 2026-04-05

- Dedicated embedded-serving integration coverage (task T022) and focused validation logs are recorded in:
  - `specs/archive/004-add-isched-webui/evidence/t022-embedded-serving-validation-2026-04-05.md`
  - `docs/validation/isched-webui-004-core-validation-2026-04-05.md`

## User Scenarios & Testing *(mandatory)*

### User Story 1 - First-Time Platform Bootstrap (Priority: P1)

As the first platform administrator, I can complete a one-time bootstrap flow that initializes the platform and creates an initial privileged account so the system becomes operable without manual database setup.

**Why this priority**: Without a reliable first-run bootstrap, no organization, user, or RBAC workflows can be used from the WebUI.

**Independent Test**: Can be fully tested by launching an uninitialized environment, completing bootstrap once, and verifying that a second bootstrap attempt is blocked while normal sign-in is enabled.

**Acceptance Scenarios**:

1. **Given** an uninitialized platform, **When** an operator opens the WebUI, completes required bootstrap fields, and submits, **Then** the platform is initialized, an initial admin identity is created, and the user is authenticated.
2. **Given** a platform that is already initialized, **When** any user attempts to access bootstrap flow, **Then** bootstrap actions are denied and the user is redirected to normal authentication.
3. **Given** a platform that is not yet initialized, **When** an unauthenticated operator opens the bootstrap route, **Then** bootstrap UI is accessible without prior sign-in only for completing first-time initialization.
4. **Given** bootstrap submission with invalid or incomplete input, **When** the operator submits, **Then** the UI blocks completion, highlights validation issues, and preserves entered values where safe.
5. **Given** a user records 5 failed authentication attempts within a rolling 15-minute window, **When** the next authentication attempt is made during lockout, **Then** authentication is rejected with deterministic lockout feedback and access remains blocked until 15 minutes have elapsed, after which authentication is automatically allowed again.

---

### User Story 2 - Organization and User Administration (Priority: P1)

As a platform or organization administrator, I can administer organizations and users according to role scope so tenant onboarding and identity management remain correctly bounded.

**Why this priority**: Multi-organization administration and user lifecycle management are core product capabilities required for day-to-day operation.

**Independent Test**: Can be fully tested by creating an organization, adding users to that organization, editing both entities, and confirming changes are visible immediately in the correct scope.

**Acceptance Scenarios**:

1. **Given** an authenticated platform admin with sufficient permissions, **When** they create an organization with valid data, **Then** the organization appears in organization listings and becomes selectable for scoped administration.
2. **Given** an existing organization, **When** a platform admin or that organization's admin edits mutable organization profile fields, **Then** updated values are persisted and shown consistently across organization detail and list views.
3. **Given** a selected organization scope, **When** an authorized admin creates or edits users, **Then** changes are applied only within the selected organization context.
4. **Given** an attempted create or edit with invalid data, a login identifier conflict within the selected organization, or out-of-scope permissions, **When** the operation is submitted, **Then** the UI shows actionable errors and prevents partial or ambiguous completion.
5. **Given** a user is deactivated and later re-enabled within the same organization, **When** an authorized admin toggles the user status, **Then** existing role assignments are retained, remain inactive while the user is disabled, and automatically reactivate upon re-enable.

---

### User Story 3 - RBAC Roles and Assignments (Priority: P1)

As an administrator, I can manage role-based access control by using built-in roles, defining custom roles, and assigning roles to users so access policies match organizational needs.

**Why this priority**: RBAC is required to safely delegate administrative tasks and enforce least-privilege access in multi-organization environments.

**Independent Test**: Can be fully tested by assigning a built-in role to a user, creating a custom role, assigning it, and verifying resulting permissions and unauthorized-action blocking.

**Acceptance Scenarios**:

1. **Given** built-in roles are available, **When** an authorized admin assigns one to a user, **Then** the assignment is reflected immediately and effective permissions change accordingly.
2. **Given** an authorized admin in an organization scope, **When** they define a custom role and assign permissions, **Then** the role is saved for that scope and becomes assignable to eligible users.
3. **Given** a user has insufficient permissions for RBAC changes, **When** they attempt role creation or assignment, **Then** the action is denied and the UI surfaces a clear authorization error.

---

### User Story 4 - Local WebUI Development Workflow (Priority: P2)

As a developer, I can run the WebUI independently in local development with a documented proxy to a locally running backend, enabling rapid UI iteration without changing backend deployment topology.

**Why this priority**: Fast and reliable developer workflow reduces cycle time and improves delivery quality for all WebUI features.

**Independent Test**: Can be fully tested by following documented steps to start backend and UI locally, verifying WebUI calls are routed through the proxy to `/graphql`, and confirming authenticated flows work end-to-end.

**Acceptance Scenarios**:

1. **Given** backend is running locally, **When** a developer starts the WebUI in standalone mode using documented steps, **Then** GraphQL requests are proxied correctly and UI pages load functional data.
2. **Given** proxy is unavailable or misconfigured, **When** the developer uses the WebUI, **Then** failures are surfaced with clear diagnostics pointing to local setup remediation.

### Edge Cases

- What happens when a JWT expires during a multi-step edit flow? The UI must require re-authentication and preserve unsaved changes in memory until user decision.
- How does the system handle switching organization context during pending edits? The UI must warn about context switch risk and prevent cross-organization accidental writes.
- What happens when network interruptions occur during role assignment? The UI must show operation status clearly and prevent duplicate submissions.
- What happens when concurrent edits target the same organization, user, or custom role? The system must enforce optimistic version checks and reject stale submissions with `CONFLICT` so the UI can refresh and reconcile before retry.
- How are backend authorization and validation errors surfaced? The UI must map each GraphQL error to field-level errors where possible and otherwise to global actionable alerts within the same interaction cycle.
- How are CSRF validation failures handled for state-changing mutations? The UI must surface a clear re-authentication/retry path and block mutation completion.
- What happens to role assignments when a user is disabled? Assignments must remain stored but inactive until the user is re-enabled.
- How are abusive or burst mutation attempts handled on auth and sensitive admin actions? The backend must enforce rate limiting and return deterministic `RATE_LIMITED` responses so the UI can show clear retry guidance.
- How are repeated failed authentication attempts handled? The system must enforce a temporary lockout after 5 failed attempts in 15 minutes, reject authentication during lockout with deterministic messaging, and auto-unlock after 15 minutes.
- What happens when operators open the backend origin root (`/`) or accidentally issue browser `GET /graphql`? Both entry points must redirect to canonical `/isched` to avoid ambiguous navigation behavior.
- What happens when a browser requests a missing embedded asset path (for example `/isched/assets/missing.js`)? The server must return a deterministic `404` JSON error envelope and must not return SPA `index.html` fallback.
- What happens when a browser requests a valid SPA deep-link route that is not a concrete asset file? The server must return SPA `index.html` fallback for client-side routing.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide an embedded WebUI experience served as part of the isched product and reachable by authenticated users for post-bootstrap administration flows.
- **FR-001a**: In non-development runtime mode, the backend MUST serve WebUI static assets directly (including SPA route fallback to `index.html`) from the isched process; this embedded serving path MUST be the default product delivery path.
- **FR-001b**: The canonical browser entry URL for embedded WebUI runtime MUST be `/isched`.
- **FR-001c**: HTTP `GET /` and HTTP `GET /graphql` MUST return redirect responses to `/isched`.
- **FR-001d**: On backend startup, the server MUST emit a deterministic Admin UI startup log line that reports embedded WebUI serving state and canonical entry route `/isched`.
- **FR-001e**: Embedded WebUI routing MUST distinguish missing static assets from SPA routes: missing asset requests MUST return HTTP `404` with a JSON error envelope; SPA deep-link navigation requests MUST return `index.html` fallback.
- **FR-001f**: Embedded WebUI responses MUST include security hardening headers: CSP baseline policy, `X-Content-Type-Options: nosniff`, and `X-Frame-Options: DENY`.
- **FR-001g**: Embedded static assets MUST include deterministic `ETag` headers and MUST honor `If-None-Match` requests with HTTP `304 Not Modified` when tags match.
- **FR-002**: The system MUST provide a one-time platform bootstrap flow that is available only before initial platform setup is completed.
- **FR-002a**: The bootstrap route MUST allow unauthenticated access only while bootstrap is allowed; once initialized, bootstrap route access MUST be denied and routed to standard authentication.
- **FR-003**: The system MUST prevent repeated bootstrap completion attempts after initial setup and MUST direct users to standard authentication.
- **FR-004**: The system MUST allow only platform administrators to create organizations.
- **FR-005**: The system MUST allow platform administrators and organization administrators to edit organization profiles only for organizations within their authorization scope.
- **FR-006**: The system MUST allow authorized administrators to create and edit users only within an explicit organization context and within their authorization scope.
- **FR-006a**: The system MUST provide RBAC management including built-in roles, custom role definition, and role assignment to users, limited to each administrator's organization scope unless platform-level permission is explicitly granted.
- **FR-006b**: The system MUST enforce user login identifier uniqueness per organization and MUST allow the same login identifier to exist in different organizations.
- **FR-006c**: The system MUST retain a user's existing role assignments when that user is deactivated, MUST treat those assignments as inactive while the user remains disabled, and MUST automatically reactivate those assignments when the user is re-enabled.
- **FR-006d**: Edit mutations for organizations, users, and custom roles MUST require an explicit version/revision check for optimistic concurrency; stale version submissions MUST fail atomically with `CONFLICT` and MUST NOT perform partial writes.
- **FR-006e**: For this feature, organization/user/RBAC lifecycle management scope MUST be limited to create, edit, assign, and deactivate/reactivate flows; hard delete and archive/restore flows are explicitly out of scope.
- **FR-006f**: Organization, user, role, and role-assignment listing/query flows in WebUI admin screens MUST use server-side pagination, filtering, and sorting (GraphQL query arguments resolved on the server), and MUST NOT rely on client-side full-dataset loading for these resources.
- **FR-007**: The system MUST enforce authorization checks for all organization, user, and RBAC mutations and MUST return clear denial feedback to unauthorized users.
- **FR-007a**: The system MUST apply rate limiting to authentication mutations and sensitive admin mutations, and MUST reject over-limit requests with deterministic GraphQL error code `RATE_LIMITED` (including retry guidance metadata when available).
- **FR-007b**: Bootstrap/authentication rate-limit and lockout policy resolution MUST use an explicit configuration chain with deterministic precedence (bootstrap/auth-specific override -> feature default -> global fallback), and resolved values MUST be traceable in diagnostics.
- **FR-008**: The WebUI MUST integrate with backend data operations exclusively through GraphQL requests to the `/graphql` endpoint.
- **FR-009**: The WebUI MUST use a JWT-based authentication model for protected operations and MUST handle token expiration, invalid token states, and sign-out safely.
- **FR-009a**: Authentication MUST enforce a temporary lockout policy of 5 failed attempts within a rolling 15-minute window; lockout duration MUST be 15 minutes and MUST auto-expire without manual admin intervention.
- **FR-010**: The WebUI MUST use secure HttpOnly SameSite cookie-based JWT handling, MUST NOT store JWTs in `localStorage`, `sessionStorage`, IndexedDB, or other script-readable browser storage, and MUST prevent token values from appearing in UI-visible errors or logs.
- **FR-010b**: The system MUST prevent JWT value leakage in frontend logs, backend logs, GraphQL error payloads, and feature telemetry/diagnostic events; verification evidence MUST include automated checks of representative success and failure flows.
- **FR-010a**: For every state-changing GraphQL mutation, the system MUST enforce CSRF defenses using double-submit token validation and strict `Origin`/`Referer` validation; requests failing either check MUST be rejected with actionable error feedback.
- **FR-010c**: Backend and integration tests that execute authenticated GraphQL mutations MUST first obtain authentication via the bootstrap/login GraphQL flow and MUST attach a valid JWT context before asserting mutation success paths.
- **FR-010d**: The repository MUST provide shared GraphQL test helper utilities for common auth/security setup (bootstrap/login, JWT header wiring, CSRF token/header wiring, and standard error-code assertions) so mutation-oriented tests do not duplicate fragile setup logic.
- **FR-010e**: Until the full secure HttpOnly SameSite cookie-based JWT flow is implemented, in-memory JWT storage via a private class field in `AuthService` (non-persistent, not written to `localStorage`, `sessionStorage`, or IndexedDB) is an acceptable interim implementation that satisfies FR-010's prohibition on persistent script-readable storage; the cookie-based JWT model MUST be enforced and all `accessToken` field usage replaced before the feature is marked complete.
- **FR-010f**: The feature implementation MUST include exactly one explicit dedicated blocking task in `tasks.md` to remove all `AuthService.accessToken` reads/writes and transition to cookie-only session handling; release readiness MUST be blocked until this task is complete and related Angular unit tests plus authentication integration/e2e tests are updated and passing.
- **FR-011**: The system MUST enforce explicit multi-organization context selection for administrative operations and MUST prevent cross-organization writes caused by stale context.
- **FR-012**: The WebUI MUST surface backend validation, authentication, authorization, and connectivity errors in a user-actionable manner.
- **FR-012a**: Error surfacing MUST be measurable: `VALIDATION_FAILED` maps to field-level messages for all invalid input fields, and `UNAUTHENTICATED`, `FORBIDDEN`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `TRANSIENT_NETWORK` map to deterministic global alerts with retry or re-auth actions.
- **FR-012b**: `CONFLICT` responses from stale-version edit attempts MUST surface deterministic conflict guidance, including a refresh/reload action and a safe path to re-apply pending edits.
- **FR-012c**: `RATE_LIMITED` responses MUST surface as deterministic global alerts with actionable retry guidance; when retry metadata is present, the UI MUST display it.
- **FR-012d**: Authentication lockout responses MUST surface as deterministic global alerts that include lockout state and retry timing guidance; successful authentication MUST be possible automatically after lockout expiry.
- **FR-013**: The WebUI MUST provide a standalone local development mode with documented proxy configuration to a locally running isched backend.
- **FR-013a**: Dev proxy scope MUST include `/graphql` HTTP routing and WebSocket upgrade routing verification; business subscription features are optional unless explicitly required by a user story.
- **FR-013b**: For embedded WebUI build acceptance, Angular unit tests are part of the required green unit-test suite; the CMake `isched_ui_build` custom target MUST run `pnpm run test` before `pnpm run build` (already enforced in the target definition), AND the CI pipeline MUST treat a failing Angular unit-test step as a blocking gate; npm/UI build execution MUST fail when Angular unit tests are not green in both contexts.
- **FR-014**: The local development documentation MUST include a runnable workflow for backend startup in the background, WebUI startup, proxy troubleshooting, and expected verification steps.
- **FR-015**: The WebUI visual system MUST align with Tailwind CSS and DaisyUI design components for consistent styling and interaction patterns.
- **FR-016**: The feature MUST include measurable performance and scalability verification for embedded WebUI serving and GraphQL admin flows: static asset GET p95 <= 200 ms (local non-dev runtime), representative admin GraphQL operations p95 <= 300 ms at 50 concurrent virtual users for 5 minutes, and error rate < 1% excluding intentionally rejected auth/CSRF cases.
- **FR-016a**: Performance/scalability verification datasets MUST include at least 10,000 users and 1,000 roles within a single organization when validating admin list/search/sort flows, using the required server-side pagination/filter/sort behavior.
- **FR-016b**: For this feature baseline, admin WebUI and `/graphql` admin operations MUST target 99.5% monthly availability, and operations documentation MUST define an incident recovery procedure with RTO <= 60 minutes.
- **FR-017**: The `isched_srv` process MUST support a `--data-dir` CLI override (and `--data-dir=<path>` form) so test and local workflows can run against isolated temporary data directories without mutating shared host state.
- **FR-017a**: When `--data-dir` is not supplied, the server default data directory MUST be `<DataHome>/isched` resolved from `sago::getDataHome()`, and all platform/tenant databases MUST be rooted under that directory.
- **FR-018**: The repository MUST provide Playwright integration coverage that starts the real backend process with a temporary `--data-dir` and verifies bootstrap UI availability and routing at `/isched/bootstrap` in seed mode.
- **FR-018a**: The Playwright global-setup harness MUST resolve the server binary from a CMake build directory configurable via the `ISCHED_BUILD_DIR` environment variable, defaulting to `cmake-build-debug`; the path MUST be resolved relative to the repository root so CI and alternate build trees are trivially supported.
- **FR-018b**: If the Playwright global-setup harness cannot find or execute the server binary (i.e., `spawn` returns without a valid PID), setup MUST throw immediately with an error message that includes the fully-resolved binary path and a build-command remediation hint (`cmake --build ./cmake-build-debug`); no retry or fallback launch is permitted. The `waitForServerReady` probe MUST use a 20-second total timeout with 250 ms polling interval and a single probe per cycle; on timeout expiry it MUST throw with the server URL and a remediation hint; no retry loop is permitted after expiry.
- **FR-019**: The system MUST emit immutable audit events for every successful or failed admin mutation affecting bootstrap, organization management, user management, role management, and role assignments; each event MUST include actor identity, organization scope, action, target, outcome, and timestamp.
- **FR-019a**: Audit events for admin mutations MUST be retained for a minimum of 90 days and MUST remain queryable for security and operational investigations during the retention window.
- **FR-020**: The post-bootstrap dashboard MUST display, at minimum, a system health summary card (server UP/DOWN status badge) and navigation quicklinks to the three primary admin flows (Organizations, Users, RBAC); this minimum surface is the verifiable entry point for SC-001 and MUST be present before any acceptance test run can be considered valid.
- **FR-021**: Bootstrap, authentication, and admin CRUD/RBAC flows in the WebUI MUST conform to WCAG 2.1 AA, including complete keyboard-only navigation paths and programmatic screen-reader labels for interactive controls.

### Architecture and Integration Constraints

- Integration scope is GraphQL-only for backend communication; no REST or alternate transport paths are introduced for WebUI backend operations.
- Backend integration endpoint is `/graphql` under the existing isched backend runtime.
- Authentication model remains JWT-based and must not be replaced by a different primary model in this feature.
- JWT transport for WebUI sessions must use secure HttpOnly SameSite cookie(s), and the Angular app must treat tokens as non-readable credentials.
- CSRF protection for cookie-authenticated state-changing GraphQL mutations must combine double-submit token checks with strict `Origin`/`Referer` validation.
- Multi-organization behavior is mandatory for admin operations and must be consistently represented across navigation, forms, and mutations.
- Administrative scope boundaries are strict: platform admins may create organizations, while organization admins are restricted to their own organization profile and in-scope user/RBAC administration.
- Security posture must prioritize token confidentiality, least privilege, and clear handling of authentication/authorization failures.

### Angular Implementation Conventions (Expected)

- State management should follow a signal-first approach for local and shared UI state.
- App-owned template state MUST be signal-backed; do not drive app-owned template state via async-pipe subscriptions to component-owned observables.
- Exception: immutable third-party stream contracts may be bridged with async pipe only when explicitly documented in code and feature docs.
- Component architecture should default to standalone components.
- Templates should use modern control flow syntax (`@if`, `@for`, `@switch`) in new feature code.
- Forms for create/edit flows should use typed reactive forms.
- NgModule-centric architecture should be avoided unless a third-party integration explicitly requires it.
- Templates and styles MUST be in separate files — use `templateUrl` and `styleUrl`, never inline `template` or `styles`. Each component gets a `.html` and `.scss` file alongside its `.ts` file.
- CSS framework: Tailwind CSS 3.x + DaisyUI 4.x — use DaisyUI component classes (e.g. `btn`, `card`, `alert`, `modal`) for consistent styling; extend with Tailwind utilities as needed.

### Key Entities *(include if feature involves data)*

- **PlatformBootstrap**: One-time initialization state and initial admin bootstrap submission outcome.
- **Organization**: Tenant boundary with identifying profile fields and lifecycle metadata used for scope selection.
  - Edit operations include a version/revision value for optimistic concurrency checks.
- **User**: Identity record scoped to an organization, with profile attributes, status, and role assignments.
  - Login identifier must be unique within its organization scope and may be reused by a different organization.
  - Edit operations include a version/revision value for optimistic concurrency checks.
- **Role**: Access control definition that can be built-in or custom, containing a permission set and organization scope.
  - Custom role edit operations include a version/revision value for optimistic concurrency checks.
- **RoleAssignment**: Mapping between user and role within an organization context.
  - Assignments are retained across user deactivation/reactivation and are effective only while the user status is active.
- **AuthSession**: Authenticated user session represented by JWT lifecycle state relevant to UI access control.
  - JWT value is opaque to the WebUI and is only conveyed via secure HttpOnly SameSite cookie(s).
  - Failed-attempt tracking includes rolling-window counters and `lockedUntil` timestamp used to enforce temporary auth lockout policy.
- **AuditEvent**: Immutable record of an admin mutation attempt/outcome used for traceability and investigations.
  - Required fields: actor identity, organization scope, action, target, outcome, timestamp.
  - Retention policy: minimum 90 days.

### Assumptions

- Existing backend GraphQL schema will expose or be extended to expose required bootstrap, organization, user, and RBAC operations.
- The WebUI is primarily used by administrative users and does not include end-customer self-service in this feature scope.
- Platform admin and organization admin roles exist with distinct scope boundaries enforced by backend authorization.
- Built-in roles are predefined by product policy and cannot be structurally modified, only assigned.
- Custom role permissions are constrained to permissions recognized by backend authorization rules.
- Local development documentation is maintained in repository docs and is updated alongside feature delivery.

### Dependencies

- Availability of backend GraphQL operations for bootstrap, organization, user, and RBAC workflows.
- JWT issuance and verification behavior from the existing authentication subsystem.
- Existing environment or scripts to run backend locally for proxy-based UI development.
- Availability of a shared backend test utility layer for GraphQL auth bootstrap and CSRF/JWT header setup used by integration and contract tests.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In UAT, at least 95% of first-time operators complete platform bootstrap in under 5 minutes without manual backend intervention, measured using a sample of 20 clean-environment bootstrap trials (`CompleteBootstrap`) executed via the feature quickstart workflow from bootstrap screen load to successful mutation response; pass threshold is >=19/20 runs under 5:00, and raw timing/outcome evidence is recorded in `specs/archive/004-add-isched-webui/evidence/sc001-sc003-uat.md`.
- **SC-002**: In role-based acceptance testing, 100% of unauthorized attempts to create/edit organizations, users, or role assignments are blocked and surfaced with clear user feedback.
- **SC-003**: In scripted acceptance tests, admins complete organization/user create-or-edit flows with at least a 95% first-attempt completion rate, measured using 40 scripted trials (10 organization-create, 10 organization-edit, 10 user-create, 10 user-edit) run without operator retries or facilitator intervention; pass threshold is >=38/40 first-attempt successes, and per-trial outcomes/failure reasons are recorded in `specs/archive/004-add-isched-webui/evidence/sc001-sc003-uat.md`.
- **SC-004**: In RBAC validation tests, 100% of tested built-in and custom role assignment changes are reflected in effective access behavior within one user refresh cycle.
- **SC-005**: In multi-organization test runs, 0 cross-organization write operations occur when users switch context during active sessions.
- **SC-006**: For local developer onboarding, a new team member can follow documented setup and run the WebUI against a local backend via proxy in under 20 minutes.
- **SC-007**: In security verification tests, 100% of authenticated WebUI flows operate without any JWT persisted in script-readable browser storage (`localStorage`, `sessionStorage`, IndexedDB).
- **SC-008**: In security tests for authenticated state-changing GraphQL mutations, 100% of cross-site or missing/invalid CSRF token attempts are rejected, and 100% of same-origin valid-token mutation attempts succeed.
- **SC-009**: In non-development runtime integration tests, 100% of validated WebUI entry routes are served by backend static asset hosting with SPA fallback behavior and no dependency on Angular dev server.
- **SC-010**: In automated log/telemetry redaction tests, 0 JWT token values appear in captured frontend logs, backend logs, GraphQL error payloads, or telemetry events across representative auth success/failure scenarios.
- **SC-011**: In performance verification runs with 50 concurrent virtual users for 5 minutes, p95 latency remains <= 200 ms for embedded WebUI static asset GETs and <= 300 ms for representative admin GraphQL operations, with non-intentional error rate < 1%.
- **SC-011a**: In admin list/query verification against a dataset containing at least 10,000 users and 1,000 roles in one organization, 100% of validated organization/user/role screens execute pagination, filtering, and sorting server-side (verified by GraphQL request parameters and bounded response page size), with no client-side full-dataset fetches.
- **SC-012**: In acceptance tests, 100% of checked GraphQL error codes (`VALIDATION_FAILED`, `UNAUTHENTICATED`, `FORBIDDEN`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `TRANSIENT_NETWORK`) surface through the required field/global UI channels.
- **SC-013**: In CI/local automation, the Playwright bootstrap integration test passes in 100% of runs where the backend is started with an isolated temporary `--data-dir`, proving seed-mode bootstrap UI rendering at `/isched/bootstrap`.
- **SC-014**: In concurrent-edit tests, 100% of stale-version organization/user/custom-role edit attempts are rejected with `CONFLICT`, and 100% of validated conflict responses surface refresh/reconcile guidance in the UI.
- **SC-015**: In a representative 30-day staging or synthetic uptime window, measured availability for admin WebUI and `/graphql` admin operations is >= 99.5%, and at least one documented recovery drill demonstrates restoration within RTO <= 60 minutes.
- **SC-016**: In audit-verification tests across representative bootstrap, organization, user, role, and role-assignment admin mutation flows (success and failure cases), 100% of validated mutations produce immutable audit events containing actor identity, organization scope, action, target, outcome, and timestamp, and retention checks confirm event availability for at least 90 days.
- **SC-017**: In post-bootstrap navigation tests, 100% of validated bootstrap completion flows land on a dashboard page that displays a system health summary card (server status badge) and clickable quicklinks to the Organizations, Users, and RBAC admin flows, confirming FR-020 minimum content is present before SC-001 acceptance trials begin.
- **SC-018**: In rate-limit verification tests for authentication and sensitive admin mutations, 100% of over-limit attempts return GraphQL error code `RATE_LIMITED` and 100% of validated UI flows display deterministic retry guidance for those responses.
- **SC-019**: In accessibility verification for bootstrap/auth/admin CRUD/RBAC flows, 100% of validated critical journeys meet WCAG 2.1 AA checks, including keyboard-only completion without pointer input and presence of screen-reader-accessible labels for all tested interactive controls.
- **SC-020**: In authentication security tests, 100% of accounts reaching 5 failed attempts within 15 minutes are locked for exactly 15 minutes (with deterministic lockout feedback), 100% of in-lockout attempts are rejected, and 100% of post-expiry attempts are accepted without manual unlock.
- **SC-021**: In embedded-serving security tests, 100% of validated WebUI responses include required hardening headers (CSP baseline, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`) and 0 validated responses regress these controls.
- **SC-022**: In embedded-serving cache tests, 100% of validated static asset responses include deterministic `ETag` values and 100% of `If-None-Match` requests with matching tags return HTTP `304` with empty body.
- **SC-023**: In route-contract integration tests, 100% of validated missing embedded asset requests return HTTP `404` JSON envelopes while 100% of validated SPA route deep links return `index.html` fallback.
- **SC-024**: In startup/diagnostics validation, 100% of backend startups emit the required Admin UI startup status log line, and rate-limit configuration-chain verification confirms deterministic bootstrap/auth policy precedence in all tested configuration permutations.
