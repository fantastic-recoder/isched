# Tasks: Seed Mode and Administration UI

**Status**: 🔲 OPEN
**Input**: Design documents from `/specs/003-seed-mode-admin-ui/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`
**Tests**: TDD — test tasks precede or accompany every implementation task.
**Organization**: Tasks are grouped by phase matching `plan.md`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel with other [P] tasks in the same phase
- **[Story]**: User story association (`US1`, `US2`, `US3`)
- Exact file paths are included where practical

## Path Conventions

- **C++ implementation**: `src/main/cpp/isched/`
- **C++ tests**: `src/test/cpp/`
- **Angular subproject**: `src/ui/`
- **Spec and planning**: `specs/003-seed-mode-admin-ui/`

## Constitutional Compliance Checklist

Each task implementation MUST verify:

- ✅ **Tests pass**: `cd cmake-build-debug && ctest --output-on-failure` MUST exit 0 after every C++ task is marked done. This is a hard gate.
- ✅ **Angular tests pass**: `pnpm --filter isched-ui run test` MUST exit 0 after every Angular task is marked done.
- ✅ **GraphQL-only transport**: No new REST or management endpoints. All data goes through `/graphql`.
- ✅ **Boost.Beast only in server binary**: `cpp-httplib` is a test-client dependency only; never linked into `isched` library or binary.
- ✅ **Security**: `createPlatformAdmin` gated, rate-limited, password hashed. No plaintext passwords stored anywhere.
- ✅ **C++ Core Guidelines**: Smart-pointer ownership, RAII, no raw `new`/`delete`.
- ✅ **Portability**: Linux/Conan compatibility maintained; `xxd` fallback Python script when tool is absent.

> **Rule**: If a task causes an existing test to fail, the task MUST fix the broken test in the same change or revert. Never leave a broken test and move on.

---

## Phase A: Angular Subproject Bootstrap

**Purpose**: Create the Angular 21 project at `src/ui/`, configure Tailwind + DaisyUI, replace Karma with Jest, and wire into the pnpm workspace. No CMake or C++ changes yet — this phase is purely Node.js/TypeScript.

- [ ] T-UI-A-001 [P] [US1] Create Angular 21.0.1 standalone project at `src/ui/` using `ng new isched-ui --standalone --routing --style=scss --skip-git`; verify `ng build` produces output in `src/ui/dist/isched-ui/browser/`; set `"outputPath": "dist/isched-ui"` in `angular.json`; set `<base href="/isched/">` in `src/ui/src/index.html`

- [ ] T-UI-A-002 [P] [US1] Install and configure Tailwind CSS 3.x + DaisyUI 4.x in `src/ui/`: add `tailwindcss`, `postcss`, `autoprefixer`, `daisyui` as devDependencies; create `tailwind.config.js` with `content: ["./src/**/*.{html,ts}"]` and `plugins: [require("daisyui")]`; add `@tailwind base; @tailwind components; @tailwind utilities;` to `src/ui/src/styles.scss`; set `daisyui.themes: ["corporate"]` as default theme in `tailwind.config.js`

- [ ] T-UI-A-003 [P] [US1] Replace Karma with Jest in `src/ui/`: install `@angular-builders/jest`, `jest`, `jest-environment-jsdom`, `@types/jest`; update `angular.json` test builder to `@angular-builders/jest:run`; create `jest.config.ts`; add `"test": "ng test --watch=false"` script to `src/ui/package.json`; verify `pnpm --filter isched-ui run test` exits 0 with an empty test suite

- [ ] T-UI-A-004 [P] [US1] Add `src/ui` to `pnpm-workspace.yaml` packages list; run `pnpm install` at repo root and verify the workspace resolves; add `src/ui/dist/` and `src/ui/.angular/` to `.gitignore`

- [ ] T-UI-A-005 [P] [US1] Configure Angular Router in `src/ui/src/app/app.config.ts` with `provideRouter(routes)` and three lazy-loaded routes: `/seed` → `SeedComponent`, `/login` → `LoginComponent`, `/dashboard` → `DashboardComponent`; add a redirect from `''` to `/login`; add `withNavigationErrorHandler` for UNAUTHENTICATED redirect; skeleton component stubs are sufficient to make routing compile

