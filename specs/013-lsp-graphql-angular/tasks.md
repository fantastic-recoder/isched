# Tasks: LSP GraphQL Angular Integration

**Input**: Design documents in `specs/013-lsp-graphql-angular/` (`plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`, and `threat-model.md`)

**Tests**: Required by the feature specification and constitution. Unit/component, contract, integration, security, strict Angular, GraphQL-boundary, and separate per-journey performance verification are completion gates.

**Organization and ID preservation**: Existing task IDs T001–T040 are retained. The nine tasks identified as outdated or unsupported by the finalized design have been reopened and corrected in place; other checked tasks remain checked as existing-scope completions. Newly identified requirements are appended as T041–T070, grouped by story and then by priority. Follow the dependency edges below rather than assuming every appended task must wait for every earlier-numbered task.

**Design constraints**: The only browser API is GraphQL `/graphql` over HTTP/WS. Authentication is Secure, HttpOnly, SameSite=Strict cookie-only; there is no bearer alternative, JWT persistence, Authorization header, or JWT in `connection_init`. Enforce exact configured Origin on every GraphQL HTTP request and WS handshake. Authenticated HTTP mutations require session-bound CSRF; pre-session login/first-run bootstrap are narrowly exempt only with no valid session, and successful operations establish the cookie session and return CSRF through GraphQL. Schema-aware completion, hover, and live diagnostics are browser-local. Playground and administration are equally required P1 journeys.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Preserve completed setup work and its repository-level scaffolding.

- [X] T001 Add GraphQL-only transport boundary static test (no application `fetch`/`HttpClient` calls outside `/graphql`) in `src/ui/src/app/graphql-boundary.static.spec.ts`
- [X] T002 [P] Add `test:graphql-boundary` and `e2e:subscription-proxy` script entries in `src/ui/package.json`
- [X] T003 [P] Register C++ regression test executable slots (`test_graphql_subscription_auth`, `test_graphql_only_boundary`) in `src/test/cpp/isched/CMakeLists.txt`
- [X] T004 [P] Add operation start/end latency measurement helper in `src/ui/src/app/services/operation-latency.util.ts`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The shared transport and cookie/session security foundation blocks authenticated business journeys. Keep WS-specific verification distinct from HTTP-only flows.

- [X] T005 Replace the `connection_init` bearer-token payload with cookie-only WS authentication; ensure no JWT/token is serialized in the protocol payload in `src/ui/src/app/services/graphql-subscription.service.ts`
- [X] T006 Remove `AuthService` token sourcing from `GraphQLSubscriptionService`; preserve subscription lifecycle state without an in-memory bearer credential in `src/ui/src/app/services/graphql-subscription.service.ts`
- [X] T007 [P] Validate the browser cookie/session and exact Origin during the `/graphql` WebSocket handshake, rejecting unauthenticated protected subscription sessions without executing subscription work in `src/main/cpp/isched/backend/isched_Server.cpp` and `src/main/cpp/isched/backend/isched_SubscriptionBroker.cpp`
- [X] T008 [P] Add backend regression coverage for cookie-authenticated WS handshake acceptance/rejection and unauthorized close behavior in `src/test/cpp/integration/test_graphql_subscription_auth.cpp`
- [X] T009 [P] Extend the dev-proxy e2e test to assert WebSocket upgrade success through the proxy in `src/ui/e2e/dev-proxy.spec.ts`
- [X] T010 [P] Add IndexedDB to the no-persistent-JWT assertion alongside localStorage/sessionStorage checks in `src/ui/src/app/services/auth.service.spec.ts`

**Checkpoint**: Complete the shared HTTP security prerequisites T053 and T055–T060 before marking either business journey complete. WS handshake/authentication tasks T005–T008, T054, and T063 are additionally required for subscription acceptance and for final security closeout, but do not block Admin CRUD or Playground's query/mutation/editor path when those paths do not use subscriptions.

---

## Phase 3: User Story 1 — Explore and Execute GraphQL in the Playground (Priority: P1)

**Goal**: Provide schema browsing, schema-aware completion, hover documentation, live syntax/type diagnostics, and reliable query/mutation execution through `/graphql`.

**Independent Test**: With a backend exposing `/graphql`, load and browse the active schema; verify completion, hover descriptions, syntax/type diagnostics and an explicit unavailable state; then execute a query and mutation, inspect rendered results/errors, and confirm no editor-language request leaves `/graphql`.

