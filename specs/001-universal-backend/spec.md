# Feature Specification: Universal Application Server Backend

**Feature Branch**: `001-universal-backend`  
**Created**: 2025-11-01  
**Updated**: 2026-04-04  
**Status**: Closeout Approved  
**Input**: User description: "The Isched universal application server backend should simplify web application development. No IPC, no scripting. The only interface MUST be GraphQL via HTTP and WebSocket."

## Clarifications

### Session 2026-03-12

- Q: What transport interfaces are allowed? → A: Only GraphQL over HTTP and GraphQL over WebSocket are allowed as externally supported interfaces.
- Q: How is backend behavior configured without scripting? → A: Backend behavior is configured through GraphQL mutations that persist versioned configuration snapshots and data model definitions.
- Q: What runtime architecture replaces CLI executables and IPC? → A: A single server runtime hosts organization-aware services, configuration management, authentication, GraphQL execution, and subscriptions in-process.
- Q: How are health and operational metrics exposed? → A: Health, server info, and operational status are exposed through built-in GraphQL queries and subscriptions, not through REST endpoints.
- Q: Which WebSocket protocol is required? → A: The server uses the `graphql-transport-ws` protocol for GraphQL subscriptions.
- Q: How is organization isolation handled without separate processes? → A: Organization isolation is logical and data-scoped within a single runtime using organization-scoped SQLite databases, connection pools, authorization checks, and scheduler quotas.
- Q: How are runtime configuration changes applied safely? → A: Configuration mutations produce versioned snapshots, validate them in-process, and apply them atomically with rollback to the previous active snapshot on failure.
- Q: What is the memory management requirement? → A: All resource ownership uses `std::unique_ptr` or `std::shared_ptr`; raw owning pointers are forbidden.
- Q: What documentation is required? → A: The build must generate API and source-reference documentation automatically.
- Q: How is GraphQL input parsed? → A: The server implements its own PEGTL-based GraphQL parser. The grammar is defined in `src/main/cpp/isched/backend/isched_gql_grammar.hpp`. No third-party GraphQL parsing library is used for the core parsing pipeline. The parser produces structured output consumed by the execution engine, schema generation, and SDL validation subsystems.
- Q: Why implement a custom parser rather than using a library? → A: Tight control over parse-tree structure, error reporting, and SDL fragment handling within a C++23/PEGTL-only dependency footprint. The grammar is tested directly against the GraphQL specification.
- Q: Does the server support GraphQL introspection? → A: Yes. The server MUST implement complete GraphQL introspection as defined in the GraphQL specification, including all meta-types (`__Schema`, `__Type`, `__TypeKind`, `__Field`, `__InputValue`, `__EnumValue`, `__Directive`, `__DirectiveLocation`), the `__schema` and `__type(name:)` root fields, and the `__typename` meta-field on every object type. Introspection must be accurate enough for standard GraphQL UI tools (GraphiQL, Apollo Sandbox, Altair) and code-generation clients to function correctly and reflect the current active schema.
- Q: Why is a full introspection implementation required rather than the skeleton? → A: Partial introspection causes silent failures in standard tooling. GraphQL clients use introspection to build type-safe queries, and UIs use it to provide auto-complete and documentation. Incomplete `kind`, missing built-in scalar types, absent `ofType` for wrapped types, or missing `inputFields` for `INPUT_OBJECT` types all cause standard tools to fail or produce incorrect results.

### Session 2026-04-04

