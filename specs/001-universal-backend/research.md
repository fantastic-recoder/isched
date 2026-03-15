# Research: Universal Application Server Backend

**Phase**: 0 (Research & Validation)  
**Created**: 2025-11-01  
**Updated**: 2026-03-12  
**Feature**: Universal Application Server Backend  
**Status**: Revised for GraphQL-only transport scope

## Technical Decisions

### Decision: C++23 Language Standard

**Rationale**: C++23 provides the modern language support needed for a high-performance server runtime while staying within the project's existing toolchain direction.

**Alternatives considered**:

- C++20: acceptable but offers fewer language improvements
- C++26: insufficiently mature for the target build environments

**Implementation impact**: Requires a current compiler toolchain and continued RAII-first patterns.

### Decision: GraphQL as the Only External Interface

**Rationale**: The revised product constraint explicitly forbids non-GraphQL external interfaces. HTTP handles queries and mutations, while WebSocket handles subscriptions and real-time events.

**Alternatives considered**:

- REST plus GraphQL: violates the revised scope
- gRPC: adds another external contract and client burden
- Scripting APIs: explicitly rejected by the revised scope

**Implementation impact**: Operational concerns such as health, config, and runtime status must be represented inside the GraphQL schema instead of separate management endpoints.

### Decision: No IPC and No Out-of-Process Scripting

**Rationale**: The revised scope removes CLI runtimes, subprocess orchestration, and shared-memory coordination. This reduces operational complexity and avoids split-brain configuration flows.

**Alternatives considered**:

- Python and TypeScript execution: rejected by user request
- Shared-memory IPC: rejected by user request
- Sidecar configuration service: unnecessary if configuration is GraphQL-native

**Implementation impact**: All runtime coordination occurs in-process through tenant-aware services and internal state transitions.

### Decision: Versioned Configuration Snapshots

**Rationale**: Without scripts, configuration must still be expressive and auditable. Versioned configuration snapshots provide structured change history, validation boundaries, and rollback points.

**Alternatives considered**:

- Direct in-place mutation of active schema: risky and hard to audit
- Flat config files: weaker transactional model and poorer tenant isolation
- Manual SQL edits: unsuitable for frontend developer workflow

**Implementation impact**: Configuration mutations create candidate snapshots, validate them, and activate them atomically.

### Decision: Custom PEGTL-Based GraphQL Parser (Mandatory, No Third-Party Library)

**Rationale**: The server implements its own complete GraphQL parser using `tao::PEGTL`. The grammar lives in `src/main/cpp/isched/backend/isched_gql_grammar.hpp` and is a first-class production artifact, not a prototype. This design gives full control over parse-tree structure, error message quality, SDL fragment handling, and the integration between parsing and execution (`GqlExecutor`). It also eliminates a runtime dependency on any third-party GraphQL parsing library, keeping the C++ dependency surface narrow and predictable.

**Grammar coverage** (current in `isched_gql_grammar.hpp`):

| Group | Rules included |
|---|---|
| Lexical | `Whitespace`, `LineTerminator`, `Comment`, `Comma`, `Ignored`, `UnicodeBOM`, `Name`, `Token`, `Punctuator`, `Ellipsis` |
| Numeric literals | `IntValue`, `FloatValue` (with sign, fractional, exponent, and follow-restriction) |
| String literals | Quoted strings with full escape sequences, block strings (`"""…"""`), `BlockStringCharacter` |
| Types | `NamedType`, `ListType`, `NonNullType`, built-in scalars (`String`, `Int`, `Float`, `Boolean`, `ID`) |
| Values | `Variable`, `IntValue`, `FloatValue`, `StringValue`, `BooleanValue`, `NullValue`, `EnumValue`, `ListValue`, `ObjectValue` |
| Type-system definitions | `ScalarTypeDefinition`, `ObjectTypeDefinition`, `FieldDefinition`, `FieldsDefinition`, `ArgumentsDefinition`, `InputValueDefinition`, `DirectivesConst`, `Description` |
| Executable definitions | `OperationType`, `SelectionSet`, `Field`, `Alias`, `Arguments`, `GqlQuery`, `VariableDefinitions` |
| Schema definitions | `SchemaDefinition` and related non-terminals |
| Document | Top-level `Document` entry point |

