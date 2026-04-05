# GraphQL Contract: Auth + Bootstrap Consistency

**Feature**: `005-rate-limited-auth-bootstrap`  
**Transport**: GraphQL over `/graphql` (HTTP + WebSocket where applicable)

## Contract Goals

- Keep auth/bootstrap behavior GraphQL-only.
- Ensure deterministic lockout signaling through stable GraphQL error codes.
- Define startup/guard/bootstrap checks needed for route consistency.

## Operations in Scope

### 1) Query bootstrap eligibility

```graphql
query BootstrapStatus {
  systemState {
    seedModeActive
  }
}
```

**Behavioral Contract**:
- `seedModeActive: true` means bootstrap route has precedence at startup.
- `seedModeActive: false` means bootstrap route is unavailable and sign-in flow applies.

### 2) Authenticate user

```graphql
mutation Login($email: String!, $password: String!) {
  login(email: $email, password: $password) {
    token
  }
}
```

**Behavioral Contract**:
- Success yields session-capable auth response.
- Lockout failures MUST use GraphQL error `extensions.code = RATE_LIMITED`.
- `extensions.retryAfterMs` MAY be included; UI MUST tolerate its absence.

### 3) Bootstrap initial admin

```graphql
mutation BootstrapPlatformAdmin($input: BootstrapPlatformAdminInput!) {
  bootstrapPlatformAdmin(input: $input) {
    token
    expiresAt
  }
}
```

**Behavioral Contract**:
- Allowed only while bootstrap is active.
- If bootstrap becomes unavailable, mutation errors MUST map to a deterministic bootstrap-unavailable UI path (redirect to sign-in + notice).
- Duplicate submissions while request is in flight are suppressed in UI (single-flight).

### 4) Validate session for guarded routes

```graphql
query CurrentUser {
  currentUser {
    id
  }
}
```

**Behavioral Contract**:
- Guard flow performs this revalidation once at first guarded navigation after initialization.
- Null/unauthenticated result redirects to sign-in.

### 5) Sign out

```graphql
mutation Logout {
  logout
}
```

**Behavioral Contract**:
- Successful sign-out clears in-memory frontend auth/session indicators.
- Subsequent guarded navigation behaves as unauthenticated until a new successful sign-in.

## Error Envelope Contract

Lockout and auth/bootstrap transitions depend on stable GraphQL error metadata:

```json
{
  "errors": [
    {
      "message": "Too many authentication attempts",
      "extensions": {
        "code": "RATE_LIMITED",
        "retryAfterMs": 15000
      }
    }
  ]
}
```

Fallback contract when timing metadata is absent:

```json
{
  "errors": [
    {
      "message": "Too many authentication attempts",
      "extensions": {
        "code": "RATE_LIMITED"
      }
    }
  ]
}
```

## Compatibility Rules

- No REST fallback endpoints are introduced.
- Existing GraphQL operation names remain stable unless a migration note is added in `tasks.md`.
- Unknown error codes continue to map to generic transient handling; `RATE_LIMITED` remains explicit and deterministic.