- Q: How are concurrent configuration updates for the same organization handled? → A: Use optimistic concurrency per organization: every configuration mutation must include `expectedVersion`, and the mutation is rejected on version mismatch.
- Q: What authentication model is required for GraphQL operations? → A: Require JWT Bearer tokens for all GraphQL queries, mutations, and subscriptions, except the two unauthenticated entry mutations `bootstrapPlatformAdmin` and `login`. Validate an organization claim for organization-scoped operations on every HTTP request and on each WebSocket operation/subscription.
- Q: Should bootstrap/admin setup be unauthenticated, and if so, when? → A: Allow unauthenticated bootstrap/admin setup only once when no admin exists, and automatically disable bootstrap setup immediately after the initial admin is provisioned.
- Q: What is the bootstrap scope in multi-organization mode? → A: Bootstrap scope is global to the server instance; unauthenticated setup is allowed only once for the entire server instance.
- Q: Are admin identities and admin operations organization-scoped or global? → A: The system uses an RBAC model with multiple platform admins, multiple organization admins, and users. Implementations MAY also expose a built-in `service` role for machine-to-machine principals. Bootstrap provisions the initial platform admin for the server instance, platform-scoped administrative operations are performed by platform admins, organization-scoped administrative operations are performed by organization admins within their organization, and both platform admins and organization admins can define additional scoped roles for use in schema-defined access control.
- Q: What threat-model documentation is required for security-sensitive functionality? → A: Maintain both a feature-scoped threat model at `specs/001-universal-backend/threat-model.md` and a summarized project-wide security threat model at `docs/security-threat-model.md`. They must cover bootstrap flow, JWT auth, RBAC, organization isolation, session revocation, WebSocket authentication, and outbound HTTP secret handling.
- Q: What GraphQL API shape is required for optimistic concurrency on configuration updates? → A: Configuration-changing mutations use input objects, and the optimistic-concurrency token is carried as a required `expectedVersion` field inside the mutation input object (for example, `ApplyConfigurationInput.expectedVersion`).
- Q: What GraphQL API shape is required for one-time bootstrap admin provisioning? → A: Use a dedicated mutation `bootstrapPlatformAdmin(input: BootstrapPlatformAdminInput!): AuthPayload!`. It is the only unauthenticated bootstrap path, it is callable only while no platform admin exists for the server instance, and it becomes permanently unavailable after the initial platform admin is created.
- Q: What performance validation protocol is required for FR-012/SC-006? → A: Use a two-tier protocol: (1) in-process benchmarks are a fast regression guard during development, and (2) HTTP-level `/graphql` benchmarks are the normative acceptance gate for p95 claims. The canonical procedure is documented in `specs/001-universal-backend/performance-protocol.md`; `docs/performance.md` provides release-facing measurement summaries.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Immediate GraphQL Startup (Priority: P1)

A frontend developer can start Isched, complete the initial bootstrap/login flow, and immediately use a built-in GraphQL API over HTTP without installing any other backend software.

**Why this priority**: This is the core promise of the product. If the default GraphQL endpoint is not available immediately, the backend does not reduce setup cost.

**Independent Test**: Start the server binary, perform the one-time bootstrap flow on a fresh instance (or log in on an already bootstrapped instance), obtain a valid JWT Bearer token, then send GraphQL requests over HTTP to `/graphql` and verify built-in queries for server info and health without creating any custom configuration.

**Acceptance Scenarios**:

1. **Given** Isched is installed, **When** the frontend developer starts the server with default settings, **Then** a GraphQL endpoint is available over HTTP at `/graphql`, supports the one-time bootstrap flow on a fresh instance, and exposes built-in queries such as `hello`, `version`, `uptime`, and `health` for authenticated use after bootstrap/login
2. **Given** a running server and a valid JWT Bearer token, **When** the frontend developer sends valid GraphQL queries over HTTP, **Then** responses conform to the GraphQL over HTTP response format
3. **Given** a running server, **When** the frontend developer queries built-in health and configuration information, **Then** they receive operational data through GraphQL rather than a separate REST management API

---

### User Story 2 - GraphQL-Native Configuration (Priority: P2)

Frontend developers can configure data models, authentication options, and organization-specific backend behavior through GraphQL mutations instead of scripts or external tools.

**Why this priority**: Removing scripting only works if the GraphQL-native configuration model is expressive enough to replace it.