**Parser facade**: `GqlParser` in `isched_GqlParser.hpp/.cpp` wraps the PEGTL grammar behind a stable `parse(string, name)` interface returning `IGdlParserTree`. This is the entry point called by `GqlExecutor` and the SDL validation subsystem.

**Test coverage**: `src/test/cpp/isched/isched_grammar_tests.cpp` contains per-rule and integration tests covering positive cases, negative/invalid inputs, spec edge cases (nested types, block strings, numeric edge cases, comments embedded in queries), and complex schema documents.

**Alternatives considered**:

- Third-party GraphQL parsing library (e.g., `libgraphqlparser`, `graphql-cpp`): rejected — would duplicate internal grammar control, introduce an additional runtime dependency, and prevent direct integration between parse-tree nodes and the execution context.
- Hand-written recursive-descent parser: rejected — more code, more maintenance, harder to map rules back to the spec.
- ANTLR C++ target: rejected — requires an external code-generation step incompatible with the Conan/CMake-only build.
- Boost.Spirit: rejected — significantly higher template complexity for maintainers.

**Implementation requirements**:

- Grammar rules MUST map 1-to-1 with GraphQL specification non-terminals where the spec is precise.
- Each rule MUST carry a Doxygen `@see` comment referencing the spec section or rule name.
- Parse errors MUST be converted to standards-compliant GraphQL error objects (`message` + `locations`) before reaching the transport boundary.
- Grammar incompleteness (missing rules) MUST be tracked as open tasks, not silently ignored.
- New grammar constructs required by `GqlExecutor` or schema generation MUST be added to `isched_gql_grammar.hpp` and covered by tests in the same change.

### Decision: SQLite for Embedded Storage

**Rationale**: SQLite still best matches the batteries-included requirement and supports per-tenant file isolation, transactional updates, and local deployments.

**Alternatives considered**:

- External PostgreSQL/MySQL: violates no-extra-software goal
- RocksDB or LMDB: weaker fit for relational and schema-driven workloads

**Implementation impact**: Tenant configuration, model metadata, auth data, and application data can all be persisted locally.

### Decision: Full GraphQL Introspection (Mandatory for Standard Client Interoperability)

**Rationale**: The GraphQL specification mandates a complete introspection system. Standard tooling — GraphiQL, Apollo Sandbox, Altair, code-generation clients (`graphql-codegen`, `gql.tada`) — relies on introspection to discover schema types, fields, arguments, and directives. A partial or skeleton implementation prevents these tools from functioning and reduces the `FR-GQL-001` conformance claim to marketing.

**Current state**: The existing `GqlExecutor::generate_schema_introspection()` is a skeleton. Confirmed gaps:
- `kind` always returns `"OBJECT"` regardless of actual type kind
- Built-in scalars (`String`, `Int`, `Float`, `Boolean`, `ID`) are not in the `types` array
- `ofType` chain for `LIST` and `NON_NULL` wrapping types is absent
- `__type(name:)` root field is not implemented
- `__typename` meta-field is not dispatched
- `isDeprecated` / `deprecationReason` are absent from `__Field`, `__InputValue`, `__EnumValue`
- `inputFields` for `INPUT_OBJECT` types is absent
- `enumValues` for `ENUM` types is absent
- `interfaces` and `possibleTypes` for `OBJECT` / `INTERFACE` types are absent
- Built-in directives (`@skip`, `@include`, `@deprecated`, `@specifiedBy`) are absent from the directives list
- `__DirectiveLocation` enum values are absent
- Test assertions are largely commented out

**Required implementation approach**:

1. Introduce a self-contained introspection data model in `GqlExecutor` that mirrors the GraphQL spec's introspection type system (`__Schema`, `__Type`, `__TypeKind`, `__Field`, `__InputValue`, `__EnumValue`, `__Directive`).
2. Populate the introspection model from the active schema parse tree during `load_schema()` and `update_type_map()`.
3. Implement `__schema`, `__type(name:)`, and `__typename` dispatch in the execution engine as special-cased system fields resolved before user-registered resolvers are checked.
4. Represent wrapped types (`LIST`, `NON_NULL`) through recursive `ofType` references, not as expanded flat objects.
5. Include built-in scalars and built-in directives unconditionally.
6. Refresh introspection data whenever the active schema changes (configuration snapshot activation).

