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

> **Terminology note**: `organization` is the canonical architecture term. Historical GraphQL schema/type names that still use `tenant` remain equivalent for cross-artifact traceability unless a reference is explicitly platform-scoped.

## Constitutional Compliance Checklist

Each task implementation MUST verify:

- ✅ **Tests pass**: `cd cmake-build-debug && ctest --output-on-failure` MUST exit 0 after every task is marked done. This is a hard gate — no task is complete while any test is failing.
- ✅ **Performance**: HTTP and WebSocket performance remain within targets
- ✅ **GraphQL Spec**: Behavior matches GraphQL and transport expectations
- ✅ **Security**: Secure-by-default auth and organization isolation
- ✅ **Testing**: Required verification coverage exists by story completion, including HTTP integration tests, WebSocket integration tests, and performance coverage where applicable
- ✅ **Portability**: Linux/Conan compatibility maintained
- ✅ **C++ Core Guidelines**: Smart-pointer ownership and justified deviations only
- ✅ **Review Evidence**: Code-review records explicitly confirm constitution compliance (performance, GraphQL behavior, security/isolation, portability, and C++ Core Guidelines/deviations)

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
**Parser ownership**: `GqlExecutor` invokes PEGTL grammar directly via `src/main/cpp/isched/backend/isched_gql_grammar.hpp` (no `GqlParser` facade layer)  
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
- [x] T-GQL-015 [P] Add Doxygen comments in `src/main/cpp/isched/backend/isched_gql_grammar.hpp` mapping grammar rules and rule groups to their corresponding GraphQL specification sections (FR-PARSER-006)

### Parser integration

- [x] T-GQL-020 [P] Remove `isched_GqlParser.hpp` and `isched_GqlParser.cpp` — `GqlExecutor` owns PEGTL grammar invocation directly via `isched_gql_grammar.hpp`; also remove `IGdlParserTree` if it is only referenced by `GqlParser`
- ~~T-GQL-021~~ **Eliminated** — `GqlExecutor` already calls PEGTL directly; no separate `GqlParser` integration layer is needed
- [x] T-GQL-022 [P] Verify parse-error conversion in `GqlExecutor`: PEGTL parse errors MUST become standards-compliant GraphQL error objects with `message` and `locations` before reaching transport (no `GqlParser` layer required)
- [x] T-GQL-023 [P] Use `GqlExecutor`'s PEGTL grammar for SDL schema validation in the configuration snapshot subsystem (not a regex or string-match approach)

### Grammar test coverage

- [x] T-GQL-030 Maintain positive-case grammar tests in `isched_grammar_tests.cpp` for: lexical elements, numeric literals, string literals, type system, executable queries, schema documents
- [x] T-GQL-031 [P] Add negative/rejection tests for every major grammar group (invalid tokens, malformed strings, bad numeric formats, unclosed braces)
- [x] T-GQL-032 [P] Add spec-derived conformance tests: multi-level selection sets, inline fragments, named fragments, mutations, subscriptions, operation variables, aliases
- [x] T-GQL-033 [P] Add parse-error-message tests: verify location information (`line`, `column`) and message text in GraphQL `errors` output produced by `GqlExecutor` (including executor/transport-facing parse error responses)
- [x] T-GQL-034 [P] Verify parser documentation coverage by building docs and reviewing generated output to confirm Doxygen exposes the PEGTL grammar rule-to-spec mappings for representative lexical, executable, and type-system rule groups (FR-PARSER-006)

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
  - Populate `ResolverCtx` with `organization_id` (string), `db` (pointer to organization DB connection), and `current_user_id` (string); update all resolver call sites
  - Verify `ctest` is green after removal
- [x] T007 [P] Refactor `isched_TenantManager.hpp/cpp` for in-process organization isolation rather than process management
- [x] T008 [P] Complete `isched_DatabaseManager.hpp/cpp` for organization-scoped SQLite storage and connection pooling
- [x] T009 [P] Complete `ConnectionPool` behavior in `isched_DatabaseManager.hpp/cpp`
- [x] T010 [P] Complete `isched_GqlExecutor.hpp/cpp` for query, mutation, and schema execution
- [x] T011 [P] Complete `isched_AuthenticationMiddleware.hpp/cpp` for real JWT validation and session handling
- [x] T012 [P] Add a subscription broker implementation in `src/main/cpp/isched/backend/isched_SubscriptionBroker.hpp/.cpp` for WebSocket subscriptions
- [x] T013 [P] Implement GraphQL HTTP transport at `/graphql` and remove non-GraphQL transport assumptions
- [x] T014 [P] Refactor `src/main/cpp/isched/shared/config/` around configuration snapshots instead of scripts
- [x] T015 [P] Ensure Catch2 and transport-level test wiring is correct in `src/test/cpp/`

**Checkpoint**: Foundation ready for HTTP queries, WebSocket subscriptions, auth, and configuration snapshots. `ctest` MUST be green before moving to Phase 3.

---

## Phase 3: User Story 1 - Immediate GraphQL Startup (Priority: P1)

**Goal**: Frontend developers can start the server, complete bootstrap/login, and use built-in GraphQL immediately over HTTP.

### Tests for User Story 1

- [x] T016 [P] [US1] Add integration test for server startup, bootstrap/login, and `/graphql` availability in `src/test/cpp/integration/test_server_startup.cpp`
- [x] T017 [P] [US1] Add integration test for authenticated built-in GraphQL queries in `src/test/cpp/integration/test_builtin_schema.cpp`
- [x] T018 [P] [US1] Add integration test for GraphQL-based health and server info queries in `src/test/cpp/integration/test_health_queries.cpp`

### Implementation for User Story 1

