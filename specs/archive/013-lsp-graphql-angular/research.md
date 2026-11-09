# Research: LSP GraphQL Angular Integration

## Decision 1 — Preserve the GraphQL-only application boundary

- **Decision**: The only browser-facing application API remains `/graphql`, using HTTP for queries/mutations and the existing `graphql-transport-ws` WebSocket path for subscriptions. Editor language features run in the browser. No separate LSP/JSON-RPC endpoint or REST route is added.
- **Rationale**: FR-001 is the canonical GraphQL-only requirement and FCR-004 references it. The repository already has `GraphQLService` with relative `/graphql`, GraphQL introspection, an existing subscription client, and a dev proxy.
- **Alternatives considered**: REST endpoints for Admin or editor metadata (rejected: violates FR-001); an external GraphQL language server endpoint (rejected: breaks the GraphQL-only application boundary); wire-level LSP JSON-RPC in a browser worker (not required and adds architecture without user-visible benefit).
- **Repository evidence**: `src/ui/src/app/services/graphql.service.ts`, `graphql-subscription.service.ts`, `playground-introspection.service.ts`, `src/ui/proxy.conf.json`, and the existing `/graphql` server handler in `src/main/cpp/isched/backend/isched_Server.cpp`.

## Decision 2 — Schema-aware editor capabilities, not LSP wire conformance

- **Decision**: Provide schema-aware completion, hover documentation, and live syntax/type diagnostics in the Playground editor. Obtain a full introspection result including descriptions through `/graphql`, build an active GraphQL schema locally, and supply/update it in CodeMirror. Show an explicit unavailable state if introspection/schema construction fails. Do not claim schema-backed assistance before the schema is ready.
- **Rationale**: `cm6-graphql` 0.2.1 is installed and its local README documents schema-backed autocomplete/lint plus `updateSchema`. The current `query-editor.ts` calls `graphql()` without a schema. `playground-introspection.service.ts` already fetches introspection but its current query/model omit descriptions and the complete schema shape needed by `buildClientSchema` and hover documentation.
- **Dependency**: `src/ui/package.json` has no direct `graphql` dependency although `cm6-graphql` declares GraphQL.js as a peer dependency and the local version supports it. Declare a compatible GraphQL.js runtime dependency directly; if directly importing GraphQL Language Service hover APIs, declare that dependency directly as well rather than depending on a transitive package. Update the UI lockfile during implementation.
- **Alternatives considered**: syntax highlighting only (rejected: fails required capabilities); SDL via a separate route (rejected: violates FR-001); remote LSP endpoint or new editor framework (rejected: unnecessary API/architecture migration).
- **Repository evidence**: `src/ui/node_modules/cm6-graphql/README.md` and `package.json`; `src/ui/src/app/components/playground/query-editor/query-editor.ts`; `src/ui/src/app/services/playground-introspection.service.ts` and `playground-introspection.models.ts`.

## Decision 3 — Cookie-only browser authentication

- **Decision**: Browser credentials use a server-managed JWT cookie with `Secure`, `HttpOnly`, and `SameSite=Strict`. JavaScript receives no JWT, retains no in-memory bearer token, sends no Authorization header, and sends no JWT in WebSocket protocol payloads. There is no bearer-token alternative. FR-004 remains the separate prohibition on JWT persistence in `localStorage`, `sessionStorage`, and IndexedDB; FCR-005 governs the cookie transport and other browser credential controls.
- **Rationale**: User decision, security-first constitution, and existing GraphQL-only relative route support automatic browser cookie attachment. Current `AuthService` stores `accessToken`; the auth interceptor adds Authorization; the subscription service sends token material in `connection_init`. These are gaps to replace, not target patterns.
- **Alternatives considered**: persistent browser storage (prohibited); in-memory bearer token or WebSocket `connection_init` bearer payload (prohibited); REST login/session endpoint (rejected: violates FR-001).
- **Repository evidence**: `src/ui/src/app/services/auth.service.ts`, `src/ui/src/app/interceptors/auth.interceptor.ts`, `src/ui/src/app/services/graphql-subscription.service.ts`, and existing login/current-user/logout GraphQL operations.

