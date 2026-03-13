# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2026-03-12 | **Spec**: [Universal Application Server Backend](spec.md)
**Input**: Revised feature specification from `/specs/001-universal-backend/spec.md`

**Note**: This plan supersedes the earlier scripting and IPC-oriented design. The implementation baseline is now a GraphQL-only HTTP/WebSocket backend.

## Summary

Create a GraphQL-native application server that exposes only GraphQL over HTTP and WebSocket, removes scripting runtimes and IPC completely, persists configuration through GraphQL mutations, uses SQLite per tenant, and delivers built-in authentication, schema management, subscriptions, and documentation generation within a single C++23 runtime.

## Technical Context

**Language/Version**: C++23  
**Build System**: CMake 3.22.6 + Ninja 1.12.1 (both provided via Conan `[tool_requires]`)  
**Dependency Manager**: Conan 2.x — all dependencies declared in `conanfile.txt`; generators `CMakeDeps` + `CMakeToolchain`  
**Primary Dependencies**: `taocpp-pegtl`, `nlohmann_json`, `spdlog`, `jwt-cpp`, `sqlite3`, `boost/1.84.0` (Boost.URL), `cpp-httplib` (sole HTTP/WebSocket transport), `openssl`, `platformfolders`  
**Architecture**: Single-process, tenant-aware runtime with GraphQL over HTTP and WebSocket  
**Storage**: SQLite per tenant with pooled connections and persisted configuration snapshots  
**Memory Management**: Mandatory smart pointers (`std::unique_ptr`, `std::shared_ptr`)  
**Testing**: Catch2 3.x unit/integration tests  
**Documentation**: Automated Doxygen source documentation via `cmake --build . --target docs`  
**Target Platform**: Linux primary, cross-platform portability documented  
**Project Type**: High-performance GraphQL backend server  
**Performance Goals**: 20ms response times, thousands of concurrent clients, bounded subscription overhead  
**Constraints**: GraphQL-only external interface, no scripting, no IPC, secure-by-default, C++ Core Guidelines  
**Scale/Scope**: Multi-tenant backend with built-in auth, schema configuration, and subscriptions

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
│   │   ├── isched_SubscriptionBroker.hpp/cpp    # (to be created — Phase 5)
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
        │   ├── test_builtin_schema.cpp          # (to be created — T017)
        │   ├── test_health_queries.cpp          # (to be created — T018)
        │   ├── test_configuration_snapshots.cpp # (to be created — T026)
        │   ├── test_schema_activation.cpp       # (to be created — T027)
        │   ├── test_configuration_rollback.cpp  # (to be created — T028)
        │   ├── test_graphql_websocket.cpp       # (to be created — T037)
        │   ├── test_graphql_subscriptions.cpp   # (to be created — T038)
        │   └── test_client_compatibility.cpp    # (to be created — T039)
        ├── isched/
        │   ├── isched_grammar_tests.cpp         # Grammar conformance tests
        │   ├── isched_gql_executor_tests.cpp    # GqlExecutor unit tests
        │   ├── isched_ast_node_tests.cpp        # AST node helper tests
        │   ├── isched_auth_tests.cpp            # AuthenticationMiddleware tests
        │   ├── isched_config_tests.cpp          # Configuration system tests
        │   ├── isched_database_test.cpp         # DatabaseManager unit tests
        │   ├── isched_fs_utils_tests.cpp        # Filesystem utility tests
        │   ├── isched_graphql_tests.cpp         # GqlExecutor end-to-end tests
        │   ├── isched_multi_dim_map_tests.cpp   # multi_dim_map container tests
        │   ├── isched_pattern_match_tests.cpp   # match_pattern() tests
        │   ├── isched_resolver_tests.cpp        # GqlParser resolver integration tests
        │   ├── isched_rest_hello_world.cpp      # Legacy Restbed sanity test (legacy)
        │   ├── isched_srv_tests.cpp             # Server HTTP integration tests
        │   ├── isched_test_run_listener.cpp     # Catch2 event listener
        │   └── isched_ipc_tests.cpp             # IPC tests (legacy)
        └── performance/
            └── benchmark_suite.cpp             # (to be created — T052)

