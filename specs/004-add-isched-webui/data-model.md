# Data Model: Isched WebUI

**Phase**: 1 (Design & Contracts)  
**Updated**: 2026-04-05  
**Feature**: `004-add-isched-webui`

## Entity: PlatformBootstrap

**Purpose**: Captures one-time platform initialization status and completion metadata.

**Fields**:
- `isBootstrapAllowed: Boolean`
- `bootstrapState: Enum(PendingInitialization, Initialized)`
- `initialAdminLoginId: String`
- `initialAdminDisplayName: String`
- `completedAt: DateTime?`
- `completedByActorId: ID?`

**Validation Rules**:
- Allowed exactly once platform-wide.
- Route available unauthenticated only while `isBootstrapAllowed = true`.
- Repeated attempts are denied and routed to normal authentication.

## Entity: Organization

**Purpose**: Tenant boundary and top-level scope for admin operations.

**Fields**:
- `id: ID`
- `name: String`
- `slug: String`
- `status: Enum(Active, Suspended)`
- `profile: OrganizationProfile`
- `revision: Int` (optimistic concurrency)
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `Organization 1..* User`
- `Organization 1..* Role` (custom roles)
- `Organization 1..* RoleAssignment`

**Validation Rules**:
- Create allowed to platform admins only.
- Edit allowed to platform admins and same-org admins only.
- Updates require `revision` match; stale writes fail with `CONFLICT`.

## Entity: User

**Purpose**: Organization-scoped identity and lifecycle state.

**Fields**:
- `id: ID`
- `organizationId: ID`
- `loginId: String`
- `displayName: String`
- `email: String?`
- `status: Enum(Active, Disabled)`
- `revision: Int` (optimistic concurrency)
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `User *..* Role` via `RoleAssignment`
- `User 1..* AuthSession`

**Validation Rules**:
- `loginId` unique per `organizationId`.
- Same `loginId` may exist in different organizations.
- Create/edit limited to selected organization context and caller scope.
- Updates require `revision` match; stale writes fail with `CONFLICT`.

**State Transitions**:
- `Active <-> Disabled`
- Disabling preserves assignments but marks them ineffective.

## Entity: Role

**Purpose**: RBAC role definition (built-in or custom) used for permission grants.

**Fields**:
- `id: ID`
- `organizationId: ID?` (nullable for platform built-in scope)
- `name: String`
- `kind: Enum(BuiltIn, Custom)`
- `permissions: String[]`
- `revision: Int` (custom roles only; optimistic concurrency)
- `createdAt: DateTime`
- `updatedAt: DateTime`

**Relationships**:
- `Role 1..* RoleAssignment`

**Validation Rules**:
- Built-in roles are assignable but structurally immutable.
- Custom roles editable in authorized scope only.
- Custom role updates require `revision` match; stale writes fail with `CONFLICT`.

## Entity: RoleAssignment

**Purpose**: Assignment linking a user to a role inside one organization.

**Fields**:
- `id: ID`
- `organizationId: ID`
- `userId: ID`
- `roleId: ID`
- `storedActive: Boolean`
- `effective: Boolean` (derived: `storedActive && user.status == Active`)
- `assignedAt: DateTime`
- `assignedByActorId: ID`

**Validation Rules**:
- `organizationId` must match role/user organization scope.
- Duplicate user-role assignment in same org is rejected.
- Assignment/unassignment restricted by RBAC permissions.

## Entity: AuthSession

**Purpose**: Session state for authenticated WebUI interactions.

**Fields**:
- `sessionId: ID`
- `subjectUserId: ID`
- `activeOrganizationId: ID?`
- `status: Enum(Authenticated, Expired, Revoked, Anonymous)`
- `csrfToken: String`
- `expiresAt: DateTime`

**Validation Rules**:
- JWT remains opaque to WebUI code (cookie transport only).
- Mutation requests must pass CSRF + Origin/Referer checks.
- Expired/revoked sessions map to `UNAUTHENTICATED` and re-auth flow.

## Entity: UiOrganizationContext

**Purpose**: Current explicit organization selected in UI scope controls.

**Fields**:
- `selectedOrganizationId: ID`
- `selectedAt: DateTime`
- `isDirtyEditState: Boolean`

**Validation Rules**:
- Required for org-scoped user/role/assignment operations.
- Context switch with dirty forms requires confirm/discard action.
- Mutation payload org scope must match selected context or fail `CONTEXT_MISMATCH`.

## Entity: AuditEvent

**Purpose**: Immutable evidence trail for admin mutation attempts/outcomes.

**Fields**:
- `id: ID`
- `actorId: ID?`
- `organizationScope: ID?`
- `action: String` (bootstrap/org/user/role/assignment mutation verb)
- `targetType: String`
- `targetId: ID?`
- `outcome: Enum(Success, Failure)`
- `failureCode: String?`
- `timestamp: DateTime`

**Validation Rules**:
- Emitted for successful and failed admin mutations.
- Immutable once written.
- Retention minimum: 90 days.

## Query Models (Server-Side List Operations)

- `OrganizationListQuery(page, pageSize, sort, filter)`
- `UserListQuery(organizationId, page, pageSize, sort, filter)`
- `RoleListQuery(organizationId, page, pageSize, sort, filter)`
- `RoleAssignmentListQuery(organizationId, userId?, page, pageSize, sort, filter)`

All list models require bounded page size and explicit sort/filter args for scale baseline compliance.

## Lifecycle Scope Boundaries

- In scope: create, edit, assign/unassign, deactivate/reactivate.
- Out of scope: hard delete, archive/restore.

## Derived Invariants

- No cross-organization writes occur without explicit matching organization context.
- Stale edit revisions are rejected atomically with `CONFLICT`.
- Disabled users keep stored assignments but have no effective permissions.
- Admin list screens must never rely on full-dataset client loading.
- All admin mutations produce immutable audit events.

