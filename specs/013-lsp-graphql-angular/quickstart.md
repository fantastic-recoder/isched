# Quickstart: LSP GraphQL Angular Integration

This is the planned verification procedure, not a record that implementation or tests have already passed. The plan is planning-only and introduces no database migration.

## Prerequisites

- Backend configured with the repository's C++23, Conan, CMake/Ninja workflow (`python3 configure.py`) and exposing the existing `/graphql` HTTP and WebSocket endpoint.
- UI dependencies installed from the repository workspace (`pnpm install` in `src/ui`). Implementation must declare GraphQL.js directly for schema construction; directly imported GraphQL Language Service APIs must not rely on an undeclared transitive dependency.
- Configure an explicit exact-match allowlist containing the browser UI origin(s). Wildcard origin is invalid. The Angular development origin must be explicitly configured when using the proxy.
- Use Secure, HttpOnly, SameSite=Strict cookie authentication. Do not disable Secure, add an Authorization bearer fallback, or place JWTs in `localStorage`, `sessionStorage`, IndexedDB, or application memory.
- No schema migration is required by this plan; use existing session/revocation support.

## Local Run and Manual Acceptance

1. Start the backend with `/graphql` available over HTTP and WebSocket. Confirm configured allowed origins include the UI origin and reject unlisted origins.
2. Start Angular using the existing proxy: `cd src/ui && pnpm start`. Confirm `src/ui/proxy.conf.json` forwards `/graphql` and enables WebSocket upgrades.
3. Use a fresh browser session and sign in through the GraphQL login operation or complete first-run `bootstrapPlatformAdmin`. Verify each successful response sets a Secure, HttpOnly, SameSite=Strict cookie. Each GraphQL `AuthPayload` supplies the resulting session-bound CSRF value; Angular retains it only in memory. Confirm neither response exposes a JWT to JavaScript. Both operations are exempt from a CSRF header only before a valid session exists, and remain Origin-validated and rate-limited.
4. Reload the page. Verify the read-only `currentSessionCsrfToken` GraphQL query field authenticates by cookie and returns/reacquires a CSRF value. This query has no mutation-CSRF prerequisite, so startup is not circular.
5. In Playground, load the active schema through GraphQL introspection and verify schema-aware completion, hover descriptions, and live syntax/type diagnostics. Verify unavailable-schema feedback if introspection fails. Execute a valid query and mutation, and verify visible results/errors and retry behavior.
6. In Admin, list and manage an organization and its users through GraphQL; verify typed-form validation, authorization/conflict/transport feedback, and final visible state.
7. Verify an authenticated HTTP GraphQL mutation includes the current `X-CSRF-Token`; missing/stale/cross-session values are rejected before resolver side effects. Verify login/first-run bootstrap is exempt only when no valid session is present and still requires allowed Origin and existing rate limiting. Logout is authenticated and requires CSRF.
8. Verify every HTTP request to `/graphql` and every WS handshake rejects a missing, `null`, malformed, or unlisted Origin. Verify WebSocket uses the browser cookie and no JWT in `connection_init`. A failed subscription must not break ordinary HTTP query/mutation flow.

## Automated Verification

- **Strict UI compilation**: `cd src/ui && pnpm build` (strict TypeScript and Angular template checks must remain enabled).
- **Frontend tests**: `cd src/ui && pnpm test`; include typed forms, standalone usage, app-owned signal state, modern template control flow, auth/bootstrap, editor schema assistance, and failure states.
- **GraphQL boundary**: `cd src/ui && pnpm test:graphql-boundary`; retain network assertions that all feature requests use `/graphql` only.
- **Focused browser tests**: `cd src/ui && pnpm e2e:dev-proxy`, `pnpm e2e:playground`, plus organization/user administration and cookie/CSRF/origin scenarios.
- **Backend tests**: build/run the relevant Catch2/CTest targets from `cmake-build-debug`, including auth/session, GraphQL HTTP, WebSocket handshake/subscription authorization, allowed-Origin, CSRF, login bootstrap, and no-side-effect rejection cases.
- **Security checks**: inspect HTTP and WebSocket traffic and browser storage; assert no JWT in app-managed memory, localStorage, sessionStorage, IndexedDB, Authorization headers, URLs, or `connection_init`. Assert the cookie attributes and no wildcard credentialed CORS behavior.
- **Threat models**: update and review `specs/013-lsp-graphql-angular/threat-model.md` and the corresponding summary in `docs/security-threat-model.md`; describe intended controls separately from verified implementation evidence.