- [ ] T-UI-A-006 [P] [US1] Add Angular build size budgets in `angular.json`: `maximumWarning: "400kb"`, `maximumError: "600kb"` for the initial bundle; verify `pnpm --filter isched-ui run build` produces a production bundle that passes budget checks

**Checkpoint**: `pnpm --filter isched-ui run build` succeeds with a valid `dist/` output; `pnpm --filter isched-ui run test` exits 0. No C++ changes yet; existing `ctest` suite remains green.

---

## Phase B: CMake Build Integration

**Purpose**: Wire the Angular build and asset embedding into the CMake build graph so the `isched` library automatically includes the compiled UI assets.

- [ ] T-UI-B-001 [US1] Add `isched_ui_build` custom target in the root `CMakeLists.txt`: runs `pnpm --filter isched-ui run build`; uses `file(GLOB_RECURSE UI_SOURCES src/ui/src/*)` as `DEPENDS` so the target re-runs on any Angular source change; `BYPRODUCTS src/ui/dist/isched-ui/browser/index.html`; skip gracefully (warn, do not error) if `pnpm` is not found on PATH

- [ ] T-UI-B-002 [US1] Write `tools/embed_ui_assets.py` — Python 3 script that:
  1. Iterates all files under `src/ui/dist/isched-ui/browser/`
  2. For each file computes a compile-time ETag as the first 16 hex chars of SHA-256 of its content
  3. Runs `xxd -i` (or falls back to `binascii.hexlify` when `xxd` is absent) to produce the `unsigned char` array
  4. Writes `src/main/cpp/isched/backend/isched_ui_assets.hpp` containing: one `extern const unsigned char[]` + `size_t` + `const char* etag` per file, plus a `static const UiAssetEntry ISCHED_UI_ASSET_MAP[]` array mapping URL path → asset struct
  5. Emits a table of file sizes; fails with exit code 1 if any single gzip-compressed file exceeds 500 KB (NFR-UI-002)

- [ ] T-UI-B-003 [US1] Add `isched_ui_embed` custom target in `CMakeLists.txt`: runs `${Python3_EXECUTABLE} tools/embed_ui_assets.py`; `DEPENDS isched_ui_build`; `BYPRODUCTS src/main/cpp/isched/backend/isched_ui_assets.hpp`; the generated header is added to the `isched` library sources via `target_sources(isched PRIVATE ...isched_ui_assets.hpp)` (header-only inclusion sufficient; the `.cpp` that `#include`s it is `isched_UiAssetRegistry.cpp` created in Phase C)

- [ ] T-UI-B-004 [US1] Gate the `isched` library target on `isched_ui_embed` via `add_dependencies(isched isched_ui_embed)`; document in `CMakeLists.txt` that this dependency is intentional and load-bearing

- [ ] T-UI-B-005 [P] [US1] Add `src/ui/dist/` to `.gitignore` (if not already done in Phase A); add `src/main/cpp/isched/backend/isched_ui_assets.hpp` to `.gitignore` (generated file — not committed)

**Checkpoint**: `cmake --build ./cmake-build-debug/` runs the Angular build and embedding step and produces `isched_ui_assets.hpp`; link step succeeds; `ctest` remains green.

---

## Phase C: C++ Static Asset Serving

**Purpose**: Implement the `/isched` route handler in `Server::Impl` using Boost.Beast, backed by the in-memory `UiAssetRegistry`.