**Alternatives considered**:

- Delegating introspection to a third-party library: rejected — the custom PEGTL parser produces a project-specific AST, so any introspection library would need adapters that amount to re-implementing the feature anyway.
- Keeping the skeleton and documenting the gaps: rejected — standard GraphQL tooling requires conformant introspection; gaps silently break UI tools and code generators.
- Generating introspection responses lazily from the live resolver registry: rejected — the registry does not carry type metadata (kind, deprecation, inputFields, ofType chain); schema-sourced introspection is more accurate.

**Implementation impact**:
- `GqlExecutor` must maintain a typed introspection model alongside the AST type map.
- Introspection unit tests in `isched_gql_executor_tests.cpp` must be un-commented and extended to cover all `__Type` fields, all type kinds, `ofType` chains, built-in types, and `__type(name:)`.
- Introspection correctness must be verified against GraphiQL or an equivalent standard tool as an acceptance gate.

**Rationale**: The protocol is widely supported by modern GraphQL clients and provides a clear contract for connection lifecycle, subscribe, complete, ping, and pong messages.

**Alternatives considered**:

- Custom WebSocket payloads: breaks interoperability
- Legacy `subscriptions-transport-ws`: older ecosystem contract
- Polling over HTTP: fails the real-time transport requirement

**Implementation impact**: The server needs a subscription broker, connection authentication, keepalive support, and cleanup on disconnect.

### Decision: JWT-Centered Authentication

**Rationale**: JWT remains appropriate for secure-by-default token handling and interoperates well with GraphQL clients over both HTTP and WebSocket.

**Alternatives considered**:

- Session-only cookies: less suitable for API-first clients
- Custom token formats: higher security risk and poorer interoperability

**Implementation impact**: HTTP requests and WebSocket connection initialization both require token validation paths.

### Decision: Smart Pointer Memory Management

**Rationale**: C++ Core Guidelines compliance remains mandatory, especially in a long-lived server with pooled resources and subscription sessions.

**Alternatives considered**:

- Raw-pointer ownership: unacceptable safety risk
- Custom ownership wrappers: unnecessary duplication of the standard library

**Implementation impact**: All owned connections, sessions, schema objects, and background resources use RAII and standard smart pointers.

### Decision: Doxygen for Documentation Generation

**Rationale**: Doxygen remains the most direct way to generate C++ API documentation with source references.

**Alternatives considered**:

- Manually maintained docs only: insufficient for API coverage
- Sphinx/Breathe: more tooling complexity than needed

**Implementation impact**: Public APIs, transport contracts, and schema generation code should remain documented and build-integrated.

## Architecture Research Summary

### Single Runtime Tenant Isolation

**Approach**: Use one server process with tenant-scoped runtime state, SQLite file separation, authorization boundaries, and bounded worker allocation.

**Benefits**:

- No IPC orchestration overhead
- Simpler deployment and debugging
- Easier configuration rollout and rollback

**Risks**:

- Requires careful isolation inside shared memory space
- Requires stronger scheduling and connection-pool safeguards

### GraphQL-Native Configuration

**Approach**: Store configuration snapshots, model definitions, and auth settings as internal metadata managed by GraphQL mutations.

**Benefits**:

- Removes scripting complexity
- Keeps all client interaction on one API surface
- Enables auditable configuration history

**Risks**:

- Schema evolution becomes part of the server core
- Validation must be robust to prevent broken schemas

### Observability Through GraphQL

**Approach**: Expose health, uptime, active configuration, and selected metrics through built-in GraphQL queries and subscriptions.

**Benefits**:

- No separate management API surface
- Consistent client tooling

**Risks**:

- Access control must prevent leaking sensitive operational details

## Dependency Impact

### Dependencies Retained

