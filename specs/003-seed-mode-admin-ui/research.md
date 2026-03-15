# Research: Seed Mode and Administration UI

**Phase**: 0 (Research & Validation)
**Created**: 2026-03-15
**Updated**: 2026-03-15
**Feature**: Seed Mode and Administration UI
**Status**: Draft

---

## Technical Decisions

### Decision: Boost.Beast as the Sole Server Transport for Asset Serving

**Rationale**: The server binary uses Boost.Beast exclusively for all HTTP and WebSocket handling (established in story 001, T006). Static asset serving for the `/isched` route is implemented as an additional Boost.Beast request handler, consistent with the existing `/graphql` handler. `cpp-httplib` is retained as a **test-only client dependency** — it is used in C++ integration tests (`test_admin_ui.cpp`, `test_server_startup.cpp`, etc.) to send HTTP requests against the running server, but it is never linked into the `isched` library or binary.

**Implementation impact**: The `/isched` route handler is added to `Server::Impl` in `isched_Server.cpp` alongside the existing `/graphql` handler, reusing the same Boost.Beast `http::response<http::vector_body<uint8_t>>` pattern for binary asset responses. No new transport dependency is introduced.

---



**Rationale**: Angular 21.0.1 is the current latest stable release. The standalone component API (introduced in Angular 14, stable from Angular 17) removes the `NgModule` boilerplate and aligns Angular with the component-first patterns used by modern React, Svelte, and Vue applications. The project's developer audience is frontend-oriented, so we must use the current idiomatic Angular approach.

**Alternatives considered**:
- Angular + NgModules: outdated pattern; deprecated since Angular 17 for new projects.
- React 19 / Vue 3: valid for a new project, but the user request explicitly named Angular.
- SvelteKit: no mention in the brief; out of scope.

**Implementation impact**: All components use `@Component({ standalone: true, imports: [...] })`. Routing uses `provideRouter()` in `main.ts`. No `AppModule` file is created.

---

### Decision: DaisyUI 4.x over Tailwind CSS 3.x

**Rationale**: DaisyUI provides semantic component class names (`btn`, `card`, `modal`, `form-control`, `input`, `alert`, `badge`) on top of Tailwind's utility layer. This keeps templates concise, avoids class-name sprawl, and gives the app a professional appearance without a custom design system. The user request explicitly required DaisyUI.

**Tailwind version**: Tailwind CSS 3.x is the dependency of DaisyUI 4.x. Tailwind 4.x uses a fundamentally different configuration model and is not yet supported by DaisyUI 4. For this story, Tailwind 3.x is pinned.

**Alternatives considered**:
- Angular Material: heavier, more opinionated, requires significant theming work.
- PrimeNG: third-party Angular component library; adds a separate dependency tree.
- Plain Tailwind without DaisyUI: requires writing all component patterns from scratch.

**Implementation impact**: `tailwind.config.js` requires `daisyui` in `plugins`. The Angular `styles.scss` or `styles.css` must include `@tailwind base; @tailwind components; @tailwind utilities;`. DaisyUI theme is set at `<html data-theme="corporate">` or via `tailwind.config.js` `daisyui.defaultTheme`.

---

### Decision: Embed Angular Distribution in C++ Binary via `xxd`

**Rationale**: Running `isched_srv` as a single self-contained binary with no runtime filesystem dependencies is a core operational property of the server (same philosophy as the embedded SQLite approach). Embedding the Angular `dist/` via `xxd -i` keeps that promise.

**How it works**:
1. A CMake custom target `isched_ui_build` invokes `pnpm --filter isched-ui run build -- --configuration production`.
2. A Python or CMake script iterates over the files in `src/ui/dist/isched-ui/browser/`, runs `xxd -i` on each, and writes them into `src/main/cpp/isched/backend/isched_ui_assets.hpp` as `const unsigned char` arrays with their byte counts.
3. A C++ `UiAssetRegistry` (a `static std::unordered_map<std::string_view, std::span<const unsigned char>>` initialised at program startup) maps URL paths to their embedded byte arrays.
4. The `Server` class registers a `/isched` route handler that looks up the path in `UiAssetRegistry` and writes the bytes as the response body with the correct MIME type.