- [x] T019 [P] [US1] Implement built-in GraphQL schema and resolvers for `hello`, `version`, `uptime`, `serverInfo`, and `health`
- [x] T020 [P] [US1] Route HTTP GraphQL requests through the real executor in `isched_Server.cpp`
- [x] T021 [US1] Enforce GraphQL as the only external interface in server routing and documentation
- [x] T022 [US1] Remove or disable REST-style health and management endpoint expectations from the runtime
- [x] T023 [US1] Implement GraphQL over HTTP request parsing, validation, and serialization
- [x] T024 [US1] Implement enhanced GraphQL error formatting with request IDs and extensions
- [x] T025 [US1] Document startup and built-in query behavior in user-facing docs

**Checkpoint**: User Story 1 is independently testable over HTTP. `ctest` MUST be green before moving to Phase 4.

---

## Phase 4: User Story 2 - GraphQL-Native Configuration (Priority: P2)

**Goal**: Backend behavior is configured through GraphQL mutations with no scripting and no IPC.

### Tests for User Story 2

- [x] T026 [P] [US2] Add integration test for configuration snapshot creation in `src/test/cpp/integration/test_configuration_snapshots.cpp`
- [x] T027 [P] [US2] Add integration test for schema updates after configuration mutation in `src/test/cpp/integration/test_schema_activation.cpp`
- [x] T028 [P] [US2] Add integration test for configuration rollback on invalid updates in `src/test/cpp/integration/test_configuration_rollback.cpp`
- [x] T028a [P] [US2] Add integration test for optimistic concurrency rejection in `src/test/cpp/integration/test_configuration_conflicts.cpp`: submit a configuration-changing mutation with `ApplyConfigurationInput.expectedVersion` set to a stale value, verify a conflict error is returned, and confirm no snapshot is persisted or activated

### Implementation for User Story 2

- [x] T029 [P] [US2] Implement `ConfigurationSnapshot` persistence in `src/main/cpp/isched/shared/config/`
- [x] T030 [P] [US2] Implement `applyConfiguration` and related mutations in `isched_GqlExecutor.cpp` using input objects for configuration-changing operations
- [x] T030a [P] [US2] Require `expectedVersion` in `ApplyConfigurationInput` (and equivalent configuration-changing input objects), validate it against the organization's active snapshot version before persistence or activation, and return a conflict error with no state change on mismatch
- [x] T031 [P] [US2] Implement model-definition persistence and schema generation support
- [x] T032 [P] [US2] Implement atomic activation and rollback for configuration snapshots
- [x] T033 [P] [US2] Implement safe schema migration and backup handling in `isched_DatabaseManager.cpp`
- [x] T034 [US2] Integrate configuration activation with organization runtime refresh in `isched_Server.cpp` and `isched_TenantManager.cpp`
- [x] T035 [US2] Implement queryable configuration history, active configuration, and FR-017 deployment-history exposure resolvers (activation timestamps + persisted JSON/SDL audit metadata)
- [x] T036 [US2] Replace obsolete scripting assumptions in docs and runtime comments

> **FR-017 traceability**: T029-T035 collectively cover persistence of configuration metadata, generated schema SDL, and the queryable deployment-history/audit surface required for debugging and audit.

**Checkpoint**: User Story 2 is independently testable with mutation-driven configuration. `ctest` MUST be green before moving to Phase 5.

---

## Phase 5: User Story 3 - Real-Time GraphQL Transport (Priority: P3)

**Goal**: Clients can use GraphQL subscriptions over WebSocket with standards-compatible behavior.

### Tests for User Story 3

- [x] T037 [P] [US3] Add WebSocket integration test for `graphql-transport-ws` connection lifecycle in `src/test/cpp/integration/test_graphql_websocket.cpp`
- [x] T038 [P] [US3] Add integration test for configuration and health subscriptions in `src/test/cpp/integration/test_graphql_subscriptions.cpp`
- [x] T039 [P] [US3] Add interoperability test for GraphQL HTTP plus WebSocket clients in `src/test/cpp/integration/test_client_compatibility.cpp`

### Implementation for User Story 3

- [x] T040 [P] [US3] Implement complete GraphQL introspection in `isched_GqlExecutor.cpp` — see Phase 5b below for detailed breakdown
- [x] T041 [P] [US3] Add GraphQL query complexity and depth analysis
- [x] T042 [P] [US3] Implement WebSocket `/graphql` endpoint and subscription session lifecycle
- [x] T043 [P] [US3] Implement subscription broker fan-out and disconnect cleanup
- [x] T044 [US3] Add GraphQL compliance validation for HTTP and WebSocket transport behavior
- [x] T045 [US3] Implement authentication during WebSocket `connection_init`
- [x] T046 [US3] Add subscription event types for configuration and health changes

**Checkpoint**: User Story 3 is independently testable over WebSocket. `ctest` MUST be green before moving to Phase 6.

---

## Phase 5b: Full GraphQL Introspection (Standard Client Interoperability)

**Purpose**: Replace the current skeleton `generate_schema_introspection()` with a spec-compliant implementation so that standard GraphQL tools (GraphiQL, Apollo Sandbox, Altair, code-generation clients) function correctly against the running server.

**Implementation file**: `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`  
**Test file**: `src/test/cpp/isched/isched_gql_executor_tests.cpp`  
**Spec reference**: GraphQL specification, Section "Introspection"

### Introspection data model