## Reproducible Performance Protocol (SC-001)

Run the Playground and administration journeys as two separate benchmark runs. For each run:

1. Start **10 concurrent users** at a synchronized barrier against the same documented local/test environment and seeded fixture state. Record browser/runtime, backend build, UI build, machine, proxy configuration, and fixture revision.
2. Each user performs **10 warmup operations, unmeasured**, for that journey; then performs **100 measured operations**. Warmups do not appear in the sample list, latency statistics, or denominator.
3. Freeze and version a deterministic 100-operation manifest and its operation-class counts for each journey. It must exercise the journey's required operation types and use repeatable seeded/resettable data; reuse the same manifest for every user and run. For each user's unmeasured warmup, replay the manifest's first 10 operations; then replay the full 100-operation manifest as measured work. This fixes the warmup sequence and ensures warmup exercises the same operation classes without adding warmups to the measured denominator. Report the manifest with results.
4. Start a monotonic timer immediately before dispatching each operation on user submission. Stop only after the expected final result is visible in the UI, submitting/loading state has settled, and an Angular render frame has occurred. This includes client processing, proxy/network/server response, and UI rendering; excludes setup, login, schema loading, fixture seeding, think time, and warmups.
5. Keep exactly **1,000 measured operations** in each journey's denominator (10 users × 100). A failed operation or timeout is a failed benchmark run; do not omit it or count only successes. Compute nearest-rank p95 over the 1,000 durations (rank `ceil(0.95 × 1000) = 950`). The p95 must be **strictly below 2,000 ms** for Playground and separately for Admin. Never combine both journeys into one percentile.
6. Preserve per-user completion counts, operation-class counts, failed-operation count, all durations, aggregate journey p95, and per-class p95 as diagnostic artifacts. The aggregate journey p95 is the acceptance metric; reporting class p95 prevents a workload mix from hiding a slow class.

## Completion Gates

- Both P1 journeys meet their independent functional tests; Playground language capabilities and any subscription sub-scenario pass.
- Allowed-Origin validation covers all GraphQL HTTP requests and WebSocket handshakes; authenticated HTTP mutations require session-bound CSRF. Login/first-run bootstrap creates a session and CSRF value from the pre-session path, remains origin/rate-limit checked, and existing-session token reacquisition is non-circular.
- Cookie attributes and no-bearer/no-persistence checks pass; cookie session works for HTTP and WebSocket.
- The two separate 10-user performance runs meet the strict p95 threshold using the denominator and timing boundaries above.
- Feature and project threat-model updates are reviewed.
- A documented constitution-compliance review is complete. Pending or failed review blocks journey and feature closeout.

## Historical Validation Record (Earlier Scope Only)

The pre-existing T039 record documented checks for the earlier GraphQL/Playground/proxy scope, including a sequential 10-run Playground latency check. Preserve that as historical evidence only. It does **not** establish completion of the newly required cookie-only auth behavior, schema-aware hover/completion/diagnostics, the organization/user Admin journey, the 10-concurrent-user × 100-measured-operation run for each journey, the 10-operation unmeasured warmup, exact Origin/session-bound CSRF checks, or the constitution-compliance review. Those checks must be implemented and recorded in the later implementation workflow.

## Current Implementation Checkpoint (Partial Verification)

- `cd src/ui && pnpm test:ci`: passed, 33 suites and 229 tests.
- `cd src/ui && pnpm build`: passed with strict TypeScript and Angular template compilation enabled. Angular reported a CommonJS optimization warning for the transitive `nullthrows` dependency; it did not fail the build.
- `cmake --build cmake-build-debug -j2`: passed after exact-Origin test clients were updated to send their configured UI origin.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 57/57 tests. The 12 Origin-sensitive regressions also passed as a targeted set; the security-foundation expectation was updated to check the fail-closed Origin-validation message.
- **Not a feature-closeout record**: cookie-only session issuance/validation, session-bound CSRF lifecycle, cookie-authenticated WS, Admin E2E, the two required UI-level 1,000-sample performance runs, threat-model review against implementation evidence, and constitution closeout remain unverified/incomplete.