- [ ] T-UI-C-001 [US1] Create `src/main/cpp/isched/backend/isched_UiAssetRegistry.hpp/.cpp`:
  - `struct UiAssetEntry { std::span<const uint8_t> data; std::string_view mime_type; std::string_view etag; }`
  - `class UiAssetRegistry` with a `static const UiAssetRegistry& instance()` singleton; constructor populates an `std::unordered_map<std::string, UiAssetEntry>` from the generated `ISCHED_UI_ASSET_MAP` array in `isched_ui_assets.hpp`
  - `std::optional<UiAssetEntry> find(std::string_view url_path) const`
  - `bool has_index_html() const` — used to detect missing build (503 guard)
  - `constexpr` MIME type map for `.html`, `.js`, `.css`, `.ico`, `.svg`, `.png`, `.woff2`, `.json`

- [ ] T-UI-C-002 [US1] Add `/isched/**` GET handler to `Server::Impl` in `isched_Server.cpp` using Boost.Beast, implementing:
  1. Strip `/isched` prefix from the request target; treat empty remainder as `/index.html`
  2. Look up trimmed path in `UiAssetRegistry::instance()`
  3. **Asset found**: build `http::response<http::vector_body<uint8_t>>` with correct `Content-Type`, `ETag`, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`; if `If-None-Match` matches ETag, return 304 with empty body (FR-UI-012, SR-UI-005)
  4. **Asset not found + `index.html` exists**: serve `index.html` bytes with `Content-Type: text/html; charset=utf-8` and HTTP 200 (HTML5 push-state fallback, FR-UI-013)
  5. **Asset not found + no `index.html`**: return HTTP 503 with `Content-Type: text/plain` body `"Admin UI assets unavailable"` (edge case)
  6. Pattern: exact asset path match takes priority; unmatched paths fall through to `index.html`

- [ ] T-UI-C-003 [US1] Extend startup log in `isched_srv_main.cpp` to emit `Admin UI:           http://<host>:<port>/isched` immediately after the existing `GraphQL endpoint:` log line (FR-UI-002, Q8)

**Checkpoint**: `GET /isched` returns 200 + `<app-root>`; `GET /isched/main-<hash>.js` returns 200 + `application/javascript`; `GET /isched/deep/route` returns 200 + `index.html`; startup log contains `Admin UI:` line. `ctest` green.

---

## Phase D: Backend GraphQL Additions

**Purpose**: Add `systemState`, `createPlatformAdmin`, `remote_ip` to `ResolverCtx`, and the rate-limiter. These are the only C++ backend changes required beyond the asset serving route.

- [ ] T-UI-D-001 [P] [US2] Add `remote_ip: std::string` field to `ResolverCtx` in `isched_GqlExecutor.hpp`; populate it from the Boost.Beast `socket().remote_endpoint().address().to_string()` in `Server::Impl` for every HTTP and WebSocket request before dispatching to `GqlExecutor::execute()` (Q16)

- [ ] T-UI-D-002 [P] [US2] Add to `isched_builtin_server_schema.graphql`:
  ```graphql
  type SystemState {
    """True when no active platform administrator exists; seed form must be used first"""
    seedModeActive: Boolean!
  }
  ```
  and add `systemState: SystemState!` to the `Query` type; register resolver in `GqlExecutor::setup_builtin_resolvers()` that executes `SELECT COUNT(*) FROM platform_admins WHERE is_active = 1` via `DatabaseManager` system DB and returns `{ "seedModeActive": count == 0 }`; add `"systemState"` to the authentication middleware allowlist so it is callable without a JWT (RISK-UI-004)

- [ ] T-UI-D-003 [P] [US2] Add `createPlatformAdmin(email: String!, password: String!): User!` mutation to `isched_builtin_server_schema.graphql`; implement resolver in `GqlExecutor::setup_builtin_resolvers()`:
  - Re-query `seedModeActive`; throw `FORBIDDEN` if false (FR-UI-004)
  - Validate `password.size() >= 12`; throw `VALIDATION_ERROR` with message `"password must be at least 12 characters"` if not (Q20, FR-UI-004a)
  - Hash password with Argon2id via `AuthenticationMiddleware::hash_password()`
  - `INSERT INTO platform_admins (id, email, password_hash, display_name, is_active, created_at) VALUES (...)`
  - Return new `User` JSON `{ id, email, displayName, roles: ["platform_admin"], isActive: true, createdAt, lastLogin: null }`
  - Add `"createPlatformAdmin"` to the authentication middleware allowlist (unauthenticated operation)

