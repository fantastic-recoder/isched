# Feature Specification: Isched WebUI Enhancements

**Feature Branch**: `007-webui-nav-status-bars`  
**Created**: 2026-04-06  
**Status**: Done  
**Input**: User description: "Isched WebUI enhancements"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Persistent App Shell Navigation (Priority: P1)

As an authenticated WebUI user, I want a top navigation bar with the isched logo and a navigation menu so I can move between major areas quickly and stay oriented.

**Why this priority**: Navigation is foundational. Without it, users cannot efficiently reach core workflows.

**Independent Test**: Can be fully tested by loading the WebUI, verifying the top bar appears on all primary screens, confirming the logo is rendered from assets, and using the menu to navigate between sections.

**Acceptance Scenarios**:

1. **Given** an authenticated user is on any primary WebUI page, **When** the page is displayed, **Then** a top navigation bar is visible and includes the isched logo.
2. **Given** an authenticated user opens the navigation menu, **When** they select a destination, **Then** the selected section opens and the active location is visually clear.

---

### User Story 2 - Status Visibility and Context (Priority: P1)

As an authenticated WebUI user, I want a bottom status bar that shows my current user name and the latest operation digest so I always know who is signed in and what the system is currently doing.

**Why this priority**: Visibility of current state reduces confusion and support overhead during data-loading and workflow transitions.

**Independent Test**: Can be fully tested by triggering representative operations and verifying the status bar updates from in-progress to completion messages while always showing the current user name.

**Acceptance Scenarios**:

1. **Given** a user triggers an organization user fetch, **When** loading starts, **Then** the status digest shows "Loading organization users".
2. **Given** organization user data finishes loading, **When** the operation completes successfully, **Then** the status digest shows "Organization users loaded".
3. **Given** a user is authenticated, **When** the bottom status bar is displayed, **Then** it shows the current user name.

---

### User Story 3 - Confidence Through Automated UI Validation (Priority: P2)

As a delivery team member, I want thorough unit coverage and smoke end-to-end checks for the new shell elements so regressions are detected before release.

**Why this priority**: The shell is globally visible. Automated checks are needed to keep future changes from silently breaking navigation or status visibility.

**Independent Test**: Can be fully tested by executing the WebUI unit test suite and smoke end-to-end suite and verifying they validate shell rendering, digest updates, and current-user visibility.

**Acceptance Scenarios**:

1. **Given** the new shell behavior is implemented, **When** unit tests run, **Then** they verify top bar content, status bar content, and digest state transitions.
2. **Given** the deployed app is reachable in a test environment, **When** smoke end-to-end tests run, **Then** they verify top navigation, status bar presence, and operation digest updates for a representative flow.

### Edge Cases

- If the current user name is temporarily unavailable, the status bar shows a non-empty fallback identity label and updates automatically once identity data is available.
- If a tracked operation fails, the status digest changes to a failure-oriented message that is understandable to end users.
- If operation digests are emitted in rapid succession, the most recent digest is displayed without showing stale text.
- If the operation digest text is unusually long, the status bar remains readable without overlapping the user name.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The WebUI MUST display a top navigation bar on all authenticated application screens in scope: `/dashboard`, `/admin/organizations`, `/admin/users`, and `/admin/rbac`.
- **FR-002**: The top navigation bar MUST display the isched logo sourced from the WebUI assets directory.
- **FR-003**: The top navigation bar MUST include a navigation menu that allows users to move between defined primary application destinations.
- **FR-004**: The WebUI MUST display a bottom status bar on all authenticated application screens in scope: `/dashboard`, `/admin/organizations`, `/admin/users`, and `/admin/rbac`.
- **FR-005**: The bottom status bar MUST display a last-operation digest describing the most recent tracked user-visible operation.
- **FR-006**: The operation digest MUST support at least in-progress and success states using normalized user-facing wording, including "Loading organization users" and "Organization users loaded" for the representative flow.
- **FR-007**: The bottom status bar MUST display the current authenticated user name.
- **FR-008**: The shell styling for top navigation and bottom status bars MUST use DaisyUI/Tailwind classes, keep controls visually distinct, and avoid overlapping interactive content at viewport widths >= 320px.
- **FR-009**: The navigation and status shell MUST remain usable at viewport widths 320px, 768px, and 1280px without clipping the current-user label or digest text region.
- **FR-010**: The feature MUST include thorough unit tests that cover shell rendering, digest state transitions, and current-user display behavior.
- **FR-011**: The feature MUST include smoke end-to-end tests that validate the top navigation bar, bottom status bar, and representative digest transition behavior in a running environment.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`) where control flow is needed.
- **FCR-003**: Templates and styles for any new or refactored components MUST be in separate `.html` and `.scss` files using `templateUrl` and `styleUrl`.
- **FCR-004**: Browser API consumption for this feature MUST use GraphQL `/graphql` only (HTTP/WebSocket) with no REST fallback.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB).
- **FCR-006**: Local development behavior for this feature MUST preserve proxy routing for `/graphql` including WebSocket upgrades.
- **FCR-007**: UI styling MUST follow Tailwind CSS and DaisyUI conventions, preferring DaisyUI component classes for consistent shell presentation.

### Key Entities *(include if feature involves data)*

- **Navigation Item**: A top-bar menu entry that includes a display label, destination, and active-state indicator.
- **Operation Digest**: A user-facing status message representing the most recent tracked operation, including operation context and lifecycle state.
- **Session Identity Summary**: A lightweight representation of the current authenticated user used for status-bar display.

### Assumptions

- The feature applies to authenticated WebUI experiences rather than pre-authentication entry pages.
- Existing user-visible operations already emit or can emit operation-state events suitable for digest updates.
- Current user identity is already available in the WebUI session context and does not require a new authentication flow.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In acceptance testing, 100% of authenticated primary screens in scope show both the top navigation bar and bottom status bar.
- **SC-002**: In automated UI tests, the shell status bar shows a non-empty current-user label and latest-operation digest on every successful authenticated render of `/dashboard` and `/admin/users`.
- **SC-003**: For the representative organization-user loading flow, status text changes from in-progress to success wording in every observed successful run.
- **SC-004**: Before release, all new unit and smoke end-to-end tests for this feature pass in the CI validation run.
- **SC-005**: Local dev proxy validation confirms `/graphql` forwarding from Angular dev server to backend (HTTP and WebSocket routing configuration preserved).
