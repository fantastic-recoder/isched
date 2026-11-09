# Threat Model: LSP GraphQL Angular Integration

## Scope and Review Status

Reviewed against the implemented GraphQL Playground and organization/user administration journeys, cookie-backed session behavior, local proxy, and browser-local GraphQL editor assistance. The controls marked **Verified** below are backed by the named automated checks run on 2026-11-09; this is regression evidence, not a claim of a comprehensive penetration test. Residual and operational risks are listed separately. No database migration or alternate API was added.

## Trust Boundaries

- **Browser UI**: user-entered GraphQL documents, administration form input, rendered schema descriptions/results, non-secret session indicators, and in-memory CSRF value.
- **Cookie boundary**: browser-managed Secure, HttpOnly, SameSite=Strict session JWT cookie; Angular cannot inspect or copy it.
- **GraphQL backend**: only `/graphql` over HTTP and WebSocket; authenticates session, validates Origin/CSRF, authorizes tenant scope, and executes operations.
- **Browser-local language service**: parses/validates editor text and produces completion/hover/diagnostics from `/graphql` introspection; no separate server connection or LSP endpoint.
- **Local development**: Angular proxy forwards `/graphql` HTTP and WS; UI origin is explicitly allowlisted; no backend origin is hard-coded into client source.

## Threats, Verified Controls, and Residual Risks

### 1. JWT exposure or persistence

- **Threat**: bearer credentials are exposed to XSS, logs, storage, browser extensions, app memory, HTTP headers, URL, or WebSocket payload.
- **Verified controls**: Secure, HttpOnly, SameSite=Strict cookie only; successful login/bootstrap response data excludes JWT; the browser security suite checks no Authorization header, no URL token, inaccessible cookie via `document.cookie`, and no JWT-like or token-named localStorage/sessionStorage/IndexedDB entries. Static/frontend tests prohibit bearer-token state; WS tests assert credential-free `connection_init`.
- **Evidence**: `test_graphql_http_security`, `test_webui_jwt_leakage`, frontend auth/storage tests, `e2e/auth-session-security.spec.ts`, `e2e/dev-proxy.spec.ts`, and `graphql-boundary.static.spec.ts` passed.
- **Residual risk**: same-origin script may still perform authenticated actions while a session is active; HttpOnly reduces direct token extraction but does not neutralize XSS.

### 2. CSRF against cookie-authenticated mutations

- **Threat**: browsers attach cookies automatically and an attacker causes cross-site state-changing GraphQL operations.
- **Verified controls**: exact Origin validation applies to GraphQL HTTP requests and WebSocket handshakes. Authenticated HTTP mutations require an unpredictable CSRF value bound server-side to the authenticated session and sent via `X-CSRF-Token`; missing/invalid values are rejected before resolver side effects. Login and first-run `bootstrapPlatformAdmin` are exempt from the CSRF header only when no authenticated session exists, remain Origin-checked and rate-limited, and return a session-bound CSRF value through GraphQL while the response layer sets the cookie without returning JWT. An authenticated read-only `currentSessionCsrfToken` query supports reacquisition; logout requires CSRF.
- **Evidence**: `test_graphql_http_security`, `test_session_revocation`, and `e2e/auth-session-security.spec.ts` passed. The browser test observes that a missing-CSRF organization mutation creates no organization, then verifies a CSRF-protected organization mutation succeeds.
- **Residual risk**: same-origin compromise can read the CSRF response and issue same-origin mutations; maintain XSS defenses and do not treat CSRF as authorization.

### 3. Origin confusion and WebSocket hijacking

- **Threat**: wildcard, substring, missing-Origin, or Referer-fallback checks allow a hostile site to use a cookie-authenticated GraphQL HTTP or WebSocket request.
- **Verified controls**: canonical exact allowlist match for GraphQL HTTP and WS; missing, `null`, malformed, and unlisted origins fail closed; Referer is not a bypass; credentialed CORS does not use wildcard origins. WS validation uses the browser cookie/session before protected subscription work and `connection_init` carries no credentials.
- **Evidence**: `test_graphql_http_security`, `test_graphql_subscription_auth`, `test_graphql_subscriptions`, and the browser security/proxy suites passed.
- **Residual risk**: deployment allowlist errors can block legitimate clients or expose a trusted origin; review configured values per deployment.

### 4. GraphQL-only boundary erosion