- [ ] T-UI-D-004 [P] [US2] Create `src/main/cpp/isched/backend/isched_RateLimiter.hpp`:
  - `struct TokenBucket` — capacity, tokens (float), last_refill timestamp; `bool try_consume()` refills tokens proportionally to elapsed time then attempts to consume 1
  - `class RateLimiter` — `std::unordered_map<std::string, TokenBucket>` protected by `std::mutex`; `bool allow(std::string_view ip, int max_per_minute)`
  - Instantiate one `RateLimiter` scoped to the `createPlatformAdmin` resolver; resolve `max_per_minute` at construction from `ISCHED_SEED_RATE_LIMIT` env var → `ConfigManager::get("server.seed_rate_limit")` → default 5; throw `RATE_LIMITED` GraphQL error when `allow()` returns false (Q17, SR-UI-001)

- [ ] T-UI-D-005 [P] [US2] Add server-side password length validation (≥ 12 chars, `VALIDATION_ERROR`) to the existing `createUser` and `updateUser` resolvers in `GqlExecutor` for consistency with `createPlatformAdmin` (Q20)

**Checkpoint**: GraphQL queries `{ systemState { seedModeActive } }`, mutation `createPlatformAdmin`, rate-limiter, and `remote_ip` all functioning. `ctest` green.

---

## Phase E: Angular Application

**Purpose**: Implement the three screens (seed, login, dashboard), the GraphQL client service, auth interceptor, and route guard.

### Phase E1: Core Infrastructure

- [ ] T-UI-E-001 [P] [US1] Implement `GraphQLService` in `src/ui/src/app/services/graphql.service.ts`: `query<T>(doc: string, variables?: object): Observable<T>` and `mutate<T>(doc: string, variables?: object): Observable<T>` methods; both POST to `/graphql`; surface the first entry in the `errors` array as a thrown `Error` with the GraphQL message; use Angular `HttpClient` injected via `inject()`

- [ ] T-UI-E-002 [P] [US1] Implement `AuthService` in `src/ui/src/app/services/auth.service.ts`: `setToken(token: string): void`, `getToken(): string | null`, `clearToken(): void` backed by `sessionStorage` key `"isched_token"`; `isLoggedIn(): boolean` returns `!!getToken()`; `logout()` clears token and calls `mutation { logout }` via `GraphQLService` (SR-UI-003)

- [ ] T-UI-E-003 [P] [US1] Implement `authInterceptor` functional interceptor in `src/ui/src/app/interceptors/auth.interceptor.ts`: if `AuthService.getToken()` is non-null, clone the request and add `Authorization: Bearer <token>` header; provide via `withInterceptors([authInterceptor])` in `app.config.ts`

- [ ] T-UI-E-004 [P] [US2] Implement `AuthGuard` functional guard in `src/ui/src/app/guards/auth.guard.ts`: call `{ systemState { seedModeActive } }`; if `seedModeActive` → redirect to `/seed`; else if `!AuthService.isLoggedIn()` → redirect to `/login`; else → return `true`; show no screen until the guard resolves (loading spinner via `ResolveFn` or `CanActivateFn` with async); attach to the `/dashboard` route

### Phase E2: Seed Screen

- [ ] T-UI-E-010 [P] [US2] Implement `SeedComponent` as `src/ui/src/app/pages/seed/seed.component.ts`:
  - Reactive form: `email` (required, email format), `password` (required, minLength 12), `confirmPassword` (required, must match `password`)
  - Each password field has a show/hide visibility toggle button (`👁` icon); initial type `"password"`
  - Submit calls `mutation { createPlatformAdmin(email: $email, password: $password) { id email } }`; button disabled while in-flight
  - On success: navigate to `/login`
  - On GraphQL error: display `alert alert-error` banner at top of form with the error message
  - On client validation error: per-field error message below the relevant `input`
  - DaisyUI classes: `card`, `form-control`, `label`, `input input-bordered`, `btn btn-primary`, `alert alert-error`
  - Responsive: centered card, full-width on mobile, max-w-md on desktop