- [x] T-INTRO-001 Define an internal `IntrospectionType` struct (or equivalent) in `GqlExecutor` that holds `kind`, `name`, `description`, `fields`, `interfaces`, `possibleTypes`, `enumValues`, `inputFields`, and `ofType` — populated from the active schema parse tree during `load_schema()` / `update_type_map()`
- [x] T-INTRO-002 Populate built-in scalar types (`String`, `Int`, `Float`, `Boolean`, `ID`) unconditionally in the introspection model regardless of whether they appear in the active schema
- [x] T-INTRO-003 Represent wrapped types (`LIST`, `NON_NULL`) as recursive `ofType` chains rather than flattened strings; ensure a field typed `[String!]!` resolves to `NON_NULL → LIST → NON_NULL → SCALAR(String)`
- [x] T-INTRO-004 Populate introspection meta-types themselves (`__Schema`, `__Type`, `__TypeKind`, `__Field`, `__InputValue`, `__EnumValue`, `__Directive`, `__DirectiveLocation`) in the types list
- [x] T-INTRO-005 Refresh the introspection model whenever a configuration snapshot is activated so `__schema` results reflect the current live schema

### `__schema` root field

- [x] T-INTRO-010 Emit correct `queryType`, `mutationType`, `subscriptionType` names (not always `null`) based on the loaded schema's schema definition
- [x] T-INTRO-011 Emit all types (user-defined + built-in scalars + meta-types) in `__schema { types }`
- [x] T-INTRO-012 Emit correct `kind` for each type: `OBJECT`, `SCALAR`, `INTERFACE`, `UNION`, `ENUM`, `INPUT_OBJECT`, `LIST`, `NON_NULL` — remove the hard-coded `"OBJECT"` default
- [x] T-INTRO-013 Populate `fields` correctly for `OBJECT` and `INTERFACE` types; return `null` for other kinds
- [x] T-INTRO-014 Populate `inputFields` for `INPUT_OBJECT` types; return `null` for other kinds
- [x] T-INTRO-015 Populate `enumValues` for `ENUM` types; return `null` for other kinds
- [x] T-INTRO-016 Populate `interfaces` for `OBJECT` types; return `null` or empty for other kinds
- [x] T-INTRO-017 Populate `possibleTypes` for `INTERFACE` and `UNION` types; return `null` for other kinds
- [x] T-INTRO-018 Populate `isDeprecated` and `deprecationReason` on `__Field`, `__InputValue`, and `__EnumValue` (derive from `@deprecated` directive on the definition node)

### `__type(name:)` root field

- [x] T-INTRO-020 Implement the `__type(name: String!)` root field in the execution dispatch path
- [x] T-INTRO-021 Return `null` for unknown type names without raising an execution error

### `__typename` meta-field

- [x] T-INTRO-025 Dispatch `__typename` as a special field in the selection-set executor; return the runtime type name of the current object
- [x] T-INTRO-026 Support `__typename` in nested selection sets, not only at the query root

### Built-in directives

- [x] T-INTRO-030 Include `@skip`, `@include`, `@deprecated`, and `@specifiedBy` in the `directives` array returned by `__schema`; populate correct `locations` and `args` for each
- [x] T-INTRO-031 Populate `isRepeatable` on `__Directive` objects

### Test coverage (un-comment and extend `isched_gql_executor_tests.cpp`)

- [x] T-INTRO-040 Un-comment all commented-out assertions in the existing "GraphQL Introspection" `TEST_CASE`; every assertion MUST pass
- [x] T-INTRO-041 [P] Add test: `__schema { types }` contains all five built-in scalars by name and kind `SCALAR`
- [x] T-INTRO-042 [P] Add test: `__schema { types }` contains user-defined `OBJECT` type with correct `fields`, `name`, and `description`
- [x] T-INTRO-043 [P] Add test: `__schema { types }` contains user-defined `INPUT_OBJECT` type with correct `inputFields`
- [x] T-INTRO-044 [P] Add test: `__schema { types }` contains user-defined `ENUM` type with correct `enumValues`
- [x] T-INTRO-045 [P] Add test: `__type(name: "User")` returns correct `__Type` for a user-defined object type
- [x] T-INTRO-046 [P] Add test: `__type(name: "NonExistent")` returns `null` without error
- [x] T-INTRO-047 [P] Add test: field of type `[String!]!` produces `ofType` chain `NON_NULL → LIST → NON_NULL → SCALAR`
- [x] T-INTRO-048 [P] Add test: `__typename` returns correct type name in a nested selection set
- [x] T-INTRO-049 [P] Add test: `__schema { directives }` contains `@skip`, `@include`, `@deprecated` with correct `locations` and `args`
- [x] T-INTRO-050 [P] Add test: `@deprecated` on a field sets `isDeprecated: true` and `deprecationReason` in introspection
- [x] T-INTRO-051 [P] Add test: `__schema { queryType { name } }` returns `"Query"` when a `Query` type is defined

**Checkpoint**: Standard GraphQL tools (GraphiQL, Altair, Apollo Sandbox) can connect to the server, load and browse the full schema, and auto-complete queries without errors. `ctest` MUST be green before closing Phase 5b.

---

## Phase 6: Data Model, Performance, and Auth Completion

**Purpose**: Cross-cutting capabilities required after the GraphQL-only baseline is functional.

---

### T047: Organization-scoped RBAC, User and Organization Persistence

