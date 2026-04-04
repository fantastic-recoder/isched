# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2026-03-14 | **Spec**: [Universal Application Server Backend](spec.md)
**Input**: Revised feature specification from `/specs/001-universal-backend/spec.md`

**Note**: This plan supersedes the earlier scripting and IPC-oriented design. The implementation baseline is now a GraphQL-only HTTP/WebSocket backend.

**Status**: This plan tracks completed and clarified implementation across Phases 0–7 and reflects the current state recorded in `tasks.md`. Validation remains gated by `ctest --output-on-failure` at task boundaries.

## Summary

Create a GraphQL-native application server that exposes only GraphQL over HTTP and WebSocket, removes scripting runtimes and IPC completely, persists configuration through GraphQL mutations, uses SQLite per organization, and delivers built-in authentication, schema management, subscriptions, and documentation generation within a single C++23 runtime.

**Phase 6 scope**: full RBAC-based user and organization persistence with per-org SQLite isolation, Argon2id password hashing, a `isched_system.db` bootstrap database for platform-level entities, AES-256-GCM-encrypted API-key storage for outbound HTTP data sources (Apollo RESTDataSource pattern), persistent per-organization session management with WebSocket force-revocation, global adaptive thread pool, dual-scope performance metrics (interval-window + cumulative), and a Catch2 benchmark suite with FR-012 latency assertion.

## Technical Context

**Language/Version**: C++23
**Build System**: CMake 3.22.6 + Ninja 1.12.1 (both provided via Conan `[tool_requires]`)
**Dependency Manager**: Conan 2.x — all dependencies declared in `conanfile.txt`; generators `CMakeDeps` + `CMakeToolchain`
**Primary Dependencies**: `taocpp-pegtl`, `nlohmann_json`, `spdlog`, `jwt-cpp`, `sqlite3`, `boost/1.84.0` (Boost.URL), `cpp-httplib` (sole HTTP/WebSocket transport), `openssl/[>=3.2.0]` (≥3.2 required for Argon2id via `EVP_KDF_fetch`), `platformfolders`
**Architecture**: Single-process, organization-aware runtime with GraphQL over HTTP and WebSocket
**Storage**: SQLite per organization with pooled connections and persisted configuration snapshots; plus `isched_system.db` for platform-level entities (`platform_admins`, `platform_roles`, `organizations`)
**Memory Management**: Mandatory smart pointers (`std::unique_ptr`, `std::shared_ptr`)
**Testing**: Catch2 3.x unit/integration/benchmark tests
**Documentation**: Automated Doxygen source documentation via `cmake --build . --target docs`
**Target Platform**: Linux primary; Raspberry Pi / ARM Linux deployment documented; cross-platform portability documented
**Project Type**: High-performance GraphQL backend server
**Performance Goals**: 20ms p95 response time (FR-012) under the documented HTTP-level `/graphql` benchmark profile; FR-PERF-001 also requires ≥50 concurrent WebSocket subscribers and ≥100 concurrent HTTP clients. Separately, the in-process `hello` throughput check (`~1000 req/s`) is retained only as a non-normative local regression guard.
**Constraints**: GraphQL-only external interface, no scripting, no IPC, secure-by-default, C++ Core Guidelines
**Scale/Scope**: Multi-organization backend with built-in auth, schema configuration, subscriptions, outbound HTTP integrations, and performance metrics

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