## Decision 4 — Exact Origin and session-bound CSRF policy

- **Decision**: For every browser GraphQL HTTP request and every WebSocket upgrade, require an `Origin` value that exactly matches a configured allowlist entry after canonical origin parsing; missing, `null`, wildcard, substring-only, and non-allowlisted values are rejected. Do not use Referer as a bypass. Credentialed CORS, where needed, may echo only the matched origin, never `*`.
- **Authenticated HTTP mutations**: require `X-CSRF-Token` matching the authenticated session-bound value before executing the mutation. `login` and first-run `bootstrapPlatformAdmin` are explicit pre-session operations when no authenticated session exists: they remain subject to exact Origin checks and existing rate limits, but no authenticated CSRF token is required before the session is created. Repository inspection confirms both successful operations create an authenticated session. They must set the cookie through the HTTP response layer and return the associated CSRF value in their GraphQL `AuthPayload`, never the JWT. Add a read-only `currentSessionCsrfToken` query field within the existing `/graphql` schema to return/reacquire the value for an already authenticated cookie session after reload; this query needs no CSRF header, so acquisition is not circular. A login/bootstrap CSRF exemption does not apply when a valid session cookie is present. Authenticated logout and all other mutations require CSRF. Keep the token only in app memory; never treat it as a bearer credential.
- **Lifecycle/no migration**: Generate a CSRF token for each authenticated session and associate it with the existing session identifier in server process state. Clear/invalidate that association on logout/revocation. Reuse existing session IDs/revocation support and existing GraphQL operations; no SQLite migration is planned. The initial authenticated-session query is read-only and needs the cookie plus allowed Origin, not a CSRF token, so acquisition is not circular.
- **WebSocket**: validate allowed Origin and the cookie/session at the `/graphql` handshake. The browser cookie jar supplies the credential; `connection_init` carries no JWT. WebSocket protocol behavior remains `graphql-transport-ws`.
- **Rationale**: Exact Origin checks cover both cross-site HTTP and WS use; session-bound tokens prevent cross-site state-changing HTTP operations while preserving the endpoint boundary. Bootstrap/login is separately executable before a session exists.
- **Alternatives considered**: no Origin validation; Referer fallback; wildcard CORS; double-submit or random client-generated CSRF values; adding a REST CSRF endpoint; treating login as if it already had an authenticated CSRF token (all rejected). A random token is server-generated and tied to the existing session, not generated by Angular.
- **Repository evidence**: `src/main/cpp/isched/backend/isched_Server.cpp` currently has draft mutation-only CSRF/Origin checks and `src/main/cpp/isched/shared/config/isched_config.hpp` exposes origin configuration; current check must be tightened to an exact allowlist on all GraphQL HTTP requests and WS handshakes. Existing session identity/revocation is in `isched_AuthenticationMiddleware.*` and the existing `sessions` table in `isched_DatabaseManager.*`. The `login` and `bootstrapPlatformAdmin` resolvers both call `create_session` and return `AuthPayload`; the schema currently exposes `currentUser` as the authenticated read-only query. Current UI CSRF generation is client-side ephemeral and therefore does not meet the session-binding requirement.

## Decision 5 — Reuse Angular and server structure; no migration

- **Decision**: Extend existing standalone Angular services/components and server request/auth/resolver paths in place. Keep app-owned template state signal-backed, use typed reactive forms, strict TypeScript and template checks, standalone APIs, modern `@if`/`@for`/`@switch`, and OnPush/zoneless-compatible patterns where feasible. Do not introduce a schema migration, new service, or new external API.
- **Rationale**: Existing repository paths already implement the Playground, organization/user admin pages, GraphQL operations, auth bootstrap, and proxy. The existing per-tenant SQLite sessions table supports session/revocation identity; the planned CSRF association is process memory, not persisted schema.
- **Alternatives considered**: new Angular project, REST admin layer, separate LSP service, SQLite table migration, NgModule-first or observable/async-pipe template state (rejected: unnecessary and/or violates constitution).
- **Repository evidence**: paths in `src/ui/src/app/pages/{playground,admin}`, `services/`, `components/playground/`, `src/main/cpp/isched/backend/`, and `src/test/cpp/integration/`.

