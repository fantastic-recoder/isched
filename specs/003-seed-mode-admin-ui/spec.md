# Feature Specification: Seed Mode and Administration UI

**Feature Branch**: `003-seed-mode-admin-ui`
**Created**: 2026-03-15
**Updated**: 2026-03-15
**Status**: Draft
**Input**: User story — "As a developer I want to check initial Isched functionality. I start `isched_srv`, get the configured host and port on stdout, open `/isched`, and find a responsive Angular UI with a login form. When no users exist the server enters seed mode and lets me create the first platform administrator; after that it switches to normal mode."

---

## Clarifications

**Q1**: Where does the Angular app run relative to the server binary?
**A1**: The compiled Angular distribution (`dist/`) is embedded into the C++ binary at build time using CMake's `xxd`-based binary-to-header trick (or `cmake_embed`). The server extracts and serves the files in-memory from the `/isched` route prefix. No separate Node.js process is required at runtime.

**Q2**: Does the Angular app communicate through the existing `/graphql` endpoint?
**A2**: Yes. All data access — login, seed-mode check, user creation, RBAC reads and writes — goes through `POST /graphql`. The Angular app is a pure GraphQL client. No new REST or management endpoints are added.

**Q3**: How does the server signal that seed mode is active?
**A3**: A new built-in GraphQL query field `systemState { seedModeActive: Boolean! }` is added to the existing `Query` type. It returns `true` when the `platform_admins` table in `isched_system.db` contains no active records. The Angular app polls (or queries on load) this field and renders the seed form instead of the login form when `seedModeActive` is `true`.

**Q4**: What Angular version and UI library?
**A4**: Angular **21.0.1** (latest stable as of 2026-03-15). UI layer is **DaisyUI 4.x** (Tailwind CSS component library). The app is built with the Angular CLI using the standalone component API (no NgModules). Tailwind CSS 3.x is configured via PostCSS in the Angular build.

**Q5**: What is the complete list of screens in scope?
**A5**: Three screens for this story:
  1. **Seed screen** — shown when `seedModeActive = true`; form to create the first platform administrator (email + password + password-confirm).
  2. **Login screen** — shown when `seedModeActive = false`; email + password form with a "Sign in" button.
  3. **Dashboard screen** — shown after successful login; displays server health/version and a minimal RBAC panel (list users, list organizations, create user, create organization — all via existing GraphQL mutations from story 001).

**Q6**: How is the Angular build integrated with the C++ CMake build?
**A6**: A CMake custom target `isched_ui_build` runs `pnpm --filter isched-ui run build`. The build output (`src/ui/dist/isched-ui/browser/`) is converted by a second CMake target `isched_ui_embed` that runs `xxd -i` on each file and generates `src/main/cpp/isched/backend/isched_ui_assets.hpp`. The `isched` library target depends on `isched_ui_embed`. The Angular subproject lives at `src/ui/`.

**Q7**: How does the server serve the Angular app?
**A7**: The `Server` class registers a GET catch-all handler under the `/isched` prefix. Requests for `/isched` or `/isched/` serve `index.html`; requests for `/isched/<asset>` are matched against the in-memory embedded file map and returned with correct MIME types. A 404 response with `{ "errors": [{ "message": "asset not found" }] }` is returned for unknown paths, preserving the GraphQL error shape convention.

**Q8**: What happens to the `/isched` route on startup stdout output?
**A8**: The existing startup log line is extended:
```
GraphQL endpoint:   http://0.0.0.0:8080/graphql
Admin UI:           http://0.0.0.0:8080/isched
```

**Q9**: What security constraints apply to the seed endpoint?
**A9**: The seed mutation `createPlatformAdmin` (already present in the schema from T047) may only be called when `systemState.seedModeActive` is `true`. Attempting to call it when seed mode is inactive returns a `FORBIDDEN` GraphQL error. This is enforced at the resolver level, not at the HTTP layer.