- `taocpp-pegtl` — grammar engine for the custom GraphQL parser in `isched_gql_grammar.hpp`
- SQLite — embedded per-tenant storage
- JSON library (`nlohmann_json`) — serialization and configuration persistence
- Logging library (`spdlog`) — structured logging
- JWT library (`jwt-cpp`) — authentication token handling
- HTTP/WebSocket libraries (`restbed`, `cpp-httplib`) — GraphQL transport
- Boost.URL — URL parsing for HTTP request routing
- `openssl` — TLS and cryptographic primitives
- `platformfolders` — cross-platform data directory resolution
- Catch2 — unit and integration test runner

### Dependencies Removed from the Architecture

- Python embedding or execution libraries
- TypeScript or JavaScript runtime embedding
- Shared-memory IPC libraries or subprocess-oriented coordination dependencies

### Additional Capability Required

- HTTP and WebSocket transport support sufficient for GraphQL over HTTP and `graphql-transport-ws`

## Architecture Clarification Decisions (2026-03-13)

Six open questions from the spec-vs-code gap analysis were resolved. Decisions are recorded below for traceability.

---

### Decision 1 — HTTP/WebSocket Transport Library: `cpp-httplib`

**Question**: Should the `/graphql` endpoint be served by `restbed`, `cpp-httplib`, or a combination?

**Decision**: **`cpp-httplib`** is the sole HTTP and WebSocket library. `restbed` is removed from `conanfile.txt` and all source files.

**Rationale**: `restbed` is a REST-oriented library that models every path as a "Resource" — a mismatch for a single-endpoint GraphQL server. `cpp-httplib` is already a declared Conan dependency, supports HTTP POST and WebSocket upgrade on the same socket, requires no per-route resource objects, and keeps the transport code minimal (see implementation discipline guideline).

---

### Decision 2 — GraphQL Transport Surface: HTTP POST + WebSocket Upgrade

**Question**: What HTTP methods and protocols should the `/graphql` endpoint support?

**Decision**: HTTP POST to `/graphql` for queries and mutations; WebSocket upgrade to `/graphql` for subscriptions. No other methods (GET, PUT, etc.) are required for the GraphQL endpoint.

**Rationale**: Standard GraphQL over HTTP uses POST. Subscriptions require WebSocket. Restricting to these two surfaces keeps the transport layer minimal and trivially auditable.

---

### Decision 3 — `GqlParser` Facade: Eliminated

**Question**: Should `GqlExecutor` call `GqlParser`, or should `GqlExecutor` own the PEGTL invocation directly?

**Decision**: **`GqlParser` is removed.** `GqlExecutor` owns the PEGTL grammar invocation directly through `isched_gql_grammar.hpp`. The files `isched_GqlParser.hpp` and `isched_GqlParser.cpp` (and `IGdlParserTree` if it has no other users) are deleted.

**Rationale**: `GqlParser` was a pure delegation facade with no added behaviour. Indirection that adds no value is complexity that must be removed (implementation discipline). The executor already calls PEGTL directly; `GqlParser` would have been a wrapper around code the executor already contains.

---

### Decision 4 — `ResolverCtx` Contents

**Question**: What should `ResolverCtx` contain at the end of Phase 1c?

**Decision**: `ResolverCtx` MUST contain three fields when Phase 2 (T006) lands:
- `std::string tenant_id` — identifies the tenant on whose behalf the resolver runs
- `DatabaseManager::Connection* db` (or equivalent handle pointer) — scoped to the tenant
- `std::string current_user_id` — authenticated user for this request (empty string for unauthenticated)

**Rationale**: These are the minimum fields for FR-SEC-003 (tenant isolation) and FR-SEC-001 (auth in resolvers). Adding more fields later is non-breaking. Adding them too early (before T007/T008 are ready) would force stub values. T006 is the right point to introduce them since it is when the server receives a real request and can populate the context from the HTTP/WebSocket session.

---

### Decision 5 — Missing Resolver + Missing Parent Key: Emit Error

**Question**: When no explicit resolver is registered and `parent_value` does NOT contain the field name, should the engine return `null` or emit `MISSING_GQL_RESOLVER`?

**Decision**: **Emit `MISSING_GQL_RESOLVER`** error. Do not silently return `null`.

