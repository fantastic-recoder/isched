# isched Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-04

## Active Technologies
- C++23 backend + TypeScript (strict) / Angular 21 frontend + `cpp-httplib`, `nlohmann_json`, `sqlite3`, `jwt-cpp`, `spdlog`, `boost` (backend); Angular standalone APIs, signals, Tailwind CSS, DaisyUI (frontend) (004-add-isched-webui)
- SQLite per-tenant data + immutable audit-event persistence (>= 90 days retention) (004-add-isched-webui)
- C++23 backend + Angular 21 / strict TypeScript frontend + `cpp-httplib`, `jwt-cpp`, `nlohmann_json`, `sqlite3`, `spdlog`, `boost` (backend); Angular Router, typed reactive forms, signals, RxJS interop (frontend) (005-rate-limited-auth-bootstrap)
- SQLite (`isched_system.db` + tenant databases), in-memory frontend auth/bootstrap state (005-rate-limited-auth-bootstrap)

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
- 005-rate-limited-auth-bootstrap: Added C++23 backend + Angular 21 / strict TypeScript frontend + `cpp-httplib`, `jwt-cpp`, `nlohmann_json`, `sqlite3`, `spdlog`, `boost` (backend); Angular Router, typed reactive forms, signals, RxJS interop (frontend)
- 004-add-isched-webui: Added C++23 backend + TypeScript (strict) / Angular 21 frontend + `cpp-httplib`, `nlohmann_json`, `sqlite3`, `jwt-cpp`, `spdlog`, `boost` (backend); Angular standalone APIs, signals, Tailwind CSS, DaisyUI (frontend)

- 004-add-isched-webui: Added

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