**Decisions recorded (2026-03-14)**:
- Four built-in roles: `platform_admin`, `organization_admin`, `user`, `service`; both `*_admin` roles may create additional custom roles scoped to their level (platform or organization)
- RBAC is enforced at the Query/Mutation operation level; `ResolverCtx` carries `user_id`, `user_name`, and `roles`
- `Organization` is the partition boundary; all users/admins for an org live in that org's own SQLite file — no shared user table; a user in multiple orgs has separate per-partition records
- Bootstrap uses a dedicated mutation `bootstrapPlatformAdmin(input: BootstrapPlatformAdminInput!): AuthPayload!`; it provisions the initial `platform_admin` only once for the server instance and is rejected after any platform admin exists; authenticated workflows may create additional `platform_admin` and `organization_admin` accounts afterward
- All mutations require prior authentication after bootstrap; `Organization` create is `platform_admin`-only; update/delete by `organization_admin` of that org or by `platform_admin`
- Platform admins and organization admins may define additional custom roles in their own scope and those roles may be referenced by schema-defined access-control rules
- Password hashing: Argon2id via OpenSSL 3.x `EVP_KDF` — no new Conan dependency
- Login: `login(email: String!, password: String!, organizationId: ID)` → `{ token: String!, expiresAt: String! }`; omitting `organizationId` signals platform-level login
- **System DB (2026-03-14)**: a single `isched_system.db` SQLite file (path via `platformfolders`, owned by `DatabaseManager`) holds `platform_admins`, `platform_roles`, and `organizations`; created on first startup, never deleted; writable only by `platform_admin`

#### System database

- [x] T047-000 Define and create the `isched_system.db` bootstrap database in `DatabaseManager`: opened/created at server startup using the platform data directory (`platformfolders`); contains tables `platform_admins` (`id`, `email`, `password_hash`, `display_name`, `is_active`, `created_at`, `last_login`), `platform_roles` (`id`, `name`, `description`, `created_at`), and `organizations` (`id`, `name`, `domain`, `subscription_tier`, `user_limit`, `storage_limit`, `created_at`); document schema and lifecycle in `specs/001-universal-backend/data-model.md`; all writes require `platform_admin` role
- [x] T047-000b [P] Add `BootstrapPlatformAdminInput` and `bootstrapPlatformAdmin(input: BootstrapPlatformAdminInput!): AuthPayload!` to `isched_builtin_server_schema.graphql`; implement the mutation so it is available only while `platform_admins` is empty, creates the initial platform admin, creates the initial authenticated session, and becomes unavailable immediately afterward

#### SDL schema additions for Phase 6 core types

- [x] T047-000a [P] Add Phase 6 SDL type definitions to `isched_builtin_server_schema.graphql` before any Phase 6 resolver is implemented — this task is a prerequisite for T047-004 through T047-018; add: `type User { id: ID!, email: String!, displayName: String!, roles: [String!]!, isActive: Boolean!, createdAt: String!, lastLogin: String }`, `type Organization { id: ID!, name: String!, domain: String, subscriptionTier: String!, userLimit: Int!, storageLimit: Int!, createdAt: String! }`, `type AuthPayload { token: String!, expiresAt: String! }` (per T047 decision, not the 4-field variant in the old contract), mutations `login(email: String!, password: String!, organizationId: ID): AuthPayload!`, `logout: Boolean!`; input types `CreateUserInput`, `UpdateUserInput`, `CreateOrganizationInput`, `UpdateOrganizationInput`; verify the resulting SDL parses cleanly through `GqlExecutor` before proceeding

#### Role infrastructure

- [x] T047-001 [P] Add `Role` enum (or string-based open enum) to `isched_AuthenticationMiddleware.hpp` representing built-in roles such as `platform_admin`, `organization_admin`, `user`, `service`; add storage for custom platform roles in `isched_system.db` (`platform_roles` table) and custom organization roles in each organization's SQLite schema
- [x] T047-002 [P] Extend `ResolverCtx` in `isched_GqlExecutor.hpp` with `user_id: std::string`, `user_name: std::string`, and `roles: std::vector<std::string>`; populate these fields from the validated JWT in `isched_Server.cpp` request dispatch
- [x] T047-003 [P] Implement operation-level RBAC gate in `isched_GqlExecutor.cpp`: before dispatching any Query or Mutation, evaluate `ResolverCtx::roles` against a per-operation `required_roles` annotation and the operation scope (platform or organization); reject with a GraphQL `FORBIDDEN` error if the caller lacks a required role in the correct scope
- [x] T047-004 [P] Add `createRole` and `deleteRole` mutations (scoped: `platform_admin` creates platform-scope roles; `organization_admin` creates organization-scope roles) and make those roles available for schema-defined access-control rules

> **Auth traceability**: T047 and T049 are the primary task groups satisfying both the FR-008 authentication/RBAC requirement family and FR-SEC-001 scope-aware JWT authorization.

#### Organization persistence

- [x] T047-005 [P] *(schema created by T047-000)* Implement `DatabaseManager` helper methods for CRUD on `organizations`, `platform_admins`, and `platform_roles` tables in `isched_system.db`; enforce `platform_admin`-only write access at the method level
- [x] T047-005a [P] Implement authenticated platform account-management workflow over `platform_admins` in `isched_system.db` so an existing `platform_admin` can create additional platform admin accounts without using the one-time bootstrap mutation
- [x] T047-006 [P] Implement `createOrganization(name: String!, domain: String, subscriptionTier: String, userLimit: Int, storageLimit: Int)` mutation — `platform_admin` only; creates the org record and provisions the organization SQLite file
- [x] T047-007 [P] Implement `updateOrganization(id: ID!, ...)` mutation — `organization_admin` of that org or an equivalent delegated organization-scoped role; `platform_admin` may also update any org
- [x] T047-008 [P] Implement `deleteOrganization(id: ID!)` mutation — `organization_admin` of that org (self-delete) or an equivalent delegated organization-scoped role, or `platform_admin`; cascades to deprovisioning the organization SQLite file
- [x] T047-009 [P] Implement `organization(id: ID!)` and `organizations` Query fields — `platform_admin` sees all; `organization_admin` sees own org only

#### User persistence

