# Threat Model: GraphQL Playground

## Scope

Authenticated `/playground` UI for schema browsing, query generation, query execution, and panel layout persistence.

## Trust Boundaries

- **Browser UI boundary**: user-entered GraphQL text, tree selection state, and persisted layout preferences are all client-controlled.
- **GraphQL transport boundary**: all introspection and query execution goes through `/graphql`.
- **Authentication boundary**: the route is accessible only after authentication; unauthorized navigation must redirect to `/login`.
- **Schema document boundary**: uploaded schema documents are merged into the tree but remain read-only in this feature.

## Threats and Mitigations

### 1. Unauthorized schema visibility
- **Threat**: unauthenticated users could see schema details or uploaded schema documents.
- **Mitigation**: guard `/playground` behind existing auth checks and reuse authenticated shell routing patterns.
- **Residual risk**: authenticated users still see schema metadata that is already exposed by introspection.

### 2. Unsafe rendering of schema or query content
- **Threat**: malicious schema names, descriptions, or query text could trigger HTML injection if rendered unsafely.
- **Mitigation**: render all schema/query/result data as text, not HTML; keep CodeMirror content isolated; avoid `[innerHTML]`.
- **Residual risk**: syntax highlighting still processes user text, but within the editor sandbox.

### 3. Sensitive data leakage through result output
- **Threat**: query results or GraphQL errors could expose information to the user or make debugging output hard to distinguish from valid data.
- **Mitigation**: use explicit success/error/advisory states in the result panel and preserve existing auth boundaries on the backend.
- **Residual risk**: backend-returned data is visible to the authenticated caller by design.

### 4. Layout preference persistence misuse
- **Threat**: persistent panel size storage could be confused with credential storage or be overused for broader state.
- **Mitigation**: store only non-sensitive layout values; keep JWTs and session data out of browser-persistent storage.
- **Residual risk**: browser storage may retain harmless UI preferences across sessions.

### 5. Subscription execution confusion
- **Threat**: users may expect subscriptions to execute when the feature only generates stubs.
- **Mitigation**: show a clear advisory message in the result panel when Run is used for a subscription stub.
- **Residual risk**: no subscription transport path is added in this iteration.

## Security Requirements

- Do not add REST fallbacks or alternate transports.
- Do not store access tokens in persistent browser storage.
- Do not render untrusted schema content as HTML.
- Preserve authenticated-only access to the route.
- Keep layout persistence limited to non-sensitive UI preferences.

## Verification Notes

- Unit tests should cover route access behavior, advisory handling, and safe state transitions.
- E2E tests should verify unauthenticated redirect behavior and that the playground remains functional only after authentication.

