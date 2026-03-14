# Tasks: Universal Application Server Backend

**Input**: Revised design documents from `/specs/001-universal-backend/`  
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`  
**Tests**: Test tasks included based on constitutional TDD requirements.  
**Organization**: Tasks are grouped by user story to match the revised GraphQL-only scope.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel
- **[Story]**: User story association (`US1`, `US2`, `US3`)
- Exact file paths are included where practical

## Path Conventions

- **Implementation**: `src/main/cpp/`
- **Tests**: `src/test/cpp/`
- **Contracts and planning**: `specs/001-universal-backend/`

## Constitutional Compliance Checklist

Each task implementation MUST verify:

- ✅ **Tests pass**: `cd cmake-build-debug && ctest --output-on-failure` MUST exit 0 after every task is marked done. This is a hard gate — no task is complete while any test is failing.
- ✅ **Performance**: HTTP and WebSocket performance remain within targets
- ✅ **GraphQL Spec**: Behavior matches GraphQL and transport expectations
- ✅ **Security**: Secure-by-default auth and tenant isolation
- ✅ **Testing**: TDD, HTTP integration tests, WebSocket integration tests, performance coverage
- ✅ **Portability**: Linux/Conan compatibility maintained
- ✅ **C++ Core Guidelines**: Smart-pointer ownership and justified deviations only

> **Rule**: If implementing a task causes an existing test to fail, the task MUST either fix the broken test in the same change or revert the breaking change. Leaving a passing test broken while moving to the next task is not allowed.

---

## Phase 1: Setup

**Purpose**: Maintain the base project structure and toolchain for the revised scope.

- [x] T001 Create and maintain the C++23 project structure in `src/main/cpp/isched/`
- [x] T002 [P] Configure `CMakeLists.txt` with C++23 and Conan integration
- [x] T003 [P] Maintain Conan dependencies for GraphQL, storage, auth, logging, and testing
- [x] T004 [P] Configure clang-tidy or equivalent guideline checks
- [x] T005 [P] Maintain Doxygen integration in `CMakeLists.txt`

---

## Phase 1b: Custom PEGTL GraphQL Parser

**Purpose**: The server MUST implement its own PEGTL-based GraphQL parser. No third-party GraphQL parsing library is used. This phase tracks grammar completeness and parser integration.

**Grammar file**: `src/main/cpp/isched/backend/isched_gql_grammar.hpp`  
**Parser facade**: `src/main/cpp/isched/backend/isched_GqlParser.hpp/.cpp`  
**Tests**: `src/test/cpp/isched/isched_grammar_tests.cpp`

### Grammar completeness

- [x] T-GQL-001 [P] Implement lexical rules: `Whitespace`, `LineTerminator`, `Comment`, `Comma`, `Ignored`, `UnicodeBOM`, `Name`, `Token`, `Punctuator`, `Ellipsis`
- [x] T-GQL-002 [P] Implement numeric literals: `IntValue`, `FloatValue` (sign, fractional, exponent, follow-restrictions)
- [x] T-GQL-003 [P] Implement string literals: quoted strings, escape sequences, block strings (`"""…"""`), `BlockStringCharacter`
- [x] T-GQL-004 [P] Implement type-reference rules: `NamedType`, `ListType`, `NonNullType`, built-in scalars
- [x] T-GQL-005 [P] Implement value rules: `Variable`, `IntValue`, `FloatValue`, `StringValue`, `BooleanValue`, `NullValue`, `EnumValue`, `ListValue`, `ObjectValue`, `DefaultValue`
- [x] T-GQL-006 [P] Implement type-system definition rules: `ScalarTypeDefinition`, `ObjectTypeDefinition`, `FieldDefinition`, `FieldsDefinition`, `ArgumentsDefinition`, `InputValueDefinition`, `Description`
- [x] T-GQL-007 [P] Implement directive rules: `DirectivesConst`, `Directive`, `DirectiveDefinition`
- [x] T-GQL-008 [P] Implement executable definition rules: `OperationType`, `SelectionSet`, `Field`, `Alias`, `Arguments`, `GqlQuery`, `VariableDefinitions`, `VariableDefinition`
- [x] T-GQL-009 [P] Implement schema definition rules: `SchemaDefinition`, `RootOperationTypeDefinition`
- [x] T-GQL-010 [P] Implement top-level `Document` entry point
- [x] T-GQL-011 [P] Implement fragment rules: `FragmentDefinition`, `FragmentSpread`, `InlineFragment`, `TypeCondition`
- [x] T-GQL-012 [P] Implement remaining type-system definitions: `InterfaceTypeDefinition`, `UnionTypeDefinition`, `EnumTypeDefinition`, `InputObjectTypeDefinition`
- [x] T-GQL-013 [P] Implement type extension rules: `ObjectTypeExtension`, `InterfaceTypeExtension`, `UnionTypeExtension`, `EnumTypeExtension`, `InputObjectTypeExtension`, `ScalarTypeExtension`
- [x] T-GQL-014 [P] Ensure `Selection` rule matches all three variants: `Field`, `FragmentSpread`, `InlineFragment`

### Parser integration

- [x] T-GQL-020 [P] Remove `isched_GqlParser.hpp` and `isched_GqlParser.cpp` — `GqlExecutor` owns PEGTL grammar invocation directly via `isched_gql_grammar.hpp`; also remove `IGdlParserTree` if it is only referenced by `GqlParser`
- ~~T-GQL-021~~ **Eliminated** — `GqlExecutor` already calls PEGTL directly; no separate `GqlParser` integration layer is needed
- [x] T-GQL-022 [P] Verify parse-error conversion in `GqlExecutor`: PEGTL parse errors MUST become standards-compliant GraphQL error objects with `message` and `locations` before reaching transport (no `GqlParser` layer required)
- [ ] T-GQL-023 [P] Use `GqlExecutor`'s PEGTL grammar for SDL schema validation in the configuration snapshot subsystem (not a regex or string-match approach)

### Grammar test coverage

- [x] T-GQL-030 Maintain positive-case grammar tests in `isched_grammar_tests.cpp` for: lexical elements, numeric literals, string literals, type system, executable queries, schema documents
- [x] T-GQL-031 [P] Add negative/rejection tests for every major grammar group (invalid tokens, malformed strings, bad numeric formats, unclosed braces)
- [x] T-GQL-032 [P] Add spec-derived conformance tests: multi-level selection sets, inline fragments, named fragments, mutations, subscriptions, operation variables, aliases
- [x] T-GQL-033 [P] Add parse-error-message tests: verify location information (`line`, `column`) and message text in `IGdlParserTree` rejection results

**Checkpoint**: Custom PEGTL parser is complete, owned directly by `GqlExecutor`, `GqlParser` facade removed, standards-compliant parse errors verified, grammar conformance tests pass. `ctest` MUST be green before moving to Phase 1c.

---

## Phase 1c: GraphQL Execution Engine — Field Resolution Correctness

**Purpose**: The current `GqlExecutor` sub-field dispatch path contains three structural defects: parent values are never forwarded to sub-resolvers; sub-selection results are placed at the wrong JSON nesting level; and the default field resolver (extract `parent[field_name]`) is not wired into the dispatch path. These defects must be corrected and covered by unit tests before the higher-level runtime work in Phase 2 begins.

### Fix Sub-Resolver Dispatch

- [x] T-EXEC-001 [P] Extend `resolve_field_selection_details()` to accept a `const json& p_parent` parameter and forward it to every resolver call — replace the `json::object()` placeholder with the actual resolved parent value
- [x] T-EXEC-002 [P] Fix sub-selection result placement: after resolving a field to `my_result`, create `p_result[fieldName]` as the container and write sub-field results into it, so `{ a { b } }` produces `{"a": {"b": ...}}` rather than `{"a": ..., "b": ...}`; update `process_field_sub_selections()` and `process_sub_selection()` accordingly
- [x] T-EXEC-003 [P] Implement the **default field resolver**: when `ResolverRegistry::get_resolver()` finds no explicit resolver for a sub-field and `parent_value` is a JSON object containing the field's key, return `parent_value[field_name]`; when the key is absent and there is no explicit resolver, emit `MISSING_GQL_RESOLVER`
- [x] T-EXEC-004 [P] Remove the `json my_args=json::object(); //<TODO` placeholder in `process_sub_selection()` and implement proper argument extraction for each sub-field before dispatch into `process_field_selection()`
- [x] T-EXEC-005 [P] Thread `p_parent` consistently through the full call chain: `process_field_selection(p_parent, ...) → resolve_field_selection_details(..., p_parent, ...) → resolver(p_parent, args, ctx)` — no step in the chain may silently drop the parent value