**Q10**: Is the Angular app included in the automated test suite?
**A10**: Unit tests for the Angular components use the Angular testing framework (Karma + Jasmine or Jest via `@angular-builders/jest`). Integration smoke-tests (`src/test/cpp/integration/test_admin_ui.cpp`) use `cpp-httplib` **as an HTTP client** (test-only dependency — the server uses Boost.Beast exclusively) to assert that:
- `GET /isched` returns HTTP 200 with `Content-Type: text/html`
- `GET /isched/main.js` or equivalent bundle chunk returns HTTP 200
- `GET /isched/nonexistent.xyz` returns HTTP 404

**Q11**: Which fields does the RBAC panel expose in this story?
**A11**: Read-only panel listing organizations (`organizations { id name subscriptionTier }`) and their users (`users { id email roles isActive }`). Create-user and create-organization forms are included as modal dialogs in the dashboard. Role assignment uses a multi-select tied to the existing `createRole` and `updateUser` mutations.

**Q12**: Should the Angular app be a standalone SPA or use Angular routing?
**A12**: Standalone SPA with Angular Router. Three routes: `/isched/seed`, `/isched/login`, `/isched/dashboard`. A route guard redirects from `/isched` to `/isched/seed` if `seedModeActive` is `true`, else to `/isched/login`. After login the guard redirects to `/isched/dashboard`. The server always serves `index.html` for any `/isched/**` path (HTML5 history API mode).

**Q13**: Which HTTP transport library does the server use to serve the Angular assets?
**A13**: The server uses **Boost.Beast exclusively** for all HTTP transport, including static asset serving under `/isched`. `cpp-httplib` is a **test-only client dependency** used in C++ integration tests to make HTTP requests against the running server. `cpp-httplib` is never linked into the `isched` library or binary.

**Q14**: What does `createPlatformAdmin` return, and what happens in the UI after success?
**A14**: `createPlatformAdmin(email: String!, password: String!): User!` — it creates the record and returns the new `User` object (id, email, displayName, roles). It does **not** issue a JWT. After a successful call the Angular app navigates to `/isched/login` and the admin logs in with the newly created credentials via the normal `login` mutation.

**Q15**: How are password fields and server errors handled in the seed and login forms?
**A15**: All password inputs MUST include a show/hide toggle button (visibility icon); the default state is masked (`type="password"`). Server-side GraphQL errors returned from `createPlatformAdmin` or `login` MUST be displayed as an inline DaisyUI `alert alert-error` banner at the top of the form. Client-side validation errors (mismatched passwords, minimum length) MUST be shown as per-field `label` text beneath the relevant input. The form submit button is disabled while a mutation is in flight.

**Q16**: Should `remote_ip` be added to `ResolverCtx` and populated from the Boost.Beast connection?
**A16**: Yes. A `remote_ip: std::string` field is added to `ResolverCtx` and populated on every request from the Boost.Beast socket endpoint in `Server::Impl`. This field is available to all resolvers and is used by the `createPlatformAdmin` rate-limiter; it also benefits future audit-logging and per-IP throttling of other mutations.

**Q17**: Is the per-IP rate-limiter on `createPlatformAdmin` (SR-UI-001) in scope for this story, and how is the limit configured?
**A17**: Yes, in scope. The limit is resolved using the same three-tier priority chain as other server settings:
1. **Env var** `ISCHED_SEED_RATE_LIMIT` (highest priority — overrides everything)
2. **`ConfigManager` key** `server.seed_rate_limit` (set via JSON config file or `ConfigManager::set()`)
3. **Hard-coded default** of `5` calls per minute per IP

The rate-limiter is implemented inside the `createPlatformAdmin` resolver using an in-process `std::unordered_map<std::string, TokenBucket>` keyed on `ResolverCtx::remote_ip`. The `TokenBucket` refill rate is derived from the resolved limit value at resolver construction time. When the bucket for an IP is exhausted the resolver returns a GraphQL `RATE_LIMITED` error without touching the database.

**Q18**: Where does the Angular subproject live in the repository?
**A18**: `src/ui/` — alongside the C++ sources it is compiled into. The CMake build references it at `${CMAKE_SOURCE_DIR}/src/ui/`. The pnpm workspace entry is `src/ui` in `pnpm-workspace.yaml`.

**Q19**: Which test runner is used for Angular component tests?
**A19**: **Jest** via `@angular-builders/jest`. No browser or launcher is required; tests run headless in Node.js. `@angular-builders/jest` replaces the default Karma builder in `angular.json`. The `test` script in `src/ui/package.json` runs `ng test --watch=false`.

