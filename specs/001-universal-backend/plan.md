# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2026-03-14 | **Spec**: [Universal Application Server Backend](spec.md)
**Input**: Revised feature specification from `/specs/001-universal-backend/spec.md`

**Note**: This plan supersedes the earlier scripting and IPC-oriented design. The implementation baseline is now a GraphQL-only HTTP/WebSocket backend.

**Status as of 2026-03-14**: Phases 1–5 (including 1b, 1c, 5b) are fully complete and committed. All 23 ctest tests pass. Phase 6 architecture decisions have been recorded in `tasks.md` through the `speckit.clarify` + `speckit.analyze` sessions; this document has been updated to reflect those decisions.

## Summary

Create a GraphQL-native application server that exposes only GraphQL over HTTP and WebSocket, removes scripting runtimes and IPC completely, persists configuration through GraphQL mutations, uses SQLite per tenant, and delivers built-in authentication, schema management, subscriptions, and documentation generation within a single C++23 runtime.

**Phase 6 scope** (in progress): full RBAC-based user and organization persistence with per-org SQLite isolation, Argon2id password hashing, a `isched_system.db` bootstrap database for platform-level entities, AES-256-GCM-encrypted API-key storage for outbound HTTP data sources (Apollo RESTDataSource pattern), persistent per-tenant session management with WebSocket force-revocation, global adaptive thread pool, dual-scope performance metrics (interval-window + cumulative), and a Catch2 benchmark suite with FR-012 latency assertion.

## Technical Context

**Language/Version**: C++23
**Build System**: CMake 3.22.6 + Ninja 1.12.1 (both provided via Conan `[tool_requires]`)
**Dependency Manager**: Conan 2.x — all dependencies declared in `conanfile.txt`; generators `CMakeDeps` + `CMakeToolchain`
**Primary Dependencies**: `taocpp-pegtl`, `nlohmann_json`, `spdlog`, `jwt-cpp`, `sqlite3`, `boost/1.84.0` (Boost.URL), `cpp-httplib` (sole HTTP/WebSocket transport), `openssl/[>=3.2.0]` (≥3.2 required for Argon2id via `EVP_KDF_fetch`), `platformfolders`
**Architecture**: Single-process, tenant-aware runtime with GraphQL over HTTP and WebSocket
**Storage**: SQLite per tenant with pooled connections and persisted configuration snapshots; plus `isched_system.db` for platform-level entities (`platform_admins`, `platform_roles`, `organizations`)
**Memory Management**: Mandatory smart pointers (`std::unique_ptr`, `std::shared_ptr`)
**Testing**: Catch2 3.x unit/integration/benchmark tests
**Documentation**: Automated Doxygen source documentation via `cmake --build . --target docs`
**Target Platform**: Linux primary; Raspberry Pi / ARM Linux deployment documented; cross-platform portability documented
**Project Type**: High-performance GraphQL backend server
**Performance Goals**: 20ms p95 response time (FR-012), ≥1000 req/s throughput, ≥50 concurrent WebSocket subscribers, ≥100 concurrent HTTP clients
**Constraints**: GraphQL-only external interface, no scripting, no IPC, secure-by-default, C++ Core Guidelines
**Scale/Scope**: Multi-tenant backend with built-in auth, schema configuration, subscriptions, outbound HTTP integrations, and performance metrics

## Build Commands

### First-time setup

```bash
conan profile detect
```

### Configure and build

```bash
python3 configure.py
```

Internally `configure.py` runs:

```bash
conan install . -of cmake-build-debug -s build_type=Debug --build=missing
cmake . -B ./cmake-build-debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake \
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build ./cmake-build-debug/
```

### Run tests

```bash
cd cmake-build-debug && ctest --output-on-failure
```

### Build a specific target

```bash
cmake --build ./cmake-build-debug/ --target <target>
```

### Regenerate documentation

```bash
cmake --build ./cmake-build-debug/ --target docs
```

## Constitution Check

*GATE: Must pass before implementation starts. Re-check after design updates.*

