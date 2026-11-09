# Data Model: LSP GraphQL Angular Integration

This document describes conceptual state for the feature. `UiFlowState` and `GraphQLErrorPresentation` are internal frontend models only; they are not public API contracts. No new persisted business entity or database migration is planned.

## BrowserAuthCookie (server-managed credential)

- **Description**: JWT credential held by the browser cookie jar and server-validated on `/graphql` HTTP requests and WebSocket handshakes. Angular JavaScript cannot read or copy it.
- **Attributes**: cookie name/configuration; JWT session identifier (`jti`); expiry; `Secure=true`; `HttpOnly=true`; `SameSite=Strict`; path/scope appropriate to `/graphql`.
- **Validation**: reject invalid, expired, revoked or malformed values; cookie is the only browser authentication transport. Do not send it as Authorization or WebSocket protocol payload. Clear/expire it on logout and invalidation. Do not downgrade Secure for local development.

## AuthSessionContext

- **Description**: Authenticated server context associated with the validated cookie; UI-visible representation contains only non-secret identity/session indicators.
- **Fields**: `sessionId` (server-side existing session identifier); `userId`; `tenantId`; `roles`/permissions as needed by existing policy; `expiresAt`; server-held CSRF association.
- **Relationships**: one active session context maps to one `SessionBoundCsrfToken`; existing session/revocation support determines validity.
- **Validation**: authentication and tenant/role authorization are server-side. UI identity signals are not credentials and not authorization evidence. Logout/revocation invalidates the session and its CSRF association.
- **Persistence**: reuse existing session ID and revocation implementation. CSRF association lives in server process state; no SQLite schema change/migration.

## SessionBoundCsrfToken

- **Description**: Unpredictable, non-authentication value for CSRF defense, associated server-side with one authenticated session. It is not a JWT or bearer credential.
- **Fields**: `value` (opaque string); `sessionId` (binding); `createdAt`/rotation lifecycle as needed by implementation.
- **Lifecycle**: created when login or first-run `bootstrapPlatformAdmin` establishes the authenticated session; returned in the GraphQL `AuthPayload` for either operation. A read-only `currentSessionCsrfToken` GraphQL query field under the existing `/graphql` endpoint returns/reacquires it after reload. Angular retains it in memory only and sends it as `X-CSRF-Token` on authenticated HTTP mutations. The server validates exact session binding before mutation execution and invalidates the association on logout/revocation.
- **Pre-session handling**: Login and first-run bootstrap have no authenticated session to bind a token to before execution. They are exempt from the CSRF header only when no authenticated session is present; exact Origin validation and existing rate limits still apply. Repository behavior confirms bootstrap creates an authenticated session on success, so the response layer sets the cookie and the GraphQL result returns that session's CSRF value. The authenticated session-token query is read-only, needs no CSRF header, and is therefore a non-circular acquisition path. Authenticated logout and every other HTTP mutation require the token.
- **Storage**: never store in localStorage/sessionStorage/IndexedDB or URL. It is not the cookie value and cannot authenticate a request alone.
- **Synchronization**: all `csrf_sessions_` accesses use an exclusive RAII lock on `sessions_mutex_`, including reads that may erase expired entries. Lookup returns an owned string copy; comparison occurs outside the lock. Database operations and subscription callbacks are not invoked while holding it. Restart loses the association and requires reauthentication; reload in the same server process can reacquire it.

## AllowedOrigin

- **Description**: Canonical origin from the server-configured allowlist, used to guard GraphQL HTTP requests and WebSocket handshakes.
- **Fields**: scheme, host, port; canonical serialized origin.
- **Validation**: exact canonical match, not wildcard or substring. Missing, `null`, malformed, or unlisted Origin fails closed. Apply to login/bootstrap as well as authenticated requests; do not fall back to Referer. Include only explicitly configured UI origins, including the dev-server origin when enabled.

## GraphQLOperation

