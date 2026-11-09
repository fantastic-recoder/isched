# Implementation Plan: LSP GraphQL Angular Integration

**Branch**: `013-lsp-graphql-angular` | **Date**: 2026-11-08 | **Spec**: [spec.md](spec.md)
**Input**: `specs/013-lsp-graphql-angular/spec.md`
**Status**: Completed; implementation and review evidence is recorded in [quickstart.md](quickstart.md).

## Summary

Deliver both required Angular journeys—the GraphQL Playground and organization/user administration—over the existing GraphQL-only `/graphql` HTTP and WebSocket boundary. The Playground editor will use the active schema obtained through GraphQL introspection for schema-aware completion, hover documentation, and live syntax/type diagnostics; these are browser-local capabilities, not a remote LSP endpoint or a requirement for LSP JSON-RPC wire conformance. Replace browser bearer-token handling with Secure, HttpOnly, SameSite=Strict cookie transport. Enforce an exact configured Origin allowlist for GraphQL HTTP requests and WebSocket handshakes, and require a session-bound CSRF token for authenticated HTTP mutations. Login and first-run platform-admin bootstrap are the explicit pre-session operations: each remains Origin-validated and rate-limited, and on success creates the cookie session and returns its session-bound CSRF token through the GraphQL response. An existing session reacquires the token through a read-only GraphQL session-bootstrap query, avoiding a circular prerequisite. These new GraphQL fields/response behaviors remain within `/graphql`; no separate endpoint or database migration is planned. Reuse existing session identity/revocation support and in-process session state. Verify both journeys independently under the required concurrent performance protocol.

## Technical Context

**Language/Version**: C++23 backend; TypeScript 5.9 / Angular 21 frontend (strict mode and strict templates)
**Primary Dependencies**: Existing CMake + Ninja, Conan 2.x, Catch2 3.x, cpp-httplib/Boost.Beast HTTP+WebSocket server, jwt-cpp, SQLite; Angular standalone APIs, signals, typed reactive forms, RxJS where stream semantics are needed, CodeMirror 6, `cm6-graphql` 0.2.x, GraphQL.js 16.x, GraphQL Language Service APIs, Jest/Angular test harness, Playwright
**Storage**: Existing per-tenant SQLite sessions table and session/revocation support; no feature-specific schema migration. The CSRF value is associated with the existing session identifier in server process state and is reacquirable through GraphQL after page reload. No JWT in `localStorage`, `sessionStorage`, or IndexedDB.
**Testing**: Catch2/CTest backend unit and integration coverage; Angular/Jest unit/component tests; Playwright E2E and network-boundary checks; strict Angular build; focused security checks; reproducible UI-level latency tests
**Target Platform**: Linux backend and supported modern browsers; local Angular dev server through the existing `/graphql` HTTP+WS proxy
**Project Type**: Existing C++ GraphQL web service plus Angular WebUI; extend in place, no new project or external service
**Performance Goals**: Separately for Playground and administration, 10 concurrent users; each performs 10 unmeasured warmup operations, then 100 measured operations. Each journey therefore has exactly 1,000 measured samples. The nearest-rank p95 (sample 950 of 1,000) must be strictly below 2,000 ms. Timing is operation submission through the final result rendered and visible in the UI.
**Constraints**: Preserve GraphQL-only `/graphql` HTTP/WebSocket boundary; no REST fallback and no new external LSP endpoint. Cookie authentication is Secure, HttpOnly, SameSite=Strict only; no Authorization or WebSocket bearer payload. Exact allowed-origin validation applies to all GraphQL HTTP requests and WebSocket handshakes. Authenticated HTTP mutations require a session-bound CSRF header. No migration.
**Scale/Scope**: Four independently verifiable user journeys (Playground, administration, browser session security, local HTTP/WS proxy workflow); both P1 business journeys are equally required. Performance runs are separate per business journey with a 10-user concurrency barrier.

## Constitution Check

*Gate: design reviewed before Phase 0 and again after Phase 1. Implementation closeout, including changed backend C++/Clean Code and synchronization review, passed on 2026-11-09; see quickstart.md.*

