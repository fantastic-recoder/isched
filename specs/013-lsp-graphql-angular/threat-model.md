# Threat Model: LSP GraphQL Angular Integration

## Scope and Status

Planning threat model for the required GraphQL Playground and organization/user administration journeys, browser cookie authentication, local proxy, and browser-local GraphQL editor assistance. Controls below are design requirements, not evidence that implementation or tests have passed.

## Trust Boundaries

- **Browser UI**: user-entered GraphQL documents, administration form input, rendered schema descriptions, non-secret session indicators, and in-memory CSRF value.
- **Cookie boundary**: browser-managed Secure, HttpOnly, SameSite=Strict JWT cookie; Angular cannot inspect or copy it.
- **GraphQL backend**: only `/graphql` over HTTP and WebSocket; authenticates session, validates Origin/CSRF, authorizes tenant scope, and executes operations.
- **Browser-local language service**: parses/validates editor text and produces completion/hover/diagnostics from `/graphql` introspection; no separate server connection or LSP endpoint.
- **Local development**: Angular proxy forwards `/graphql` HTTP and WS; UI origin is explicitly allowlisted; no backend origin is hard-coded into client source.

## Threats and Planned Mitigations

### 1. JWT exposure or persistence

- **Threat**: bearer credentials are exposed to XSS, logs, storage, browser extensions, app memory, HTTP headers, URL, or WebSocket payload.
- **Mitigation**: Secure, HttpOnly, SameSite=Strict cookie only. Do not return JWT to Angular, retain in-memory bearer state, set Authorization, persist to localStorage/sessionStorage/IndexedDB, include in URLs, or put it in `connection_init`. Clear/invalidate cookie on logout, expiry, and revocation.
- **Residual risk**: same-origin script may still perform authenticated actions while a session is active; HttpOnly reduces direct token extraction but does not neutralize XSS.

### 2. CSRF against cookie-authenticated mutations

- **Threat**: browsers attach cookies automatically and an attacker causes cross-site state-changing GraphQL operations.
- **Mitigation**: require exact configured Origin validation for all GraphQL HTTP requests and WebSocket handshakes. Authenticated HTTP mutations require an unpredictable CSRF value bound server-side to the authenticated session and sent via `X-CSRF-Token`; validate before resolver side effects. Login and first-run `bootstrapPlatformAdmin` are exempt from the CSRF header only when no authenticated session exists, but remain Origin-checked and rate-limited. Both successful operations create the authenticated session, set its cookie through the HTTP response layer, and return a session-bound CSRF value through GraphQL without returning the JWT. An existing-session read-only `currentSessionCsrfToken` GraphQL query field reacquires it without a circular mutation dependency. Logout requires CSRF.
- **Residual risk**: same-origin compromise can read the CSRF response and issue same-origin mutations; maintain XSS defenses and do not treat CSRF as authorization.

### 3. Origin confusion and WebSocket hijacking

- **Threat**: wildcard, substring, missing-origin, or Referer-fallback checks allow a hostile site to use a cookie-authenticated GraphQL HTTP or WebSocket request.
- **Mitigation**: canonical exact allowlist match for every HTTP request and WS handshake; reject missing, `null`, malformed, and unlisted origins; no Referer bypass; no wildcard credentialed CORS. Authenticate WS using the cookie/session at `/graphql` handshake and carry no JWT in `connection_init`.
- **Residual risk**: deployment allowlist errors can block legitimate clients or expose a trusted origin; review configured values per deployment.

### 4. GraphQL-only boundary erosion

- **Threat**: Admin or editor code adds REST calls, direct backend URLs, or a separate language-server endpoint.
- **Mitigation**: all server-derived data uses `/graphql`; editor language support runs locally; retain static and network boundary checks and verify FR-001/FCR-004.
- **Residual risk**: a dependency may add an indirect external request; review network traffic and dependency behavior.

### 5. Malicious, incomplete, or stale schema/editor data

- **Threat**: malformed/partial introspection, untrusted descriptions, or stale schema state causes unsafe rendering or false confidence in validation.
- **Mitigation**: require complete introspection before enabling language features, safely render descriptions as text, show unavailable state on failure, and never fabricate missing documentation. Server-side GraphQL validation remains authoritative.
- **Residual risk**: an authorized schema administrator may supply misleading descriptions; editor help is advisory.

### 6. Cross-tenant or unauthorized administration

- **Threat**: user/admin action crosses organization boundaries or client state claims success after authorization/conflict failure.
- **Mitigation**: preserve server authorization and tenant scoping; use typed forms and explicit UI error outcomes; test organization and user management with both success and failure cases. Client state is not authorization.
- **Residual risk**: server policy or fixture gaps; implementation review and integration tests must validate scope.

### 7. Proxy or Secure-cookie misconfiguration

- **Threat**: missing WS upgrade, incorrect cookie scope, or dev-only security downgrade causes failures or insecure behavior.
- **Mitigation**: proxy `/graphql` for HTTP and WS; use relative client paths; explicitly allowlist the dev origin; preserve Secure cookie behavior and use supported local secure-context/TLS configuration rather than disabling Secure or adding bearer auth.
- **Residual risk**: browser support and deployment origin configuration differ; verify in supported browsers/environments.

### 8. Performance/load regression

- **Threat**: repeated parsing/introspection, session/CSRF checks, Admin writes, or concurrent editor activity degrades responsiveness or overloads the backend.
- **Mitigation**: reuse active schema snapshots; avoid repeated introspection where not required; separately test each journey with 10 concurrent users and each user's 10 unmeasured warmups plus 100 measured operations. Time submission through final rendered UI result; p95 is strict `<2,000 ms` over 1,000 observations per journey.
- **Residual risk**: benchmark hardware/workload differs from production; record environment and fixed operation manifest with results.

## Repository Baseline Gaps (Planning Evidence)

The inspected tree shows existing UI access-token state and Authorization/`connection_init` handling; current client CSRF generation is ephemeral rather than server-bound. Existing backend HTTP CSRF/Origin code is a draft that must be checked against the exact allowlist, all-request HTTP coverage, session-bound CSRF, and pre-session rules above. Existing WebSocket upgrade code must validate Origin and cookie session. The current editor invokes `graphql()` without schema; introspection types/query omit descriptions needed for hover. The repository also contains baseline unauthorized WebSocket close behavior (4401), IndexedDB-related no-persistence assertions, and non-blocking WS failure diagnostics; retain and retest these under the cookie/handshake design. The existing 4401 check is currently tied to bearer material in `connection_init` and must be retargeted to cookie-authenticated handshake/authorization behavior rather than preserving that payload. These are gaps or baseline controls to resolve in implementation, not changes made by this planning run.

## Closeout Requirements

- Verify secure cookie-only auth for HTTP and WS, no bearer/persistent JWT handling, exact Origin checks on HTTP/WS, and session-bound CSRF on authenticated HTTP mutations.
- Verify pre-session login/bootstrap creates the cookie session and CSRF value through GraphQL, and existing-session CSRF acquisition works through the read-only query without an additional endpoint.
- Verify GraphQL-only boundary, both equally required P1 journeys, all editor capabilities, and strict Angular conventions.
- Update and review this feature model and the corresponding project-wide summary in `docs/security-threat-model.md`.
- Record implementation/test evidence separately, and complete the blocking constitution-compliance review before closeout.