✅ **High Performance**: Single-runtime design must still meet multi-organization performance targets  
✅ **GraphQL Compliance**: HTTP and WebSocket behavior must remain standards-aligned  
✅ **Security-First**: Authentication and organization isolation remain mandatory  
✅ **Test-Driven Development**: Core transport and configuration behavior requires tests first  
✅ **Cross-Platform Portability**: Linux build required; portability documented  
✅ **C++ Core Guidelines**: No raw ownership, RAII throughout
✅ **Review Evidence Gate**: Completion requires explicit code-review evidence that checks constitution compliance (performance, GraphQL spec behavior, security, portability, and C++ Core Guidelines)

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md
├── research.md
├── data-model.md
├── closeout-validation.md
├── performance-protocol.md
├── quickstart.md
├── threat-model.md
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
│   │   ├── isched_gql_grammar.hpp/cpp           # Custom PEGTL GraphQL grammar
│   │   ├── isched_GqlExecutor.hpp/cpp           # Query/mutation executor + built-in resolvers
│   │   ├── isched_Server.hpp/cpp                # GraphQL-only HTTP/WebSocket server (cpp-httplib)
│   │   ├── isched_TenantManager.hpp/cpp         # Organization isolation + advisory tenant config
│   │   ├── isched_DatabaseManager.hpp/cpp       # Per-organization SQLite storage + pooling
│   │   ├── isched_AuthenticationMiddleware.hpp/cpp  # JWT auth + session validation
│   │   ├── isched_SubscriptionBroker.hpp/cpp    # WebSocket subscription broker
│   │   ├── isched_CryptoUtils.hpp/cpp           # Argon2id + AES-256-GCM helpers
│   │   ├── isched_MetricsCollector.hpp/cpp      # Dual-scope performance metrics
│   │   ├── isched_RestDataSource.hpp/cpp        # Outbound HTTP data-source client
│   │   ├── isched_builtin_server_schema.graphql # Built-in GraphQL schema
│   │   └── isched_srv_main.cpp                  # Server process entry point
│   └── shared/
│       ├── config/isched_config.hpp/cpp
│       ├── fs/isched_fs_utils.hpp/cpp
│       └── isched_exception_*.hpp/cpp
└── test/
    ├── basic_server_test.cpp
    ├── tenant_manager_test.cpp
    └── cpp/
        ├── integration/
        │   ├── test_server_startup.cpp
        │   ├── test_builtin_schema.cpp
        │   ├── test_health_queries.cpp
        │   ├── test_configuration_snapshots.cpp
        │   ├── test_configuration_conflicts.cpp
        │   ├── test_schema_activation.cpp
        │   ├── test_configuration_rollback.cpp
        │   ├── test_graphql_websocket.cpp
        │   ├── test_graphql_subscriptions.cpp
        │   ├── test_client_compatibility.cpp
        │   ├── test_bootstrap_platform_admin.cpp
        │   ├── test_user_management.cpp
        │   └── test_session_revocation.cpp
        ├── isched/
        │   ├── isched_grammar_tests.cpp
        │   ├── isched_gql_executor_tests.cpp
        │   ├── isched_auth_tests.cpp
        │   ├── isched_metrics_tests.cpp
        │   ├── isched_rest_datasource_tests.cpp
        │   ├── isched_subscription_broker_tests.cpp
        │   ├── isched_tenant_thread_pool_tests.cpp
        │   └── isched_srv_tests.cpp
        └── performance/benchmark_suite.cpp