**Q20**: Where is the 12-character password minimum enforced?
**A20**: **Both** client and server. Angular reactive form validators provide immediate per-field feedback in the seed and login forms. The `createPlatformAdmin`, `createUser`, and `updateUser` resolvers also reject passwords shorter than 12 characters server-side, returning a GraphQL `VALIDATION_ERROR` with message `"password must be at least 12 characters"`. This ensures the constraint is enforced regardless of how the API is called.

**Q21**: What is the HTTP 404 response body for unknown `/isched/**` paths?
**A21**: JSON — `{ "errors": [{ "message": "asset not found" }] }` with `Content-Type: application/json`. This is consistent with the GraphQL error envelope used everywhere else in the server and makes automated assertions straightforward.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Static Admin UI Reachable After Server Start (Priority: P1)

A developer starts `isched_srv`, sees the admin UI URL printed to stdout, opens the URL in a browser, and sees a styled, responsive Angular application.

**Why this priority**: First developer experience. If the UI is not immediately available without extra setup steps, the "start and use" promise is not kept.

**Independent Test**: Start the server binary, `GET /isched`, assert HTTP 200 and `Content-Type: text/html; charset=utf-8`, assert the response body contains the Angular bootstrap tag (`<app-root>`).

**Acceptance Scenarios**:

1. **Given** `isched_srv` is started with default configuration,
   **When** the process emits the startup log,
   **Then** the log contains a line `Admin UI: http://<host>:<port>/isched`

2. **Given** a running server,
   **When** an HTTP GET request is sent to `/isched`,
   **Then** the response is HTTP 200 with `Content-Type: text/html` and a body containing `<app-root>`

3. **Given** a running server,
   **When** an HTTP GET request is sent to `/isched/main.js` (or whatever the Angular build names its main chunk),
   **Then** the response is HTTP 200 with `Content-Type: application/javascript`

4. **Given** a running server,
   **When** an HTTP GET request is sent to `/isched/does-not-exist.xyz`,
   **Then** the response is HTTP 404

5. **Given** a running server,
   **When** an HTTP GET request is sent to `/isched/some/deep/route`,
   **Then** the server returns `index.html` (HTML5 push-state fallback)

---

### User Story 2 — Seed Mode: Create First Platform Administrator (Priority: P1)

When no platform administrator exists in `isched_system.db`, the Angular app shows a seed form instead of the login form. Submitting a valid email and password pair creates the first administrator and transitions the app to the login screen.

**Why this priority**: Without seeding, a new installation is inaccessible — there is no way to authenticate and no way to perform any administrative action. Seed mode is the zero-to-operational path.

**Independent Test**: Start the server with an empty (or absent) `isched_system.db`. Query `{ systemState { seedModeActive } }`. Assert `true`. Call `mutation { createPlatformAdmin(email:"admin@example.com", password:"ValidP@ss1") { token } }`. Assert a JWT token is returned. Query `{ systemState { seedModeActive } }` again. Assert `false`.

**Acceptance Scenarios**:

1. **Given** `isched_system.db` has no active platform administrators,
   **When** the Angular app loads at `/isched`,
   **Then** the router redirects to `/isched/seed` and the seed form is displayed

2. **Given** the seed screen is displayed,
   **When** the developer submits a valid email, password, and matching password-confirm,
   **Then** `mutation { createPlatformAdmin(email: ..., password: ...) { id email } }` is called, the new `User` is returned, and the router navigates to `/isched/login`

2b. **Given** the seed screen is displayed,
    **When** the developer clicks the password-visibility toggle,
    **Then** the password field switches between `type="password"` (masked) and `type="text"` (visible); the default state is masked

2c. **Given** the seed form is submitted and the server returns a GraphQL error,
    **Then** a DaisyUI `alert alert-error` banner appears at the top of the form showing the error message; the form remains editable

3. **Given** the seed screen is displayed,
   **When** the developer submits mismatched passwords,
   **Then** a DaisyUI inline validation alert is shown and the mutation is not called