- **Threat**: Admin or editor code adds REST calls, direct backend URLs, or a separate language-server endpoint.
- **Verified controls**: all server-derived UI data and schema assistance use `/graphql`; editor language support runs locally; source-level boundary tests prohibit raw `fetch`/XHR, non-GraphQL `HttpClient` routes, hard-coded backend hosts, and bearer WS payloads. Browser acceptance records only `/graphql` application HTTP/WS traffic for Admin and Playground; the proxy forwards HTTP and WS `/graphql`.
- **Evidence**: `test_graphql_only_boundary`, `test_graphql_schema_upload`, `graphql-boundary.static.spec.ts`, `e2e/admin.spec.ts`, `e2e/playground.spec.ts`, and `e2e/dev-proxy.spec.ts` passed.
- **Residual risk**: dependencies may add indirect external requests; review dependency changes and browser network traffic.

### 5. Malicious, incomplete, or stale schema/editor data

- **Threat**: malformed/partial introspection, untrusted descriptions, or stale schema state causes unsafe rendering or false confidence in validation.
- **Verified controls**: schema comes from GraphQL introspection; browser-local completion, hover, and syntax/type diagnostics are active only after schema construction; failure state is explicit; descriptions are rendered as text and absent descriptions are not invented. Server-side GraphQL validation remains authoritative.
- **Evidence**: introspection/editor unit tests and the Playground browser acceptance covering schema tree, completion, hover description, diagnostics, query/mutation execution, and failure retention passed.
- **Residual risk**: an authorized schema administrator may supply misleading descriptions; editor help is advisory and schema refresh may lag an active schema change.

### 6. Cross-tenant or unauthorized administration

- **Threat**: user/admin action crosses organization boundaries or client state claims success after authorization/conflict failure.
- **Verified controls**: server authorization and tenant scoping remain authoritative; Admin forms use typed reactive controls, expose error outcomes, and show final server-visible status. Organization/user service and component suites cover success and validation/authorization/conflict/transport branches; browser acceptance performs organization and user create/update/status operations.
- **Evidence**: `isched_gql_executor_orgs_resolvers_tests`, `isched_gql_executor_users_resolvers_tests`, `test_user_management`, full Angular tests, and `e2e/admin.spec.ts` passed.
- **Residual risk**: this browser acceptance verifies the main authorized journey; deployment-specific RBAC policy and operational account assignment still require review.

### 7. Proxy or Secure-cookie misconfiguration

- **Threat**: missing WS upgrade, incorrect cookie scope, or dev-only security downgrade causes failures or insecure behavior.
- **Verified controls**: proxy client path is relative `/graphql`, proxy enables WS upgrades, and exact local dev origins are allowlisted. Chromium browser acceptance on `http://localhost:4200` verifies Secure, HttpOnly, SameSite=Strict cookie behavior through the local proxy; the proxy's `secure: false` is upstream TLS certificate verification only and does not change cookie policy. No Authorization workaround is used.
- **Evidence**: `src/ui/e2e/dev-proxy.spec.ts` passed both HTTP and authenticated WS upgrade/ack tests; config and Origin allowlist were reviewed.
- **Residual risk**: local secure-context handling differs across browsers and numeric loopback/deployment origins; use the documented `localhost` route locally or HTTPS plus an exact allowlist entry elsewhere.

### 8. Performance/load regression

- **Threat**: repeated parsing/introspection, session/CSRF checks, Admin writes, or concurrent editor activity degrades responsiveness or overloads the backend.
- **Verified controls**: each journey independently ran 10 concurrent browser users, 10 unmeasured warmups/user, and 100 measured UI operations/user; each retained 1,000 successful samples and computed nearest-rank p95 under the strict 2,000 ms threshold. The Admin run initially exposed SQLite writer contention; pooled database connections now configure a 5-second SQLite busy timeout and the repeated Admin run had no warmup or measured failures.
- **Evidence**: final Playground aggregate p95 was **450.39 ms** (900 query, 100 mutation); final Admin aggregate p95 was **629.33 ms** (250 organization list, 250 user list, 500 status updates). Per-class metrics and all samples are attached to the respective Playwright runs; full counts and commands are in `quickstart.md`.
- **Residual risk**: benchmark hardware/workload differs from production; record environment and fixed operation manifest with future runs.

## Review Conclusion and Remaining Operational Risks

The feature threat controls are implemented and covered by the backend, Angular, browser security, proxy, and per-journey performance gates listed above. The review found no unhandled feature-specific threat requiring a code change for this closeout. This is not a general penetration test. Preserve these residual risks in deployment review: same-origin XSS, configuration mistakes in exact Origin allowlists, browser-specific Secure-cookie handling, potentially misleading authorized schema descriptions, RBAC assignment/policy drift, and performance differences between benchmark and production environments. The required implementation evidence and constitution review are recorded in `quickstart.md`.
