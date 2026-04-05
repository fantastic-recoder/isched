# WebUI GraphQL Contract

**Feature**: `004-add-isched-webui`  
**Interface Type**: GraphQL-only (`/graphql` HTTP + WebSocket)

## Transport Contract

- Endpoint: `/graphql`
- HTTP: `POST` with GraphQL JSON payload
- WebSocket: `graphql-transport-ws` on `/graphql` (required in proxy/dev parity)
- No REST/alternate admin endpoints are allowed
- Server default data root (when `--data-dir` is omitted): `<DataHome>/isched` via `sago::getDataHome()`

## Security Contract

- JWT session credentials are transported only via secure HttpOnly SameSite cookie(s).
- WebUI must not read JWT values or persist them in `localStorage`, `sessionStorage`, IndexedDB, or logs.
- Every state-changing mutation must include CSRF double-submit token material and pass strict `Origin`/`Referer` checks.
- CSRF or origin failures return `CSRF_FAILED` with actionable retry/re-auth semantics.

## Shared Inputs and List Pagination Types

```graphql
input PageInput {
  number: Int!
  size: Int!
}

input SortInput {
  field: String!
  direction: SortDirection!
}

input FilterInput {
  field: String!
  op: String!
  value: String!
}

type PageInfo {
  number: Int!
  size: Int!
  totalElements: Int!
  totalPages: Int!
}
```

All organization/user/role/assignment list queries must accept `page`, `sort`, and `filter` arguments and return bounded pages.

## Required Query/Mutation Surface (WebUI-facing)

> Names are contract-level and map to concrete schema definitions during implementation.

### Bootstrap

```graphql
query BootstrapStatus {
  platformBootstrapStatus {
    isBootstrapAllowed
    bootstrapState
  }
}

mutation CompleteBootstrap($input: CompleteBootstrapInput!) {
  completePlatformBootstrap(input: $input) {
    success
    bootstrapState
    requiresRedirectToLogin
  }
}
```

### Auth Session

```graphql
query CurrentSession {
  currentSession {
    isAuthenticated
    principal {
      userId
      organizationId
      permissions
    }
    csrfTokenPresent
    expiresAt
  }
}

mutation SignIn($input: SignInInput!) {
  signIn(input: $input) {
    success
    user {
      id
      organizationId
      displayName
    }
  }
}

mutation SignOut {
  signOut {
    success
  }
}
```

### Organization Administration

```graphql
query Organizations($page: PageInput!, $sort: [SortInput!], $filter: [FilterInput!]) {
  organizations(page: $page, sort: $sort, filter: $filter) {
    nodes {
      id
      name
      status
      revision
      updatedAt
    }
    pageInfo {
      number
      size
      totalElements
      totalPages
    }
  }
}

mutation CreateOrganization($input: CreateOrganizationInput!) {
  createOrganization(input: $input) {
    id
    name
    status
    revision
  }
}

mutation UpdateOrganization($id: ID!, $input: UpdateOrganizationInput!, $expectedRevision: Int!) {
  updateOrganization(id: $id, input: $input, expectedRevision: $expectedRevision) {
    id
    name
    status
    revision
    updatedAt
  }
}
```

### User Administration (Organization-Scoped)

```graphql
query Users(
  $organizationId: ID!
  $page: PageInput!
  $sort: [SortInput!]
  $filter: [FilterInput!]
) {
  users(organizationId: $organizationId, page: $page, sort: $sort, filter: $filter) {
    nodes {
      id
      loginId
      displayName
      status
      revision
      roleAssignments {
        roleId
        effective
      }
    }
    pageInfo {
      number
      size
      totalElements
      totalPages
    }
  }
}

mutation CreateUser($organizationId: ID!, $input: CreateUserInput!) {
  createUser(organizationId: $organizationId, input: $input) {
    id
    organizationId
    loginId
    status
    revision
  }
}

mutation UpdateUser(
  $organizationId: ID!
  $id: ID!
  $input: UpdateUserInput!
  $expectedRevision: Int!
) {
  updateUser(
    organizationId: $organizationId
    id: $id
    input: $input
    expectedRevision: $expectedRevision
  ) {
    id
    displayName
    status
    revision
    updatedAt
  }
}
```

### RBAC (Built-in + Custom Roles + Assignments)

```graphql
query Roles(
  $organizationId: ID!
  $page: PageInput!
  $sort: [SortInput!]
  $filter: [FilterInput!]
) {
  roles(organizationId: $organizationId, page: $page, sort: $sort, filter: $filter) {
    nodes {
      id
      name
      kind
      permissions
      revision
    }
    pageInfo {
      number
      size
      totalElements
      totalPages
    }
  }
}

mutation CreateCustomRole($organizationId: ID!, $input: CreateRoleInput!) {
  createRole(organizationId: $organizationId, input: $input) {
    id
    name
    kind
    permissions
    revision
  }
}

mutation UpdateCustomRole(
  $organizationId: ID!
  $id: ID!
  $input: UpdateRoleInput!
  $expectedRevision: Int!
) {
  updateRole(
    organizationId: $organizationId
    id: $id
    input: $input
    expectedRevision: $expectedRevision
  ) {
    id
    name
    permissions
    revision
  }
}

mutation AssignRole($organizationId: ID!, $userId: ID!, $roleId: ID!) {
  assignRole(organizationId: $organizationId, userId: $userId, roleId: $roleId) {
    id
    effective
  }
}

mutation UnassignRole($organizationId: ID!, $userId: ID!, $roleId: ID!) {
  unassignRole(organizationId: $organizationId, userId: $userId, roleId: $roleId) {
    success
  }
}
```

## Error Contract

GraphQL errors must provide stable `extensions.code` values:

- `VALIDATION_FAILED`: mapable field-level validation details.
- `UNAUTHENTICATED`: missing/expired/revoked auth session.
- `FORBIDDEN`: caller lacks required permission/scope.
- `CSRF_FAILED`: failed CSRF token or origin validation.
- `CONTEXT_MISMATCH`: mutation org scope differs from selected context.
- `CONFLICT`: stale revision or uniqueness conflict (e.g., per-org loginId collision).
- `TRANSIENT_NETWORK`: retryable backend/network failure.

UI behavior obligations:

- `VALIDATION_FAILED` -> field-level messages for all invalid fields.
- Other listed codes -> deterministic global alerts with retry/re-auth guidance.
- `CONFLICT` -> explicit refresh/reconcile path for pending edits.

## Authorization and Scope Rules

- Platform admin: create organization.
- Platform admin and in-scope organization admin: edit organization profile.
- User and RBAC operations require explicit `organizationId` and strict scope checks.
- Deactivate/reactivate user retains stored assignments; assignment effectiveness follows user status.
- Lifecycle scope for this feature: create/edit/assign/deactivate/reactivate only.

## Audit and Reliability Expectations

- Every successful and failed admin mutation emits immutable audit events with actor, org scope, action, target, outcome, and timestamp.
- Audit events must remain queryable for at least 90 days.
- Admin operation paths are designed to support 99.5% monthly availability and documented RTO <= 60 minutes.

## Verification Expectations

- Contract tests assert GraphQL-only access and reject REST fallback assumptions.
- Integration tests assert CSRF rejection for missing/invalid token and origin mismatch.
- Concurrency tests assert stale revisions are rejected with `CONFLICT` and no partial writes.
- Scale tests assert server-side pagination/filter/sort behavior at 10,000-user / 1,000-role baseline.
- Playwright bootstrap integration runs against real backend started with temporary `--data-dir`.