### Phase E3: Login Screen

- [ ] T-UI-E-011 [P] [US3] Implement `LoginComponent` as `src/ui/src/app/pages/login/login.component.ts`:
  - Reactive form: `email` (required, email format), `password` (required)
  - Password field has show/hide toggle; default masked
  - Submit calls `mutation { login(email: $email, password: $password) { token expiresAt } }`; button disabled while in-flight
  - On success: `AuthService.setToken(token)`, navigate to `/dashboard`
  - On GraphQL error: display `alert alert-error` banner with the error message (e.g. "Invalid credentials")
  - On `UNAUTHENTICATED` error from any subsequent request (handled in `GraphQLService`): `AuthService.clearToken()`, navigate to `/login`
  - Same DaisyUI card layout as seed screen

### Phase E4: Dashboard Screen

- [ ] T-UI-E-012 [P] [US3] Implement `DashboardComponent` as `src/ui/src/app/pages/dashboard/dashboard.component.ts`:
  - On init: query `{ health { status } version }` → display health badge (`badge-success` / `badge-warning` / `badge-error`) and version string in a `navbar`
  - Query `{ organizations { id name subscriptionTier } }` → populate organizations table
  - For each organization row: sub-query `{ users(organizationId: $id) { id email roles isActive } }` → collapsible user list
  - "Create Organization" button → DaisyUI `modal`: form for `name`, `domain`, `subscriptionTier`, `userLimit`, `storageLimit`; submit calls `mutation { createOrganization(input: {...}) { id name } }`; on success closes modal and refreshes orgs table
  - "Create User" button per org row → DaisyUI `modal`: form for `email`, `password` (with toggle, minLength 12), `displayName`, `roles` multi-select; submit calls `mutation { createUser(organizationId: $id, input: {...}) { id email } }`; on success closes modal and refreshes user list
  - "Sign out" link in navbar → confirmation then `AuthService.logout()`, navigate to `/login`
  - Loading spinner (`loading loading-spinner`) while queries are in flight

- [ ] T-UI-E-013 [P] [US1] Add global loading indicator in `AppComponent`/route guard: while `AuthGuard` awaits `systemState`, display a full-screen DaisyUI `loading loading-spinner` overlay so the user never briefly sees the wrong screen (edge case from spec)

- [ ] T-UI-E-014 [P] [US1] Add `<meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:">` to `src/ui/src/index.html` (SR-UI-004); note `unsafe-inline` for styles is required by Tailwind's JIT output — document the rationale in a comment

- [ ] T-UI-E-015 [P] [US1] Verify responsive behaviour: run `pnpm --filter isched-ui run build` and manually confirm (or add snapshot tests) at 320 px, 768 px, 1280 px viewports for all three screens; fix any overflow or unreadable layout issues (FR-UI-008)

**Checkpoint**: Angular app builds cleanly; `pnpm --filter isched-ui run test` green; all three screens render correctly and route guard behaviour is correct.

---

## Phase F: Testing

**Purpose**: Write all automated tests and verify the full system end-to-end. C++ integration tests use `cpp-httplib` as a test client only.

### C++ Integration Tests

- [ ] T-UI-F-001 [US1] Create `src/test/cpp/integration/test_admin_ui.cpp` (Catch2) with test cases:
  - `GET /isched` → HTTP 200, `Content-Type` contains `text/html`, body contains `<app-root>`
  - `GET /isched/` → same as above (trailing slash)
  - `GET /isched/<main-js-bundle>` → HTTP 200, `Content-Type` contains `application/javascript` (discover filename from directory listing or embed a sentinel)
  - `GET /isched/nonexistent.xyz` → HTTP 404, body parses as JSON with `errors[0].message == "asset not found"`
  - `GET /isched/a/deep/nested/route` → HTTP 200, body contains `<app-root>` (push-state fallback)
  - `GET /isched` with `If-None-Match: <valid-etag>` → HTTP 304, empty body
  - `GET /isched` with `If-None-Match: "stale"` → HTTP 200 (ETag mismatch, full response)
  - All asset responses include `X-Content-Type-Options: nosniff` and `X-Frame-Options: DENY` headers

