# Quickstart: LSP GraphQL Angular Integration

This document records the supported implementation workflow and observed verification evidence for Spec 13. No database migration was introduced.

## Prerequisites

- Backend configured with the repository's C++23, Conan, CMake/Ninja workflow (`python3 configure.py`) and exposing the existing `/graphql` HTTP and WebSocket endpoint.
- UI dependencies installed from the repository workspace (`pnpm install` in `src/ui`). GraphQL.js is declared directly for browser-local schema construction; no separate language-service backend is used.
- Configure an explicit exact-match allowlist containing the browser UI origin(s). Wildcard origin is invalid. The default local allowlist includes `http://localhost:4200` for Angular development and the `localhost`/loopback backend origins; deployed origins must be explicitly configured.
- Use Secure, HttpOnly, SameSite=Strict cookie authentication. Do not disable Secure, add an Authorization bearer fallback, or place JWTs in `localStorage`, `sessionStorage`, IndexedDB, or application memory.
- No schema migration is required; the implementation reuses existing session/revocation support.

## Local Run and Manual Acceptance

1. Start the backend with `/graphql` available over HTTP and WebSocket. Confirm configured allowed origins include the UI origin and reject unlisted origins.
2. Start Angular using the existing proxy: `cd src/ui && pnpm start`. Open the development UI as `http://localhost:4200` (not `http://127.0.0.1:4200`) in the supported Chromium setup. Confirm `src/ui/proxy.conf.json` forwards relative `/graphql` traffic to the local backend and enables WebSocket upgrades.
3. Use a fresh browser session and sign in through the GraphQL login operation or complete first-run `bootstrapPlatformAdmin`. Verify each successful response sets a Secure, HttpOnly, SameSite=Strict cookie. Each GraphQL `AuthPayload` supplies the resulting session-bound CSRF value; Angular retains it only in memory. Confirm neither response exposes a JWT to JavaScript. Both operations are exempt from a CSRF header only before a valid session exists, and remain Origin-validated and rate-limited.
4. Reload the page. Verify the read-only `currentSessionCsrfToken` GraphQL query field authenticates by cookie and returns/reacquires a CSRF value. This query has no mutation-CSRF prerequisite, so startup is not circular.
5. In Playground, load the active schema through GraphQL introspection and verify schema-aware completion, hover descriptions, and live syntax/type diagnostics. Verify unavailable-schema feedback if introspection fails. Execute a valid query and mutation, and verify visible results/errors and retry behavior.
6. In Admin, list and manage an organization and its users through GraphQL; verify typed-form validation, authorization/conflict/transport feedback, and final visible state.
7. Verify an authenticated HTTP GraphQL mutation includes the current `X-CSRF-Token`; missing/stale/cross-session values are rejected before resolver side effects. Verify login/first-run bootstrap is exempt only when no valid session is present and still requires allowed Origin and existing rate limiting. Logout is authenticated and requires CSRF.
8. Verify every HTTP request to `/graphql` and every WS handshake rejects a missing, `null`, malformed, or unlisted Origin. Verify WebSocket uses the browser cookie and no JWT in `connection_init`. A failed subscription must not break ordinary HTTP query/mutation flow.

### Secure-cookie local development note

The supported local browser route is `http://localhost:4200` through the Angular dev server. Chromium treats the localhost loopback origin as a trustworthy local context and browser acceptance confirmed that it retains and sends the `Secure` session cookie there. The proxy's `secure: false` setting applies only to upstream TLS certificate verification; it does not change or relax the cookie's `Secure` attribute. Do not open the UI through the numeric loopback address, disable `Secure`, or add bearer credentials as a workaround. For non-local development origins, use HTTPS and add the exact origin to `ISCHED_ALLOWED_ORIGINS`.

## Automated Verification

- **Strict UI compilation**: `pnpm --filter isched-ui run build` (strict TypeScript and Angular template checks remain enabled).
- **Frontend tests**: `pnpm --filter isched-ui run test:ci`; include typed forms, standalone usage, app-owned signal state, modern `@if`/`@for`/`@switch`, auth/bootstrap, editor schema assistance, and failure states.
- **Playwright source typecheck**: `pnpm --filter isched-ui exec tsc --noEmit -p e2e/tsconfig.json`.
- **GraphQL boundary**: `pnpm --filter isched-ui run test:graphql-boundary`; retain network assertions that all feature requests use `/graphql` only.
- **Focused browser tests**: `python3 tools/run_e2e.py -- e2e/dev-proxy.spec.ts`, plus the Admin, Playground, and cookie/CSRF/Origin acceptance suites from `src/ui`.
- **Backend tests**: `cmake --build cmake-build-debug -j2` followed by `ctest --test-dir cmake-build-debug --output-on-failure`, including auth/session, GraphQL HTTP, WebSocket handshake/subscription authorization, allowed-Origin, CSRF, login bootstrap, and no-side-effect rejection cases.
- **Security checks**: inspect HTTP and WebSocket traffic and browser storage; assert no JWT in app-managed memory, localStorage, sessionStorage, IndexedDB, Authorization headers, URLs, or `connection_init`. Assert the cookie attributes and no wildcard credentialed CORS behavior.
- **Threat models**: review `specs/013-lsp-graphql-angular/threat-model.md` and the corresponding summary in `docs/security-threat-model.md`; distinguish verified controls from residual risks.