**Rationale**: Silent `null` hides configuration mistakes — a developer who forgot to register a resolver would see nulls in the response and wonder why, rather than seeing a clear error. An explicit error makes the missing resolver immediately visible. This differs from the GraphQL spec's default null-propagation but is a deliberate stricter choice for this server.

---

### Decision 6 — Error `path` Array Format: Mixed `string`/`int` from Day One

**Question**: Should the `path` array in errors use `string`-only elements now, adding `int` elements later, or use mixed types from the start?

**Decision**: **Mixed `string`/`int` from the first implementation.** Strings for named object fields, integers for list item indices.

**Rationale**: Changing from `string`-only to mixed types later is a breaking change for any consumer that assumed all path elements are strings. Implementing mixed types now costs nothing — nlohmann JSON arrays are heterogeneous by default — and eliminates the need for a future breaking migration.

---

## Implementation Guidance

- Prefer transport abstraction that cleanly separates HTTP request handling from WebSocket session handling.
- Treat configuration snapshot validation as a first-class subsystem, not an afterthought in resolver code.
- Keep tenant-aware state explicit in resolver context and subscription session state.
- Test WebSocket behavior at the protocol-message level, not only at internal broker boundaries.

---

## GraphQL Field Resolution Algorithm

**Decision Record: Sub-Resolver Dispatch Design**

### Problem

The initial `GqlExecutor` implementation contained three structural defects in the sub-field dispatch path:

1. **Parent never forwarded** — `resolve_field_selection_details()` called every resolver with `json::object()` as the parent argument, ignoring the `p_parent_result` parameter that was threaded through the call chain but silently dropped before reaching the resolver invocation. Sub-resolvers were incapable of accessing their parent field's result.

2. **Result placed at wrong level** — `process_field_sub_selections()` wrote sub-field results directly into the top-level `p_result` object (`p_result[subField]`) instead of into the nested container for the parent field (`p_result[parentField][subField]`), producing a flat response regardless of query depth.

3. **Default field resolver absent** — `ResolverRegistry::get_resolver()` emitted a `MISSING_GQL_RESOLVER` error for any sub-field without an explicitly registered resolver function, rather than applying the GraphQL-standard default field resolver which extracts `parent[fieldName]` when the parent is a JSON object containing that key.

### Decision

Implement the GraphQL field resolution algorithm as defined in the [GraphQL specification §6.4 — Executing Fields](https://spec.graphql.org/October2021/#sec-Executing-Fields):

1. **Root fields**: invoke resolver with `parent = json::object()`.
2. **Sub-fields**: invoke resolver with `parent = result of the parent field's resolver`.
3. **Default resolver**: if no explicit resolver is registered for a field and `parent` is a JSON object containing a key equal to the field name, return `parent[fieldName]` without error.
4. **Result nesting**: write each field's resolved value into `result[fieldName]`; when recursing into sub-selections, write sub-field results into that same nested object, not into the outer result.
5. **Error containment**: a failing sub-resolver records a null for that field in the result and appends an entry to `errors` with the full field path; sibling fields at the same level continue resolving.

### Call Chain Signature Changes Required

| Method | Change |
|---|---|
| `resolve_field_selection_details(path, node, result, errors)` | Add `const json& p_parent` parameter; forward it to every resolver call |
| `process_field_selection(p_parent, path, node, result, errors)` | Already has `p_parent`; must forward it to `resolve_field_selection_details` instead of dropping it |
| `process_sub_selection(p_parent, path, node, result, errors)` | Already has `p_parent`; must pass the specific parent field's resolved value (not the outer parent) when dispatching into child fields |
| `process_field_sub_selections(p_parent, path, node, result, errors, fieldName)` | Use `result[fieldName]` as the target for all sub-field writes; pass `p_parent` (the current field's resolved value) down |

### Rationale

This is the normative GraphQL field execution algorithm. Deviating from it makes nested queries produce incorrect response shapes and means sub-resolvers cannot implement computed properties that depend on their parent. The fix is entirely internal to `GqlExecutor` and does not change any public API surface (`register_resolver`, `execute`, or `ResolverFunction` signature).
