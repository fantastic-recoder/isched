# Feature Specification: Isched WebUI

**Feature Branch**: `004-add-isched-webui`  
**Created**: 2026-04-04  
**Status**: Draft  
**Input**: User description: "Isched WebUI. Requirements: Add an embedded Angular 21 based Web UI to isched backend. UI capabilities must include: one-time platform bootstrap flow, create/edit organizations, create/edit users, and RBAC management (built-in roles + custom role definitions and assignments). UI stack must use Tailwind CSS and DaisyUI. Angular app must support standalone local development with proxy to locally running isched server (background), documenting proxy setup and dev workflow. Include acceptance criteria and measurable success criteria for these flows. Ensure architecture remains GraphQL-only integration with backend /graphql endpoint and JWT-based auth model. Include constraints for security (token handling), multi-organization context, and error surfacing.

Also include a section that references modern Angular coding conventions expected in this feature implementation: signal-first state management, standalone components, `@if/@for/@switch` template control flow, typed reactive forms, and no NgModule-centric architecture unless required by third-party constraints."

## Clarifications

### Session 2026-04-04

- Q: Which JWT handling model is required for WebUI auth state? → A: JWT is carried in secure HttpOnly SameSite cookie(s); the WebUI must never read token values directly.
- Q: What is the admin responsibility split for organization management? → A: Platform admins create organizations; organization admins can edit only their own organization profile and manage users/RBAC within their organization scope.
- Q: What is the uniqueness scope for user login identifiers? → A: Login identifier must be unique within an organization; the same identifier may exist in different organizations.
- Q: Which CSRF protection model is required for state-changing GraphQL mutations? → A: Use double-submit CSRF token validation plus strict Origin/Referer validation for state-changing mutations.
- Q: What happens to role assignments when a user is deactivated and later re-enabled? → A: Deactivated users keep role assignments; assignments are inactive while user is disabled and auto-reactivate when user is re-enabled.

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
- How are backend authorization and validation errors surfaced? The UI must map each GraphQL error to field-level errors where possible and otherwise to global actionable alerts within the same interaction cycle.
- How are CSRF validation failures handled for state-changing mutations? The UI must surface a clear re-authentication/retry path and block mutation completion.
- What happens to role assignments when a user is disabled? Assignments must remain stored but inactive until the user is re-enabled.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide an embedded WebUI experience served as part of the isched product and reachable by authenticated users for post-bootstrap administration flows.
- **FR-001a**: In non-development runtime mode, the backend MUST serve WebUI static assets directly (including SPA route fallback to `index.html`) from the isched process; this embedded serving path MUST be the default product delivery path.
- **FR-002**: The system MUST provide a one-time platform bootstrap flow that is available only before initial platform setup is completed.
- **FR-002a**: The bootstrap route MUST allow unauthenticated access only while bootstrap is allowed; once initialized, bootstrap route access MUST be denied and routed to standard authentication.
- **FR-003**: The system MUST prevent repeated bootstrap completion attempts after initial setup and MUST direct users to standard authentication.
- **FR-004**: The system MUST allow only platform administrators to create organizations.
- **FR-005**: The system MUST allow platform administrators and organization administrators to edit organization profiles only for organizations within their authorization scope.
- **FR-006**: The system MUST allow authorized administrators to create and edit users only within an explicit organization context and within their authorization scope.
- **FR-006a**: The system MUST provide RBAC management including built-in roles, custom role definition, and role assignment to users, limited to each administrator's organization scope unless platform-level permission is explicitly granted.
- **FR-006b**: The system MUST enforce user login identifier uniqueness per organization and MUST allow the same login identifier to exist in different organizations.
- **FR-006c**: The system MUST retain a user's existing role assignments when that user is deactivated, MUST treat those assignments as inactive while the user remains disabled, and MUST automatically reactivate those assignments when the user is re-enabled.
- **FR-007**: The system MUST enforce authorization checks for all organization, user, and RBAC mutations and MUST return clear denial feedback to unauthorized users.
- **FR-008**: The WebUI MUST integrate with backend data operations exclusively through GraphQL requests to the `/graphql` endpoint.
- **FR-009**: The WebUI MUST use a JWT-based authentication model for protected operations and MUST handle token expiration, invalid token states, and sign-out safely.
- **FR-010**: The WebUI MUST use secure HttpOnly SameSite cookie-based JWT handling, MUST NOT store JWTs in `localStorage`, `sessionStorage`, IndexedDB, or other script-readable browser storage, and MUST prevent token values from appearing in UI-visible errors or logs.
- **FR-010b**: The system MUST prevent JWT value leakage in frontend logs, backend logs, GraphQL error payloads, and feature telemetry/diagnostic events; verification evidence MUST include automated checks of representative success and failure flows.
- **FR-010a**: For every state-changing GraphQL mutation, the system MUST enforce CSRF defenses using double-submit token validation and strict `Origin`/`Referer` validation; requests failing either check MUST be rejected with actionable error feedback.
- **FR-010c**: Backend and integration tests that execute authenticated GraphQL mutations MUST first obtain authentication via the bootstrap/login GraphQL flow and MUST attach a valid JWT context before asserting mutation success paths.
- **FR-010d**: The repository MUST provide shared GraphQL test helper utilities for common auth/security setup (bootstrap/login, JWT header wiring, CSRF token/header wiring, and standard error-code assertions) so mutation-oriented tests do not duplicate fragile setup logic.
- **FR-011**: The system MUST enforce explicit multi-organization context selection for administrative operations and MUST prevent cross-organization writes caused by stale context.
- **FR-012**: The WebUI MUST surface backend validation, authentication, authorization, and connectivity errors in a user-actionable manner.
- **FR-012a**: Error surfacing MUST be measurable: `VALIDATION_FAILED` maps to field-level messages for all invalid input fields, and `UNAUTHENTICATED`, `FORBIDDEN`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `TRANSIENT_NETWORK` map to deterministic global alerts with retry or re-auth actions.
- **FR-013**: The WebUI MUST provide a standalone local development mode with documented proxy configuration to a locally running isched backend.
- **FR-013a**: Dev proxy scope MUST include `/graphql` HTTP routing and WebSocket upgrade routing verification; business subscription features are optional unless explicitly required by a user story.
- **FR-014**: The local development documentation MUST include a runnable workflow for backend startup in the background, WebUI startup, proxy troubleshooting, and expected verification steps.
- **FR-015**: The WebUI visual system MUST align with Tailwind CSS and DaisyUI design components for consistent styling and interaction patterns.
- **FR-016**: The feature MUST include measurable performance and scalability verification for embedded WebUI serving and GraphQL admin flows: static asset GET p95 <= 200 ms (local non-dev runtime), representative admin GraphQL operations p95 <= 300 ms at 50 concurrent virtual users for 5 minutes, and error rate < 1% excluding intentionally rejected auth/CSRF cases.

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
- Component architecture should default to standalone components.
- Templates should use modern control flow syntax (`@if`, `@for`, `@switch`) in new feature code.
- Forms for create/edit flows should use typed reactive forms.
- NgModule-centric architecture should be avoided unless a third-party integration explicitly requires it.