### Existing core execution tasks

- [X] T011 [P] [US1] Add GraphQL operation contract test for the primary query/mutation shapes (`isched { ... }`) in `src/test/cpp/isched/isched_gql_executor_coverage_tests.cpp`
- [X] T012 [P] [US1] Add component test for success-path state transitions (`idle -> submitting -> success`) in `src/ui/src/app/pages/playground/playground.spec.ts`
- [X] T013 [P] [US1] Add component test for recoverable GraphQL error and retry with preserved editor content in `src/ui/src/app/pages/playground/playground.spec.ts`
- [X] T014 [P] [US1] Add `PlaygroundQueryService` error-category mapping tests in `src/ui/src/app/services/playground-query.service.spec.ts`
- [X] T015 [P] [US1] Add end-to-end integration coverage for the primary query/mutation journey in `src/test/cpp/integration/test_webui_bootstrap_contract.cpp`
- [X] T016 [US1] Map validation/authorization/transport/unknown errors and retryability in `src/ui/src/app/services/playground-query.service.ts`
- [X] T017 [US1] Preserve editor content and expose retry state on recoverable failures in `src/ui/src/app/pages/playground/playground.ts`
- [X] T018 [US1] Render actionable error/retry affordances using modern control flow in `src/ui/src/app/pages/playground/playground.html`
- [X] T019 [US1] Surface normalized GraphQL error fields through `PlaygroundQueryResult` in `src/ui/src/app/services/playground-query.service.ts`
- [X] T020 [US1] Add retry handler that resubmits preserved editor content in `src/ui/src/app/components/playground/result-panel/result-panel.ts`

---

## Phase 4: User Story 3 — Maintain Secure Session Behavior in Browser (Priority: P1; existing baseline tasks)

**Goal**: Keep non-secret session UI state and reauthentication UX while all credentials remain browser-managed cookies and CSRF values remain session-bound, in-memory-only values.

**Independent Test**: Authenticate, perform protected HTTP operations with the browser cookie and session-bound CSRF, revoke/expire the session, and verify reauthentication with no JWT in storage, application bearer state, headers, URLs, or WS payloads.

- [X] T021 [P] [US3] Add contract test for `UNAUTHENTICATED`/`FORBIDDEN` GraphQL error envelopes in `src/test/cpp/isched/isched_gql_executor_auth_resolvers_tests.cpp`
- [X] T022 [P] [US3] Add integration test for mid-session expiry and reauthentication redirect behavior in `src/test/cpp/integration/test_session_revocation.cpp`
- [X] T023 [P] [US3] Verify `reauth-required` clears in-memory CSRF and non-secret session state without any bearer-token state in `src/ui/src/app/guards/auth.guard.spec.ts`
- [X] T024 [P] [US3] Add Playwright checks for zero JWT artifacts in browser storage APIs, including `indexedDB.databases()`, after login/expiry in `src/ui/e2e/rate-limiting.spec.ts`
- [X] T025 [US3] Map unauthenticated/forbidden outcomes to `reauth-required`, clear CSRF/non-secret session state, and remove access-token/bearer handling in `src/ui/src/app/services/auth.service.ts`
- [X] T026 [US3] Redirect protected flows to `/login` with return-URL preservation on reauthentication-required outcomes in `src/ui/src/app/guards/auth.guard.ts`
- [X] T027 [US3] Preserve safe in-progress Playground content across reauthentication redirect/return in `src/ui/src/app/services/session-bootstrap-state.service.ts`
- [X] T028 [US3] Add user-facing reauthentication guidance in `src/ui/src/app/pages/login/login.html`

---

## Phase 5: User Story 4 — Enable Predictable Local Development Workflow (Priority: P2; existing baseline tasks)

**Goal**: Route GraphQL HTTP and WebSocket traffic through the local proxy without hard-coded backend URLs or insecure cookie workarounds.

**Independent Test**: Run the local backend and Angular proxy; verify HTTP and WS `/graphql` traffic, cookie/Origin handling, and subscription diagnostics while ordinary query/mutation flows remain usable.

