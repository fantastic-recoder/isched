# Contract: GraphQL Operations

## Endpoint and Boundary

- Application API endpoint: `/graphql` only.
- HTTP: GraphQL queries and mutations. WebSocket: existing `graphql-transport-ws` subscriptions.
- The editor obtains schema/introspection/documentation through GraphQL. No REST endpoint or external LSP endpoint is introduced.
- **FR-001 is the canonical GraphQL-only boundary; FCR-004 references it. FR-005 is the canonical local proxy HTTP/WS requirement; FCR-006 references it.**

## Required Journeys

- **Playground (P1)**: schema browsing, schema-aware completion, hover documentation, live syntax/type diagnostics, editing/running a query and mutation, rendered result/error and retry. Subscription behavior, where included, requires cookie-authenticated WS and proxy support.
- **Administration (P1)**: required organization and user list/detail/manage operations through GraphQL, with server authorization and tenant scope preserved.
- Both journeys are equally required and independently tested; neither can be waived or made dependent on the other.

## Browser Session, Origin, and CSRF

- Browser authentication is by Secure, HttpOnly, SameSite=Strict cookie only; no Authorization header, JWT in-memory bearer, URL token, or WS `connection_init` bearer payload.
- Validate exact configured allowed Origin for every GraphQL HTTP request and WebSocket handshake. Missing, null, malformed, and unlisted origins are rejected; Referer is not a fallback. Credentialed CORS never uses wildcard origin.
- Authenticated HTTP mutations require `X-CSRF-Token` bound to the current session, validated before resolver side effects. Login and first-run `bootstrapPlatformAdmin` are explicit pre-session exceptions only when no authenticated session exists; they remain Origin-validated and rate-limited. Repository behavior confirms both successful operations create a session. Each returns the new session's CSRF value in its GraphQL `AuthPayload`; a read-only `currentSessionCsrfToken` GraphQL query field obtains/reacquires it after reload. Authenticated logout requires CSRF.
- The GraphQL `login` and `bootstrapPlatformAdmin` mutation HTTP responses set the Secure, HttpOnly, SameSite=Strict cookie through the server response layer; their JSON results MUST omit the JWT and include the session-bound CSRF value. The read-only `currentSessionCsrfToken` query field returns that CSRF value when authenticated by cookie. Logout expires the cookie through the HTTP response layer. These schema capabilities stay on `/graphql`.
- This acquisition/validation flow uses GraphQL operations and does not add an endpoint or require a schema migration.
- FR-004's no-persistent-JWT-storage rule (localStorage/sessionStorage/IndexedDB) is separate from the FCR-005 cookie transport rule.

## Error Contract

- Frontend maps GraphQL/transport failures to internal UI categories such as `validation`, `authorization`, `csrf`, `conflict`, `transport`, and `unknown`. These frontend models are not a new external error schema.
- Show actionable feedback and preserve safe entered data for retry. Do not silently replay a non-idempotent mutation after ambiguous failure or CSRF rejection.

## Performance Contract

- Measure each required journey independently with 10 concurrent users. Each user performs 10 unmeasured warmups followed by 100 measured operations; denominator is exactly 1,000 per journey.
- Time submit dispatch through final expected UI result visible after loading/submitting settles and an Angular render frame. Failed/time-out operations fail the benchmark and are not omitted.
- Nearest-rank p95 is rank 950 and must be strictly `< 2,000 ms` for each journey. See `quickstart.md` for the deterministic workload/timing protocol.
