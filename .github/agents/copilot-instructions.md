# isched Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-04

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
- 009-raise-pass-improvement-ratio: Added C++23 for product/test code; Python 3 for artifact-validation helpers and workflow scripts + Catch2 3.x, CMake + Ninja, Conan 2.x, existing `tools/refactor_pass/*` shell/Python tooling, JSON Schema Draft 2020-12 contracts
- 008-dod-mech-refactor: Added C++23 + Catch2 3.x, Boost 1.84, cpp-httplib, nlohmann_json, spdlog, jwt-cpp, sqlite3, taocpp-pegtl
- 008-dod-mech-refactor: Added [if applicable, e.g., PostgreSQL, CoreData, files or N/A]


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