- [X] T029 [P] [US4] Add dev-proxy WebSocket upgrade regression coverage in `src/ui/e2e/dev-proxy.spec.ts`
- [X] T030 [P] [US4] Add backend integration coverage for proxied subscription delivery in `src/test/cpp/integration/test_graphql_subscriptions.cpp`
- [X] T031 [P] [US4] Add frontend test for WS failure fallback messaging while non-subscription flows remain usable in `src/ui/src/app/pages/dev/dev-proxy-health.page.spec.ts`
- [X] T032 [US4] Surface subscription connection lifecycle as signal-backed state in `src/ui/src/app/services/graphql-subscription.service.ts`
- [X] T033 [US4] Render a non-blocking failed-subscription diagnostic in `src/ui/src/app/pages/dev/dev-proxy-health.page.html`
- [X] T034 [US4] Add local dev-proxy verification for HTTP and WS forwarding in `tools/run_e2e.py`
- [X] T035 [US4] Document local proxy run/troubleshooting steps in `specs/013-lsp-graphql-angular/quickstart.md`

---

## Phase 6: Polish & Cross-Cutting Concerns (Existing Tasks; Reopened Scope Gates)

**Purpose**: Closeout work must reflect the finalized two-journey performance protocol and feature-wide verification, not the earlier Playground-only scope.

- [ ] T036 [P] Replace the earlier Playground-only `p95 <= 2s` check with a deterministic 100-operation manifest and fixed fixture/class counts; synchronize 10 users at a start barrier; have each replay the first 10 manifest operations unmeasured, then all 100 as measured UI operations; time submit dispatch through visible final result after loading settles and an Angular render frame; retain all 1,000 samples and fail on any timeout/error; report aggregate and per-class counts/p95; require nearest-rank rank-950 p95 strictly `< 2,000 ms` in `src/ui/e2e/playground.spec.ts`
- [X] T037 [P] Update feature threat-model controls and residual-risk notes without claiming implementation evidence in `specs/013-lsp-graphql-angular/threat-model.md`
- [X] T038 [P] Synchronize the feature threat-model summary in `docs/security-threat-model.md`
- [ ] T039 Run the completed quickstart verification after implementation: strict UI build/tests, full applicable CTest gate, security checks, both independent performance runs, and record actual results in `specs/013-lsp-graphql-angular/quickstart.md`
- [X] T040 Rework the schema-upload integration regression to prove upload operations remain on GraphQL `/graphql` and no alternate API route is added in `src/test/cpp/integration/test_graphql_schema_upload.cpp`

---

## Phase 7: Remaining User Story 1 — Schema-Aware Browser Editor (Priority: P1)

**Goal**: Complete the editor language capabilities absent from the existing execution-flow tasks. All schema/documentation data comes from introspection through `/graphql`; this is not an LSP JSON-RPC endpoint.

**Independent Test**: Test completion, hover descriptions, live syntax and schema/type diagnostics, incomplete-schema failure feedback, and GraphQL-only network traffic independently of Admin.

- [X] T041 [P] [US1] Add introspection contract tests proving the response includes the schema/type/field/argument/input/enum references and descriptions needed to build an active client schema in `src/ui/src/app/services/playground-introspection.service.spec.ts`
- [X] T042 [P] [US1] Add editor component tests for schema-aware completion, hover descriptions, syntax/type diagnostics, absent descriptions, and unavailable-schema behavior in `src/ui/src/app/components/playground/query-editor/query-editor.spec.ts`
- [X] T043 [US1] Expand the `/graphql` introspection query and models to retain the complete schema shape and descriptions, and declare directly imported GraphQL runtime dependencies in `src/ui/src/app/services/playground-introspection.service.ts`, `src/ui/src/app/services/playground-introspection.models.ts`, `src/ui/package.json`, and `pnpm-lock.yaml`
- [X] T044 [US1] Build/update the browser-local GraphQL schema and connect schema-backed completion, hover, and live syntax/type diagnostics to CodeMirror in `src/ui/src/app/components/playground/query-editor/query-editor.ts`
- [X] T045 [US1] Display explicit language-service-unavailable state when introspection/schema construction fails, without implying validation is active, in `src/ui/src/app/components/playground/query-editor/query-editor.html`
- [ ] T046 [US1] Add Playground browser acceptance coverage for schema browsing, completion, hover, diagnostics, query/mutation execution, and absence of non-`/graphql` editor requests in `src/ui/e2e/playground.spec.ts`

---

## Phase 8: User Story 2 — Administer Organizations and Users (Priority: P1)

**Goal**: Complete the equally required organization/user Admin journey via GraphQL, including visible success state and actionable validation, authorization, conflict, and transport failures.