## Reproducible Performance Protocol (SC-001)

Run the Playground and administration journeys as two separate benchmark runs. For each run:

1. Start **10 concurrent users** at a synchronized barrier against the same documented local/test environment and seeded fixture state. Record browser/runtime, backend build, UI build, machine, proxy configuration, and fixture revision.
2. Each user performs **10 warmup operations, unmeasured**, for that journey; then performs **100 measured operations**. Warmups do not appear in the sample list, latency statistics, or denominator.
3. Freeze and version a deterministic 100-operation manifest and its operation-class counts for each journey. It must exercise the journey's required operation types and use repeatable seeded/resettable data; reuse the same manifest for every user and run. For each user's unmeasured warmup, replay the manifest's first 10 operations; then replay the full 100-operation manifest as measured work. This fixes the warmup sequence and ensures warmup exercises the same operation classes without adding warmups to the measured denominator. Report the manifest with results.
4. Start a monotonic timer immediately before dispatching each operation on user submission. Stop only after the expected final result is visible in the UI, submitting/loading state has settled, and an Angular render frame has occurred. This includes client processing, proxy/network/server response, and UI rendering; excludes setup, login, schema loading, fixture seeding, think time, and warmups.
5. Keep exactly **1,000 measured operations** in each journey's denominator (10 users × 100). A failed operation or timeout is a failed benchmark run; do not omit it or count only successes. Compute nearest-rank p95 over the 1,000 durations (rank `ceil(0.95 × 1000) = 950`). The p95 must be **strictly below 2,000 ms** for Playground and separately for Admin. Never combine both journeys into one percentile.
6. Preserve per-user completion counts, operation-class counts, failed-operation count, all durations, aggregate journey p95, and per-class p95 as diagnostic artifacts. The aggregate journey p95 is the acceptance metric; reporting class p95 prevents a workload mix from hiding a slow class.

## Implementation Verification Record (2026-11-09)

The following gates were run against the implementation on branch `013-lsp-graphql-angular` using isolated Playwright test data:

- `pnpm --filter isched-ui run test:ci`: **passed, 33 suites / 238 tests**.
- `pnpm --filter isched-ui run build`: **passed** with strict TypeScript and Angular template checking. Angular emitted a non-fatal CommonJS optimization warning for transitive `nullthrows`.
- `pnpm --filter isched-ui exec tsc --noEmit -p e2e/tsconfig.json`: **passed**.
- `cmake --build cmake-build-debug -j2`: **passed**, including verification that the embedded UI asset header was current.
- `ctest --test-dir cmake-build-debug --output-on-failure`: **passed, 58/58 tests**.
- Browser Admin, cookie/Origin/CSRF security, and Playground acceptance: **9 passed, 1 skipped**. The subscription-advisory case was skipped because the test schema exposes no subscription field; the separate authenticated proxy WebSocket handshake test passed.
- Browser proxy acceptance: **2/2 passed** (HTTP GraphQL request and cookie-authenticated `graphql-transport-ws` upgrade/ack; empty credential-free `connection_init`).
- Browser security acceptance verified Secure/HttpOnly/SameSite=Strict cookie attributes, browser-managed cookie attachment, allowed and rejected HTTP Origins, pre-session bootstrap/login response shape, CSRF rejection before organization-creation side effects, CSRF-protected mutation success, no Authorization header, no URL token, and no JWT-like browser storage entry.
- Independent Playground performance run: **1,000/1,000 measured operations**, 10 users, 10 warmups/user, zero warmup/measured failures. Manifest classes: 900 `query-system-state` and 100 `mutation-echo`. Aggregate nearest-rank p95: **450.39 ms**; per-class p95: query **451.47 ms**, mutation **425.56 ms**.
- Independent Admin performance run: **1,000/1,000 measured operations**, 10 users, 10 warmups/user, zero warmup/measured failures. Manifest classes: 250 `organization-list`, 250 `organization-user-list`, and 500 `user-status-update`. Aggregate nearest-rank p95: **629.33 ms**; per-class p95: organization list **406.58 ms**, user list **747.99 ms**, status update **463.52 ms**.
- Performance reports retain the exact 1,000 samples and per-class counts/latencies as Playwright test attachments; each benchmark fails on any timeout/error and computes rank-950 over its full denominator.