**Independent Test**: Apply a configuration snapshot through GraphQL mutations, verify the schema updates, confirm the new behavior is persisted, and validate rollback behavior when an invalid configuration is submitted.

**Acceptance Scenarios**:

1. **Given** an authenticated administrative client, **When** it submits GraphQL mutations that define data models and organization configuration, **Then** Isched persists a new versioned configuration snapshot and updates the active schema
2. **Given** a valid configuration update, **When** it is applied, **Then** the server makes the updated schema available without requiring an external script runner or out-of-process coordinator
3. **Given** an invalid configuration update, **When** validation fails, **Then** the system keeps the previous active configuration and returns GraphQL errors that explain the rejection

---

### User Story 3 - Real-Time GraphQL Transport (Priority: P3)

Clients can use GraphQL over WebSocket for subscriptions and other real-time events while staying fully compatible with standard GraphQL clients.

**Why this priority**: The user explicitly constrained the product to GraphQL over HTTP and WebSocket only, so WebSocket support is a first-class transport requirement rather than an optional add-on.

**Independent Test**: Connect using a GraphQL WebSocket client, subscribe to configuration or health events, trigger changes, and verify the server emits standards-compliant subscription messages.

**Acceptance Scenarios**:

1. **Given** a GraphQL WebSocket client using `graphql-transport-ws`, **When** it connects and authenticates, **Then** it can establish subscriptions successfully
2. **Given** configuration, health, or data changes, **When** subscribed clients are connected, **Then** the server pushes events over WebSocket without polling
3. **Given** a GraphQL client library that supports HTTP queries and WebSocket subscriptions, **When** it connects to Isched, **Then** it interoperates without custom transport adapters

---

### Edge Cases