### Key Entities *(include if feature involves data)*

- **PlatformBootstrap**: One-time initialization state and initial admin bootstrap submission outcome.
- **Organization**: Tenant boundary with identifying profile fields and lifecycle metadata used for scope selection.
- **User**: Identity record scoped to an organization, with profile attributes, status, and role assignments.
  - Login identifier must be unique within its organization scope and may be reused by a different organization.
- **Role**: Access control definition that can be built-in or custom, containing a permission set and organization scope.
- **RoleAssignment**: Mapping between user and role within an organization context.
  - Assignments are retained across user deactivation/reactivation and are effective only while the user status is active.
- **AuthSession**: Authenticated user session represented by JWT lifecycle state relevant to UI access control.
  - JWT value is opaque to the WebUI and is only conveyed via secure HttpOnly SameSite cookie(s).

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

- **SC-001**: In UAT, at least 95% of first-time operators complete platform bootstrap in under 5 minutes without manual backend intervention, measured using a sample of 20 clean-environment bootstrap trials (`CompleteBootstrap`) executed via the feature quickstart workflow from bootstrap screen load to successful mutation response; pass threshold is >=19/20 runs under 5:00, and raw timing/outcome evidence is recorded in `specs/004-add-isched-webui/evidence/sc001-sc003-uat.md`.
- **SC-002**: In role-based acceptance testing, 100% of unauthorized attempts to create/edit organizations, users, or role assignments are blocked and surfaced with clear user feedback.
- **SC-003**: In scripted acceptance tests, admins complete organization/user create-or-edit flows with at least a 95% first-attempt completion rate, measured using 40 scripted trials (10 organization-create, 10 organization-edit, 10 user-create, 10 user-edit) run without operator retries or facilitator intervention; pass threshold is >=38/40 first-attempt successes, and per-trial outcomes/failure reasons are recorded in `specs/004-add-isched-webui/evidence/sc001-sc003-uat.md`.
- **SC-004**: In RBAC validation tests, 100% of tested built-in and custom role assignment changes are reflected in effective access behavior within one user refresh cycle.
- **SC-005**: In multi-organization test runs, 0 cross-organization write operations occur when users switch context during active sessions.
- **SC-006**: For local developer onboarding, a new team member can follow documented setup and run the WebUI against a local backend via proxy in under 20 minutes.
- **SC-007**: In security verification tests, 100% of authenticated WebUI flows operate without any JWT persisted in script-readable browser storage (`localStorage`, `sessionStorage`, IndexedDB).
- **SC-008**: In security tests for authenticated state-changing GraphQL mutations, 100% of cross-site or missing/invalid CSRF token attempts are rejected, and 100% of same-origin valid-token mutation attempts succeed.
- **SC-009**: In non-development runtime integration tests, 100% of validated WebUI entry routes are served by backend static asset hosting with SPA fallback behavior and no dependency on Angular dev server.
- **SC-010**: In automated log/telemetry redaction tests, 0 JWT token values appear in captured frontend logs, backend logs, GraphQL error payloads, or telemetry events across representative auth success/failure scenarios.
- **SC-011**: In performance verification runs with 50 concurrent virtual users for 5 minutes, p95 latency remains <= 200 ms for embedded WebUI static asset GETs and <= 300 ms for representative admin GraphQL operations, with non-intentional error rate < 1%.
- **SC-012**: In acceptance tests, 100% of checked GraphQL error codes (`VALIDATION_FAILED`, `UNAUTHENTICATED`, `FORBIDDEN`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `TRANSIENT_NETWORK`) surface through the required field/global UI channels.