- **Description**: Client operation sent only to `/graphql` using HTTP or `graphql-transport-ws`.
- **Fields**: `operationName`; `operationType` (`query|mutation|subscription`); `document`; `variables`; result/error envelope; `submittedAt`/`completedAt` for transient measurement only.
- **Validation**: validate against the active GraphQL schema. For HTTP, parse/select the actual operation before applying mutation CSRF policy; do not classify mutations using a string-prefix heuristic. Authenticated mutations require the session-bound header; pre-session login/bootstrap rules are narrowly scoped. Subscriptions require an allowed-Origin handshake and cookie-authenticated session where protected.
- **External interface**: contract is GraphQL `/graphql` only; no REST/LSP endpoint is introduced.
- **Subscription policy**: no cookie plus an allowed Origin permits public anonymous subscriptions; protected subscriptions require valid cookie/session and role/tenant authorization before resolver or broker work. An invalid supplied cookie is never treated as absent.

## GraphQLSchemaSnapshot (internal frontend model)

- **Description**: Browser-memory GraphQL schema built from a complete active-schema introspection result fetched through `/graphql`, including descriptions required for documentation hover.
- **Fields**: `schema` (`GraphQLSchema`, runtime-only); `loadedAt`; `source` (fixed `/graphql`); `status` (`loading|ready|unavailable`).
- **Validation**: include all fields required by `buildClientSchema`, including type references and descriptions; only report schema-aware language assistance after successful build and editor update; no persistence or non-GraphQL schema fetch.

## GraphQLEditorLanguageState (internal frontend model)

- **Description**: Browser-local language-service state for the current editor document.
- **Fields**: `documentText`; `documentVersion`; `diagnostics` (syntax/type issues and source ranges); current `schemaStatus`; completion/hover result state.
- **Validation**: completion and type diagnostics derive from `GraphQLSchemaSnapshot`; hover shows only schema descriptions and never fabricates absent documentation. It implements required user-visible capabilities, not wire-level LSP JSON-RPC conformance. Schema failures produce an explicit unavailable state.

## ProxyRouteConfig

- **Description**: Existing Angular dev-server route for the GraphQL application boundary.
- **Fields**: `route` (literal `/graphql`); `targetOrigin` (environment configuration); `ws=true`; dev TLS behavior/configuration.
- **Validation**: forwards HTTP and WS upgrades; client uses relative same-origin paths; preserve Secure cookie behavior; no hard-coded origin in frontend source and no separate language-service path.

## UiFlowState (internal frontend model)

- **Description**: Signal-backed state for Playground/Admin operation UX; not a public API, persisted record, or external interface contract.
- **Fields**: `status` (`idle|loading|submitting|success|error|reauth-required`); typed form/editor value; safe error presentation; `canRetry`; rendered result.
- **State transitions**: `idle -> submitting -> success|error|reauth-required`; retryable failures may return to `submitting` without losing safe user input. Do not automatically replay an ambiguous non-idempotent mutation after a CSRF/transport failure.
- **Validation**: app-owned template state is exposed through signals or signal-derived view models; user input flows use typed reactive forms.

## GraphQLErrorPresentation (internal frontend model)

- **Description**: Display-safe mapping from GraphQL/transport failures to user guidance; not a public error contract.
- **Fields**: `category` (`validation|authorization|csrf|conflict|transport|unknown`); `message`; `retryable`; optional field-level messages.
- **Validation**: never expose cookie/token material. Retry classification must not label a rejected/ambiguous mutation as safe to replay. CSRF failure may reacquire the token through a read-only GraphQL bootstrap query, but a state-changing operation is not silently retried.

## PerformanceSample (test-only transient model)

- **Description**: One submission-to-final-render measurement for one operation in one journey. Not persisted as application data.
- **Fields**: journey (`playground|administration`); user index; operation class; monotonic `startedAt`; `endedAt`; `durationMs`; completion outcome.
- **Validation**: separate run for each journey; 10 concurrent users; 10 unmeasured warmups and then exactly 100 measured operations per user; exactly 1,000 denominator samples per journey. Nearest-rank p95 is rank 950 and must be `<2,000 ms`. Failed or timed-out operations fail the run and cannot be dropped. Start immediately before dispatch; end only after final expected UI result is visible, loading/submitting settles, and one render frame completes.