✅ **High Performance**: Single-runtime design must still meet multi-tenant performance targets  
✅ **GraphQL Compliance**: HTTP and WebSocket behavior must remain standards-aligned  
✅ **Security-First**: Authentication and tenant isolation remain mandatory  
✅ **Test-Driven Development**: Core transport and configuration behavior requires tests first  
✅ **Cross-Platform Portability**: Linux build required; portability documented  
✅ **C++ Core Guidelines**: No raw ownership, RAII throughout

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── graphql-schema.md
│   └── http-api.md
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── main/cpp/isched/
│   ├── isched.hpp                               # Top-level convenience include
│   ├── backend/
│   │   ├── isched_gql_grammar.hpp/cpp           # Custom PEGTL GraphQL grammar (mandatory)
│   │   ├── isched_GqlParser.hpp/cpp             # *(deleted in Phase 1b — T-GQL-020)*
│   │   ├── isched_GqlExecutor.hpp/cpp           # Query/mutation executor + built-in resolvers
│   │   ├── isched_Server.hpp/cpp                # GraphQL-only HTTP/WebSocket server (cpp-httplib)
│   │   ├── isched_TenantManager.hpp/cpp         # Multi-tenant isolation manager (refactor pending)
│   │   ├── isched_DatabaseManager.hpp/cpp       # Per-tenant SQLite storage + connection pooling
│   │   ├── isched_AuthenticationMiddleware.hpp/cpp  # JWT auth (stub — Phase 2)
│   │   ├── isched_SubscriptionBroker.hpp/cpp    # WebSocket subscription broker (exists — Phase 5)
│   │   ├── isched_CryptoUtils.hpp/cpp           # Argon2id + AES-256-GCM helpers (to be created — Phase 6, T047/T048)
│   │   ├── isched_MetricsCollector.hpp/cpp      # Dual-scope performance metrics (to be created — Phase 6, T051)
│   │   ├── isched_RestDataSource.hpp/cpp        # Outbound HTTP data-source client (to be created — Phase 6, T048)
│   │   ├── isched_common.hpp                    # Smart-pointer aliases, namespace declarations
│   │   ├── isched_ExecutionResult.hpp           # GraphQL execution result type
│   │   ├── isched_gql_error.hpp                 # EErrorCodes, Error, TErrorVector
│   │   ├── isched_log_result.hpp                # log_result(), concat_vector() helpers
│   │   ├── isched_multi_dim_map.hpp             # Multidimensional associative container
│   │   ├── isched_LogEnvLoader.hpp              # spdlog env-level loader (static init)
│   │   ├── isched_srv_main.cpp                  # Server process entry point (main)
│   │   │
│   │   │   Legacy REST layer (deleted in Phase 2 — T006):
│   │   ├── isched_BaseRestResolver.hpp/cpp      # Abstract REST resolver interface (legacy)
│   │   ├── isched_DocRootRestResolver.hpp/cpp   # Static doc-root REST resolver (legacy)
│   │   ├── isched_DocRootSvc.hpp/cpp            # Doc-root service integration (legacy)
│   │   ├── isched_EHttpMethods.hpp              # HTTP method enum (legacy)
│   │   ├── isched_MainSvc.hpp/cpp               # Restbed service wrapper (legacy)
│   │   └── isched_SingleActionRestResolver.hpp/cpp  # Fixed-response REST resolver (legacy)
│   └── shared/
│       ├── config/
│       │   └── isched_config.hpp/cpp            # Configuration management + match_pattern()
│       ├── fs/
│       │   └── isched_fs_utils.hpp/cpp          # Unix-style glob, filesystem helpers
│       ├── ipc/
│       │   └── isched_ipc.hpp/cpp               # Legacy IPC framework (Phase 7 removal)
│       ├── isched_exception_base.hpp/cpp        # Abstract exception base
│       ├── isched_exception_doc_path_not_found.hpp/cpp
│       └── isched_exception_unknown_enum_value.hpp
└── test/
    ├── basic_server_test.cpp                    # Basic server foundation test
    ├── tenant_manager_test.cpp                  # TenantManager lifecycle test
    └── cpp/
        ├── integration/
        │   ├── test_server_startup.cpp          # Server startup + built-in GraphQL (exists)
        │   ├── test_builtin_schema.cpp          # Built-in schema queries (exists — Phase 1/2)
        │   ├── test_health_queries.cpp          # Health query coverage (exists — Phase 3)
        │   ├── test_configuration_snapshots.cpp # Configuration snapshot roundtrip (exists — Phase 3)
        │   ├── test_schema_activation.cpp       # Schema activation + rollback (exists — Phase 3)
        │   ├── test_configuration_rollback.cpp  # Rollback on validation failure (exists — Phase 3)
        │   ├── test_graphql_websocket.cpp       # WebSocket connection lifecycle (exists — Phase 5)
        │   ├── test_graphql_subscriptions.cpp   # Subscription delivery (exists — Phase 5)
        │   ├── test_client_compatibility.cpp    # Client compatibility (exists — Phase 5)
        │   ├── test_user_management.cpp         # RBAC user/org CRUD (to be created — Phase 6, T047)
        │   └── test_session_revocation.cpp      # Session revocation + WS force-close (to be created — Phase 6, T049)
        ├── isched/
        │   ├── isched_grammar_tests.cpp         # Grammar conformance tests (exists)
        │   ├── isched_gql_executor_tests.cpp    # GqlExecutor unit tests (exists)
        │   ├── isched_ast_node_tests.cpp        # AST node helper tests (exists)
        │   ├── isched_auth_tests.cpp            # AuthenticationMiddleware tests (exists)
        │   ├── isched_config_tests.cpp          # Configuration system tests (exists)
        │   ├── isched_database_test.cpp         # DatabaseManager unit tests (exists)
        │   ├── isched_fs_utils_tests.cpp        # Filesystem utility tests (exists)
        │   ├── isched_graphql_tests.cpp         # GqlExecutor end-to-end tests (exists)
        │   ├── isched_multi_dim_map_tests.cpp   # multi_dim_map container tests (exists)
        │   ├── isched_pattern_match_tests.cpp   # match_pattern() tests (exists)
        │   ├── isched_resolver_tests.cpp        # GqlParser resolver integration tests (exists)
        │   ├── isched_rest_hello_world.cpp      # Legacy Restbed sanity test (legacy)
        │   ├── isched_srv_tests.cpp             # Server HTTP integration tests (exists)
        │   ├── isched_test_run_listener.cpp     # Catch2 event listener (exists)
        │   ├── isched_ipc_tests.cpp             # IPC tests (legacy)
        │   └── isched_rest_datasource_tests.cpp # RestDataSource unit tests (to be created — Phase 6, T048)
        └── performance/
            └── benchmark_suite.cpp             # Catch2 benchmark suite (to be created — Phase 6, T052)