**Independent Test**: Using an authorized cookie-authenticated administrator, independently list and manage an organization and its users; verify server-visible final state, retained safe form input on failures, and `/graphql`-only network requests.

- [ ] T047 [P] [US2] Add organization list/create/update/status and validation/conflict/error component coverage in `src/ui/src/app/pages/admin/organizations.page.spec.ts` and `src/ui/src/app/services/organization.service.spec.ts`
- [ ] T048 [P] [US2] Add user list/create/update/status and validation/authorization/error component coverage in `src/ui/src/app/pages/admin/users.page.spec.ts` and `src/ui/src/app/services/user.service.spec.ts`
- [ ] T049 [US2] Complete signal-backed organization management and typed reactive forms, preserving safe form values on retryable failures, in `src/ui/src/app/pages/admin/organizations.page.ts` and `src/ui/src/app/pages/admin/organizations.page.html`
- [ ] T050 [US2] Complete organization-scoped user management, typed reactive forms, and actionable error outcomes in `src/ui/src/app/pages/admin/users.page.ts` and `src/ui/src/app/pages/admin/users.page.html`
- [ ] T051 [US2] Add Admin end-to-end acceptance for organization and user management, final visible state, and GraphQL-only requests in `src/ui/e2e/admin.spec.ts`

---

## Phase 9: Remaining User Story 3 — Cookie, Origin, and CSRF Security (Priority: P1; shared blocking prerequisite)

**Goal**: Implement and independently verify the finalized cookie-only session, exact-Origin, and session-bound CSRF contract before either business journey is marked complete.

**Independent Test**: Exercise pre-session login/bootstrap, authenticated session bootstrap, HTTP mutation CSRF success/failure before side effects, cookie logout/revocation, and WS authentication. Reject missing/null/malformed/unlisted Origin for all HTTP requests and WS handshakes; observe no JWT in browser storage, JavaScript bearer state, Authorization headers, URLs, or `connection_init`.