- [x] **GraphQL-only boundary**: FR-001 is canonical; all browser/server application requests stay at `/graphql` over HTTP or WebSocket. FCR-004 explicitly references FR-001. No LSP protocol endpoint is planned.
- [x] **Language/build baseline**: Preserve C++23, Conan-managed dependencies, CMake + Ninja.
- [x] **Angular conventions**: Plan explicit verification of app-owned signal state, standalone components, modern `@if`/`@for`/`@switch`, typed reactive forms, strict TypeScript and template checks, and zoneless/OnPush-compatible patterns where feasible.
- [x] **Browser credential rules**: FR-004 prohibits JWT persistence in localStorage/sessionStorage/IndexedDB. Separately, FCR-005 requires Secure, HttpOnly, SameSite=Strict cookie transport and prohibits in-memory bearer credentials and bearer alternatives.
- [x] **Proxy**: FR-005 is the canonical `/graphql` HTTP+WebSocket proxy requirement; FCR-006 references it for Angular. No backend origin is hard-coded into client source.
- [x] **Verification**: Include unit/component, integration, security and E2E checks for each journey; test evidence is required before story completion.
- [x] **Threat models**: Maintain the feature threat model and corresponding project-wide summary; both describe planned controls without claiming implementation evidence.
- [x] **Clean Code**: Plan focused server/client responsibilities and require implementation review for conditional complexity and any profiled hot-path abstraction trade-off.
- [x] **Closeout gate**: The documented implementation review covers frontend conventions, changed backend C++/Clean Code, CSRF synchronization, and both public/protected subscription policies. Verification and review scope are recorded in `quickstart.md`.

**Gate result**: Design and implementation reviews passed. The post-analysis refactor removed duplicated HTTP/WS cookie validation and extracted focused parsed-operation policy helpers. No new complexity exception is requested; this review covers feature changes, not a repository-wide audit of legacy code.

## Design Decisions and Execution Notes

1. **Browser-local editor assistance**: Use the installed CodeMirror/`cm6-graphql` stack with a complete described schema built from `/graphql` introspection. Declare GraphQL.js directly (and GraphQL Language Service directly if its APIs are imported rather than reached through an undeclared transitive dependency). Implement completion, hover descriptions, and live syntax/type diagnostics. No LSP JSON-RPC wire compatibility or external language-service endpoint is required.
2. **Cookie session and CSRF bootstrap**: Login and first-run `bootstrapPlatformAdmin` are pre-session operations when no authenticated session exists. Repository inspection confirms that both successful operations create an authenticated session. They remain subject to exact Origin validation and existing login/bootstrap abuse protections, do not require a CSRF token before that session exists, set the cookie through the HTTP response layer, and return the newly session-bound CSRF value in the GraphQL result but never the JWT. Add/extend GraphQL fields within the existing `/graphql` schema: include the CSRF value in the login/bootstrap `AuthPayload`, and expose a read-only `currentSessionCsrfToken` query field for an already authenticated cookie session to reacquire the value after reload. This read-only query needs no CSRF header, so acquisition is non-circular. A login/bootstrap CSRF exemption applies only when no valid session cookie is present; if a valid session is present, authenticated mutation validation applies. Store the value only in app memory and send it as `X-CSRF-Token` for authenticated HTTP mutations. Validate it against the current authenticated session before mutation resolvers run. Logout is an authenticated mutation and is not exempt; the HTTP response layer expires the cookie. Reuse existing session IDs/revocation facilities; keep any CSRF association in server process state so no database migration is required.
3. **Origin policy**: Normalize and exactly match `Origin` against an explicit configured allowlist for every GraphQL HTTP request and WebSocket handshake. Missing, `null`, wildcard, substring-only, or unlisted origins fail closed. Do not use Referer as a bypass. Credentialed CORS responses, if required, echo only the matched origin; never use `Access-Control-Allow-Origin: *`. Replace the current wildcard default with explicitly configured trusted origins. The same allowlist includes the configured Angular development origin and deployed UI origins.
4. **WebSocket authentication**: Browser-managed cookie is the only credential at the `/graphql` upgrade handshake; validate Origin and the cookie/session before accepting protected subscriptions. With an allowed Origin and no cookie, public anonymous subscriptions remain supported. Invalid supplied cookies fail rather than downgrading to anonymous access. Protected subscriptions must pass role/tenant checks before resolver or broker work on both listener routes. Keep `graphql-transport-ws` for protocol messages, but do not place a token in `connection_init`. Origin validation is required even for unauthenticated/failed handshakes.
5. **Session token mechanism (additional design choice)**: Generate an unpredictable CSRF value per authenticated session, associate it with the existing session identifier in process state, expose it only through the GraphQL login/session-bootstrap responses, and invalidate it on logout/revocation. This is intentionally separate from the JWT and does not change the SQLite schema. Never silently downgrade to a double-submit cookie or bearer-token fallback.
6. **Performance protocol**: Run each journey separately with 10 users synchronized at a start barrier. Freeze a deterministic 100-operation workload manifest, fixture data, and operation-class counts per journey. Each user first replays the manifest's first 10 operations as unmeasured warmup, then replays all 100 operations as measured UI work; reuse the same manifest for every user and run. Do not drop failed/timeout operations from the 1,000-sample denominator; any operation that does not reach its expected final visible result fails the run. Start a monotonic timer immediately before dispatch on submit; stop only after the expected final result is visible, loading/submitting UI has settled, and an Angular render frame has occurred. Include browser/client processing, HTTP/WS proxy, server, and UI render; exclude login, schema setup, fixture seeding, think time, and warmups. Compute nearest-rank p95 over all 1,000 measured observations for that journey; require `< 2,000 ms`, not `<=`. Report operation-class counts and p95 alongside the required aggregate.