CMakeLists.txt
conanfile.txt
configure.py                                    # One-shot Conan + CMake + Ninja build script
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

## Phase Plan

### Invariant: Test Suite Always Green

**After every task**, the full test suite MUST pass:

```bash
cd cmake-build-debug && ctest --output-on-failure
```

This is a hard gate at every task boundary, not only at phase checkpoints. If a task causes a regression, it MUST fix the broken test in the same change. Moving to the next task while any test is failing is not allowed.

### Invariant: Commit After Every Task

Once a task is complete and `ctest` is fully green, a Git commit MUST be created before moving to the next task. Completed work must never remain uncommitted when starting the next task. The commit message must reference the task ID(s) addressed and note any bugs fixed.

### Phase 0: Research Update

- Remove scripting, IPC, and out-of-process execution assumptions
- Confirm GraphQL-only transport requirements for HTTP and WebSocket
- Validate versioned configuration snapshot approach
- Document PEGTL custom parser as a mandatory, first-class subsystem

### Phase 1: Design Update

- Define built-in GraphQL schema for startup, health, auth, and configuration
- Define configuration snapshot, model-definition, and subscription-session entities
- Define transport contracts for HTTP and WebSocket

### Phase 1b: PEGTL Grammar Completion

- Audit `isched_gql_grammar.hpp` against the full GraphQL specification
- Implement missing grammar rules: fragments (`FragmentDefinition`, `FragmentSpread`, `InlineFragment`), remaining type-system definitions (`InterfaceTypeDefinition`, `UnionTypeDefinition`, `EnumTypeDefinition`, `InputObjectTypeDefinition`), and type extension rules
- Ensure `Selection` handles all three alternatives (Field, FragmentSpread, InlineFragment)
- Add negative and spec-derived conformance tests in `isched_grammar_tests.cpp`
- Implement parse-error conversion from PEGTL exceptions to GraphQL error objects with location info
- Connect `GqlParser` to `GqlExecutor` so all incoming queries go through the custom grammar

### Phase 2: Implementation Realignment

- Align server transport around `/graphql` only
- Add or complete WebSocket subscription handling
- Replace any scripting-oriented code paths with configuration mutation handling
- Remove IPC assumptions from runtime planning and tasking

### Phase 3: Verification

- GraphQL over HTTP integration coverage
- GraphQL over WebSocket integration coverage
- Configuration rollback and schema migration coverage
- Performance and security regression coverage

## Revised Implementation Focus

### Custom PEGTL Parser

- `isched_gql_grammar.hpp` — complete, spec-mapped PEGTL grammar for the full GraphQL document language
- `GqlParser` facade — stable `parse(string, name)` entry point used by `GqlExecutor` and SDL subsystems
- Parse-error conversion to GraphQL `errors` array with `locations`
- Grammar conformance tests per spec section
- No third-party GraphQL parsing library used

### Core Runtime

- GraphQL over HTTP request handling
- GraphQL over WebSocket connection lifecycle and subscriptions
- Built-in schema for startup, health, auth, and configuration
- Tenant-aware runtime state and connection pooling

### Configuration System

- Versioned configuration snapshots stored internally
- GraphQL mutations for apply, validate, activate, and inspect operations
- Atomic activation with rollback on validation failure
- Schema regeneration from model definitions

### Observability

- Server info and health via GraphQL queries
- Optional operational event subscriptions via WebSocket
- Request, error, and subscription metrics captured internally

## Current Planning Status

**Status**: Revised planning baseline complete.  
**Impact**: Existing implementation and prior tasks must be measured against the revised GraphQL-only scope before further delivery.  
**Next Action**: Execute the revised task breakdown in `tasks.md` and remove any obsolete IPC or scripting-oriented implementation paths.