- [x] T047-010 [P] Create SQLite schema for `users` table in each organization DB: `id`, `email`, `password_hash` (Argon2id), `display_name`, `roles` (JSON array), `is_active`, `created_at`, `last_login`
- [x] T047-011 [P] Pin `openssl/[>=3.2.0]` in `conanfile.txt` (RISK-002: `EVP_KDF_fetch("ARGON2ID")` requires OpenSSL ≥ 3.2); then implement Argon2id hashing helper using OpenSSL `EVP_KDF` in `isched_AuthenticationMiddleware.cpp`; function signatures: `hash_password(plaintext) → hash_string` and `verify_password(plaintext, hash) → bool`
  > **Note**: `conanfile.txt` already pins `openssl/3.5.0` (> 3.2) — version constraint satisfied.
- [x] T047-012 [P] Implement `createUser(email: String!, password: String!, displayName: String, roles: [String!])` mutation — `organization_admin`, an equivalent delegated organization-scoped role, or `platform_admin` only
- [x] T047-013 [P] Implement `updateUser(id: ID!, displayName: String, roles: [String!], isActive: Boolean)` mutation — `organization_admin` (own organization), an equivalent delegated organization-scoped role, or `platform_admin`
- [x] T047-014 [P] Implement `deleteUser(id: ID!)` mutation — `organization_admin`, an equivalent delegated organization-scoped role, or `platform_admin`
- [x] T047-015 [P] Implement `user(id: ID!)` and `users` Query fields — `organization_admin` or an equivalent delegated organization-scoped role sees own-organization users; `platform_admin` sees all
- [x] T047-016 [P] Implement `login(email: String!, password: String!, organizationId: ID)` mutation: look up user in appropriate organization DB (or `isched_system.db` if no `organizationId`), verify Argon2id hash, issue JWT with `user_id`, `user_name`, `roles`, `organization_id`; call `AuthenticationMiddleware::create_session()` (defined in T049-002) to persist the session; return `{ token: String!, expiresAt: String! }` — **depends on T049-001 and T049-002**

#### Tests

- [x] T047-017 [P] Add unit tests in `src/test/cpp/isched/isched_auth_tests.cpp` *(extend or create)*: Argon2id hash+verify round-trip; wrong password rejected; role-gate allows/denies correct roles
- [x] T047-018 [P] Add integration tests in `src/test/cpp/integration/test_user_management.cpp`: full CRUD for User and Organization over GraphQL; login returns valid JWT; RBAC rejects unauthorized mutations; platform- and organization-scoped permissions are enforced correctly for built-in and custom roles
- [x] T047-018b [P] Add integration coverage verifying an authenticated `platform_admin` can create an additional platform admin through the platform account-management workflow, while unauthenticated callers and organization-scoped admins cannot
- [x] T047-018c [P] Add explicit negative auth matrix coverage verifying that every administrative provisioning path other than `bootstrapPlatformAdmin` is rejected for unauthenticated callers (FR-008-C enforcement)
- [x] T047-018a [P] Add integration tests in `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`: `bootstrapPlatformAdmin` succeeds exactly once on a fresh server, returns `AuthPayload`, persists the initial platform admin, and is rejected on all subsequent unauthenticated attempts with no additional account creation
- [x] T047-019 [P] Update `specs/001-universal-backend/data-model.md` with all Phase 6 entity definitions that live in organization SQLite files: `users` table (from T047-010), `sessions` table (from T049-001), `data_sources` table (from T048-001); cross-reference `isched_system.db` tables already documented by T047-000; mark each entity with its owning DB, write access role, and created-by task

---

### T048: Outbound HTTP Integration Resolvers (RESTDataSource pattern)

**Decisions recorded (2026-03-14)**:
- Apollo `RESTDataSource` pattern: data sources configured via mutations, stored in organization DB; resolvers call them at runtime
- JSON response auto-coerced to GraphQL return type by key-name matching (default field resolver)
- Auth forwarding: bearer-token pass-through or static API key — selected per data source, stored in organization DB, configured via mutations
- Upstream errors return a structured `HttpError` type as the field value (not null + GraphQL error)

#### Data source configuration

- [x] T048-001 [P] Create SQLite schema for `data_sources` table in each organization DB: `id`, `name`, `base_url`, `auth_kind` (`none`|`bearer_passthrough`|`api_key`), `api_key_header`, `api_key_value_encrypted` (AES-256-GCM ciphertext + nonce, base64-encoded), `timeout_ms`, `created_at` — **depends on T047-006**; `api_key_value` MUST NOT be stored in plaintext (FR-SEC-002)
- [x] T048-001a [P] Implement an `isched_CryptoUtils.hpp/.cpp` helper providing `encrypt_secret(plaintext, organization_key) → base64_ciphertext` and `decrypt_secret(base64_ciphertext, organization_key) → plaintext` using AES-256-GCM via OpenSSL `EVP_AEAD`; derive the per-organization key from a server-level master secret + organization ID using HKDF (OpenSSL — no new dependency); use this helper in `RestDataSource` when reading `api_key_value`
- [x] T048-002 [P] Implement `createDataSource(name: String!, baseUrl: String!, authKind: String, ...)` mutation — `organization_admin` only
- [x] T048-003 [P] Implement `updateDataSource(id: ID!, ...)` and `deleteDataSource(id: ID!)` mutations — `organization_admin` only
- [x] T048-004 [P] Implement `dataSources` Query field — returns all data sources for the caller's organization

#### Resolver binding