### Sub-Resolver Unit Tests

- [x] T-EXEC-006 [P] Add unit tests to `isched_gql_executor_tests.cpp` covering all cases required by FR-EXEC-006:
  - Flat query (existing coverage, must remain green)
  - Single-level nested sub-selection with an explicit sub-resolver receiving the correct parent value
  - Default field resolver: parent resolver returns `{version:"1.0"}`, `version` sub-field is extracted without any explicit resolver registered
  - Multi-level nesting: `{ a { b { c } } }` produces `{"a":{"b":{"c":...}}}` at all levels
  - Sub-resolver receives the correct non-empty parent value (assert the parent arg equals the parent resolver's return value)
  - Error propagation: a failing sub-resolver sets that field to null and populates `errors`, sibling fields still resolve
  - Argument passing: arguments supplied in the query reach the sub-resolver's `p_args` parameter
  - Missing default resolver: parent resolves to `{}`, sub-field `x` has no explicit resolver and parent has no key `x` → `MISSING_GQL_RESOLVER` error appears in `errors`
  - Error `path` array elements: string for named fields, int for list indices (mixed type from first implementation)

**Checkpoint**: Sub-resolver dispatch is correct, default field resolver is wired, results are correctly nested in the response, and all T-EXEC-006 test variants pass. `ctest` MUST be green before moving to Phase 2.

---

## Phase 2: Foundational GraphQL Runtime

**Purpose**: Core infrastructure that must exist before user stories can be completed.

- [x] T006 [P] Replace `restbed` with `cpp-httplib` as the sole HTTP/WebSocket library:
  - Rewrite `isched_Server.hpp/cpp` to use `httplib::Server` for HTTP POST `/graphql` and `httplib::Server::set_pre_routing_handler` / WebSocket upgrade for WebSocket connections to `/graphql`
  - Remove `restbed` from `conanfile.txt`
  - Delete `isched_BaseRestResolver.hpp/cpp`, `isched_DocRootRestResolver.hpp/cpp`, `isched_SingleActionRestResolver.hpp/cpp`, `isched_DocRootSvc.hpp/cpp`, `isched_EHttpMethods.hpp`
  - Replace `isched_MainSvc.hpp/cpp` with a minimal cpp-httplib bootstrap (or delete if `isched_Server` absorbs the role)
  - Populate `ResolverCtx` with `tenant_id` (string), `db` (pointer to tenant DB connection), and `current_user_id` (string); update all resolver call sites
  - Verify `ctest` is green after removal
- [ ] T007 [P] Refactor `isched_TenantManager.hpp/cpp` for in-process tenant isolation rather than process management
- [ ] T008 [P] Complete `isched_DatabaseManager.hpp/cpp` for tenant-scoped SQLite storage and connection pooling
- [ ] T009 [P] Complete `ConnectionPool` behavior in `isched_DatabaseManager.hpp/cpp`
- [ ] T010 [P] Complete `isched_GqlExecutor.hpp/cpp` for query, mutation, and schema execution
- [ ] T011 [P] Complete `isched_AuthenticationMiddleware.hpp/cpp` for real JWT validation and session handling
- [ ] T012 [P] Add a subscription broker implementation in `src/main/cpp/isched/backend/isched_SubscriptionBroker.hpp/.cpp` *(to be created)* for WebSocket subscriptions
- [ ] T013 [P] Implement GraphQL HTTP transport at `/graphql` and remove non-GraphQL transport assumptions
- [ ] T014 [P] Refactor `src/main/cpp/isched/shared/config/` around configuration snapshots instead of scripts
- [ ] T015 [P] Ensure Catch2 and transport-level test wiring is correct in `src/test/cpp/`

**Checkpoint**: Foundation ready for HTTP queries, WebSocket subscriptions, auth, and configuration snapshots. `ctest` MUST be green before moving to Phase 3.

---

## Phase 3: User Story 1 - Immediate GraphQL Startup (Priority: P1)

**Goal**: Frontend developers can start the server and use built-in GraphQL immediately over HTTP.

### Tests for User Story 1

- [ ] T016 [P] [US1] Add integration test for server startup and `/graphql` availability in `src/test/cpp/integration/test_server_startup.cpp` *(exists — extend with real transport assertions)*
- [ ] T017 [P] [US1] Add integration test for built-in GraphQL queries in `src/test/cpp/integration/test_builtin_schema.cpp` *(to be created)*
- [ ] T018 [P] [US1] Add integration test for GraphQL-based health and server info queries in `src/test/cpp/integration/test_health_queries.cpp` *(to be created)*

### Implementation for User Story 1

- [ ] T019 [P] [US1] Implement built-in GraphQL schema and resolvers for `hello`, `version`, `uptime`, `serverInfo`, and `health`
- [ ] T020 [P] [US1] Route HTTP GraphQL requests through the real executor in `isched_Server.cpp`
- [ ] T021 [US1] Enforce GraphQL as the only external interface in server routing and documentation
- [ ] T022 [US1] Remove or disable REST-style health and management endpoint expectations from the runtime
- [ ] T023 [US1] Implement GraphQL over HTTP request parsing, validation, and serialization
- [ ] T024 [US1] Implement enhanced GraphQL error formatting with request IDs and extensions
- [ ] T025 [US1] Document startup and built-in query behavior in user-facing docs

**Checkpoint**: User Story 1 is independently testable over HTTP. `ctest` MUST be green before moving to Phase 4.

---

## Phase 4: User Story 2 - GraphQL-Native Configuration (Priority: P2)

**Goal**: Backend behavior is configured through GraphQL mutations with no scripting and no IPC.

### Tests for User Story 2

- [ ] T026 [P] [US2] Add integration test for configuration snapshot creation in `src/test/cpp/integration/test_configuration_snapshots.cpp` *(to be created)*
- [ ] T027 [P] [US2] Add integration test for schema updates after configuration mutation in `src/test/cpp/integration/test_schema_activation.cpp` *(to be created)*
- [ ] T028 [P] [US2] Add integration test for configuration rollback on invalid updates in `src/test/cpp/integration/test_configuration_rollback.cpp` *(to be created)*

### Implementation for User Story 2

- [ ] T029 [P] [US2] Implement `ConfigurationSnapshot` persistence in `src/main/cpp/isched/shared/config/`
- [ ] T030 [P] [US2] Implement `applyConfiguration` and related mutations in `isched_GqlExecutor.cpp`
- [ ] T031 [P] [US2] Implement model-definition persistence and schema generation support
- [ ] T032 [P] [US2] Implement atomic activation and rollback for configuration snapshots
- [ ] T033 [P] [US2] Implement safe schema migration and backup handling in `isched_DatabaseManager.cpp`
- [ ] T034 [US2] Integrate configuration activation with tenant runtime refresh in `isched_Server.cpp` and `isched_TenantManager.cpp`
- [ ] T035 [US2] Implement queryable configuration history and active configuration resolvers
- [ ] T036 [US2] Replace obsolete scripting assumptions in docs and runtime comments

**Checkpoint**: User Story 2 is independently testable with mutation-driven configuration. `ctest` MUST be green before moving to Phase 5.

---

## Phase 5: User Story 3 - Real-Time GraphQL Transport (Priority: P3)

**Goal**: Clients can use GraphQL subscriptions over WebSocket with standards-compatible behavior.

### Tests for User Story 3

- [ ] T037 [P] [US3] Add WebSocket integration test for `graphql-transport-ws` connection lifecycle in `src/test/cpp/integration/test_graphql_websocket.cpp` *(to be created)*
- [ ] T038 [P] [US3] Add integration test for configuration and health subscriptions in `src/test/cpp/integration/test_graphql_subscriptions.cpp` *(to be created)*
- [ ] T039 [P] [US3] Add interoperability test for GraphQL HTTP plus WebSocket clients in `src/test/cpp/integration/test_client_compatibility.cpp` *(to be created)*

### Implementation for User Story 3

- [ ] T040 [P] [US3] Implement complete GraphQL introspection in `isched_GqlExecutor.cpp` — see Phase 5b below for detailed breakdown
- [ ] T041 [P] [US3] Add GraphQL query complexity and depth analysis
- [ ] T042 [P] [US3] Implement WebSocket `/graphql` endpoint and subscription session lifecycle
- [ ] T043 [P] [US3] Implement subscription broker fan-out and disconnect cleanup
- [ ] T044 [US3] Add GraphQL compliance validation for HTTP and WebSocket transport behavior
- [ ] T045 [US3] Implement authentication during WebSocket `connection_init`
- [ ] T046 [US3] Add subscription event types for configuration and health changes

**Checkpoint**: User Story 3 is independently testable over WebSocket. `ctest` MUST be green before moving to Phase 6.

---

## Phase 5b: Full GraphQL Introspection (Standard Client Interoperability)

**Purpose**: Replace the current skeleton `generate_schema_introspection()` with a spec-compliant implementation so that standard GraphQL tools (GraphiQL, Apollo Sandbox, Altair, code-generation clients) function correctly against the running server.

**Implementation file**: `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`  
**Test file**: `src/test/cpp/isched/isched_gql_executor_tests.cpp`  
**Spec reference**: GraphQL specification, Section "Introspection"

### Introspection data model

- [ ] T-INTRO-001 Define an internal `IntrospectionType` struct (or equivalent) in `GqlExecutor` that holds `kind`, `name`, `description`, `fields`, `interfaces`, `possibleTypes`, `enumValues`, `inputFields`, and `ofType` — populated from the active schema parse tree during `load_schema()` / `update_type_map()`
- [ ] T-INTRO-002 Populate built-in scalar types (`String`, `Int`, `Float`, `Boolean`, `ID`) unconditionally in the introspection model regardless of whether they appear in the active schema
- [ ] T-INTRO-003 Represent wrapped types (`LIST`, `NON_NULL`) as recursive `ofType` chains rather than flattened strings; ensure a field typed `[String!]!` resolves to `NON_NULL → LIST → NON_NULL → SCALAR(String)`
- [ ] T-INTRO-004 Populate introspection meta-types themselves (`__Schema`, `__Type`, `__TypeKind`, `__Field`, `__InputValue`, `__EnumValue`, `__Directive`, `__DirectiveLocation`) in the types list
- [ ] T-INTRO-005 Refresh the introspection model whenever a configuration snapshot is activated so `__schema` results reflect the current live schema

### `__schema` root field

- [ ] T-INTRO-010 Emit correct `queryType`, `mutationType`, `subscriptionType` names (not always `null`) based on the loaded schema's schema definition
- [ ] T-INTRO-011 Emit all types (user-defined + built-in scalars + meta-types) in `__schema { types }`
- [ ] T-INTRO-012 Emit correct `kind` for each type: `OBJECT`, `SCALAR`, `INTERFACE`, `UNION`, `ENUM`, `INPUT_OBJECT`, `LIST`, `NON_NULL` — remove the hard-coded `"OBJECT"` default
- [ ] T-INTRO-013 Populate `fields` correctly for `OBJECT` and `INTERFACE` types; return `null` for other kinds
- [ ] T-INTRO-014 Populate `inputFields` for `INPUT_OBJECT` types; return `null` for other kinds
- [ ] T-INTRO-015 Populate `enumValues` for `ENUM` types; return `null` for other kinds
- [ ] T-INTRO-016 Populate `interfaces` for `OBJECT` types; return `null` or empty for other kinds
- [ ] T-INTRO-017 Populate `possibleTypes` for `INTERFACE` and `UNION` types; return `null` for other kinds
- [ ] T-INTRO-018 Populate `isDeprecated` and `deprecationReason` on `__Field`, `__InputValue`, and `__EnumValue` (derive from `@deprecated` directive on the definition node)

### `__type(name:)` root field

- [ ] T-INTRO-020 Implement the `__type(name: String!)` root field in the execution dispatch path
- [ ] T-INTRO-021 Return `null` for unknown type names without raising an execution error

### `__typename` meta-field

- [ ] T-INTRO-025 Dispatch `__typename` as a special field in the selection-set executor; return the runtime type name of the current object
- [ ] T-INTRO-026 Support `__typename` in nested selection sets, not only at the query root

### Built-in directives

- [ ] T-INTRO-030 Include `@skip`, `@include`, `@deprecated`, and `@specifiedBy` in the `directives` array returned by `__schema`; populate correct `locations` and `args` for each
- [ ] T-INTRO-031 Populate `isRepeatable` on `__Directive` objects

### Test coverage (un-comment and extend `isched_gql_executor_tests.cpp`)

- [ ] T-INTRO-040 Un-comment all commented-out assertions in the existing "GraphQL Introspection" `TEST_CASE`; every assertion MUST pass
- [ ] T-INTRO-041 [P] Add test: `__schema { types }` contains all five built-in scalars by name and kind `SCALAR`
- [ ] T-INTRO-042 [P] Add test: `__schema { types }` contains user-defined `OBJECT` type with correct `fields`, `name`, and `description`
- [ ] T-INTRO-043 [P] Add test: `__schema { types }` contains user-defined `INPUT_OBJECT` type with correct `inputFields`
- [ ] T-INTRO-044 [P] Add test: `__schema { types }` contains user-defined `ENUM` type with correct `enumValues`
- [ ] T-INTRO-045 [P] Add test: `__type(name: "User")` returns correct `__Type` for a user-defined object type
- [ ] T-INTRO-046 [P] Add test: `__type(name: "NonExistent")` returns `null` without error
- [ ] T-INTRO-047 [P] Add test: field of type `[String!]!` produces `ofType` chain `NON_NULL → LIST → NON_NULL → SCALAR`
- [ ] T-INTRO-048 [P] Add test: `__typename` returns correct type name in a nested selection set
- [ ] T-INTRO-049 [P] Add test: `__schema { directives }` contains `@skip`, `@include`, `@deprecated` with correct `locations` and `args`
- [ ] T-INTRO-050 [P] Add test: `@deprecated` on a field sets `isDeprecated: true` and `deprecationReason` in introspection
- [ ] T-INTRO-051 [P] Add test: `__schema { queryType { name } }` returns `"Query"` when a `Query` type is defined

**Checkpoint**: Standard GraphQL tools (GraphiQL, Altair, Apollo Sandbox) can connect to the server, load and browse the full schema, and auto-complete queries without errors. `ctest` MUST be green before closing Phase 5b.

---

## Phase 6: Data Model, Performance, and Auth Completion

**Purpose**: Cross-cutting capabilities required after the GraphQL-only baseline is functional.

- [ ] T047 [P] Implement full tenant-scoped user and organization persistence
- [ ] T048 [P] Implement outbound HTTP integration resolvers while preserving GraphQL as the only client-facing interface
- [ ] T049 [P] Complete per-tenant session management and revocation in `isched_AuthenticationMiddleware.cpp`
- [ ] T050 [P] Add adaptive worker-thread and subscription resource controls in `isched_TenantManager.cpp`
- [ ] T051 [P] Add performance metrics surfaced through GraphQL queries or subscriptions
- [ ] T052 [P] Add performance benchmark coverage in `src/test/cpp/performance/benchmark_suite.cpp` *(to be created)*

---

## Phase 7: Polish and Hardening

**Purpose**: Production hardening for the revised architecture.

- ~~T053~~ **Superseded by T006** — REST file deletion and CMakeLists.txt cleanup is fully covered by T006 in Phase 2
- [x] T054 [P] Remove legacy IPC files: `shared/ipc/isched_ipc.hpp/cpp`, `src/test/cpp/isched/isched_ipc_tests.cpp`, `src/test/cpp/isched/isched_rest_hello_world.cpp` — and all CMakeLists.txt references to them
- [ ] T055 [P] Review docs and generated references for GraphQL-only terminology consistency
- [ ] T056 [P] Add security hardening and vulnerability scanning integration
- [ ] T057 [P] Add deployment documentation for HTTP and WebSocket operation

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1**: Baseline project tooling
- **Phase 2**: Blocks all user stories
- **Phase 3**: Depends on foundational GraphQL runtime
- **Phase 4**: Depends on foundational GraphQL runtime and should follow Phase 3 for MVP sequencing
- **Phase 5**: Depends on HTTP runtime, auth, and subscription broker foundations
- **Phase 6-7**: Depend on desired user stories being functional

### User Story Dependencies

- **User Story 1 (P1)**: No dependency on other user stories
- **User Story 2 (P2)**: Depends on the HTTP GraphQL runtime and auth foundations
- **User Story 3 (P3)**: Depends on HTTP runtime, auth, and subscription broker foundations

### Within Each User Story

- Tests must be written and fail before implementation
- Transport behavior should be exercised end-to-end, not only through direct method calls
- Parallel tasks must touch separate files or clearly separable subsystems

## Implementation Strategy

### MVP First

1. Complete Phase 2 foundational GraphQL runtime work
2. Complete Phase 3 for immediate GraphQL startup
3. Validate HTTP-based built-in schema behavior end-to-end

### Incremental Delivery

1. HTTP GraphQL baseline
2. GraphQL-native configuration snapshots
3. WebSocket subscriptions
4. Performance, auth, and hardening

### Scope Guardrails

- Do not reintroduce IPC, CLI runtimes, or scripting-based configuration.
- Do not add REST admin endpoints for health, config, or auth flows.
- Keep GraphQL over HTTP and WebSocket as the only externally documented interfaces.