- [X] T052 [P] [US3] Add backend HTTP security integration coverage for valid/missing/null/malformed/unlisted Origin, cookie attributes, CSRF success/failure, pre-session login/bootstrap, and no resolver side effects on rejection in `src/test/cpp/integration/test_graphql_http_security.cpp` and register the target in `src/test/cpp/isched/CMakeLists.txt`
- [X] T053 [US3] Parse and exactly validate configured Origins for every GraphQL HTTP request (including queries, mutations, login/bootstrap, and applicable preflight); fail closed and never use Referer or wildcard credentialed CORS in `src/main/cpp/isched/backend/isched_Server.cpp`, `src/main/cpp/isched/backend/isched_Server.hpp`, and `src/main/cpp/isched/shared/config/isched_config.hpp`
- [X] T054 [US3] Validate exact configured Origin and browser cookie/session at the `/graphql` WS handshake before protected subscription work in `src/main/cpp/isched/backend/isched_Server.cpp` and `src/main/cpp/isched/backend/isched_SubscriptionBroker.cpp`
- [X] T055 [US3] Generate unpredictable per-session CSRF values, bind them to existing session identifiers in process state, and invalidate them on logout/revocation without a database migration in `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.cpp` and `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.hpp`
- [X] T056 [US3] Add the session CSRF value to successful login/bootstrap GraphQL results and expose the read-only authenticated `currentSessionCsrfToken` query, without returning a JWT, in `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql` and `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T057 [US3] Set and expire the Secure, HttpOnly, SameSite=Strict session cookie through HTTP response handling for login/bootstrap/logout while keeping JWT out of GraphQL JSON in `src/main/cpp/isched/backend/isched_Server.cpp` and `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T058 [US3] Identify the actual parsed GraphQL operation and validate session-bound `X-CSRF-Token` before every authenticated HTTP mutation side effect; exempt login/first-run bootstrap only when no valid session cookie exists, retaining Origin checks and rate limits in `src/main/cpp/isched/backend/isched_Server.cpp` and `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T059 [US3] Replace JavaScript access-token state with non-secret session state and in-memory-only CSRF acquisition from login/bootstrap or the read-only GraphQL bootstrap query in `src/ui/src/app/services/auth.service.ts` and `src/ui/src/app/services/session-bootstrap-state.service.ts`
- [X] T060 [US3] Ensure browser-cookie GraphQL HTTP requests attach the in-memory CSRF header only for mutations and never add Authorization/bearer credentials in `src/ui/src/app/services/graphql.service.ts` and `src/ui/src/app/interceptors/auth.interceptor.ts`
- [X] T061 [P] [US3] Add frontend tests for CSRF acquisition/reacquisition/clearing, mutation-only header behavior, and absence of bearer state/Authorization headers in `src/ui/src/app/services/auth.service.spec.ts` and `src/ui/src/app/services/graphql.service.spec.ts`
- [ ] T062 [P] [US3] Add browser security acceptance asserting cookie attributes/automatic attachment, exact-Origin outcomes, pre-session login/bootstrap behavior, session CSRF rejection before side effects, and zero JWT exposure/persistence in `src/ui/e2e/auth-session-security.spec.ts`
- [X] T063 [P] [US3] Assert WebSocket authentication uses the browser cookie and exact Origin with no JWT or Authorization data in `connection_init` in `src/ui/src/app/services/graphql-subscription.service.spec.ts`

---

## Phase 10: Remaining User Story 4 — Local HTTP/WS Proxy Acceptance (Priority: P2)

**Goal**: Verify the existing `/graphql` proxy under cookie-only auth and exact-Origin policy without disabling Secure or adding a bearer workaround.

**Independent Test**: Use the configured secure local browser/proxy setup for HTTP query/mutation and WS subscription traffic; verify cookie and Origin behavior, WS upgrade, and graceful failure diagnostics.

- [ ] T064 [P] [US4] Add end-to-end proxy acceptance for HTTP and WS cookie/Origin forwarding, successful upgrade, and `connection_init` without credentials in `src/ui/e2e/dev-proxy.spec.ts`
- [ ] T065 [US4] Verify the local `/graphql` proxy remains relative-path and WS-enabled, the dev origin is explicitly present in backend allowlist configuration, and Secure-cookie behavior is not downgraded; document supported secure local setup in `src/ui/proxy.conf.json`, `src/main/cpp/isched/shared/config/isched_config.hpp`, and `specs/013-lsp-graphql-angular/quickstart.md`

---

## Phase 11: Final Performance and Cross-Cutting Gates

**Purpose**: Complete both separate benchmark protocols, frontend convention checks, threat-model review, and blocking closeout reviews.

- [ ] T066 [P] [US2] Add the independent Admin performance run with a deterministic 100-operation manifest, fixed fixture and operation-class counts, 10 concurrent users at a start barrier, each replaying the first 10 operations as unmeasured warmups then all 100 as measured UI operations; time submit dispatch through visible final result after loading settles and an Angular render frame; retain all 1,000 samples and fail on any timeout/error; report aggregate/per-class counts and p95; require nearest-rank rank-950 p95 strictly `< 2,000 ms` in `src/ui/e2e/admin-performance.spec.ts`
- [X] T067 [P] Verify strict TypeScript/template compilation and explicitly check typed reactive forms, standalone components, signal-backed app-owned template state, and modern `@if`/`@for`/`@switch` in `src/ui/package.json` and the affected Playground/Admin component specs
- [ ] T068 [P] Extend the static and browser network boundary gates to cover editor introspection and both P1 business journeys, allowing only GraphQL `/graphql` HTTP/WS application traffic in `src/ui/src/app/graphql-boundary.static.spec.ts` and `src/ui/e2e/playground.spec.ts`
- [ ] T069 Review the feature and project threat models against implemented evidence, distinguishing verified controls from residual/planned risks in `specs/013-lsp-graphql-angular/threat-model.md` and `docs/security-threat-model.md`
- [ ] T070 Record the required constitution-compliance review as complete only after both P1 journeys, all security/strict-Angular/performance checks, and threat-model review pass; document review evidence in `specs/013-lsp-graphql-angular/quickstart.md`

---

## Dependencies and Exact Journey Gates

### Shared foundation

- T001–T004 are existing setup completions. T005–T008 are reopened because the old implementation/task wording made `connection_init` bearer authentication authoritative; the finalized design requires cookie/session authentication and exact Origin validation at the handshake.
- The security implementation has test/implementation edges: **T052 → T053, T054, T055, T056, T057, T058**; **T055 → T056 → T057 → T058**; **T056 and T057 → T059 → T060 → T061**. T062 depends on T053 and T055–T060. T063 depends on T054 and T005–T008.
- Login and first-run bootstrap remain Origin-checked and rate-limited; they need no session-bound CSRF header only if no valid session exists. Both successful operations set the cookie and return CSRF through GraphQL. Existing-session acquisition is read-only GraphQL. Authenticated logout and every other HTTP mutation require CSRF before side effects. No endpoint or migration is introduced.

### User Story 1 — Playground

- Existing query/mutation/error/retry tasks T011–T020 remain independently useful. Remaining editor chain: **T041 → T043 → T044 → T045**; T042 can be authored in parallel with T041; **T044 and T046** are required for editor acceptance.
- Core authenticated HTTP journey depends on shared HTTP security tasks **T053, T055–T060**. The language-service schema request also uses only `/graphql` and does not depend on the Admin story. If a Playground subscription is included, it additionally depends on **T054, T063, T064**.
- The Playground performance gate is T036 and depends on T004, T041–T046, and the shared HTTP security tasks. Its 10-user/1,000-sample result is not pooled with Admin.

### User Story 2 — Administration

- Organization and user test authoring can run in parallel: T047 and T048. Implementations depend on their corresponding tests and the shared authenticated HTTP prerequisites **T053, T055–T060**: **T047 → T049**, **T048 → T050**, and **T049 + T050 → T051**.
- Admin is independent of Playground editor implementation; it does not depend on T041–T046. The separate Admin performance gate T066 depends on T049–T051 and shared HTTP security tasks.

### User Story 3 — Browser session security

- T021–T028 retain the existing auth-error/reauth baseline except T023 and T025, which are reopened to remove bearer-state assumptions. T052–T063 implement the finalized cookie, Origin, and CSRF contract. Complete the WS handshake chain T005–T008/T054/T063 only for WS acceptance; HTTP-only security does not depend on proxy WebSocket tasks.

### User Story 4 — Local development proxy

- Existing HTTP/WS proxy tasks T029–T035 are retained. Cookie-authenticated proxy acceptance is **T054 + T063 → T064**, with T065 verifying configuration/documentation. This journey does not block Admin CRUD; it blocks only the proxy/subscription acceptance path.

### Closeout and verification

- T036 and T066 are separate per-journey performance gates, each with its own fixed 100-operation workload, 10 users, 10 warmups/user, 100 measured operations/user, and 1,000-sample denominator. Each has nearest-rank p95 rank 950 and strict `< 2,000 ms`; failure/timeouts fail the run and are not dropped.
- T067–T069 require both journeys and relevant security implementations to be in place. T040/T068 verify the canonical `/graphql` boundary. **T070 is the final blocking gate** and depends on T036, T039, T040, T066–T069 plus both independent P1 journeys and all security checks.

---

## Parallel Opportunities

- Setup: T002–T004 are independent. In the foundational phase, T009/T010 can proceed independently from the WS client/backend correction, while T007 and T008 can be developed as separate server/test changes after the contract is agreed.
- Playground: T041/T042 can be authored in parallel; introspection modeling T043 precedes editor wiring T044; editor availability UI T045 follows the schema lifecycle; T046 follows the integrated editor behavior.
- Admin: T047/T048 test authoring can proceed in parallel; T049/T050 can be implemented in parallel after shared security is ready; T051 integrates both.
- Security: T053 (HTTP Origin), T054 (WS handshake), and T055 (session CSRF state) touch distinct concerns and can proceed in parallel after T052's cases are agreed. T061/T062/T063 are independent test tracks once their APIs/contract are stable.
- Proxy: T064 is independent of Admin after cookie/WS security foundations; T065 can proceed in parallel on configuration/documentation.
- Closeout: T036, T066, T067, T068, and T069 are parallelizable only after their respective journey/security implementation prerequisites; T070 waits for all gates.

## Implementation Strategy

1. Preserve already-completed baseline work and finish shared cookie/Origin/CSRF prerequisites; do not treat the old bearer-based WS tasks as satisfying the new contract.
2. Complete the two P1 journeys independently: Playground schema assistance/execution (US1) and organization/user management (US2). Neither is optional or a prerequisite for the other.
3. Complete the P1 browser security story (US3) as a blocking shared prerequisite. Complete the P2 proxy story (US4) independently, with WS-specific dependencies only for subscription acceptance.
4. Run each journey's deterministic performance protocol separately and require strict p95 `< 2,000 ms` with the full 1,000 measured operations in its denominator.
5. Close out only after strict Angular, CTest, security, boundary, threat-model, and documented constitution-review gates pass. Tasks generation itself is not evidence that implementation or any test has passed.
