# isched Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-11-09

## Active Technologies
- C++23 backend + TypeScript (strict) / Angular 21 frontend + `cpp-httplib`, `nlohmann_json`, `sqlite3`, `jwt-cpp`, `spdlog`, `boost` (backend); Angular standalone APIs, signals, Tailwind CSS, DaisyUI (frontend) (004-add-isched-webui)
- SQLite per-tenant data + immutable audit-event persistence (>= 90 days retention) (004-add-isched-webui)
- C++23 backend + Angular 21 / strict TypeScript frontend + `cpp-httplib`, `jwt-cpp`, `nlohmann_json`, `sqlite3`, `spdlog`, `boost` (backend); Angular Router, typed reactive forms, signals, RxJS interop (frontend) (005-rate-limited-auth-bootstrap)
- SQLite (`isched_system.db` + tenant databases), in-memory frontend auth/bootstrap state (005-rate-limited-auth-bootstrap)
- C++23 backend (no frontend code required for this increment) + `cpp-httplib`, `nlohmann_json`, `taocpp-pegtl`, `sqlite3`, `jwt-cpp`, `spdlog` (006-upload-schema)
- Per-tenant SQLite databases via `DatabaseManager` (new `schema_documents` table per tenant DB) (006-upload-schema)
- C++23 + `cpp-httplib`, `nlohmann_json`, `taocpp-pegtl`, `sqlite3`, `jwt-cpp`, `spdlog` (006-upload-schema)
- Tenant-local SQLite databases managed by `DatabaseManager` (`schema_documents` table in each tenant DB) (006-upload-schema)
- C++23 (repo baseline) + Angular 21 / strict TypeScript for `src/ui` + Angular standalone APIs/signals, Angular Router, RxJS interop (`toSignal`), Tailwind CSS 3.x, DaisyUI 4.x, existing GraphQL service (`/graphql`), Playwright, Jest/Karma test harness in `src/ui` (007-webui-nav-status-bars)
- N/A for new persistence; consumes existing in-memory auth/session state and GraphQL-backed user/session data (007-webui-nav-status-bars)
- C++23 + Catch2 3.x, Boost 1.84, cpp-httplib, nlohmann_json, spdlog, jwt-cpp, sqlite3, taocpp-pegtl (008-dod-mech-refactor)
- SQLite3 (embedded, per-tenant) (008-dod-mech-refactor)
- C++23 for product/test code; Python 3 for artifact-validation helpers and workflow scripts + Catch2 3.x, CMake + Ninja, Conan 2.x, existing `tools/refactor_pass/*` shell/Python tooling, JSON Schema Draft 2020-12 contracts (009-raise-pass-improvement-ratio)
- File-based evidence in `specs/009-raise-pass-improvement-ratio/artifacts/` (JSON/Markdown/text gate outputs) (009-raise-pass-improvement-ratio)
- TypeScript with Angular 21 on the frontend; existing C++23 backend preserved + Angular standalone APIs, RxJS for request streams, CodeMirror 6 GraphQL editor packages, existing `GraphQLService` transport, Tailwind CSS 3.x + DaisyUI 4.x (011-graphql-playground)
- Browser-persistent layout preferences only; no new server-side storage (011-graphql-playground)
- C++23 (required by constitution) + Boost.DLL 1.84.0 (header-only + `Boost::filesystem` + `${CMAKE_DL_LIBS}`), nlohmann/json (already in use), spdlog (already in use), Catch2 (testing) (012-plugin-system-boost-dll)
- SQLite via `DatabaseManager` (`isched_system.db`, `plugin_registry` table — already implemented) (012-plugin-system-boost-dll)
- C++23 + Boost.DLL/Filesystem (dynamic loading + path canonicalization), nlohmann::json (manifest/OpenAPI validation), SQLite via `DatabaseManager` (plugin_registry persistence), GraphQL executor/schema parser integration, spdlog (error logging), Catch2 + CTest (unit/integration verification) (012-plugin-system-boost-dll)
- `isched_system.db` (`plugin_registry` table + manifest JSON blob) (012-plugin-system-boost-dll)
- C++23 + Boost.DLL + Boost.Filesystem, nlohmann::json, SQLite (`DatabaseManager`), GraphQL executor/schema parser integration, spdlog (012-plugin-system-boost-dll)
- `isched_system.db` (`plugin_registry`) (012-plugin-system-boost-dll)
- C++23 + Boost.DLL + Boost.Filesystem, nlohmann::json, SQLite (`DatabaseManager`), `GqlExecutor::load_schema` with project PEGTL grammar (`isched_gql_grammar.hpp`) for SDL validation, spdlog (012-plugin-system-boost-dll)
- Existing per-tenant SQLite sessions and session/revocation support; no feature-specific schema migration is planned. Browser JWT transport is a Secure, HttpOnly, SameSite=Strict cookie; no JWT in localStorage, sessionStorage, IndexedDB, or application-managed memory. Login and first-run platform-admin bootstrap establish cookie sessions and return session-bound CSRF in GraphQL; an authenticated read-only GraphQL query reacquires it after page reload. (013-lsp-graphql-angular)
- C++23 backend; TypeScript 5.9 / Angular 21 frontend (strict mode and strict templates) + Existing CMake + Ninja, Conan 2.x, Catch2 3.x, cpp-httplib/Boost.Beast HTTP+WebSocket server, jwt-cpp, SQLite; Angular standalone APIs, signals, typed reactive forms, RxJS where stream semantics are needed, CodeMirror 6, `cm6-graphql` 0.2.x, GraphQL.js 16.x, GraphQL Language Service APIs, Jest/Angular test harness, Playwright (013-lsp-graphql-angular)
- C++23; Python 3 packaging helpers; CMake and GitHub Actions YAML; existing Angular 21/strict TypeScript frontend. No new shell helper files. + Conan 2.34.0; CMake 3.22.6 and Ninja 1.12.1 from Conan tool-requires; Node.js 24.22.2; pnpm 10.30.3; CPack DEB/RPM; Docker BuildKit/Buildx; GitHub Actions. (014-docker-linux-packaging)

- (004-add-isched-webui)

## Project Structure

```text
src/
tests/
```

## Commands

# Add commands for 

## Code Style

: Follow standard conventions

## Recent Changes
- 014-docker-linux-packaging: Added C++23; Python 3 packaging helpers; CMake and GitHub Actions YAML; existing Angular 21/strict TypeScript frontend. No new shell helper files. + Conan 2.34.0; CMake 3.22.6 and Ninja 1.12.1 from Conan tool-requires; Node.js 24.22.2; pnpm 10.30.3; CPack DEB/RPM; Docker BuildKit/Buildx; GitHub Actions.
- 013-lsp-graphql-angular: Completed C++23/Angular 21 GraphQL-only `/graphql` work with secure cookie sessions, session-bound CSRF, and browser-local CodeMirror assistance; all 70 tasks complete. Artifacts archived under `specs/archive/013-lsp-graphql-angular/`; manual acceptance testing follows separately.


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