CMakeLists.txt
conanfile.txt
configure.py
docs/
├── performance.md
└── deployment.md
```

**Structure Decision**: Keep a single C++ project but drop any plan for CLI runtimes or shared-memory coordination. The architecture centers on a GraphQL transport layer, an in-process configuration subsystem, and a subscription broker for WebSocket sessions.

**Terminology note**: `organization` is the canonical product term in the revised architecture. Historical contract/type names that still use `tenant` are equivalent for cross-artifact review unless a reference is explicitly platform-scoped.

## Complexity Tracking

> Fill only if constitutional complexity needs explicit justification.

| Violation | Why Needed | Simpler Alternative Rejected Because |
| --------- | ---------- | ----------------------------------- |
| Per-organization databases | Strong organization isolation and simpler recovery | Shared schemas in one database raise isolation risk |
| WebSocket subscription broker | Required real-time GraphQL transport | HTTP polling violates transport requirement and adds latency |
| Atomic configuration snapshots | Prevent partial schema activation | In-place mutation risks broken active schemas |
| Custom PEGTL GraphQL parser | Full parse-tree control, no library dependency, direct integration with execution context | Third-party parsing library would duplicate internal concerns and add an opaque runtime dependency |
| `isched_system.db` bootstrap database | Platform-level entities (platform_admins, platform_roles, organizations) must exist before any organization DB; cross-org queries would require joins across organization SQLite files | A single organization DB would violate multi-organization isolation; cramming platform entities into one organization DB is unsound |
| AES-256-GCM API key encryption | API keys for outbound HTTP data sources are secrets; storing plaintext in organization SQLite exposes them via any DB-file read | A simpler credential store (e.g. OS keyring) is not portable to the embedded/Raspberry Pi target; AES-GCM with HKDF-derived key is the minimal portable solution |
| Global adaptive thread pool | `cpp-httplib` exposes a single process-wide `set_thread_pool_size()` — there is no per-organization granularity in the transport layer | A custom per-organization thread pool would require a parallel async I/O layer, far exceeding scope; advisory per-organization min/max stored in `TenantConfig` and applied as aggregate |
| Dual performance counter semantics | FR-PERF-003 requires both interval-window counters (reset every 60 min) and cumulative counters (never reset) — a single counter cannot satisfy both requirements | Storing only cumulative counts prevents interval-rate queries; storing only interval counts loses absolute totals needed for capacity planning |

## Phase Plan

### Phase taxonomy crosswalk (plan.md vs tasks.md)

| `plan.md` phase label | `tasks.md` phase label(s) |
| --- | --- |
| Phase 0: Research Update | Phase 1 prerequisites context (setup intent) |
| Phase 1: Design Update | Phase 1, Phase 1b, Phase 1c |
| Phase 2: Implementation Realignment | Phase 2: Foundational GraphQL Runtime |
| Phase 3: Verification | Phase 3: User Story 1 |
| Phase 4: Tenant Isolation | Phase 4: User Story 2 |
| Phase 5: Configuration System | Phase 5 and Phase 5b |
| Phase 6: Security, Auth, Metrics & Integrations | T047-T052 block under Phase 6 |
| Phase 7: Documentation and Security Hardening | Phase 7 (T054-T058) |

This crosswalk is authoritative when labels differ; task IDs and completion state in `tasks.md` remain the execution source of truth.

### Invariant: Test Suite Always Green

**After every task**, the full test suite MUST pass:

```bash
cd cmake-build-debug && ctest --output-on-failure
```

This is a hard gate at every task boundary, not only at phase checkpoints. If a task causes a regression, it MUST fix the broken test in the same change. Moving to the next task while any test is failing is not allowed.

### Invariant: Commit After Every Task

Once a task is complete and `ctest` is fully green, a Git commit MUST be created before moving to the next task. Completed work must never remain uncommitted when starting the next task. The commit message must reference the task ID(s) addressed and note any bugs fixed.

### Invariant: Constitution Compliance Review Evidence

Before feature closeout, review records MUST explicitly confirm constitution compliance for the delivered changes: performance impact, GraphQL compliance, security/isolation behavior, portability constraints, and C++ Core Guidelines adherence (including any documented deviations).

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

- Per-organization SQLite databases with connection pooling
- Tenant lifecycle management (create, activate, suspend, destroy)
- Cross-organization isolation enforced at executor level

### ✅ Phase 5: Configuration System — COMPLETE

- Versioned configuration snapshots in SQLite
- Atomic activation with rollback on validation failure
- Input-object based optimistic concurrency for configuration mutations via required `expectedVersion`
- Schema regeneration from model definitions
- FR-017 closeout clarification: persisted snapshot metadata plus `schemaSdl`/generated SDL and the queryable `configurationHistory` + `activeConfiguration` surfaces are the documented deployment-history/audit trail for configuration rollouts

### ✅ Phase 5b: Subscription Broker — COMPLETE

- `SubscriptionBroker` for WebSocket subscription delivery
- Subscription lifecycle (subscribe, publish, unsubscribe)
- Per-organization subscription isolation

---

### ✅ Phase 6: Security, Auth, Metrics & Integrations — COMPLETE

Full details in `tasks.md` (T047–T052). Summary of sub-areas:

**T047 — RBAC and User/Org Management**
- `isched_system.db` bootstrap database: `platform_admins`, `platform_roles`, `organizations` tables; created by `DatabaseManager` on first startup
- Built-in RBAC concepts: `platform_admin`, `organization_admin`, and `user`, with support for additional scoped custom roles defined by platform admins or organization admins
- RBAC enforced at Query/Mutation resolver level with scope-aware platform and organization authorization; per-org SQLite isolation
- Traceability note: T047 and T049 jointly provide the primary implementation evidence for both the FR-008 authentication/RBAC requirement family and FR-SEC-001 scope-aware JWT authorization
- Dedicated unauthenticated bootstrap mutation `bootstrapPlatformAdmin(input: BootstrapPlatformAdminInput!): AuthPayload!`; enabled only while `platform_admins` is empty, then globally disabled after first successful provisioning
- Additional platform admins are created through authenticated platform account-management workflows over `platform_admins` / `platform_roles`; no second bootstrap path is introduced
- User CRUD mutations; `login` mutation returns JWT; Argon2id via OpenSSL ≥ 3.2 `EVP_KDF_fetch("ARGON2ID")`
- `conanfile.txt` updated to pin `openssl/[>=3.2.0]`

**T048 — Outbound HTTP RESTDataSource**
- `isched_RestDataSource` following Apollo RESTDataSource pattern: per-organization auth config in organization DB
- API key stored encrypted: `isched_CryptoUtils` provides AES-256-GCM encrypt/decrypt; HKDF-derived per-organization key
- JSON auto-coercion; structured `HttpError` type in GraphQL schema
- Column name: `api_key_value_encrypted`

**T049 — Session Management and Revocation**
- Sessions table in organization SQLite; includes `roles` JSON array (populated at login)
- `AuthenticationMiddleware::create_session()` is sole owner; `login` resolver delegates
- 4 session-control mutations: `logout`, `revokeSession`, `revokeAllSessions`, and `terminateAllSessions`; WebSocket force-close on revocation
- `last_activity` updated on validated requests with write-throttling (default 5-minute minimum update interval per session)

**T050 — Global Adaptive Thread Pool**
- Single `cpp-httplib` global pool using hybrid triggers: aggregate active-subscription count as primary signal and response-time threshold breaches as secondary signals
- Per-organization min/max stored as advisory in `TenantConfig`; applied only at aggregate level
- FR-013 policy: scale-up on >=20% aggregate subscription-load delta vs prior window, or p95 latency > 20 ms for 2 consecutive 30-second overload windows; scale-down only after 5 consecutive healthy 30-second windows; minimum 60-second cooldown between resize actions

**T051 — Performance Metrics**
- Dual counter sets: `requestsInInterval` / `errorsInInterval` (reset every 60 min) + `totalRequestsSinceStartup` / `totalErrorsSinceStartup` (cumulative)
- Both Query and Subscription exposed; per-organization and system-level queries
- `isched_MetricsCollector` owns counters; 60-minute interval timer via `std::jthread`

**T052 — Catch2 Benchmark Suite**
- `benchmark_suite.cpp` using `CATCH_BENCHMARK`
- Two-tier protocol: in-process benchmark suite for fast regression checks, plus HTTP-level `/graphql` benchmark profile as the FR-012 acceptance gate (canonical profile in `specs/001-universal-backend/performance-protocol.md`)
- `hello` throughput retains an approximately `1000 req/s` in-process check only as a non-normative local regression guard; it is not the release acceptance criterion
- T052-006: `REQUIRE(p95_latency_ms <= 20.0)` asserting FR-012
- T052-007: results documented in `docs/performance.md` as release-facing summary linked to the canonical profile

---

### ✅ Phase 7: Documentation and Security Hardening — COMPLETE

Full details in `tasks.md` (T055–T057). Summary:

**T055 — Documentation Review**: All docs (`graphql-schema.md`, `http-api.md`, `quickstart.md`, `data-model.md`) reviewed against implementation; `extensions` field documented in `http-api.md`
**T056 — Security Scan & Threat Modeling**: `security_scan` CMake target running clang-tidy with security-oriented checks, plus feature-scoped and project-wide threat-model documentation
**T057 — Deployment Docs**: `docs/deployment.md` covering Linux setup, environment variables, Raspberry Pi / ARM Linux guidance

## Revised Implementation Focus

### Custom PEGTL Parser ✅

- `isched_gql_grammar.hpp` — complete, spec-mapped PEGTL grammar for the full GraphQL document language
- `GqlExecutor` directly invokes PEGTL parsing for execution and SDL validation paths; no separate `GqlParser` facade layer
- Parse-error conversion to GraphQL `errors` array with `locations`
- Grammar conformance tests per spec section
- No third-party GraphQL parsing library used

### Core Runtime ✅

- GraphQL over HTTP request handling
- GraphQL over WebSocket connection lifecycle and subscriptions
- Built-in schema for startup, health, auth, and configuration
- Organization-aware runtime state and connection pooling

### Configuration System ✅

- Versioned configuration snapshots stored internally
- GraphQL mutations for apply, validate, activate, and inspect operations use input objects for configuration-changing calls
- `expectedVersion` is required in configuration mutation input objects and is checked before persistence or activation; mismatches return a conflict without state change
- Atomic activation with rollback on validation failure
- Schema regeneration from model definitions

### Observability ✅

- Server info and health via GraphQL queries
- Optional operational event subscriptions via WebSocket
- Request, error, and subscription metrics framework in place

### ✅ System Database (Phase 6 — T047)

- `isched_system.db` created by `DatabaseManager::ensure_system_db()` on first startup
- Tables: `platform_admins` (`id`, `email`, `password_hash`, `display_name`, `is_active`, `created_at`, `last_login`), `platform_roles` (`id`, `name`, `description`, `created_at`), `organizations` (`id`, `name`, `domain`, `subscription_tier`, `user_limit`, `storage_limit`, `created_at`), plus the reused `sessions` schema for platform-level logins
- Bootstrap provisions the initial `platform_admin` through `bootstrapPlatformAdmin`; availability is derived from whether `platform_admins` is empty, and authenticated RBAC workflows may create additional platform admins and organization admins after startup
- Platform-scoped and organization-scoped custom roles are managed in their respective databases and may be referenced by schema access-control rules

### ✅ Authentication and RBAC (Phase 6 — T047)

- `AuthenticationMiddleware` extended with full JWT validation and role extraction
- Argon2id password hashing via `isched_CryptoUtils::hash_password()` using `EVP_KDF_fetch("ARGON2ID")` (requires OpenSSL ≥ 3.2)
- `bootstrapPlatformAdmin` is the only unauthenticated admin-provisioning path; it creates the initial platform admin, returns `AuthPayload`, and is rejected once any platform admin exists
- `login` mutation: validates credentials, calls `create_session()`, returns signed JWT
- RBAC enforced at resolver level: each Query/Mutation checks minimum required role from JWT claims with explicit platform scope vs organization scope enforcement
- Per-org SQLite isolation: org data lives in its own organization DB, not in `isched_system.db`

### ✅ Session Management and Revocation (Phase 6 — T049)

- Sessions table uses the same schema in each organization DB and in `isched_system.db` for platform-level logins
- `AuthenticationMiddleware::create_session()` is the sole session-creation entry point; `login` resolver delegates to it
- 4 revocation GraphQL mutations: `revokeSession`, `revokeUserSessions`, `revokeOrgSessions`, `terminateAllSessions`
- WebSocket force-close on revocation: `SubscriptionBroker` receives revocation event and closes affected connections
- `last_activity` updates are request-driven but throttled to reduce write amplification (default 5-minute minimum interval per session)
- `roles` column stored as JSON array to enable `terminateAllSessions` without cross-table joins

### ✅ Outbound HTTP — RESTDataSource (Phase 6 — T048)

- `isched_RestDataSource`: per-organization outbound HTTP client following Apollo RESTDataSource pattern
- Auth config stored per data-source in organization SQLite; supports `Authorization: Bearer`, API key, and unauthenticated modes
- API key encrypted at rest: `isched_CryptoUtils::encrypt_aes_gcm()` / `decrypt_aes_gcm()` with HKDF-derived per-organization key
- JSON auto-coercion; structured `HttpError` type surfaced through GraphQL `errors` extensions
- Unit tests in `isched_rest_datasource_tests.cpp`

### ✅ Performance Metrics (Phase 6 — T051)

- `isched_MetricsCollector` owns all performance counters
- Interval-window counters: `requestsInInterval`, `errorsInInterval` — reset every 60 minutes via `std::jthread`
- Cumulative counters: `totalRequestsSinceStartup`, `totalErrorsSinceStartup` — never reset
- Exposed via both GraphQL Query and Subscription; per-organization and system-level views
- Auth-gated: requires `organization_admin` (or an equivalent delegated organization-scoped role) for organization metrics; `platform_admin` (or an equivalent delegated platform-scoped role) for system metrics

### ✅ Benchmarks and Security Hardening (Phase 6/7 — T052, T056)

- `benchmark_suite.cpp` uses `CATCH_BENCHMARK` for in-process regression guards covering `hello` throughput, concurrent GraphQL POST, WebSocket fan-out, introspection under load, and local p95 latency
- The `hello` throughput check remains a non-normative regression guard (`~1000 req/s` target); normative acceptance remains the HTTP-level protocol in `performance-protocol.md`
- `REQUIRE(p95_latency_ms <= 20.0)` asserting the fast local FR-012 regression guard
- Results documented in `docs/performance.md`
- `security_scan` CMake target running clang-tidy with security-oriented checks (T056)
- Threat-model deliverables maintained at `specs/001-universal-backend/threat-model.md` and `docs/security-threat-model.md`

## Current Planning Status

**Status**: Implementation for Phases 0-7 is complete and constitution closeout evidence is recorded.
**Implementation phases complete**: 0, 1, 1b, 1c, 2, 3, 4, 5, 5b, 6, 7.
**Architecture decisions recorded in `tasks.md`**: T047-T052 (Phase 6) and T055-T057 (Phase 7) are fully expanded from stubs through `speckit.clarify` + `speckit.analyze` sessions.
**Next Action**: Re-run final cross-artifact consistency pass and prepare final sign-off commit.
**Gate**: Each sub-task committed individually; `ctest --output-on-failure` must be 100% green before each commit.

## Constitution Review Evidence (Closeout Record)

This section is the authoritative closeout evidence record for constitution compliance review.

- **Closeout Status**: Evidence captured and approved for feature sign-off
- **Required Evidence Format**: each evidence line MUST include an artifact path plus a concrete reference (`commit SHA`, review note ID, or command/report link)
- **Review Date**: 2026-04-04 (`RVW-2026-04-04-001`)
- **Reviewer(s)**: Isched Development Team (`RVW-2026-04-04-001`)
- **Scope Reviewed**: `spec.md`, `plan.md`, `tasks.md`, `data-model.md`, threat-model docs, and key implementation/test changes
- **Performance Compliance Evidence**: `specs/001-universal-backend/performance-protocol.md`, `docs/performance.md`, `specs/001-universal-backend/closeout-validation.md` (`RVW-2026-04-04-002`, `SHA 1004c583edf2687119e7cd18f53ef4573422dd6b`)
- **GraphQL Compliance Evidence**: `src/test/cpp/isched/isched_gql_executor_tests.cpp`, `src/test/cpp/isched/isched_grammar_tests.cpp`, `specs/001-universal-backend/contracts/graphql-schema.md` (`RVW-2026-04-04-003`, `SHA 1004c583edf2687119e7cd18f53ef4573422dd6b`)
- **Security & Isolation Evidence**: `specs/001-universal-backend/threat-model.md`, `docs/security-threat-model.md`, `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`, `src/test/cpp/integration/test_session_revocation.cpp` (`RVW-2026-04-04-004`, `SHA 1004c583edf2687119e7cd18f53ef4573422dd6b`)
- **Portability Evidence**: `README.md`, `docs/deployment.md`, `conanfile.txt`, `CMakeLists.txt` (`RVW-2026-04-04-005`, `SHA 1004c583edf2687119e7cd18f53ef4573422dd6b`)
- **C++ Core Guidelines Evidence**: `.clang-tidy`, `CMakeLists.txt` `security_scan` target (`RVW-2026-04-04-006`, `SHA 1004c583edf2687119e7cd18f53ef4573422dd6b`)
- **SC-001 Quickstart Timing Evidence**: `specs/001-universal-backend/quickstart.md`, `src/test/cpp/integration/test_server_startup.cpp`, `specs/001-universal-backend/closeout-validation.md` (`RVW-2026-04-04-007`)
- **SC-004 Activation Latency Evidence**: `src/test/cpp/integration/test_schema_activation.cpp`, `specs/001-universal-backend/closeout-validation.md` (`RVW-2026-04-04-008`)
- **Approval Decision**: approved