- When a configuration mutation creates an invalid schema, the mutation must fail atomically and the previous active configuration snapshot must remain active.
- When a GraphQL query exceeds configured complexity or depth limits, execution must stop and the response must include a standards-compliant error with Isched-specific extensions.
- When a WebSocket client disconnects unexpectedly, the server must clean up subscription state and allow idempotent client reconnection and resubscription.
- When a configuration update would require destructive schema migration, the update must be rejected until an explicit migration workflow is provided.
- When authentication rules are changed while sessions are active, the system must apply the new rules to new sessions while preserving already-issued tokens until expiration or revocation.
- When a configuration mutation provides an `expectedVersion` that does not match the organization's active configuration version, the mutation must be rejected with a conflict error and no state change.
- When a GraphQL request or subscription operation is missing a JWT or has an invalid JWT, the system must reject it with an authorization error and must not execute any resolver logic; organization-scoped operations must also be rejected when the organization claim is missing or mismatched.
- When `bootstrapPlatformAdmin` is called after the first platform admin exists, the mutation must be rejected with a bootstrap-disabled authorization error, must not create any account or session, and bootstrap mode must remain disabled for all organizations.
- When a client attempts to execute a platform-scoped administrative operation without a platform-scoped role, the operation must be rejected even if the client holds organization-scoped administrative permissions.
- When a client attempts to execute an organization-scoped administrative operation for an organization outside the caller's authorized organization scope, the operation must be rejected.
- When a custom role is referenced in schema-defined access control, the role definition must exist in the corresponding scope (platform or organization) before the schema change can be activated.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: As an external-interface policy, the system MUST expose GraphQL over HTTP and GraphQL over WebSocket as the only supported external interface categories. The concrete transport contract for those interfaces is defined by **FR-GQL-005**.
- **FR-002**: System MUST NOT require procedural scripts, CLI runtime executables, or IPC mechanisms for configuration or runtime coordination.
- **FR-003**: System MUST allow administrative and organization-specific backend configuration through GraphQL mutations.
- **FR-004**: System MUST persist configuration as versioned snapshots that can be queried, activated, and rolled back.
- **FR-005**: System MUST provide a minimal built-in GraphQL schema for immediate startup, including health and server information queries.
- **FR-006**: System MUST generate and update organization-specific GraphQL schema elements from persisted data model definitions and configuration metadata.
- **FR-007**: System MUST provide embedded data storage without requiring a separately installed database server.
- **FR-008**: System MUST automatically provide user authentication and authorization without requiring external identity infrastructure for baseline operation.
- **FR-008-A**: System MUST require a valid JWT Bearer token for every GraphQL query, mutation, and subscription operation, except unauthenticated authentication-entry mutations (`bootstrapPlatformAdmin` and `login`).
- **FR-008-B**: System MUST expose a dedicated bootstrap mutation `bootstrapPlatformAdmin(input: BootstrapPlatformAdminInput!): AuthPayload!` as the only unauthenticated administrative provisioning path. The mutation MUST succeed only while no platform admin exists for the server instance and MUST become unavailable immediately after the initial platform admin is created.
- **FR-008-C**: Clarification of **FR-008-B**: all other administrative provisioning workflows MUST be authenticated RBAC-governed flows; no second unauthenticated administrative provisioning path is permitted.
- **FR-008-D**: System MUST bootstrap the server instance by creating an initial platform admin account, after which additional platform admin accounts MAY be created through authenticated platform account-management workflows governed by RBAC. These follow-up workflows are internal authenticated administrative flows and do not require a separate unauthenticated bootstrap mutation.
- **FR-008-E**: System MUST support an RBAC model with platform-scoped roles, organization-scoped roles, and user accounts; built-in role concepts MUST include at least platform admin, organization admin, and user. An implementation MAY additionally provide a built-in `service` role for machine-to-machine principals.
- **FR-008-F**: Platform-scoped administrative operations MUST require a platform-scoped administrative role or an explicitly delegated platform-scoped custom role.
- **FR-008-G**: Organization-scoped administrative operations MUST require an organization-scoped administrative role or an explicitly delegated organization-scoped custom role for the target organization.
- **FR-008-H**: Platform admins and organization admins MUST be able to define additional custom roles within their respective scopes and use those roles in schema-defined access-control rules.
- **FR-009**: System MUST provide enhanced GraphQL error responses with `extensions` containing error codes, timestamps, and request IDs.
- **FR-010**: System MUST support GraphQL subscriptions over WebSocket using `graphql-transport-ws`.
- **FR-011**: System MUST maintain logical organization isolation within a single runtime using organization-scoped authorization, database separation, and resource quotas.
- **FR-012**: System MUST complete requests within 20 milliseconds p95 on Ryzen 7 or Intel i5 class Linux hardware under the HTTP-level benchmark profile defined in `specs/001-universal-backend/performance-protocol.md` (fixed query mix, fixed concurrency, warmup window, and explicit percentile method). In-process benchmarks are regression guards and do not replace the HTTP-level acceptance gate.
- **FR-013**: System MUST provide adaptive worker-thread management using a hybrid policy: aggregate active subscription count is the primary scaling signal, and response-time metrics are secondary scaling signals. Acceptance bounds: scale-up when subscription load increases by >=20% over the prior window or p95 latency exceeds 20 ms for 2 consecutive 30-second windows; scale-down only after 5 consecutive healthy windows; apply a minimum 60-second cooldown between resize actions.
- **FR-014**: System MUST maintain per-organization SQLite database connections with pooling.
- **FR-014-A**: System MUST implement automatic schema migration with backup for safe changes, while rejecting destructive changes that could cause data loss.
- **FR-015**: System MUST validate and apply configuration updates atomically, rolling back to the previous active snapshot if validation or activation fails.
- **FR-015-A**: System MUST enforce per-organization optimistic concurrency for configuration mutations by requiring an `expectedVersion` field inside the mutation input object (for example, `ApplyConfigurationInput.expectedVersion`); if `expectedVersion` does not equal the organization's current active version, the mutation MUST be rejected as a conflict and MUST NOT persist or activate any change.
- **FR-016**: System MUST expose operational health, uptime, and runtime information through GraphQL queries and subscriptions instead of a REST management surface.
- **FR-017**: System MUST store configuration metadata, schema metadata, and deployment history in JSON and GraphQL SDL forms suitable for debugging and audit.
- **FR-018**: System MUST provide built-in resolvers for the common data operations exposed by the built-in GraphQL contract. For outbound HTTP integrations, the minimum supported operation set is the contract defined in `specs/001-universal-backend/contracts/graphql-schema.md`: create, update, delete, and list `DataSource` records, plus execute resolver bindings described by `ResolverBindingInput`, returning mapped data or `HttpError`. All such operations MUST remain accessible only through GraphQL.
- **FR-019**: System MUST use smart pointers (`std::unique_ptr`, `std::shared_ptr`) for all owned resources.
- **FR-020**: System MUST generate comprehensive source code documentation as part of the build process, including API references, inline code examples, and source snippets.