- [ ] T-UI-F-002 [US2] Create `src/test/cpp/integration/test_seed_mode.cpp` (Catch2) with test cases:
  - Fresh `isched_system.db` (or wiped `platform_admins`): `{ systemState { seedModeActive } }` returns `true`
  - `mutation { createPlatformAdmin(...) { id email } }` with valid email + 12-char password → returns `User`, `seedModeActive` is now `false`
  - Calling `createPlatformAdmin` again when `seedModeActive` is `false` → `FORBIDDEN` error
  - `createPlatformAdmin` with password shorter than 12 chars → `VALIDATION_ERROR`
  - `systemState` query is callable without `Authorization` header (unauthenticated)
  - `createPlatformAdmin` is callable without `Authorization` header (unauthenticated)
  - Rate-limiter: call `createPlatformAdmin` (with wrong but short password) 6 times rapidly from same IP → 6th call returns `RATE_LIMITED` error (set `ISCHED_SEED_RATE_LIMIT=5` in test env)

- [ ] T-UI-F-003 [P] [US2] Add test for `remote_ip` population in `isched_gql_executor_tests.cpp` or `test_seed_mode.cpp`: register a test-only resolver that reads `ctx.remote_ip` and returns it; assert the value is non-empty and matches the loopback address (`127.0.0.1` or `::1`)

- [ ] T-UI-F-004 [P] [US2] Add server-side password validation tests for `createUser` and `updateUser` in `test_user_management.cpp`: assert `VALIDATION_ERROR` when password is 11 chars; assert success when password is exactly 12 chars

- [ ] T-UI-F-005 [US1] Register `test_admin_ui` and `test_seed_mode` with ctest in `src/test/cpp/integration/CMakeLists.txt`; add `isched_RateLimiter` and `isched_UiAssetRegistry` sources to the `isched` library target in `src/main/cpp/CMakeLists.txt`

### Angular Jest Tests

- [ ] T-UI-F-010 [P] [US1] `GraphQLService` unit test (`graphql.service.spec.ts`): mock `HttpClient`; assert POST body and headers; assert successful data unwrapping; assert GraphQL error in `errors[]` is surfaced as thrown `Error`

- [ ] T-UI-F-011 [P] [US1] `AuthService` unit test (`auth.service.spec.ts`): `setToken` → `getToken` round-trip; `isLoggedIn()` true after set; `clearToken()` → `isLoggedIn()` false; `sessionStorage` key is `"isched_token"`

- [ ] T-UI-F-012 [P] [US2] `SeedComponent` unit test (`seed.component.spec.ts`):
  - Short password (< 12 chars) → form invalid, no mutation call
  - Mismatched passwords → form invalid, no mutation call
  - Valid form + successful mutation → router navigates to `/login`
  - Valid form + server error → `alert alert-error` banner contains the error message
  - Password toggle button switches `input.type` between `"password"` and `"text"`
  - Submit button disabled while mutation is pending

- [ ] T-UI-F-013 [P] [US3] `LoginComponent` unit test (`login.component.spec.ts`):
  - Server error → `alert alert-error` banner displayed
  - Successful login → token stored in `sessionStorage`, router navigates to `/dashboard`
  - Password toggle works as in seed component

- [ ] T-UI-F-014 [P] [US2] `AuthGuard` unit test (`auth.guard.spec.ts`):
  - `seedModeActive: true` → redirects to `/seed`
  - `seedModeActive: false`, not logged in → redirects to `/login`
  - `seedModeActive: false`, logged in → returns `true`

**Checkpoint**: `ctest --output-on-failure` passes all tests (existing 29 + new admin UI and seed mode tests); `pnpm --filter isched-ui run test` exits 0. Story ready to commit and close.