**Alternatives considered**:
- Serve Angular dist from a filesystem path next to the binary: simpler but breaks portability (binary must know runtime path); unsuitable for containerised or installed deployments.
- Separate web server (nginx) as a sidecar: adds operational complexity; violates single-binary philosophy.
- Base64-encode assets into a JSON file embedded via `#embed` (C23/C++26): not yet portable across all target compilers; `xxd -i` is universally available.

**Bundle size constraint**: The Angular production build is expected to produce ~200–350 KB of minified + gzip-compressed JavaScript for the three-screen app described. The NFR-UI-002 limit of 500 KB (gzip) is conservative. A CMake `file(SIZE ...)` check enforces this at build time.

**Implementation impact**: A robust `isched_ui_embed` CMake target is required. The target must re-run whenever any source file under `src/ui/src/` changes. This is achieved by listing the Angular source tree as a `DEPENDS` argument on the `add_custom_target` command (via `file(GLOB_RECURSE UI_SOURCES src/ui/src/*)`).

---

### Decision: `systemState { seedModeActive }` as Built-in GraphQL Field

**Rationale**: The Angular app must behave differently depending on whether any platform administrator has been created. Rather than a bespoke out-of-band HTTP probe endpoint (which would violate the GraphQL-only transport rule), a GraphQL query field is the correct approach. `systemState` is a logical extension of the existing `health` and `serverInfo` fields.

**Query definition added to bulit-in schema**:
```graphql
type SystemState {
  seedModeActive: Boolean!
}

extend type Query {
  systemState: SystemState!
}
```

**Resolver logic**: `seedModeActive` queries `SELECT COUNT(*) FROM platform_admins WHERE is_active = 1` in `isched_system.db`. Returns `true` if count is 0. This query is unauthenticated (the caller has no token yet when seed mode is relevant).

**Rate limit**: The `createPlatformAdmin` mutation resolver checks `seedModeActive` internally and throws `FORBIDDEN` if `false`. No HTTP-layer rate limiting is needed for the query itself; the mutation receives the 5-calls/minute per-IP guard (SR-UI-001) implemented via a `std::unordered_map<std::string, TokenBucket>` keyed on `ResolverCtx::remote_ip`.

---

### Decision: Angular Router in HTML5 History Mode with Server Fallback

**Rationale**: Angular's default router uses HTML5 `pushState` navigation (no `#` hash). This means refreshing at `/isched/dashboard` sends a GET request for `/isched/dashboard` to the server. The server must return `index.html` for any `/isched/**` path that is not a known static asset, so the Angular app can boot and take over routing client-side.

**Implementation impact**: The `/isched` route handler in `Server` follows a two-step dispatch:
1. Strip `/isched` prefix; look up the remainder in `UiAssetRegistry`.
2. If found → serve the asset bytes with the corresponding MIME type.
3. If not found → serve `index.html` with `Content-Type: text/html; charset=utf-8` and HTTP 200 (not 404). The Angular router then renders the correct screen based on the URL.

The Angular `app.config.ts` must include `withHashLocation()` set to `false` (default). `<base href="/isched/">` must be set in `index.html` so relative asset references resolve correctly.

---

### Decision: pnpm Workspace Integration for the Angular Subproject

**Rationale**: The `isched` repository already uses a `pnpm` workspace (`pnpm-workspace.yaml`) for the `tools/comparable_benchmark/` subproject. Adding `src/ui/` as a second workspace package follows the same pattern and keeps Node.js tooling unified under a single lockfile.

**Angular CLI installation strategy**: The Angular CLI (`@angular/cli`) is a `devDependency` of the `src/ui/` package, not a global install. CMake invokes it as `pnpm --filter isched-ui exec ng build ...` or via the `build` script in `package.json`. This avoids any global toolchain requirement beyond `pnpm` and `node` themselves.