### Constitutional Requirements

**Performance Requirements** (Constitution Principle I):

- **FR-PERF-001**: Feature MUST maintain high-concurrency, multi-organization performance characteristics within a single runtime, including at minimum (a) >=100 concurrent HTTP GraphQL clients with zero application errors and (b) >=50 concurrent WebSocket subscribers where broadcast fan-out latency is <=500 ms under the benchmark protocol.
- **FR-PERF-002**: Implementation MUST support cloud-to-embedded deployment scenarios.
- **FR-PERF-003**: Performance impact MUST be measured and documented for HTTP queries and WebSocket subscriptions.

**GraphQL Compliance** (Constitution Principle II):

- **FR-GQL-001**: All GraphQL behavior MUST conform to the [GraphQL specification](https://spec.graphql.org/).
- **FR-GQL-002**: GraphQL over HTTP behavior MUST conform to the GraphQL over HTTP conventions used by standard clients.
- **FR-GQL-003**: Any non-standard `extensions` fields MUST be explicitly documented.
- **FR-GQL-004**: The HTTP transport layer MUST use `cpp-httplib` as the sole HTTP and WebSocket library. `restbed` MUST be removed from `conanfile.txt` and from all source files.
- **FR-GQL-005**: As the concrete transport contract for **FR-001**, the server MUST accept GraphQL queries and mutations via HTTP POST to `/graphql` with `Content-Type: application/json`. WebSocket connections to `/graphql` MUST be upgraded using `cpp-httplib`'s WebSocket support for subscription and streaming use cases. No other HTTP methods or paths are required for the GraphQL endpoint.
- **FR-GQL-006**: The REST resolver infrastructure (`isched_BaseRestResolver`, `isched_DocRootRestResolver`, `isched_SingleActionRestResolver`, `isched_DocRootSvc`, `isched_EHttpMethods`) MUST be removed. `isched_MainSvc` MUST be replaced or reduced to a non-REST stub.

**Introspection Requirements** (Standard GraphQL Client Interoperability):

- **FR-INTRO-001**: The server MUST implement the full GraphQL introspection system as defined in the GraphQL specification, Section "Introspection".
- **FR-INTRO-002**: The `__schema` field MUST be available on the root query type and return a complete `__Schema` object including `queryType`, `mutationType`, `subscriptionType`, `types` (all types in the active schema), and `directives`.
- **FR-INTRO-003**: The `__type(name: String!)` field MUST be available on the root query type and return the full `__Type` representation for any named type in the active schema, or `null` for unknown names.
- **FR-INTRO-004**: The `__typename` meta-field MUST be supported on every object, interface, and union type and return the runtime type name as a non-null `String`.
- **FR-INTRO-005**: The `__Type` introspection object MUST correctly populate all fields: `kind`, `name`, `description`, `fields(includeDeprecated:)`, `interfaces`, `possibleTypes`, `enumValues(includeDeprecated:)`, `inputFields`, `ofType`, `specifiedByURL`.
- **FR-INTRO-006**: The `kind` field on `__Type` MUST return the correct `__TypeKind` enum value: `SCALAR`, `OBJECT`, `INTERFACE`, `UNION`, `ENUM`, `INPUT_OBJECT`, `LIST`, or `NON_NULL`. The current skeleton returning only `OBJECT` is non-conformant and MUST be corrected.
- **FR-INTRO-007**: All built-in scalar types (`String`, `Int`, `Float`, `Boolean`, `ID`) MUST appear in the `types` array returned by `__schema`. They MUST NOT be omitted even if not referenced in the current active schema.
- **FR-INTRO-008**: `LIST` and `NON_NULL` wrapping types MUST be represented through the `ofType` chain in `__Type`. A field of type `[String!]!` MUST be representable as `NON_NULL → LIST → NON_NULL → SCALAR(String)` via nested `ofType` references.
- **FR-INTRO-009**: The `__Field` introspection object MUST populate `name`, `description`, `args`, `type`, `isDeprecated`, and `deprecationReason`.
- **FR-INTRO-010**: The `__InputValue` introspection object MUST populate `name`, `description`, `type`, `defaultValue`, `isDeprecated`, and `deprecationReason`.
- **FR-INTRO-011**: The `__EnumValue` introspection object MUST populate `name`, `description`, `isDeprecated`, and `deprecationReason`.
- **FR-INTRO-012**: The `__Directive` introspection object MUST populate `name`, `description`, `locations`, `args`, and `isRepeatable`.
- **FR-INTRO-013**: Built-in directives (`@skip`, `@include`, `@deprecated`, `@specifiedBy`) MUST appear in the `directives` array returned by `__schema`.
- **FR-INTRO-014**: Introspection results MUST reflect the **currently active schema**, including any types added by a configuration snapshot activation.
- **FR-INTRO-015**: The introspection subsystem MUST be covered by unit tests in `src/test/cpp/isched/isched_gql_executor_tests.cpp`. Tests MUST NOT have assertions commented out. Every `__Type` field variant MUST have at least one positive test.

- **FR-PARSER-001**: The server MUST implement its own PEGTL-based GraphQL parser. The grammar MUST reside in `src/main/cpp/isched/backend/isched_gql_grammar.hpp`. Third-party GraphQL parsing libraries MUST NOT be used for the core execution parsing pipeline.
- **FR-PARSER-002**: The PEGTL grammar MUST cover the full GraphQL document language: executable definitions (query, mutation, subscription, selection sets, fields, arguments, fragments, directives), type system definitions (scalar, object, interface, union, enum, input, directive), and schema definitions.
- **FR-PARSER-003**: `GqlExecutor` is the sole entry point for GraphQL document parsing and owns the PEGTL grammar invocation directly. The `isched_GqlParser` facade class and its source files (`isched_GqlParser.hpp`, `isched_GqlParser.cpp`) MUST be removed; they add indirection with no benefit. Parse-error conversion (FR-PARSER-005) is implemented within `GqlExecutor`.
- **FR-PARSER-004**: Grammar correctness MUST be verified by the unit test suite in `src/test/cpp/isched/isched_grammar_tests.cpp`. Coverage MUST include positive cases, negative/invalid cases, and spec-derived edge cases for each grammar group (lexical, numeric, string, type-system, executable, schema, document).
- **FR-PARSER-005**: The grammar MUST produce parse errors that are surfaced as standards-compliant GraphQL error objects with `locations` and `message` fields, not as uncaught C++ exceptions at the transport boundary.
- **FR-PARSER-006**: Grammar rules MUST be documented with Doxygen comments that map each rule to the corresponding GraphQL specification section.

**Security Requirements** (Constitution Principle III):

- **FR-SEC-001**: Authentication MUST use JWT Bearer tokens as the default and required mechanism for GraphQL operations, with OAuth-compatible federation optional and non-blocking. Authorization MUST enforce scope-aware RBAC for platform and organization operations.
- **FR-SEC-002**: Default configuration MUST be secure-by-default.
- **FR-SEC-003**: Organization data isolation MUST be maintained for both HTTP and WebSocket operations.
- **FR-SEC-004**: Security-sensitive functionality MUST be documented in both `specs/001-universal-backend/threat-model.md` and `docs/security-threat-model.md`, covering assets, trust boundaries, threat scenarios, mitigations, residual risks, and operational assumptions for bootstrap, JWT auth, RBAC, organization isolation, session revocation, WebSocket auth, and outbound HTTP secret handling.

**Execution Engine Requirements** (Field Resolution Correctness):

- **FR-EXEC-001**: The execution engine MUST implement the GraphQL field resolution algorithm as defined in the GraphQL specification §6.4. For each field in a selection set, the field resolver MUST be invoked with `(parent_value, args, context)` where `parent_value` is the result returned by the parent field's resolver. For root-level fields, `parent_value` MUST be an empty JSON object.
- **FR-EXEC-002**: When no explicit resolver is registered for a sub-field and `parent_value` is a JSON object containing a key matching the field name, the execution engine MUST apply the **default field resolver** and return `parent_value[field_name]`. It MUST NOT emit a `MISSING_GQL_RESOLVER` error for sub-fields that satisfy this condition. When neither an explicit resolver is registered nor the parent object contains the field name, the execution engine MUST emit a `MISSING_GQL_RESOLVER` error for that field.
- **FR-EXEC-003**: Sub-selection results MUST be placed in the response at the correct nesting level. For a query `{ a { b } }`, the response data MUST have the form `{"a": {"b": ...}}`. Placing a sub-field result at the top level of the response object instead of nested under its parent field is non-conformant.
- **FR-EXEC-004**: `ResolverPath` MUST represent the chain of field names from the root to the immediate parent of the field being resolved, and MUST be used to look up the correct resolver function for nested fields at any depth.
- **FR-EXEC-005**: Errors originating within a sub-resolver MUST be captured in the `errors` array of the response. A sub-resolver error MUST NOT prevent sibling fields at the same level from being resolved. The `path` component of each error MUST reflect the full field path in document order, using a mixed-type JSON array: `string` elements for named object fields and `int` elements for list indices (e.g., `["users", 0, "name"]`). This format MUST be used from the first implementation to remain compatible when list field resolution is added later.
- **FR-EXEC-006**: The field resolution engine MUST be unit-tested in `src/test/cpp/isched/isched_gql_executor_tests.cpp`. Tests MUST cover: (a) flat queries (single-level fields), (b) single-level nested sub-selections with an explicit sub-resolver, (c) default field resolver extraction from a parent result object without an explicit resolver, (d) multi-level nesting (at least two levels deep, e.g. `{ a { b { c } } }`) producing the correct response shape, (e) a sub-resolver receiving the correct non-empty parent value, (f) error propagation from a failing sub-resolver without blocking sibling field resolution, and (g) argument passing to a sub-resolver.
- **FR-EXEC-007**: The `register_resolver` API MUST allow callers to register a resolver function for any `ResolverPath` and field name combination, enabling explicitly registered sub-resolvers at arbitrary nesting depths.

**Testing Requirements** (Constitution Principle IV):

- **FR-TEST-001**: Core functionality MUST follow a TDD approach.
- **FR-TEST-002**: Integration tests are required for GraphQL over HTTP and GraphQL over WebSocket.
- **FR-TEST-003**: Performance regression tests MUST validate scalability and subscription overhead.
- **FR-TEST-004**: The complete `ctest` test suite (`cd cmake-build-debug && ctest --output-on-failure`) MUST pass after every individual task is marked done. No task transitions to complete while any test — pre-existing or newly introduced — is failing. This is a hard gate, not a guideline.

**Portability Requirements** (Constitution Principle V):

- **FR-PORT-001**: Code MUST compile on Linux with Conan-managed dependencies.
- **FR-PORT-002**: Cross-platform documentation MUST be provided for supported deployment modes.

**C++ Core Guidelines Requirements** (Constitution Technical Standards):

- **FR-CPP-001**: All C++ code MUST adhere to the [ISO C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).
- **FR-CPP-002**: Code reviews MUST verify guideline compliance.
- **FR-CPP-003**: Any deviations MUST be explicitly justified and documented.

### Key Entities

- **Configuration Snapshot**: Versioned, persisted representation of organization configuration applied through GraphQL mutations.
- **Data Model Definition**: User-defined model metadata that drives schema generation and database structure.
- **Authentication Context**: User session and permission information used for HTTP and WebSocket request authorization.
- **Platform Admin Account**: Server-wide administrative identity used for platform-scoped administrative operations and management of platform-scoped custom roles.
- **Organization Admin Account**: Organization-scoped administrative identity used for organization-local administrative operations and management of organization-scoped custom roles.
- **Role Definition**: Built-in or custom RBAC role definition, scoped either to the platform or to an organization, and referenced by authorization checks and schema-defined access control.
- **GraphQL Schema**: Active schema representation composed of built-in types and configuration-derived organization types.
- **Server Instance**: Running Isched server process that hosts HTTP and WebSocket GraphQL transports.
- **Organization Runtime**: In-process organization-scoped runtime state including connection pools, quotas, auth settings, and active configuration.
- **Database Connection Pool**: Per-organization pool of SQLite connections.
- **Subscription Session**: Active WebSocket connection and subscription registry for a client.
- **Resolver Definition**: Built-in or configured GraphQL resolver metadata that maps operations to storage or integration behavior.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Frontend developers can start a working backend server in under 10 minutes without installing any external backend services, measured on a supported Linux developer machine where the base toolchain (`conan`, `cmake`, `ninja`, and a C++23-capable compiler) and the one-time `conan profile detect` setup are already complete, starting from a fresh source checkout and empty Isched data directory, and ending when bootstrap/login succeeds and one authenticated built-in GraphQL query completes successfully.
- **SC-002**: System eliminates the need for external databases, custom REST administration endpoints, scripting runtimes, and IPC services for typical web applications.
- **SC-003**: All GraphQL HTTP responses and WebSocket subscription flows pass the targeted GraphQL compliance test suite.
- **SC-004**: Valid configuration changes applied through GraphQL mutations take effect in under 5 seconds without process restarts.
- **SC-005**: At least 95% (>=19/20) of the fixed backend capability checklist below MUST pass with automated validation.

### SC-005 Capability Checklist (20 items)

1. server startup and `/graphql` availability
2. bootstrap platform admin (one-time)
3. login with JWT issuance
4. authenticated built-in query: `hello`
5. authenticated built-in query: `version`
6. authenticated built-in query: `uptime`
7. authenticated built-in query: `health`
8. configuration snapshot creation
9. configuration activation
10. configuration rollback on invalid update
11. optimistic concurrency rejection (`expectedVersion` mismatch)
12. schema introspection `__schema`
13. schema introspection `__type(name:)`
14. WebSocket connect/auth via `graphql-transport-ws`
15. subscription delivery for health/config events
16. organization CRUD authorization gates
17. user CRUD authorization gates
18. custom role creation and enforcement
19. session revocation invalidates subsequent requests
20. outbound HTTP data-source resolver path returns mapped result or `HttpError`
- **SC-006**: System serves thousands of concurrent clients with 95th percentile response times under 20 milliseconds for standard queries under the documented two-tier performance protocol, where HTTP-level `/graphql` benchmark runs are the release acceptance gate on Ryzen 7 or Intel i5 class Linux hardware.

## Assumptions

- Frontend developers are comfortable using GraphQL clients over HTTP and WebSocket.
- Typical applications target CRUD operations, user management, and moderate real-time event delivery.
- Embedded SQLite performance is sufficient for small to medium deployments and organization isolation needs.
- Configuration managed through GraphQL mutations is acceptable in place of procedural scripting.