## Constitution-Compliance Review (2026-11-09)

Review completed against the frontend requirements in `spec.md` and the implementation evidence above. The Playground and Admin remain separate required P1 journeys; both passed browser acceptance and their independent performance gates. Browser app-owned state is signal-backed, changed UI components remain standalone and use modern Angular control flow, user edit flows use typed reactive forms, and strict TypeScript/template compilation passed. The static and browser transport checks enforce the `/graphql`-only boundary. Cookie-only authentication, exact Origin validation, session-bound CSRF, the secure-cookie local proxy route, and credential-free WebSocket initialization were exercised by backend integration, browser security, and proxy tests. No constitution exception was identified and no token-policy downgrade or database migration was introduced. Residual risks remain those documented in the reviewed threat models: same-origin XSS can act within an active browser session, and deployment-specific Origin allowlists and performance environments require operational review.

### Backend C++ and Clean Code Review — C1 Follow-up

**Scope**: backend changes from planning commit `517b5d2` through implementation commit `f28187d`, plus the post-analysis refactor. Reviewed `isched_AuthenticationMiddleware.cpp/.hpp`, `isched_Server.cpp/.hpp`, `isched_GqlExecutor.cpp/.hpp`, `isched_DatabaseManager.cpp`, `isched_srv_main.cpp`, `isched_config.hpp`, embedded SDL, and its CMake dependency tracking. This is a feature-diff review, not a claim that all pre-existing repository code meets every guideline.

- **Ownership and RAII**: session/database objects use existing smart-pointer ownership; socket/timer ownership remains with the session; async callbacks retain the session or use weak references. Executor/broker/auth pointers are non-owning and their enclosing server lifetime/teardown ordering remains unchanged. AST helper pointers only borrow nodes for the current execution. The thread-local resolver context is cleared by its scope guard, including exceptional exits. No raw owning allocation, new service, or migration was introduced.
- **Synchronization**: every new `csrf_sessions_` insertion, lookup/expiry erase, and session/user/tenant invalidation takes an exclusive RAII lock on `sessions_mutex_`. Lookup returns an owned copy before releasing the lock; constant-time comparison uses that copy. Database calls and broker callbacks run outside that lock. WebSocket context, expiry timer, and closing state are serialized by the session strand; cross-thread revocation/delivery callbacks post to it. Bootstrap rate limiting uses the existing mutex-protected `RateLimiter`. Revocation prevents subsequent validation, not rollback of already admitted in-flight operations; process restart requires reauthentication because CSRF state is not persisted.
- **Focused responsibilities**: the review found duplicate HTTP/WS cookie parsing and nested pre-session CSRF inspection. Both transports now use `parse_session_cookie` and `validate_session_cookie`; AST child lookup, single-field selection, and pre-session eligibility use focused helpers with early rejection. Existing selected-operation, alias, fragment, and role/tenant semantics are retained. No extra polymorphic layer or heap allocation was added to these helpers.
- **Error handling and boundaries**: session persistence/random-generation failures now log fixed diagnostics before throwing, without credentials. Bootstrap configuration parsing catches only its expected conversion failures and logs the existing default behavior. HTTP transport metadata remains outside GraphQL JSON; supplied invalid cookies do not authenticate. The retained non-HTTP `execute_graphql` compatibility path is not a browser bearer transport.
- **Authorization ordering**: executor role/tenant checks precede resolver invocation. `WsSession::handle_subscribe` returns on execution failure before topic registration. Anonymous public subscriptions are intentional, not an exception to protected-subscription authentication.

**Verification after refactor**: `cmake --build cmake-build-debug -j2` passed; the five focused HTTP-security/WS/session suites passed **5/5**; full `ctest --test-dir cmake-build-debug --output-on-failure` passed **58/58**. Strengthened tests verify both listeners: public `healthChanged` returns `UP` with no errors and increments/removes broker registration; anonymous protected `serverMetricsUpdated` returns a null protected result with errors and leaves registration count unchanged. HTTP tests cover tab-delimited cookies and rejection/clearing of a malformed duplicate session cookie, in addition to parsed-operation/alias/fragment and session-bound CSRF regressions. Earlier browser/performance results above remain historical evidence for the same feature; those browser runs were not repeated for this helper-only backend refactor.

**Outcome**: C1 review evidence and synchronization documentation are complete; A1 policy is explicit and regression-verified; I1 specification/plan status is aligned to Completed. No unresolved issue was identified in the changed feature scope that blocks T070.
