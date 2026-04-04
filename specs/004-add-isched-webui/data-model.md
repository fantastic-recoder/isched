# Data Model: Isched WebUI

**Phase**: 1 (Design & Contracts)  
**Created**: 2026-04-04  
**Feature**: `004-add-isched-webui`

## Entity: PlatformBootstrap

**Purpose**: Represents one-time platform initialization capability and bootstrap submission outcome.

**Fields**:
- `isBootstrapAllowed: Boolean` — true only before initial setup.
- `initialAdminLoginId: String` — required bootstrap input (organization/platform policy-defined format).
- `initialAdminDisplayName: String` — required bootstrap input.
- `submittedAt: DateTime` — audit timestamp.
- `completedBySessionId: ID` — resulting authenticated session id (opaque to UI token value).

**Validation Rules**:
- Allowed exactly once globally.
- Required fields must pass typed-form validation before submit.
- Any second attempt returns blocked/redirect behavior.

**State Transitions**:
- `PendingInitialization -> Initializing -> Initialized`
- `Initialized` is terminal for bootstrap flow.

## Entity: Organization

**Purpose**: Tenant boundary and administrative scope root.

**Fields**:
- `id: ID`
- `name: String`
- `slug: String` (or equivalent unique org identifier)
- `status: Enum(Active, Suspended)`
- `profile: OrganizationProfile` (domain/contact/metadata as supported)
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `Organization 1..* User`
- `Organization 1..* Role (custom)`
- `Organization 1..* RoleAssignment`

**Validation Rules**:
- Creation restricted to platform admin scope.
- Edit restricted to platform admin or same-organization admin.
- Organization context must be explicit for downstream admin operations.

## Entity: User

**Purpose**: Identity record scoped to one organization.

**Fields**:
- `id: ID`
- `organizationId: ID`
- `loginId: String`
- `displayName: String`
- `email: String?` (optional depending on backend policy)
- `status: Enum(Active, Disabled)`
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `User *..* Role` through `RoleAssignment`
- `User 1..* AuthSession`

**Validation Rules**:
- `loginId` unique within `organizationId`.
- Same `loginId` may exist in other organizations.
- User CRUD restricted to selected org context and caller scope.

**State Transitions**:
- `Active <-> Disabled`
- Transition to `Disabled` retains assignments but suppresses assignment effectiveness.

## Entity: Role

**Purpose**: RBAC role definition used for permission assignment.

**Fields**:
- `id: ID`
- `organizationId: ID?` (`null`/platform scope for built-in global roles if applicable)
- `name: String`
- `kind: Enum(BuiltIn, Custom)`
- `permissions: String[]`
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `Role 1..* RoleAssignment`

**Validation Rules**:
- Built-in roles are assignable but structurally immutable.
- Custom roles editable only within authorized scope.
- Permission set must be recognized by backend authorization model.

## Entity: RoleAssignment

**Purpose**: Assignment mapping between a user and a role in organization scope.

**Fields**:
- `id: ID`
- `organizationId: ID`
- `userId: ID`
- `roleId: ID`
- `effective: Boolean` (derived from user status + assignment active flag)
- `assignedAt: DateTime`
- `assignedBy: ID`

**Validation Rules**:
- Assignment organization must match both user and role scope constraints.
- Duplicate role assignments for same user/role/org are rejected.
- Unauthorized assignment mutations are denied with clear error.

**State Transitions**:
- `Assigned -> InactiveByUserDisable -> Assigned` (automatic reactivation on user enable)
- `Assigned -> Removed` (explicit unassign)

## Entity: AuthSession

**Purpose**: Cookie-authenticated UI session state for authorization and expiry handling.

**Fields**:
- `sessionId: ID`
- `subjectUserId: ID`
- `organizationScope: ID?`
- `status: Enum(Authenticated, Expired, Revoked, Anonymous)`
- `csrfToken: String` (double-submit partner token, non-JWT)
- `expiresAt: DateTime`

**Validation Rules**:
- JWT value remains opaque to WebUI JavaScript.
- CSRF token must accompany state-changing mutations and pass strict Origin/Referer checks.
- Expired/revoked sessions trigger re-auth flow and safe form-state handling.

## Entity: UiOrganizationContext

**Purpose**: Current explicit organization scope selected by the administrator in WebUI.

**Fields**:
- `selectedOrganizationId: ID`
- `selectedAt: DateTime`
- `dirtyFormGuardActive: Boolean`

**Validation Rules**:
- Required for org-scoped create/edit/assign mutations.
- Context switch with unsaved edits requires explicit confirm/discard path.
- Mutation payload org id must match currently selected context.

## Relationship Summary

- `Organization` owns `User`, custom `Role`, `RoleAssignment`.
- `RoleAssignment` binds `User` and `Role` within one organization context.
- `AuthSession` controls authenticated access; `UiOrganizationContext` controls scoped administration behavior.

## Derived Invariants

- No cross-organization write can occur without explicit organization context match.
- Disabled users retain role assignments but have no effective permissions until re-enabled.
- Bootstrap operations are unavailable once initialization is complete.
- All mutation operations must satisfy both authorization and CSRF constraints.