4. **Given** the seed screen is displayed,
   **When** the developer submits a password shorter than 12 characters,
   **Then** a DaisyUI inline validation alert is shown and the mutation is not called

5. **Given** seed mode is inactive (at least one active platform admin exists),
   **When** `mutation { createPlatformAdmin(...) }` is called,
   **Then** the server returns a GraphQL `FORBIDDEN` error

6. **Given** a platform admin was just created via the seed form,
   **When** the developer refreshes the page,
   **Then** the app shows the login screen (not the seed screen)

---

### User Story 3 — Login and Dashboard with RBAC Panel (Priority: P2)

After seed mode has been completed (or on any subsequent visit), the developer can log in with the platform administrator credentials and see a dashboard that shows server health and a minimal RBAC management panel.

**Why this priority**: The login/dashboard flow completes the end-to-end developer experience and exercises the RBAC mutations already implemented in story 001, making them accessible without writing GraphQL queries by hand.

**Independent Test**: With a seeded admin account, call `mutation { login(email:"admin@example.com", password:"ValidP@ss1") { token expiresAt } }`. Assert a JWT is returned. Use that token in a subsequent `Authorization: Bearer <token>` header; query `{ organizations { id name } }`. Assert a 200 response with an `organizations` array.

**Acceptance Scenarios**:

1. **Given** seed mode is inactive,
   **When** the Angular app loads at `/isched`,
   **Then** the router displays the login screen at `/isched/login`

2. **Given** the login screen is displayed,
   **When** the developer submits valid credentials,
   **Then** `mutation { login(...) { token expiresAt } }` is called, the JWT is stored in `sessionStorage`, and the router navigates to `/isched/dashboard`

3. **Given** the login screen is displayed,
   **When** the developer submits wrong credentials,
   **Then** the server returns a GraphQL error and a DaisyUI alert banner shows "Invalid credentials"

4. **Given** the dashboard is displayed,
   **When** the page loads,
   **Then** the server health badge shows the live `status` from `{ health { status } }` and the server version from `{ version }`

5. **Given** the dashboard is displayed,
   **When** the RBAC panel loads,
   **Then** the organizations table is populated from `{ organizations { id name subscriptionTier } }`

6. **Given** the dashboard is displayed,
   **When** the developer clicks "Create Organization",
   **Then** a DaisyUI modal opens with the create-organization form; on submit, `mutation { createOrganization(...) { id name } }` is called and the table refreshes

7. **Given** the dashboard is displayed,
   **When** the developer clicks "Create User" within an organization row,
   **Then** a DaisyUI modal opens; on submit, `mutation { createUser(...) { id email } }` is called and the user list for that organization refreshes

8. **Given** a logged-in session token has expired,
   **When** the Angular app makes any GraphQL request,
   **Then** the server returns a `UNAUTHENTICATED` error and the app redirects to `/isched/login`

9. **Given** the developer clicks "Sign out",
   **When** the action is confirmed,
   **Then** `mutation { logout }` is called, `sessionStorage` is cleared, and the router navigates to `/isched/login`

---

### Edge Cases