- [x] T048-005 [P] Create `isched_RestDataSource.hpp/.cpp` implementing `fetch(path, method, body, ctx) → json` using `cpp-httplib` as the HTTP client; apply auth forwarding based on `auth_kind`; return structured `HttpError` JSON on timeout or 4xx/5xx
- [x] T048-006 [P] Add `HttpError` type to the built-in GraphQL schema in `isched_builtin_server_schema.graphql`: `type HttpError { statusCode: Int!, message: String!, url: String }`
- [x] T048-007 [P] Wire `RestDataSource` into `ResolverDefinition` dispatch: when `resolver_kind == "outbound_http"`, load data source config from organization DB and invoke `RestDataSource::fetch`; map JSON response through default field resolver

#### Tests

- [x] T048-008 [P] Add unit tests in `src/test/cpp/isched/isched_rest_datasource_tests.cpp`: bearer passthrough sets correct `Authorization` header; api_key sets configured header; 404 upstream returns `HttpError`; timeout returns `HttpError`

---

### T049: Per-Organization Session Management and Revocation

**Decisions recorded (2026-03-14)**:
- Revocation list persisted in organization's SQLite DB
- Mutations: `logout`, `revokeSession(sessionId: ID!)`, `revokeAllSessions(userId: ID!)`, `terminateAllSessions(organizationId: ID!)` (`platform_admin` only — closes all non-`platform_admin` sessions)
- Active WebSocket connections are forcibly closed immediately upon revocation
- `last_activity` updated on validated requests with throttling (default 5-minute minimum update interval per session) and always updated on explicit close/revoke

- [x] T049-001 [P] Create SQLite schema for `sessions` table in each organization DB and reuse the same schema in `isched_system.db` for platform-level logins: `id`, `user_id`, `access_token_id`, `permissions` (JSON array), `roles` (JSON array — populated at login time, required so `terminateAllSessions` can filter out `platform_admin` sessions without a cross-table join; RISK-003), `issued_at`, `expires_at`, `last_activity`, `transport_scope`, `is_revoked`
- [x] T049-002 [P] Implement `AuthenticationMiddleware::create_session()`: writes a new session record to the organization's `sessions` table (or `isched_system.db` for platform-level logins); also implement `validate_token()` to load and check revocation status on every request — session creation is owned exclusively here; the `login` resolver (T047-016) delegates to this method
- [x] T049-003 [P] Implement `logout` mutation: mark caller's session `is_revoked = true`; update `last_activity`; signal `SubscriptionBroker` to close any matching WebSocket connection
- [x] T049-004 [P] Implement `revokeSession(sessionId: ID!)` mutation — `organization_admin` only; same revocation + WebSocket close flow
- [x] T049-005 [P] Implement `revokeAllSessions(userId: ID!)` mutation — `organization_admin` only; revokes all sessions for the given user in the organization
- [x] T049-006 [P] Implement `terminateAllSessions(organizationId: ID!)` mutation — `platform_admin` only; revokes all non-`platform_admin` sessions for the entire org
- [x] T049-007 [P] Add a revocation-check hook in `SubscriptionBroker`: when a session is revoked, look up any open WebSocket connections for that `session_id` and send a `connection_terminate` message then close the socket — **depends on T042** (WebSocket session model and connection registry must exist)
- [x] T049-008 [P] Add integration tests in `src/test/cpp/integration/test_session_revocation.cpp`: logout invalidates token; revoked token is rejected on next request; WebSocket is closed after revocation; `terminateAllSessions` does not close `platform_admin` sessions; `last_activity` updates are request-driven but respect the configured minimum update interval

---

### T050: Adaptive Worker-Thread and Subscription Resource Controls