**Build caching**: `pnpm` deduplicates Angular CLI and Angular framework packages across the workspace. The Angular build output directory (`src/ui/dist/`) is listed in `.gitignore`.

---

### Decision: JWT Stored in `sessionStorage` (Not `localStorage`)

**Rationale**: `sessionStorage` is cleared when the browser tab is closed, reducing the exposure window for a stolen token compared to `localStorage`. The user story is a developer-facing admin UI, not a consumer app with persistent login sessions. For the seed and RBAC use case, requiring re-login on each browser session is an acceptable trade-off.

**Alternatives considered**:
- `localStorage`: token persists across tabs and browser restarts; increases risk window. Explicitly excluded by SR-UI-003.
- HttpOnly cookie: would require the server to set a cookie on the `/isched` route, adding server-side session plumbing that the story does not require. Deferred to a future story.
- In-memory only (not persisted): forces re-login on page refresh; poor DX for admin tasks. Less preferred than `sessionStorage`.

**Implementation impact**: Angular `AuthService` stores/retrieves the token with `window.sessionStorage.getItem('isched_token')`. An Angular `HttpInterceptor` (or `withInterceptors` functional form) adds `Authorization: Bearer <token>` to all `POST /graphql` requests.

---

### Research: Angular 21 Build Output Structure

Angular 21 production builds with `@angular/build` (esbuild-based builder, default since Angular 17) produce output like:
```
dist/isched-ui/browser/
  index.html
  main-<hash>.js
  polyfills-<hash>.js
  styles-<hash>.css
  favicon.ico
  (optional chunk files: chunk-<hash>.js)
```

The `<hash>` suffixes change on every build. The `UiAssetRegistry` map must be populated at compile time from the actual file names present in `dist/`, not from hard-coded names. The `xxd` embedding script must iterate all files in the directory and produce map entries for each one.

---

### Research: ETag Support for Static Assets

Each embedded asset will have a deterministic ETag computed at **compile time** as the first 16 hex characters of the SHA-256 of its content. The ETag value is embedded alongside the byte array in `isched_ui_assets.hpp`. The `/isched` route handler checks `If-None-Match` against the ETag and returns `304` without a body when they match. This is a zero-runtime-cost operation since no filesystem stat is needed.

---

### Research: MIME Type Detection

The following MIME type mapping is sufficient for Angular's output:

| Extension | MIME type |
|---|---|
| `.html` | `text/html; charset=utf-8` |
| `.js` | `application/javascript; charset=utf-8` |
| `.css` | `text/css; charset=utf-8` |
| `.ico` | `image/x-icon` |
| `.svg` | `image/svg+xml` |
| `.png` | `image/png` |
| `.woff2` | `font/woff2` |
| `.json` | `application/json` |

The mapping is a `constexpr std::array` of `{std::string_view, std::string_view}` pairs resolved by linear scan (small enough to be cache-friendly; no hash map needed).

---

### Risk Register

| ID | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| RISK-UI-001 | Angular 21 CLI version mismatch between developer machines | Medium | Build failure | Pin `@angular/cli` version in `src/ui/package.json`; warn if pnpm version < 10 in CMake |
| RISK-UI-002 | `xxd` not available on all build hosts | Low | Embed step fails | Fall back to a CMake-based Python embedding script that does not require `xxd`; use `configure_file()` and `file(READ ... HEX)` |
| RISK-UI-003 | Angular bundle exceeds 500 KB gzip limit | Low | NFR-UI-002 violation | Budget checked at CMake build time; Angular `budgets` in `angular.json` set to 500 KB initial and 1 MB lazy warning |
| RISK-UI-004 | `systemState` query called in unauthenticated context triggers auth middleware rejection | Medium | Login screen never displays | Auth middleware must allowlist `systemState` and `createPlatformAdmin` as unauthenticated operations |
| RISK-UI-005 | ETag collision across builds with different content but same hash prefix | Negligible | Stale cache served | ETag is 64 bits of SHA-256; collision probability at 2^-64 is acceptable |
