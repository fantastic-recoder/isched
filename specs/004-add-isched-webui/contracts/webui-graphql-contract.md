# WebUI GraphQL Contract

**Feature**: `004-add-isched-webui`  
**Interface Type**: GraphQL-only (`/graphql` HTTP + WebSocket)

## Transport Contract

- Endpoint: `/graphql`
- HTTP method: `POST` for query/mutation over JSON payload
- WebSocket: `graphql-transport-ws` on `/graphql` for subscription flows when needed
- No REST or alternate admin API is permitted

## Security Contract

- Authentication: JWT in secure HttpOnly SameSite cookie(s); WebUI never reads token value.
- Mutation CSRF requirements:
  - Double-submit CSRF token must be provided.
  - Strict `Origin`/`Referer` validation must pass.
  - If either check fails, mutation is rejected with actionable error response.
- Browser storage: JWT MUST NOT be written to `localStorage`, `sessionStorage`, or IndexedDB.

## Required Query/Mutation Surface (WebUI-facing)

> Names are contract-level and map to existing/extended backend schema during implementation.

### Bootstrap

```graphql
query BootstrapStatus {
  platformBootstrapStatus {
    isBootstrapAllowed
  }
}

mutation CompleteBootstrap($input: CompleteBootstrapInput!) {
  completePlatformBootstrap(input: $input) {
    success
    requiresRedirectToLogin
    bootstrapState
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
query Organizations($scope: OrganizationScopeInput) {
  organizations(scope: $scope) {
    id
    name
    status
    updatedAt
  }
}

mutation CreateOrganization($input: CreateOrganizationInput!) {
  createOrganization(input: $input) {
    id
    name
    status
  }
}

mutation UpdateOrganization($id: ID!, $input: UpdateOrganizationInput!) {
  updateOrganization(id: $id, input: $input) {
    id
    name
    status
    updatedAt
  }
}
```

### User Administration (Organization-Scoped)

```graphql
query Users($organizationId: ID!) {
  users(organizationId: $organizationId) {
    id
    loginId
    displayName
    status
    roleAssignments {
      roleId
      effective
    }
  }
}

mutation CreateUser($organizationId: ID!, $input: CreateUserInput!) {
  createUser(organizationId: $organizationId, input: $input) {
    id
    organizationId
    loginId
    status
  }
}

mutation UpdateUser($organizationId: ID!, $id: ID!, $input: UpdateUserInput!) {
  updateUser(organizationId: $organizationId, id: $id, input: $input) {
    id
    displayName
    status
    updatedAt
  }
}
```

### RBAC (Built-in + Custom Roles)

```graphql
query Roles($organizationId: ID!) {
  roles(organizationId: $organizationId) {
    id
    name
    kind
    permissions
  }
}

mutation CreateCustomRole($organizationId: ID!, $input: CreateRoleInput!) {
  createRole(organizationId: $organizationId, input: $input) {
    id
    name
    kind
    permissions
  }
}

mutation AssignRole($organizationId: ID!, $userId: ID!, $roleId: ID!) {
  assignRole(organizationId: $organizationId, userId: $userId, roleId: $roleId) {
    id
    effective
  }
}
```

## Error Contract

GraphQL errors must include stable `extensions.code` values usable by UI behavior:

- `UNAUTHENTICATED`: Session missing/expired/revoked; trigger re-auth flow.
- `FORBIDDEN`: Caller lacks scope/permission for operation.
- `VALIDATION_FAILED`: Input invalid; map field details to form controls.
- `CONFLICT`: Per-organization uniqueness violation (for example `loginId`).
- `CSRF_FAILED`: CSRF token and/or origin validation failed; require retry path.
- `CONTEXT_MISMATCH`: Organization context in request does not match active scope guard.
- `TRANSIENT_NETWORK`: Retryable connectivity/backend temporary failure.

## Scope Rules

- Platform admins may create organizations.
- Platform admins and organization admins may edit organization profile only within allowed scope.
- User and RBAC mutations require explicit `organizationId` and must be rejected if out of scope.
- Deactivate/reactivate user preserves role assignments; effective permissions follow user status.

## Verification Expectations

- Contract tests validate GraphQL-only access for all admin flows.
- Security tests validate mutation rejection on missing/invalid CSRF or origin mismatch.
- Authorization tests validate 100% block rate for out-of-scope organization/user/RBAC mutations.

