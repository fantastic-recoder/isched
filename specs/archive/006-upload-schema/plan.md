# Implementation Plan: Tenant Admin Schema Upload

**Branch**: `006-upload-schema` | **Date**: 2026-04-06 | **Spec**: `/home/groby/dev/isched/specs/archive/006-upload-schema/spec.md`
**Input**: Feature specification from `/home/groby/dev/isched/specs/archive/006-upload-schema/spec.md`

## Summary

Implement tenant-scoped GraphQL schema document upload/list/fetch with clarified behavioral rules: schema names must match `[A-Za-z0-9._-]{1,128}`, matching is case-sensitive, upload size limit is deployment-configurable with a 1 MB default, concurrent overwrite-enabled uploads resolve as atomic last-successful-write-wins by commit order, and list output includes only `name`, `createdAt`, `updatedAt`, and `updatedBy`. Persistence remains tenant-local SQLite, validation uses the existing PEGTL SDL path, and failures map to structured GraphQL error codes.

## Technical Context

**Language/Version**: C++23  
**Primary Dependencies**: `cpp-httplib`, `nlohmann_json`, `taocpp-pegtl`, `sqlite3`, `jwt-cpp`, `spdlog`  
**Storage**: Tenant-local SQLite databases managed by `DatabaseManager` (`schema_documents` table in each tenant DB)  
**Testing**: Catch2 unit and integration tests in `src/test/cpp/isched/` and `src/test/cpp/integration/`  
**Target Platform**: Linux server runtime (GraphQL HTTP/WebSocket transport)  
**Project Type**: Single-process GraphQL backend service  
**Performance Goals**: Keep upload validation+persistence within existing tenant DB latency budgets; keep list/fetch as indexed tenant-scoped lookups  
**Constraints**: GraphQL-only `/graphql`; strict tenant isolation; explicit overwrite flag for replacement; schema-name charset `[A-Za-z0-9._-]` with length `1..128`; configurable max document size with default `1 MB`; overwrite races resolve as atomic last-successful-write-wins  
**Scale/Scope**: Add one organization-scoped schema-document capability (upload/list/fetch) with auth, validation, metadata-contract, concurrency, and isolation coverage

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes (if any) follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, zoneless/`OnPush`-compatible patterns, and no async-pipe-driven template state for app-owned UI state (N/A: no `src/ui/` changes planned)
- [x] Frontend API calls (if any) use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced (N/A: backend contract only)
- [x] Browser JWT handling (if any) avoids persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and documents secure cookie or equivalent controls (N/A: no browser token handling changes)
- [x] Local Angular development (if any) uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames in client source (N/A: no local WebUI changes)
- [x] Test plan proves required coverage before each user story is marked complete
- [x] Security-sensitive changes include both feature-scoped and project-level threat-model updates

**Gate result (pre-Phase 0)**: PASS  
**Gate result (post-Phase 1 design)**: PASS

## Project Structure

### Documentation (archived feature record)

```text
specs/archive/006-upload-schema/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── threat-model.md
├── contracts/
│   └── graphql-schema-upload.md
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── main/cpp/isched/backend/
│   ├── isched_builtin_server_schema.graphql
│   ├── isched_DatabaseManager.hpp
│   ├── isched_DatabaseManager.cpp
│   └── isched_GqlExecutor.cpp
└── test/cpp/
    ├── isched/
    │   └── isched_gql_executor_tests.cpp
    └── integration/
        └── test_graphql_schema_upload.cpp

docs/
└── security-threat-model.md
```

**Structure Decision**: Keep the existing monorepo backend layout and implement schema-upload behavior in GraphQL schema + resolver + tenant DB layers, with unit/integration tests and required threat-model documentation updates.

## Complexity Tracking

No constitution violations or approved exceptions.