CMakeLists.txt
conanfile.txt
configure.py                                    # One-shot Conan + CMake + Ninja build script
docs/
├── performance.md                              # Benchmark results + FR-012 assertion (to be created — Phase 6, T052)
└── deployment.md                              # Deployment guide + Raspberry Pi notes (to be created — Phase 7, T057)
```

**Structure Decision**: Keep a single C++ project but drop any plan for CLI runtimes or shared-memory coordination. The architecture centers on a GraphQL transport layer, an in-process configuration subsystem, and a subscription broker for WebSocket sessions.

## Complexity Tracking

> Fill only if constitutional complexity needs explicit justification.

| Violation | Why Needed | Simpler Alternative Rejected Because |
| --------- | ---------- | ----------------------------------- |
| Per-tenant databases | Strong tenant isolation and simpler recovery | Shared schemas in one database raise isolation risk |
| WebSocket subscription broker | Required real-time GraphQL transport | HTTP polling violates transport requirement and adds latency |
| Atomic configuration snapshots | Prevent partial schema activation | In-place mutation risks broken active schemas |
| Custom PEGTL GraphQL parser | Full parse-tree control, no library dependency, direct integration with execution context | Third-party parsing library would duplicate internal concerns and add an opaque runtime dependency |
| `isched_system.db` bootstrap database | Platform-level entities (platform_admins, platform_roles, organizations) must exist before any tenant DB; cross-org queries would require joins across tenant SQLite files | A single tenant DB would violate multi-tenant isolation; cramming platform entities into one tenant DB is unsound |
| AES-256-GCM API key encryption | API keys for outbound HTTP data sources are secrets; storing plaintext in tenant SQLite exposes them via any DB-file read | A simpler credential store (e.g. OS keyring) is not portable to the embedded/Raspberry Pi target; AES-GCM with HKDF-derived key is the minimal portable solution |
| Global adaptive thread pool | `cpp-httplib` exposes a single process-wide `set_thread_pool_size()` — there is no per-tenant granularity in the transport layer | A custom per-tenant thread pool would require a parallel async I/O layer, far exceeding scope; advisory per-tenant min/max stored in `TenantConfig` and applied as aggregate |
| Dual performance counter semantics | FR-PERF-003 requires both interval-window counters (reset every 60 min) and cumulative counters (never reset) — a single counter cannot satisfy both requirements | Storing only cumulative counts prevents interval-rate queries; storing only interval counts loses absolute totals needed for capacity planning |

## Phase Plan

### Invariant: Test Suite Always Green

**After every task**, the full test suite MUST pass:

```bash
cd cmake-build-debug && ctest --output-on-failure
```

This is a hard gate at every task boundary, not only at phase checkpoints. If a task causes a regression, it MUST fix the broken test in the same change. Moving to the next task while any test is failing is not allowed.

### Invariant: Commit After Every Task

Once a task is complete and `ctest` is fully green, a Git commit MUST be created before moving to the next task. Completed work must never remain uncommitted when starting the next task. The commit message must reference the task ID(s) addressed and note any bugs fixed.

---

### ✅ Phase 0: Research Update — COMPLETE

- Removed scripting, IPC, and out-of-process execution assumptions
- Confirmed GraphQL-only transport requirements for HTTP and WebSocket
- Validated versioned configuration snapshot approach
- Documented PEGTL custom parser as a mandatory, first-class subsystem

### ✅ Phase 1: Design Update — COMPLETE

- Defined built-in GraphQL schema for startup, health, auth, and configuration
- Defined configuration snapshot, model-definition, and subscription-session entities
- Defined transport contracts for HTTP and WebSocket

### ✅ Phase 1b: PEGTL Grammar Completion — COMPLETE

- Audited `isched_gql_grammar.hpp` against full GraphQL specification
- Implemented missing grammar rules: fragments, all type-system definitions, type extension rules
- Added negative and spec-derived conformance tests
- Implemented parse-error conversion to GraphQL `errors` array with `locations`

### ✅ Phase 1c: Execution Engine — COMPLETE

- Implemented `GqlExecutor` query and mutation dispatching
- Connected custom PEGTL grammar to executor pipeline
- Built resolver registration with type-safe argument extraction

### ✅ Phase 2: Implementation Realignment — COMPLETE

- Server transport aligned to `/graphql` only
- WebSocket subscription handling added
- IPC code paths removed from runtime planning

### ✅ Phase 3: Verification — COMPLETE

- GraphQL over HTTP integration coverage
- GraphQL over WebSocket integration coverage
- Configuration rollback and schema migration coverage

### ✅ Phase 4: Tenant Isolation — COMPLETE

- Per-tenant SQLite databases with connection pooling
- Tenant lifecycle management (create, activate, suspend, destroy)
- Cross-tenant isolation enforced at executor level

### ✅ Phase 5: Configuration System — COMPLETE

- Versioned configuration snapshots in SQLite
- Atomic activation with rollback on validation failure
- Schema regeneration from model definitions

### ✅ Phase 5b: Subscription Broker — COMPLETE

- `SubscriptionBroker` for WebSocket subscription delivery
- Subscription lifecycle (subscribe, publish, unsubscribe)
- Per-tenant subscription isolation

---

### 🔄 Phase 6: Security, Auth, Metrics & Integrations — IN PROGRESS

Full details in `tasks.md` (T047–T052). Summary of sub-areas:

**T047 — RBAC and User/Org Management**
- `isched_system.db` bootstrap database: `platform_admins`, `platform_roles`, `organizations` tables; created by `DatabaseManager` on first startup
- 4 built-in roles: `PLATFORM_ADMIN`, `ORG_ADMIN`, `ORG_MEMBER`, `ORG_VIEWER`
- RBAC enforced at Query/Mutation resolver level; per-org SQLite isolation
- User CRUD mutations; `login` mutation returns JWT; Argon2id via OpenSSL ≥ 3.2 `EVP_KDF_fetch("ARGON2ID")`
- `conanfile.txt` updated to pin `openssl/[>=3.2.0]`

**T048 — Outbound HTTP RESTDataSource**
- `isched_RestDataSource` following Apollo RESTDataSource pattern: per-tenant auth config in tenant DB
- API key stored encrypted: `isched_CryptoUtils` provides AES-256-GCM encrypt/decrypt; HKDF-derived per-tenant key
- JSON auto-coercion; structured `HttpError` type in GraphQL schema
- Column name: `api_key_value_encrypted`

**T049 — Session Management and Revocation**
- Sessions table in tenant SQLite; includes `roles` JSON array (populated at login)
- `AuthenticationMiddleware::create_session()` is sole owner; `login` resolver delegates
- 4 revocation mutations + `terminateAllSessions`; WebSocket force-close on revocation
- `last_activity` updated on each request

**T050 — Global Adaptive Thread Pool**
- Single `cpp-httplib` global pool driven by aggregate active-subscription count
- Per-tenant min/max stored as advisory in `TenantConfig`; applied only at aggregate level
- Pool size updated on subscription connect/disconnect events

**T051 — Performance Metrics**
- Dual counter sets: `requestsInInterval` / `errorsInInterval` (reset every 60 min) + `totalRequestsSinceStartup` / `totalErrorsSinceStartup` (cumulative)
- Both Query and Subscription exposed; per-tenant and system-level queries
- `isched_MetricsCollector` owns counters; 60-minute interval timer via `std::jthread`

**T052 — Catch2 Benchmark Suite**
- `benchmark_suite.cpp` using `CATCH_BENCHMARK`
- 4 named thresholds: GraphQL parse, resolver dispatch, DB read, DB write
- T052-006: `REQUIRE(p95_latency_ms <= 20.0)` asserting FR-012
- T052-007: results documented in `docs/performance.md`

---

### Phase 7: Documentation and Security Hardening — NOT STARTED

Full details in `tasks.md` (T055–T057). Summary:

**T055 — Documentation Review**: All docs (`graphql-schema.md`, `http-api.md`, `quickstart.md`, `data-model.md`) reviewed against implementation; `extensions` field documented in `http-api.md`
**T056 — Security Scan**: `security_scan` CMake target running clang-tidy with security-oriented checks
**T057 — Deployment Docs**: `docs/deployment.md` covering Linux setup, environment variables, Raspberry Pi / ARM Linux guidance

## Revised Implementation Focus

### Custom PEGTL Parser ✅

- `isched_gql_grammar.hpp` — complete, spec-mapped PEGTL grammar for the full GraphQL document language
- `GqlParser` facade — stable `parse(string, name)` entry point used by `GqlExecutor` and SDL subsystems
- Parse-error conversion to GraphQL `errors` array with `locations`
- Grammar conformance tests per spec section
- No third-party GraphQL parsing library used

### Core Runtime ✅

- GraphQL over HTTP request handling
- GraphQL over WebSocket connection lifecycle and subscriptions
- Built-in schema for startup, health, auth, and configuration
- Tenant-aware runtime state and connection pooling

### Configuration System ✅

- Versioned configuration snapshots stored internally
- GraphQL mutations for apply, validate, activate, and inspect operations
- Atomic activation with rollback on validation failure
- Schema regeneration from model definitions

### Observability ✅

- Server info and health via GraphQL queries
- Optional operational event subscriptions via WebSocket
- Request, error, and subscription metrics framework in place

### System Database (Phase 6 — T047) 🔄

- `isched_system.db` created by `DatabaseManager::ensure_system_db()` on first startup
- Tables: `platform_admins` (id, username, password_hash, created_at), `platform_roles` (id, name, permissions JSON), `organizations` (id, name, tenant_id, created_at)
- 4 built-in roles seeded at startup: `PLATFORM_ADMIN`, `ORG_ADMIN`, `ORG_MEMBER`, `ORG_VIEWER`
- Bootstrap admin credentials loaded from environment variable on first run

### Authentication and RBAC (Phase 6 — T047) 🔄

- `AuthenticationMiddleware` extended with full JWT validation and role extraction
- Argon2id password hashing via `isched_CryptoUtils::hash_password()` using `EVP_KDF_fetch("ARGON2ID")` (requires OpenSSL ≥ 3.2)
- `login` mutation: validates credentials, calls `create_session()`, returns signed JWT
- RBAC enforced at resolver level: each Query/Mutation checks minimum required role from JWT claims
- Per-org SQLite isolation: org data lives in its own tenant DB, not in `isched_system.db`

### Session Management and Revocation (Phase 6 — T049) 🔄

- Sessions table in tenant SQLite: `(id, user_id, roles TEXT, created_at, last_activity, revoked_at)`
- `AuthenticationMiddleware::create_session()` is the sole session-creation entry point; `login` resolver delegates to it
- 4 revocation GraphQL mutations: `revokeSession`, `revokeUserSessions`, `revokeOrgSessions`, `terminateAllSessions`
- WebSocket force-close on revocation: `SubscriptionBroker` receives revocation event and closes affected connections
- `roles` column stored as JSON array to enable `terminateAllSessions` without cross-table joins

### Outbound HTTP — RESTDataSource (Phase 6 — T048) 🔄

- `isched_RestDataSource`: per-tenant outbound HTTP client following Apollo RESTDataSource pattern
- Auth config stored per data-source in tenant SQLite; supports `Authorization: Bearer`, API key, and unauthenticated modes
- API key encrypted at rest: `isched_CryptoUtils::encrypt_aes_gcm()` / `decrypt_aes_gcm()` with HKDF-derived per-tenant key
- JSON auto-coercion; structured `HttpError` type surfaced through GraphQL `errors` extensions
- Unit tests in `isched_rest_datasource_tests.cpp`

### Performance Metrics (Phase 6 — T051) 🔄

- `isched_MetricsCollector` owns all performance counters
- Interval-window counters: `requestsInInterval`, `errorsInInterval` — reset every 60 minutes via `std::jthread`
- Cumulative counters: `totalRequestsSinceStartup`, `totalErrorsSinceStartup` — never reset
- Exposed via both GraphQL Query and Subscription; per-tenant and system-level views
- Auth-gated: requires `ORG_ADMIN` or higher for tenant metrics; `PLATFORM_ADMIN` for system metrics

### Benchmarks and Security Hardening (Phase 6/7 — T052, T056) 🔄

- `benchmark_suite.cpp` using `CATCH_BENCHMARK` with 4 named thresholds: parse, dispatch, DB read, DB write
- `REQUIRE(p95_latency_ms <= 20.0)` asserting FR-012 (20ms p95 end-to-end latency)
- Results documented in `docs/performance.md`
- `security_scan` CMake target running clang-tidy with security-oriented checks (T056)

## Current Planning Status

**Status**: Phase 6 architecture fully decided and documented; implementation in progress.
**Phases complete**: 0, 1, 1b, 1c, 2, 3, 4, 5, 5b — all 23 ctest tests pass on commit `0103a2b`.
**Architecture decisions recorded in `tasks.md`**: T047–T052 (Phase 6) and T055–T057 (Phase 7) fully expanded from stubs through `speckit.clarify` + `speckit.analyze` sessions; all 10 analysis findings resolved.
**Next Action**: Begin Phase 6 implementation with T047-011 (pin `openssl/[>=3.2.0]` in `conanfile.txt`), then T047-000 (`isched_system.db` bootstrap), proceeding through the T047–T052 sub-task sequence in `tasks.md` order.
**Gate**: Each sub-task committed individually; `ctest --output-on-failure` must be 100% green before each commit.