- When the server is started and `isched_system.db` does not yet exist, seed mode MUST be active immediately on first request — the database is created on startup but the `platform_admins` table starts empty.
- When the Angular asset `index.html` is requested during seed mode check and the GraphQL request for `systemState` is still in flight, the UI MUST show a loading indicator rather than briefly flashing the wrong screen.
- When the embedded Angular assets are corrupted or missing at compile time (build failed before embedding), the server MUST start but return HTTP 503 with a plain text body "Admin UI assets unavailable" for any `/isched/**` request, rather than crashing.
- When the browser sends an `If-None-Match` request with the correct ETag for a cached asset, the server MUST return HTTP 304 to support browser caching of the Angular bundle.

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-UI-001**: The server MUST serve the compiled Angular application at the `/isched` path prefix, reachable immediately after `isched_srv` starts.
- **FR-UI-002**: The startup log MUST include an `Admin UI:` line with the full reachable URL of the UI.
- **FR-UI-003**: The server MUST expose a `systemState { seedModeActive: Boolean! }` field in the built-in GraphQL schema.
- **FR-UI-004**: The `createPlatformAdmin` mutation MUST return `FORBIDDEN` when `seedModeActive` is `false`.
- **FR-UI-004a**: `createPlatformAdmin(email: String!, password: String!): User!` — returns the created `User` record; does NOT issue a JWT.
- **FR-UI-016**: All password input fields in the UI MUST include a show/hide visibility toggle; the default state is masked.
- **FR-UI-017**: GraphQL errors returned by any mutation MUST be displayed as an inline `alert alert-error` DaisyUI banner on the form that triggered the mutation. Client-side validation errors MUST be shown as per-field messages beneath the relevant input.
- **FR-UI-005**: The Angular app MUST route to a seed form when `seedModeActive` is `true` and to a login form when it is `false`.
- **FR-UI-006**: The Angular app MUST use Angular 21.0.1 with standalone components and Angular Router.
- **FR-UI-007**: The UI MUST use DaisyUI 4.x over Tailwind CSS 3.x for all component styling.
- **FR-UI-008**: The UI MUST be responsive and usable on viewport widths from 320 px (mobile) to 1920 px (desktop).
- **FR-UI-009**: All Angular app data access MUST go through `POST /graphql` with bearer-token authentication (where required); no additional HTTP routes are introduced.
- **FR-UI-010**: The Angular subproject MUST live at `src/ui/` and MUST be integrated into the `pnpm` workspace.
- **FR-UI-011**: The CMake build MUST include a target `isched_ui_build` that compiles the Angular app and a target `isched_ui_embed` that generates the in-memory asset header; the main `isched` library target MUST depend on `isched_ui_embed`.
- **FR-UI-012**: The server MUST support ETag-based HTTP conditional requests (`If-None-Match` / `304 Not Modified`) for Angular static assets.
- **FR-UI-013**: The server MUST return the Angular `index.html` for any `/isched/**` path that does not match a known static asset (HTML5 push-state fallback).

### Non-Functional Requirements

- **NFR-UI-001**: Angular production build MUST complete in under 60 seconds on a modern developer workstation (Ryzen 7 / i5 class).
- **NFR-UI-002**: The total size of the embedded Angular bundle (all assets, gzip-compressed) MUST NOT exceed 500 KB, enforced by a CMake build-time size check.
- **NFR-UI-003**: Any `/isched/**` static asset request MUST be served within 5 ms from the in-memory map (no filesystem I/O at serve time).
- **NFR-UI-004**: The addition of the embedded UI assets MUST NOT increase `isched_srv` binary size by more than 5 MB compared to the pre-UI baseline.
- **NFR-UI-005**: The existing `ctest` suite MUST remain 100 % green after UI integration.

### Security Requirements

- **SR-UI-001**: The `createPlatformAdmin` mutation endpoint MUST be rate-limited per source IP. The limit is configurable via env var `ISCHED_SEED_RATE_LIMIT` (highest priority), `ConfigManager` key `server.seed_rate_limit`, or hard-coded default of 5 calls per minute. Exhausted buckets return a GraphQL `RATE_LIMITED` error without touching the database.
- **SR-UI-002**: Passwords submitted through the seed or login form are transmitted only over the existing `/graphql` POST path; the Angular app MUST NOT persist passwords in any browser storage.
- **SR-UI-003**: The JWT stored in `sessionStorage` MUST be cleared on logout and on tab/browser close (sessionStorage semantics are sufficient; localStorage MUST NOT be used for tokens).
- **SR-UI-004**: The Angular app MUST include a `Content-Security-Policy` meta tag allowing only `'self'` script and style sources; no inline scripts.
- **SR-UI-005**: Static asset responses MUST include `X-Content-Type-Options: nosniff` and `X-Frame-Options: DENY` headers.

---

## Out of Scope for This Story

- Server-side rendering (SSR) of the Angular application.
- Dark/light theme toggle (DaisyUI default theme is acceptable).
- Angular unit test CI integration (component tests are required locally; CI wiring is a follow-up).
- Tenant-scoped UI screens beyond the platform-admin RBAC panel.
- Real-time subscriptions in the dashboard (health badge is polled; subscription-driven live updates are a follow-up).
- Any form of OAuth2 / SSO / external identity provider in the login flow.
