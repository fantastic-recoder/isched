# Contract: Browser Authentication, Origin, and CSRF

## Credential Transport

- Browser authentication uses a server-managed JWT cookie with `Secure`, `HttpOnly`, and `SameSite=Strict` attributes for GraphQL `/graphql` HTTP and WebSocket traffic.
- The browser cookie jar attaches the credential. Angular MUST NOT read or retain the JWT, keep an in-memory bearer token, send an Authorization bearer header, put a token in a URL, or put a JWT in `connection_init`.
- FR-004's prohibition on JWT persistence in localStorage/sessionStorage/IndexedDB is separate from this cookie transport contract. Non-secret UI session indicators may be signal-backed and cleared on logout/invalidation.
- Logout, expiry, or revocation invalidates the server session and expires/clears the cookie using matching scope/attributes. Login/logout GraphQL resolvers and the HTTP response layer must propagate Set-Cookie/expiry metadata without exposing JWT in JSON. Secure MUST NOT be downgraded as a local-development workaround.

## Allowed-Origin Contract

- Validate `Origin` on **every** GraphQL HTTP request, including queries, mutations, login/bootstrap, and HTTP preflight as applicable, and on every WebSocket handshake before upgrade.
- Parse/canonicalize and exactly compare against explicit configured scheme/host/port origins. Missing, `null`, malformed, wildcard, or unlisted Origin fails closed. Do not use Referer as a fallback/bypass.
- The allowlist contains only approved UI origins (including the configured dev-server origin when enabled). Credentialed CORS, if enabled, echoes only the matching origin and never `*`.

## CSRF Acquisition and Mutation Validation

- For an authenticated browser session, the server associates an unpredictable CSRF value with that session identifier. The value is not a JWT and cannot authenticate by itself.
- Successful pre-session GraphQL `login` and first-run `bootstrapPlatformAdmin` mutations establish the cookie session, cause the HTTP response layer to set the cookie, and return the session-bound CSRF value—but never the JWT—in the GraphQL `AuthPayload`. Repository behavior confirms first-run bootstrap creates an authenticated session. Both flows remain subject to exact Origin validation and existing login/bootstrap abuse protections.
- On page reload, the read-only `currentSessionCsrfToken` GraphQL query field under the existing `/graphql` endpoint obtains or reissues the value using the cookie. This query does not require CSRF because it is read-only, avoiding circular acquisition. This field is a new GraphQL schema capability, not a separate endpoint.
- Every authenticated HTTP GraphQL mutation, including logout, MUST send the value in `X-CSRF-Token`. Server validates it against the authenticated session before mutation side effects. Missing, stale, invalid, or cross-session values fail closed.
- The only CSRF-header exception is a narrowly identified pre-session login/first-run-bootstrap operation when no authenticated session exists; if a valid session cookie is present, normal authenticated mutation CSRF validation applies. It does not bypass Origin validation or rate limiting. Do not exempt logout or any authenticated mutation.
- Angular retains the CSRF value in memory only, clears it on logout/revocation/reauthentication, and does not automatically replay an ambiguous state-changing operation after CSRF/transport failure.
- Implementation reuses existing session identity/revocation facilities; no SQLite migration is planned. The token/session association is process state and is invalidated on session close/revocation.
- `csrf_sessions_` is guarded by `AuthenticationMiddlewareImpl::sessions_mutex_`: insertion, expiry cleanup, lookup, and session/user/tenant invalidation all take an exclusive RAII lock. Lookup copies the value while locked; comparison uses that owned copy after unlocking. Database calls and broker callbacks run outside this lock. Revocation rejects subsequent validation; it does not roll back work already admitted before revocation. Server restart loses this process-local state and requires reauthentication; browser reload within the same process can reacquire the value.

## WebSocket Contract

- `/graphql` WS handshake validates exact allowed Origin and the cookie/session before a protected subscription is accepted.
- Browser-managed cookies are the only authentication credential. `connection_init` contains no JWT or Authorization payload; keep the existing `graphql-transport-ws` protocol.
- Reject an invalid origin/session without executing subscription work. Authentication failure may use the existing unauthorized close behavior after upgrade if required by the transport, but origin validation occurs at handshake.
- With an allowed Origin and no cookie, public subscriptions (for example `healthChanged`) remain anonymous. Protected subscriptions (for example `serverMetricsUpdated`) require cookie authentication and the existing role/tenant checks before resolver execution; a rejected operation never registers with the broker.
- A supplied invalid, empty, malformed, duplicate, expired, or revoked cookie closes with 4401, even for public operations; it does not downgrade to anonymous access. These rules apply to both same-port and dedicated listeners.

## Verification

- Backend/browser integration tests cover cookie attributes and automatic attachment, exact allowed-Origin cases (valid, missing, null, malformed, unlisted), login/bootstrap exception and rate limit, current-session token acquisition, authenticated mutation CSRF success/failure, logout/revocation, and WS cookie+Origin authentication.
- Tests assert no JWT in localStorage/sessionStorage/IndexedDB, Angular-managed memory, HTTP Authorization header, URL, or WS `connection_init`.
- Verify a rejected mutation has no resolver side effects, and UI errors do not render token/cookie material.
- `test_graphql_subscription_auth` verifies public delivery and broker registration/removal, protected rejection with unchanged active-subscription count, and valid-cookie protected success on both listener routes. Source review verifies that the executor's role check precedes resolver invocation and that an execution error returns before `WsSession` registers with the broker.