**Decisions recorded (2026-03-14, revised 2026-03-14)**:
- Adaptation signals: total active subscription count across all organizations (primary) plus response-time metrics (secondary)
- Single global thread pool (cpp-httplib's pool); per-organization `min_threads`/`max_threads` are advisory quotas stored in `TenantConfig` for intent and future enforcement, but the enforced pool size is global
- Global pool size adapts using FR-013 bounds: scale-up when aggregate active subscription load increases by >=20% over the prior window, or when p95 latency exceeds 20 ms for 2 consecutive 30-second overload windows; scale-down only after 5 consecutive healthy 30-second windows; apply a minimum 60-second cooldown between resize actions
- Rationale: `cpp-httplib::Server::set_thread_pool_size()` is a global API with no per-organization granularity; a separate per-organization dispatch layer is deferred

- [x] T050-001 [P] Add advisory `min_threads` and `max_threads` fields to `TenantManager::TenantConfig`; expose `updateTenantConfig(organizationId: ID!, minThreads: Int, maxThreads: Int)` mutation — `platform_admin` only; store in organization DB for future enforcement
- [x] T050-002 [P] In `TenantManager`, track `total_active_subscription_count` as an atomic global counter (increment on any organization subscription start, decrement on disconnect)
- [x] T050-003 [P] Implement global adaptation logic per FR-013: compare each window against the prior window and scale up when aggregate `total_active_subscription_count` increases by >=20%, or when p95 latency exceeds 20 ms for 2 consecutive 30-second overload windows; allow scale-down only after 5 consecutive healthy 30-second windows; enforce a minimum 60-second cooldown between resize actions; apply all actions through `httplib::Server::set_thread_pool_size()` within a system-wide `max_global_threads` limit
- [x] T050-004 [P] Implement global request queuing: when in-flight requests exceed the current pool size, enqueue new requests in a global `std::queue` protected by a mutex; drain as threads become free
- [x] T050-005 [P] Add unit tests in `src/test/cpp/isched/isched_tenant_thread_pool_tests.cpp` verifying FR-013 adaptation behavior: >=20% subscription-load-delta scale-up, p95 > 20 ms for 2 consecutive 30-second overload windows triggers scale-up, scale-down requires 5 consecutive healthy 30-second windows, 60-second cooldown is enforced between resize actions, and queue drain correctness after threads free up

---

### T051: Performance Metrics via GraphQL

**Decisions recorded (2026-03-14)**:
- Metrics (revised 2026-03-14): interval-window counters `requestsInInterval: Int!`, `errorsInInterval: Int!` (reset at each interval boundary); cumulative-since-startup counters `totalRequestsSinceStartup: Int!`, `totalErrorsSinceStartup: Int!`; plus `activeConnections: Int!`, `activeSubscriptions: Int!`, `avgResponseTimeMs: Float!`, `organizationCount: Int!` (on `ServerMetrics` only)
- Exposed as both `Query` field and `Subscription` (live updates)
- Per-organization scope (auth: `organization_admin` or equivalent delegated organization-scoped role) and aggregate system scope (auth: `platform_admin` or equivalent delegated platform-scoped role), both auth-gated
- Interval window default = 60 minutes (configurable); resets automatically at boundary; cumulative counters never reset

- [x] T051-001 [P] Add `ServerMetrics` and `TenantMetrics` types to `isched_builtin_server_schema.graphql`: interval-window fields `requestsInInterval: Int!`, `errorsInInterval: Int!`; cumulative fields `totalRequestsSinceStartup: Int!`, `totalErrorsSinceStartup: Int!`; plus `activeConnections: Int!`, `activeSubscriptions: Int!`, `avgResponseTimeMs: Float!`; `organizationCount: Int!` on `ServerMetrics` only
- [x] T051-002 [P] Implement an in-memory `MetricsCollector` class (dedicated `isched_MetricsCollector.hpp/.cpp`): atomic interval-window counters (reset at boundary) and separate never-resetting cumulative counters per organization; record request start/end timestamps for `avgResponseTimeMs`
- [x] T051-003 [P] Add `metricsInterval` config field (default 60 minutes) settable via `updateTenantConfig` for per-organization scope and a system-level config mutation for global scope
- [x] T051-004 [P] Implement `serverMetrics: ServerMetrics` Query resolver — `platform_admin` only; returns aggregate across all organizations — **depends on T047-002** (roles in `ResolverCtx`)
- [x] T051-005 [P] Implement `tenantMetrics(organizationId: ID): TenantMetrics` Query resolver — `organization_admin` or an equivalent delegated organization-scoped role sees their own org; `platform_admin` may specify any `organizationId` — **depends on T047-002**
- [x] T051-006 [P] Implement `subscription { serverMetricsUpdated: ServerMetrics }` — `platform_admin` only; publishes via `SubscriptionBroker` at each interval boundary — **depends on T047-002**
- [x] T051-007 [P] Implement `subscription { tenantMetricsUpdated(organizationId: ID): TenantMetrics }` — `organization_admin` or an equivalent delegated organization-scoped role sees own org; publishes at each interval boundary — **depends on T047-002**
- [x] T051-008 [P] Add unit tests verifying counter increments, interval reset, and auth gating on both Query and Subscription resolvers

---

### T052: Performance Benchmark Suite

**Decisions recorded (2026-03-14)**:
- Framework: Catch2 `BENCHMARK` macro — no new Conan dependency
- Pass/fail thresholds (canonical): align with `performance-protocol.md` — HTTP-level `/graphql` acceptance gate uses fixed-profile p95 latency <= 20 ms with zero application errors; in-process benchmarks remain regression guards (`hello` ~1000 req/s as a non-normative local guard, ≥ 100 concurrent GraphQL POST clients without error, ≥ 50 simultaneous WebSocket subscribers)
- Two-tier performance protocol: in-process benchmarks are regression guards; HTTP-level `/graphql` benchmark runs are the normative acceptance gate for FR-012/SC-006

- [x] T052-001 [P] Create `src/test/cpp/performance/benchmark_suite.cpp` with a Catch2 test executable registered in `CMakeLists.txt`
- [x] T052-002 [P] Add benchmark: `hello` query throughput — spin up server in-process, measure sequential request rate over 5 seconds, and keep an approximately `1000 req/s` assertion only as a non-normative local regression guard
- [x] T052-003 [P] Add benchmark: concurrent GraphQL POST — launch 100 threads each sending 10 `{ version }` queries, assert 0 errors and completion within 10 seconds
- [x] T052-004 [P] Add benchmark: WebSocket subscription fan-out — open 50 simultaneous WebSocket connections, subscribe each, broadcast one event, assert all 50 receive it within 500 ms
- [x] T052-005 [P] Add benchmark: introspection under load — 10 concurrent `__schema { types }` requests, assert each completes within 500 ms as a Tier 1 regression guard (non-normative; canonical FR-012/SC-006 acceptance remains the HTTP-level gate in `performance-protocol.md`)
- [x] T052-006 [P] Add benchmark: in-process p95 latency ≤ 20ms regression guard — send 1000 sequential `{ version }` queries in-process, record per-request wall time, and track p95 for fast local regression detection
- [x] T052-007 Define the canonical two-tier benchmark gate in `specs/001-universal-backend/performance-protocol.md` (fixed query mix, fixed concurrency, warmup window, percentile method, and pass/fail criteria), and record measured results in `docs/performance.md` as the release-facing summary for both tiers (FR-PERF-003)

---

## Phase 7: Polish and Hardening

**Purpose**: Production hardening for the revised architecture.

- ~~T053~~ **Superseded by T006** — REST file deletion and CMakeLists.txt cleanup is fully covered by T006 in Phase 2
- [x] T054 [P] Remove legacy IPC files: `shared/ipc/isched_ipc.hpp/cpp`, `src/test/cpp/isched/isched_ipc_tests.cpp`, `src/test/cpp/isched/isched_rest_hello_world.cpp` — and all CMakeLists.txt references to them

---

### T055: GraphQL-Only Terminology Review

**Decisions recorded (2026-03-14)**:
- Scope: all documents — `README.md`, `docs/`, `specs/`, and source-file comments
- Acceptance: manual review; no automated gate

- [x] T055-001 Search all documents and source comments for legacy terminology: "REST endpoint", "REST API", "IPC", "script", "CLI executable", "restbed", "process management" — replace with GraphQL-only equivalents
- [x] T055-002 Review `README.md` for accurate startup, query, and transport instructions; remove any REST or IPC references
- [x] T055-003 Review `docs/` generated output and `specs/` planning documents; update any outdated architecture descriptions
- [x] T055-004 Review source-file header comments in `src/main/cpp/` for obsolete descriptions; update to reflect GraphQL-only transport
- [x] T055-005 Document all non-standard `extensions` fields (`code`, `timestamp`, `requestId`) in `specs/001-universal-backend/contracts/http-api.md` with example error response shapes (FR-GQL-003)
- [x] T055-006 [P] Update `specs/001-universal-backend/contracts/graphql-schema.md` with all Phase 6 additions and reconcile existing discrepancies:
  - **Reconcile login**: replace `login(input: LoginInput!)` with `login(email: String!, password: String!, organizationId: ID): AuthPayload!` per T047 decision; update `LoginInput` or remove if unused
  - **Reconcile AuthPayload**: replace 4-field `{ accessToken, refreshToken, expiresAt, user }` with 2-field `{ token: String!, expiresAt: String! }` per T047 decision; remove `register` and `refreshToken` mutations if not in scope
  - **Add**: `HttpError` type (T048-006), `ServerMetrics` / `TenantMetrics` types (T051-001), data source types and mutations (`DataSource`, `createDataSource`, `updateDataSource`, `deleteDataSource`, `dataSources`) (T048), session revocation mutations (`revokeSession`, `revokeAllSessions`, `terminateAllSessions`) (T049), RBAC mutations (`createRole`, `deleteRole`) (T047-004), metrics subscriptions (`serverMetricsUpdated`, `tenantMetricsUpdated`) (T051), thread pool config mutation (`updateTenantConfig`) (T050-001)
---

### T056: Security Hardening — clang-tidy CMake Target

**Decisions recorded (2026-03-14)**:
- Tool: clang-tidy (already partially configured); additional scanners deferred to a later release
- Integration: CMake target `security_scan` (does not block the default build; run explicitly)
- Threat-model deliverables: maintain both `specs/001-universal-backend/threat-model.md` and `docs/security-threat-model.md` for security-sensitive behavior

- [x] T056-001 [P] Add a `security_scan` CMake custom target in `CMakeLists.txt` that runs `clang-tidy` with a security-focused `.clang-tidy` config (enable `cert-*`, `bugprone-*`, `cppcoreguidelines-*`, `clang-analyzer-security.*` checks) over all library sources in `src/main/cpp/`
- [x] T056-002 [P] Create or update `.clang-tidy` at the repo root to include the security checks listed above; suppress only checks that conflict with intentional design decisions (document each suppression)
- [x] T056-003 [P] Verify `cmake --build ./cmake-build-debug/ --target security_scan` runs without new errors on the current codebase; fix any findings before marking done
- [x] T056-004 [P] Document the `security_scan` target in `README.md` under a "Security" section
- [x] T056-005 [P] Create `specs/001-universal-backend/threat-model.md` covering assets, trust boundaries, threat scenarios, mitigations, residual risks, and assumptions for bootstrap flow, JWT auth, RBAC, organization isolation, session revocation, WebSocket auth, and outbound HTTP secret handling
- [x] T056-006 [P] Create `docs/security-threat-model.md` as the project-wide security summary, linking to `specs/001-universal-backend/threat-model.md` and capturing reusable mitigations and operational guidance

---

### T057: Deployment Documentation

- [x] T057-001 Add deployment documentation in `docs/deployment.md` for HTTP and WebSocket operation covering: TLS configuration with `cpp-httplib`, port and bind-address settings, multi-organization bootstrap, and graceful shutdown
- [x] T057-002 Add embedded/Raspberry Pi deployment guidance in `docs/deployment.md` (FR-PERF-002): minimum RAM/storage requirements, recommended SQLite page-cache sizing, reducing thread counts via `TenantConfig`, and ARM/Linux build instructions using the standard Conan + CMake toolchain — no special compiler flags required

### T058: Constitution Compliance Review Gate

- [x] T058-001 Populate closeout review evidence in `specs/001-universal-backend/plan.md` under `Constitution Review Evidence (Closeout Record)`, with explicit code-review verification for constitution compliance (performance, GraphQL spec conformance, security/isolation, portability, C++ Core Guidelines/deviation documentation) before feature completion
- [x] T058-002 Generate `specs/001-universal-backend/closeout-validation.md` with SC-005 automated capability checklist results (>=19/20 threshold), mapped evidence paths, and final pass/fail ratio
- [x] T058-003 Record SC-001 timed quickstart validation evidence in `specs/001-universal-backend/closeout-validation.md`, linking the closeout note to `quickstart.md`, `src/test/cpp/integration/test_server_startup.cpp`, and `plan.md`
- [x] T058-004 Record SC-004 configuration-activation latency validation evidence in `specs/001-universal-backend/closeout-validation.md`, linking the closeout note to `src/test/cpp/integration/test_schema_activation.cpp` and `plan.md`

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

- Automated tests covering changed behavior MUST exist before the story is marked complete; task ordering does not need to place test tasks before implementation tasks
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