These decisions supersede prior draft questions about LSP JSON-RPC conformance, SameSite mode, CSRF/origin policy, and warmup count. No clarification remains blocking plan generation. Deployment-specific allowlist values are configuration inputs, not unresolved architecture choices.

## Repository Structure

### Documentation (this feature)

```text
specs/013-lsp-graphql-angular/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── auth-session-contract.md
│   ├── dev-proxy-contract.md
│   ├── graphql-editor-language-service.md
│   └── graphql-operations.md
├── threat-model.md
└── tasks.md                 # Existing; this workflow does not modify it
```

### Source Code (repository-verified paths; implementation is not part of this run)

```text
src/main/cpp/isched/backend/
├── isched_Server.cpp/.hpp                       # HTTP GraphQL handling, response headers, WS handshake
├── isched_AuthenticationMiddleware.cpp/.hpp      # Cookie credential validation and session context
├── isched_GqlExecutor.cpp/.hpp                   # Existing GraphQL auth/session resolvers
├── isched_builtin_server_schema.graphql          # GraphQL login/session/introspection fields
├── isched_DatabaseManager.cpp/.hpp               # Existing SQLite sessions/revocation; no migration planned
└── isched_SubscriptionBroker.cpp/.hpp             # Existing subscription authorization lifecycle
src/ui/src/app/
├── services/{auth.service,graphql.service,graphql-subscription.service}.ts
├── services/{playground-introspection.service,playground-introspection.models}.ts
├── components/playground/query-editor/{query-editor.ts,query-editor.html}
├── pages/playground/ and pages/admin/{organizations.page.ts,users.page.ts}
└── interceptors/auth.interceptor.ts
src/ui/{package.json,proxy.conf.json,e2e/}
src/test/cpp/integration/                           # Existing and new GraphQL/auth/proxy integration coverage
src/test/cpp/isched/                                # Existing Catch2 test targets
```

**Structure Decision**: Extend the existing single backend and Angular WebUI in place. The frontend already has GraphQL, auth, introspection, subscription, organization/user, and proxy components. The server already has session/revocation infrastructure and the per-tenant SQLite `sessions` table. Plan no new service, REST route, external LSP endpoint, or database migration. Exact paths above are verified in the repository; no application/source files were modified by this planning run.

## Prerequisite and Journey Dependencies

- **Shared prerequisite (blocks all browser journeys)**: cookie-only session issuance/validation; exact HTTP Origin validation; GraphQL session-bound CSRF acquisition and authenticated-mutation validation; no bearer headers/payload; configured origin allowlist; GraphQL-only static/network boundary checks.
- **Playground P1**: shared prerequisite → full schema introspection/described schema snapshot → CodeMirror completion + hover + live syntax/type diagnostics and unavailable-schema state → query/mutation submit/result/error/retry integration. If subscription behavior is exercised, it additionally depends on the cookie-authenticated WS handshake and proxy upgrade capability; editor features themselves do not depend on WebSocket transport.
- **Administration P1**: shared prerequisite → existing organization/user GraphQL operations and authorization scope → typed forms, signal-backed list/detail/edit/status state and actionable conflict/error UX. It has no dependency on the Playground editor. Its authenticated writes depend on session-bound CSRF validation.
- **Local proxy / WebSocket journey**: shared cookie session → exact WS Origin and cookie validation → existing `graphql-transport-ws` client and `/graphql` proxy upgrade → subscription success/failure diagnostics. It is not a prerequisite for Admin CRUD; it is a prerequisite for any subscription acceptance path.
- **Performance and closeout**: both P1 journeys and the final UI-result timing harness must exist before their separate performance runs; the constitution review, feature/project threat-model review, strict frontend checks, and complete security regression checks block closeout.

## Complexity Tracking

No new constitution exception or additional project/service is proposed. The in-memory per-session CSRF association avoids a schema migration. Its lifecycle and exclusive `sessions_mutex_` locking strategy were reviewed against the single-process session/revocation model; see `data-model.md` and the backend review in `quickstart.md`.