## Decision 6 — Equal acceptance for Playground and administration

- **Decision**: Playground schema browsing/edit/run and organization/user administration are both P1 and equally required. Each gets independent success/error coverage and its own performance run. Admin depends on cookie/session/CSRF foundation and existing GraphQL admin operations, not on the editor. Playground requires schema editor capability work; its subscription acceptance additionally depends on authenticated WS and proxy support.
- **Rationale**: Both journeys are explicitly required. Existing admin pages/services are present, but the prior task set does not include complete new editor language capability or Admin journey acceptance work.
- **Alternatives considered**: Playground-only MVP, Admin as optional follow-on, or make Admin depend on Playground implementation (rejected).

## Decision 7 — Local proxy and WebSocket behavior

- **Decision**: Keep the Angular proxy for `/graphql`, with HTTP forwarding and WS upgrade support. Browser code uses relative URLs. The proxy preserves browser cookie semantics. Secure-cookie behavior is verified in supported local browsers; there is no silent Secure downgrade or bearer workaround.
- **Rationale**: FR-005 is the canonical HTTP/WS proxy requirement and FCR-006 references it. `src/ui/proxy.conf.json` already maps `/graphql` with `ws: true`.
- **Alternatives considered**: hard-coded backend origin, CORS-only dev setup, or a distinct LSP route (rejected).

## Decision 8 — Reproducible performance measurement

- **Decision**: Run each journey separately with 10 concurrent users. Each user performs 10 unmeasured warmup operations for that journey, followed by 100 measured operations. Denominator is exactly 1,000 measured UI operations per journey; use nearest-rank p95 (rank 950) and require strictly `< 2,000 ms`. Measure from immediately before request dispatch on submit until the expected final result is visible, loading/submitting is settled, and an Angular render frame has occurred. Include browser/client, proxy, server, and UI rendering. Exclude setup, login/schema loading/fixture creation, think time, and warmups. Failed or timed-out operations remain failures and cannot be excluded from the denominator.
- **Reproducibility**: Freeze a deterministic 100-operation manifest, operation-class counts, and seeded/resettable fixture per journey in the test harness. Synchronize all 10 users at a start barrier; do not pool Playground and Admin results. For each user, replay the first 10 manifest operations as unmeasured warmup, then replay all 100 operations as measured work. Report aggregate journey p95 and the per-operation-class counts/p95 as diagnostics.
- **Rationale**: This resolves the prior draft's missing warmup count and Playground-only sequential measurement while preserving submission-to-final-UI timing.
- **Alternatives considered**: 10 sequential Playground-only samples, backend response-time-only, `<= 2s`, or mixing both journeys into one denominator (rejected: does not satisfy the finalized journey scope or timing boundary).

## Decision 9 — Completion verification

- **Decision**: Require a recorded constitution-compliance review as an explicit blocking closeout gate. Verify GraphQL-only traffic, exact Origin and CSRF behavior, cookie-only HTTP/WS auth, editor capabilities, Admin journey, strict Angular patterns, both performance tests, and feature/project threat-model coverage. Plan documents do not claim implementation evidence.
- **Rationale**: Constitution review is mandatory; completing a plan or passing a subset of tests cannot substitute for closeout review.
- **Alternatives considered**: infer compliance from test status or defer the review beyond feature closeout (rejected).

## Resolved Clarifications / Blockers

All architecture decisions required by Phase 0 are resolved by the final user instructions and repository evidence. No LSP JSON-RPC conformance, external LSP endpoint, bearer alternative, SameSite ambiguity, CSRF acquisition cycle, or warmup-count blocker remains. Deployment-specific trusted Origin values must be supplied in configuration; this is operational configuration, not an unresolved design question. This document preserves the planning decisions; implementation, runtime verification, and constitution closeout were subsequently completed and are recorded in [quickstart.md](quickstart.md).
