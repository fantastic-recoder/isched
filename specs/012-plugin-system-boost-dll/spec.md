---
spec_id: SP-012
title: Plugin System (Boost.DLL)
status: completed
owner: isched Development Team
---

# Feature Specification: Plugin System (Boost.DLL)

**Feature Branch**: `012-plugin-system-boost-dll`  
**Created**: 2026-06-03  
**Status**: Completed  
**Input**: User description: "Implement a plugin system for isched utilising Boost.DLL. Two plugin types: request plugins (REST callbacks) and resolver plugins (GraphQL resolvers as DLLs). No duplicate route/resolver registration. Swagger UI generation support via plugin metadata. GraphQL admin API for loading/unloading on the fly. Plugin state persisted across server restarts. 80% unit test coverage."

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Load and activate a request plugin at runtime (Priority: P1)

As a server administrator, I can load a shared library that implements a request plugin and immediately have its HTTP routes available under `/plugin/*` so I can extend the server's REST surface without restarting.

**Why this priority**: Request plugin loading is the foundational capability. Until a plugin can be loaded and its routes dispatched, all other extension scenarios are blocked.

**Independent Test**: Can be fully tested by loading a request-plugin shared library via the `loadPlugin` GraphQL mutation and sending an HTTP request to the route the plugin registered, verifying a response is returned by the plugin handler.

**Acceptance Scenarios**:

1. **Given** a valid request-plugin shared library located at an absolute path under the configured `plugins_root`, **When** `loadPlugin` mutation is called with its library path and kind `REQUEST`, **Then** the plugin is loaded, its manifest is persisted to the `plugin_registry` table, and its declared routes are reachable under `/plugin/*`.
2. **Given** a request plugin is already loaded on a given route, **When** a second plugin attempts to register the same route (normalised as `METHOD /path` uppercase), **Then** loading is rejected with a conflict error and the second library is not activated.
3. **Given** a loaded request plugin, **When** an HTTP request arrives at its registered path, **Then** `HttpSession` dispatches the call to the plugin's `handleRequest` handler and returns the plugin's response to the client.
4. **Given** a loaded request plugin, **When** `unloadPlugin` mutation is called with its id, **Then** the plugin's routes are deregistered, the plugin is unloaded from memory, and the `plugin_registry` record is marked `is_loaded = false` with an `unloaded_at` timestamp.

---

### User Story 2 — Load and activate a resolver plugin at runtime (Priority: P1)

As a server administrator, I can load a shared library that implements GraphQL resolvers and have those resolvers registered in the live GraphQL executor without a server restart so I can extend the GraphQL API on the fly.

**Why this priority**: Resolver plugins allow the GraphQL schema surface to grow dynamically, which is the primary motivation for a plugin architecture in a GraphQL-first backend.

**Independent Test**: Can be fully tested by loading a resolver-plugin shared library, then executing a GraphQL query or mutation that targets a field registered by the plugin, and verifying a valid response is returned.

**Acceptance Scenarios**:

1. **Schema-Supplied Path**: **Given** a valid resolver-plugin shared library that supplies a GraphQL schema, **When** `loadPlugin` mutation is called with its library path and kind `RESOLVER`, **Then** resolver-plugin activation follows the schema-supplied path defined in **FR-021** (including validation and conflict checks before resolver registration), its manifest is persisted, and the registered fields become queryable immediately.

2. **No-Schema-Supplied Path**: **Given** a valid resolver-plugin shared library that does NOT supply a schema (returns an empty string from `graphqlSchema()`), **When** `loadPlugin` mutation is called, **Then** resolver-plugin activation follows the no-schema path defined in **FR-021**; violations (including missing or already-bound target fields) reject loading and prevent activation.

3. **Given** a resolver plugin is already registered for a given scope and field name, **When** a second plugin attempts to register the same scope+fieldName combination (either via supplied schema or to existing schema), **Then** loading is rejected with a conflict error and the second library is not activated.

4. **Given** a loaded resolver plugin, **When** `unloadPlugin` mutation is called with its id, **Then** the plugin calls `unregisterResolvers(GqlExecutor&)` to remove its bindings, the library is unloaded, and the persisted record is marked unloaded.

---

### User Story 3 — Discover loaded plugins and their metadata (Priority: P2)

As a server administrator, I can query the list of all known plugins and inspect an individual plugin's manifest, including its declared routes or resolver bindings and OpenAPI fragment, so I can understand the current server extension state.

**Why this priority**: Visibility into the plugin registry is required for safe management; administrators must be able to verify what is loaded before issuing load or unload operations.

**Independent Test**: Can be fully tested by loading one or more plugins and then issuing the `plugins` query and `plugin(id:)` query, verifying that the returned records match the loaded plugins' manifests.

**Acceptance Scenarios**:

1. **Given** one or more plugins have been loaded, **When** the `plugins` GraphQL query is executed, **Then** all `PluginRecord` entries are returned including id, kind, name, version, description, library path, `isLoaded` flag, and timestamps.
2. **Given** a specific plugin id, **When** the `plugin(id:)` GraphQL query is executed, **Then** the single matching `PluginRecord` is returned with its full manifest JSON.
3. **Given** no plugins have ever been loaded, **When** the `plugins` query is executed, **Then** an empty list is returned without error.
4. **Given** plugins with declared OpenAPI fragments, **When** the manifest JSON is inspected, **Then** it contains each plugin's `openApiFragmentJson` string suitable for inclusion in a Swagger UI generation step.

---

### User Story 4 — Plugin state survives server restarts (Priority: P2)

As a server administrator, I expect that plugins loaded before a server restart are automatically reloaded on startup so I do not have to re-issue `loadPlugin` commands after each deployment.

**Why this priority**: Without persistence, every restart requires manual re-loading of all plugins, making the system unusable in production.

**Independent Test**: Can be fully tested by loading a plugin, stopping and restarting the server process, then querying `plugins` and verifying the previously loaded plugin is active and its routes or resolvers are available.

**Acceptance Scenarios**:

1. **Given** one or more plugins are loaded and `is_loaded = true` in `plugin_registry`, **When** the server process is restarted, **Then** `PluginManager::restore_persisted_plugins()` is called during startup and all previously loaded plugins are reloaded automatically.
2. **Given** a plugin that was unloaded before shutdown (`is_loaded = false`), **When** the server restarts, **Then** the plugin is NOT reloaded.
3. **Given** a persisted plugin whose shared library file is missing at restart, **When** `restore_persisted_plugins()` attempts to load it, **Then** an error is logged, `last_restore_error` / `restore_attempts` / `last_restore_attempt_at` are updated, the server continues startup without crashing, and the plugin remains eligible for retry on subsequent startups until manual unload/remove.

---

### User Story 5 — Confidence through automated tests (Priority: P3)

As a delivery team member, I want a comprehensive Catch2 unit test suite covering the plugin system's core flows so regressions during future development are caught early.

**Why this priority**: The plugin system is intricate (conflict detection, dispatch, persistence, startup restore). Automated tests protect all of these invariants across refactors.

**Independent Test**: Can be fully tested by running `ctest` and verifying that the plugin system test suite passes all cases and that measured line/branch coverage on plugin system files meets the 80% threshold.

**Acceptance Scenarios**:

1. **Given** the plugin system test suite, **When** `ctest --output-on-failure` is run, **Then** all 17 plugin-system test cases pass (48 assertions) and the full test suite reports 53/53 passing.
2. **Given** the plugin system implementation files, **When** coverage is measured, **Then** line and branch coverage for `isched_PluginSystem.cpp` and related modified files reaches ≥ 80%.
3. **Given** the test suite, **When** reviewed, **Then** it covers: successful load/unload of both plugin kinds, conflict detection for routes and resolver bindings, HTTP dispatch to a loaded request plugin, `inject_loaded_plugin` for test-mode bypass of Boost.DLL loading, and startup restore behaviour.

---

### User Story 6 — Monitor plugin lifecycle changes in real time (Priority: P2)

As a platform administrator, I can subscribe to a dedicated admin GraphQL subscription for plugin lifecycle updates so I receive immediate notifications when plugins are loaded or unloaded.

**Why this priority**: Runtime plugin management is safety-critical. Real-time lifecycle visibility reduces operational blind spots and shortens incident response time.

**Independent Test**: Can be fully tested by opening the admin lifecycle subscription, executing `loadPlugin` and `unloadPlugin`, and verifying the subscriber receives exactly one event per successful lifecycle transition with the expected payload fields.

**Acceptance Scenarios**:

1. **Given** one or more currently connected authenticated callers with `platform_admin` role are subscribed to the plugin lifecycle admin subscription, **When** a plugin is successfully loaded via `loadPlugin`, **Then** exactly one `LOADED` lifecycle event is published for that state transition and delivered once to each currently connected authorized subscriber, containing the plugin id, plugin kind, plugin name, library path, and event timestamp.
2. **Given** one or more currently connected authenticated callers with `platform_admin` role are subscribed to the plugin lifecycle admin subscription, **When** a loaded plugin is successfully unloaded via `unloadPlugin`, **Then** exactly one `UNLOADED` lifecycle event is published for that state transition and delivered once to each currently connected authorized subscriber, containing the same identifying metadata and event timestamp.
3. **Given** a plugin load or unload request fails validation or fails before state transition, **When** the operation returns an error, **Then** no lifecycle subscription event is emitted for that failed operation.

### Edge Cases

- A shared library exports neither `isched_create_request_plugin` nor `isched_create_resolver_plugin`: loading is rejected with a descriptive error.
- A plugin's `manifest()` returns an id that is empty or already present in the registry under a different library path: conflict is detected and loading is rejected.
- Two plugins attempt to register routes that differ only by literal segment casing (e.g., `GET /plugin/Foo` vs `GET /plugin/foo`): they are treated as distinct routes unless another **FR-006** conflict rule applies.
- Two plugins attempt to register the same GraphQL field under the same scope: the second is rejected.
- A plugin shared library file exists but is not a valid shared library: Boost.DLL throws at load time; the exception is caught, logged, and a user-visible error is returned via GraphQL.
- `unloadPlugin` is called for an id that does not exist in `plugin_registry`: a not-found error is returned.
- `unloadPlugin` is called for a plugin that is already unloaded: the operation is idempotent and returns success without re-unloading.
- A resolver plugin's `unregisterResolvers` throws during unload: the exception is caught, the library is still unloaded, and the error is recorded.
- `restore_persisted_plugins()` is called when the `plugin_registry` table does not exist yet: the table is created on first use by `DatabaseManager`.
- A caller without the `platform_admin` role issues `loadPlugin`, `unloadPlugin`, `plugins`, or `plugin(id:)`: the request is rejected with an authorisation error before any plugin read or plugin state mutation logic executes.
- A plugin manifest's `abi_version` major component does not match the server's ABI major version (e.g., plugin declares `"2.0.0"` when server is `1.x.x`): loading is rejected with a version-mismatch error identifying both versions.
- A plugin manifest's `openApiFragmentJson` is not valid JSON (e.g., truncated or empty string): loading is rejected with a parse-error message; no partial state is created.
- A request plugin registers a path-template route (e.g., `GET /plugin/items/{id}`) and a second plugin tries to register an overlapping static or templated route (e.g., `GET /plugin/items/latest`): the second load is rejected with a prefix-overlap conflict error.
- A request plugin's `handleRequest()` throws an uncaught exception: the plugin is automatically unloaded, the exception is logged at ERROR level, and the caller receives HTTP 500.
- A resolver plugin's registered resolver callable throws an uncaught exception: the plugin is automatically unloaded, the exception is logged at ERROR level, and the GraphQL response contains an error node.
- Resolver plugin schema/load edge handling (empty schema, invalid non-empty SDL, schema conflicts, missing fields, already-bound fields) follows the canonical rules in **FR-021**; on violation, load is rejected with no partial activation.
- `loadPlugin` is called with a relative path, or with an absolute path outside the configured `plugins_root`: the request is rejected with a path-policy validation error before any dynamic loading is attempted.
- A persisted plugin repeatedly fails startup restore (e.g., missing/incompatible library): each startup attempt updates `last_restore_error`, increments `restore_attempts`, and sets `last_restore_attempt_at`; the server continues startup and retries again on the next startup until manual unload/remove.
- `loadPlugin` fails before a plugin reaches active state (e.g., route conflict, ABI mismatch, invalid OpenAPI JSON, invalid resolver schema): no plugin lifecycle subscription event is emitted.
- `unloadPlugin` is invoked for a plugin that does not exist or is already unloaded: no `UNLOADED` lifecycle subscription event is emitted because no lifecycle state transition occurred.

## Clarifications

### Session 2026-06-03

- Q: Should plugin-management GraphQL operations be access-controlled, and if so to which role? → A: Restrict `loadPlugin`, `unloadPlugin`, `plugins`, and `plugin(id:)` to the `platform_admin` role via `require_roles`.
- Q: How should plugin ABI / versioning compatibility be enforced? → A: Use semantic versioning in the manifest (`abi_version` field); reject plugins whose major version is incompatible with the server's current plugin ABI major version (initial ABI version: `1.0.0`).
- Q: What should happen when `openApiFragmentJson` is invalid or unparseable? → A: Validate the OpenAPI fragment JSON with `nlohmann::json::parse` at load time; if invalid or not parseable, reject the plugin load with a descriptive error.
- Q: What types of route paths should plugins be allowed to register, and how should conflicts be detected? → A: Support exact and path-template routes (e.g., `/plugin/items/{id}`). Parameterised segments (e.g., `{id}`) denote variable path components. Route conflict detection follows canonical **FR-006** segment-based rules (including case-sensitive segment matching and no conflict when segment counts differ).
- Q: How should plugin crashes (uncaught exceptions from handlers) be contained? → A: If a request plugin's `handleRequest()` or a resolver plugin's registered resolver throws an uncaught exception, automatically unload the offending plugin and return HTTP 500 (request plugins) or a GraphQL error (resolver plugins). Log the exception details at ERROR level.

### Session 2026-06-04

- Q: How should a resolver plugin's GraphQL schema be parsed and validated during load? → A: Canonical behavior is defined normatively in **FR-021** (`graphqlSchema()` authoritative; `graphqlSchemaSdl` informational only; schema-supplied and no-schema paths validated per FR-021).
- Q: Is supplying a GraphQL schema mandatory for resolver plugins, or can a plugin register resolvers to an existing schema? → A: Schema supply is optional; resolver-plugin load behavior follows **FR-021** for both paths.

### Session 2026-05-25

- Q: What plugin-load library path security policy should be enforced? → A: Allow only absolute library paths that are under the configured `plugins_root`; reject relative paths and absolute paths outside `plugins_root`.
- Q: What restore failure policy should be applied for persisted plugins that fail to reload on startup? → A: Persist restore metadata (`last_restore_error`, `restore_attempts`, `last_restore_attempt_at`) and retry failed restores on every startup until an operator manually unloads or removes the plugin.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST define a `PluginKind` enumeration with values `Request` and `Resolver` to distinguish the two supported plugin categories. For GraphQL inputs/outputs, these map to enum literals `REQUEST` and `RESOLVER` respectively.
- **FR-002**: Every plugin MUST expose a `manifest()` method returning a `PluginManifest` containing: `id` (unique string), `name`, `version`, `description`, `libraryPath`, `abi_version` (semver string declaring the plugin's ABI version, e.g., `"1.0.0"`), `openApiFragmentJson`, `graphqlSchemaSdl` (for resolver plugins; optional informational metadata only), `requestRoutes` (for request plugins), and `resolverBindings` (for resolver plugins).
- **FR-003**: Request plugins MUST implement `handleRequest(PluginHttpRequest) -> std::optional<PluginHttpResponse>` for HTTP dispatch.
- **FR-004**: Resolver plugins MUST implement `registerResolvers(GqlExecutor&)` and `unregisterResolvers(GqlExecutor&)` for GraphQL executor integration.
- **FR-005**: Plugin shared libraries MUST export factory functions with C linkage: `isched_create_request_plugin` for request plugins and `isched_create_resolver_plugin` for resolver plugins.
- **FR-006**: The `PluginManager` MUST detect and reject loading of a request plugin whose routes conflict with any already-registered route using a deterministic segment-based algorithm: (a) compare routes only when HTTP methods are equal after normalising method tokens to uppercase; (b) normalise each path by requiring a leading `/`, removing one trailing `/` when path length is greater than 1, and preserving segment case (case-sensitive matching); (c) split each normalised path into `/`-delimited segments, where template segments use `{name}` syntax and match any single non-empty segment; (d) if segment counts differ, the routes do not conflict; (e) if segment counts are equal and every segment position is compatible (same literal on both routes, or at least one side is a template segment), the routes conflict and the new plugin load MUST be rejected.
- **FR-007**: The `PluginManager` MUST detect and reject loading of a resolver plugin whose field bindings (scope + fieldName) conflict with any already-registered binding.
- **FR-008**: Request-plugin dispatch for all HTTP traffic under the `/plugin/*` path prefix MUST occur in `HttpSession::handle_request()` (the dispatch decision point), which then invokes the loaded plugin handler for matched routes.
- **FR-009**: The `PluginManager` MUST expose a `loadPlugin` operation and an `unloadPlugin` operation, both accessible as GraphQL mutations.
- **FR-010**: The GraphQL schema MUST expose a `plugins` query returning all persisted `PluginRecord` entries and a `plugin(id: String!)` query returning a single record by id.
- **FR-011**: Plugin state MUST be persisted in a `plugin_registry` SQLite table in `isched_system.db` with columns: `id`, `kind`, `name`, `version`, `description`, `library_path`, `manifest_json`, `is_loaded`, `loaded_at`, `unloaded_at`, `last_restore_error`, `restore_attempts`, `last_restore_attempt_at`.
- **FR-012**: On server startup, `PluginManager::restore_persisted_plugins()` MUST be called to reload all plugins whose `is_loaded` flag is `true` in `plugin_registry`. If a restore attempt fails, the system MUST persist `last_restore_error`, increment `restore_attempts`, set `last_restore_attempt_at`, and continue startup without crashing. Failed restore candidates MUST remain eligible for automatic restore retry on every subsequent startup until explicitly unloaded or removed by an operator.
- **FR-013**: The `PluginManager` MUST provide an `inject_loaded_plugin()` method that accepts a pre-constructed plugin instance and registers it without invoking the shared-library loader, enabling unit-test scenarios that bypass dynamic library loading.
- **FR-014**: `GqlExecutor` MUST expose an `unregister_resolver` method to support resolver plugin unloading.
- **FR-015**: The `DatabaseManager` MUST expose plugin-registry CRUD methods for persisting and querying `PluginRecord` entries.
- **FR-016**: The unit test suite MUST achieve ≥ 80% line and branch coverage on plugin system implementation files.
- **FR-017**: The `loadPlugin` and `unloadPlugin` GraphQL mutations, and the `plugins` and `plugin(id:)` GraphQL queries, MUST be protected by a `require_roles` guard restricting invocation to callers holding the `platform_admin` role. Requests from callers without this role MUST be rejected with an authorisation error before any plugin read or plugin state mutation logic executes.
- **FR-018**: Every plugin manifest MUST include an `abi_version` field containing a semantic-versioning string (e.g., `"1.0.0"`). At load time the `PluginManager` MUST compare the plugin manifest's `abi_version` major component against the server's current plugin ABI major version (initial value: **`1`**, full version **`1.0.0`**). If the major versions differ the plugin MUST be rejected with a descriptive error message identifying the version mismatch; minor and patch differences are accepted.
- **FR-019**: The `openApiFragmentJson` field in a plugin manifest MUST be validated at load time by attempting to parse it with `nlohmann::json::parse`. If the string is not valid JSON or is not parseable, the plugin load MUST be rejected with a descriptive error message; partially-loaded state MUST NOT result from this failure.
- **FR-020**: If a request plugin's `handleRequest()` throws an uncaught exception, or a resolver plugin's registered resolver callable throws an uncaught exception, the `PluginManager` MUST (a) catch the exception, (b) automatically unload the offending plugin (deregistering its routes or resolver bindings and marking `is_loaded = false` in the registry), (c) log the exception type and message at ERROR level, and (d) return an HTTP 500 response to the caller (for request plugins) or a GraphQL error node (for resolver plugins). This automatic unload MUST itself be exception-safe.
- **FR-021**: Resolver plugins MAY optionally supply their GraphQL schema via a `graphqlSchema()` method that returns `std::string`. `ResolverPlugin::graphqlSchema()` is authoritative at load time; manifest field `graphqlSchemaSdl` is informational metadata only and not used for parse/validation/merge decisions. **If schema is supplied**: non-empty SDL MUST be validated through `GqlExecutor::load_schema` using the project PEGTL grammar (`isched_gql_grammar`), then checked for conflicts against core and already-loaded plugin schema definitions before resolver registration; any validation or conflict failure MUST reject plugin load with no partial activation. **If no schema is supplied**: `registerResolvers()` may bind only to fields already defined in the existing server schema, and registration MUST fail if any target field is missing or already has a resolver.
- **FR-022**: The `loadPlugin` mutation MUST enforce a library path security policy: `libraryPath` MUST be an absolute filesystem path and MUST resolve within the configured `plugins_root` directory. Relative paths and absolute paths that are outside `plugins_root` MUST be rejected with a validation error before any shared-library load attempt or plugin state mutation occurs.
- **FR-023**: The GraphQL schema MUST expose a dedicated administration subscription for plugin lifecycle notifications (plugin load/unload). On each successful `loadPlugin` state transition, the system MUST publish exactly one `LOADED` event; on each successful `unloadPlugin` state transition, the system MUST publish exactly one `UNLOADED` event. Each published lifecycle event MUST be delivered once to each currently connected authorized subscriber.
- **FR-024**: Each delivered plugin lifecycle subscription event payload MUST include, at minimum, `eventType` (`LOADED` or `UNLOADED`), `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, and `occurredAt` (timestamp). Failed load/unload attempts that do not complete a lifecycle state transition MUST NOT publish lifecycle events and therefore MUST NOT deliver lifecycle events to subscribers.
- **FR-025**: The plugin lifecycle administration subscription MUST be access-controlled with the same `platform_admin` role policy as plugin-management mutations and queries; unauthorized callers MUST be rejected before any subscription stream is established.

### Key Entities *(include if feature involves data)*

- **PluginKind**: Enumeration with values `Request` and `Resolver` classifying the two plugin categories. GraphQL enum literals map as `REQUEST -> Request` and `RESOLVER -> Resolver`.
- **PluginManifest**: Value struct carrying all metadata for a plugin instance — `id` (unique string key), `name`, `version`, `description`, `libraryPath`, `abi_version` (semver string declaring the plugin's ABI version, e.g., `"1.0.0"`), `openApiFragmentJson` (OpenAPI 3.x JSON fragment for the plugin's routes — must be valid JSON, validated at load time), `graphqlSchemaSdl` (optional GraphQL SDL metadata snapshot for resolver plugins; informational only and not authoritative for load-time schema parsing, conflict checks, or merge), `requestRoutes` (list of route descriptors, supporting both exact paths and path-template paths using `{param}` segment syntax), `resolverBindings` (list of scope+fieldName pairs for resolver plugins).
- **Plugin**: Abstract base interface requiring all plugins to implement `manifest() -> PluginManifest`.
- **RequestPlugin**: Extends `Plugin`; adds `handleRequest(PluginHttpRequest) -> std::optional<PluginHttpResponse>` for processing HTTP calls dispatched to the plugin's registered routes.
- **ResolverPlugin**: Extends `Plugin`; adds `graphqlSchema() -> std::string` as the authoritative optional SDL source at load time (empty string = no schema supplied, non-empty string = schema supplied), plus `registerResolvers(GqlExecutor&)` and `unregisterResolvers(GqlExecutor&)`. Schema-supplied and no-schema registration rules are defined normatively in **FR-021**.
- **PluginRecord**: Persisted representation of a plugin's registry entry, mapping 1:1 to a `plugin_registry` database row; tracks `isLoaded`, `loadedAt`, `unloadedAt`, `lastRestoreError`, `restoreAttempts`, and `lastRestoreAttemptAt` in addition to manifest data.
- **PluginManager**: Central orchestrator responsible for load/unload lifecycle, conflict detection for both route and resolver bindings, HTTP dispatch to request plugins, OpenAPI fragment aggregation, and database persistence. Exposes `inject_loaded_plugin()` for test injection.
- **plugin_registry** (SQLite table): Durable store for all plugin records in `isched_system.db`. Schema: `id TEXT PRIMARY KEY`, `kind TEXT`, `name TEXT`, `version TEXT`, `description TEXT`, `library_path TEXT`, `manifest_json TEXT`, `is_loaded INTEGER`, `loaded_at TEXT`, `unloaded_at TEXT`, `last_restore_error TEXT`, `restore_attempts INTEGER`, `last_restore_attempt_at TEXT`.
- **PluginOperationResult** (GraphQL type): Represents the outcome of a `loadPlugin` or `unloadPlugin` mutation — includes a success flag and an optional error message.
- **LoadPluginInput** (GraphQL input type): Input for the `loadPlugin` mutation — carries `libraryPath` and `kind`. `libraryPath` must be an absolute path under configured `plugins_root`.
- **PluginLifecycleEventType** (GraphQL enum type): Lifecycle event classification for admin subscription notifications with values `LOADED` and `UNLOADED`.
- **PluginLifecycleEvent** (GraphQL subscription payload type): Administrative lifecycle event payload published when plugin lifecycle state changes successfully; includes `eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, and `occurredAt`.

### Assumptions

- Boost.DLL is the shared-library loading mechanism; it is header-only within Boost 1.84.0 and requires `Boost::filesystem` and the platform dynamic-linker library (`${CMAKE_DL_LIBS}`) as link dependencies.
- Plugin shared libraries are pre-compiled and accessible on the server's filesystem under a configured `plugins_root`; the administrator provides an absolute path within that root via the GraphQL mutation.
- The `plugin_registry` table is created on first use by `DatabaseManager`; schema initialisation is handled transparently alongside other system schema setup.
- OpenAPI fragment generation from plugin manifests requires a valid JSON string: the plugin author provides the `openApiFragmentJson` value, and the server validates it with `nlohmann::json::parse` at load time, rejecting the plugin if the value is not parseable. The server aggregates valid fragments but does not further validate their OpenAPI 3.x semantic correctness.
- Resolver plugin schema-supply behavior, SDL validation path (`GqlExecutor::load_schema` with `isched_gql_grammar`), and no-schema registration constraints follow the normative rules in **FR-021**.
- The system is single-process; thread-safe access to the plugin registry and dispatcher maps is required and is implemented with an appropriate synchronisation primitive.
- Plugin unload releases the shared-library handle; in-flight requests to a request plugin's route at the moment of unload may receive an error response. This race condition is accepted for this iteration.
- No hot-reload (unload + reload of the same library file in one atomic step) is explicitly supported; operators must issue separate `unloadPlugin` and `loadPlugin` mutations to update a plugin.
- The admin UI (SP-011 GraphQL Playground) can be used to issue plugin management mutations; no dedicated admin panel UI is in scope for this feature.
- Test injection via `inject_loaded_plugin()` is the unit-test mechanism for exercising plugin behaviour without real shared libraries; end-to-end testing with actual Boost.DLL loading is covered separately.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 17 plugin-system Catch2 test cases pass and 48 assertions are verified when `ctest --output-on-failure` is run, with the full test suite reporting 53/53 tests passing.
- **SC-002**: Measured code coverage on `isched_PluginSystem.cpp` (and modified `DatabaseManager` and `GqlExecutor` files) reaches ≥ 80% line and branch coverage.
- **SC-003**: A `loadPlugin` mutation loading a request-plugin library and a subsequent HTTP request to the plugin's registered route complete successfully in a functional acceptance test, with the plugin's response returned to the caller.
- **SC-004**: A `loadPlugin` mutation loading a resolver-plugin library and a subsequent GraphQL query targeting a field registered by that plugin return a valid response in a functional acceptance test.
- **SC-005**: Attempting to load a second plugin claiming a route or resolver field already owned by a loaded plugin returns a conflict error from the `loadPlugin` mutation and leaves the existing plugin unaffected.
- **SC-006**: After an `unloadPlugin` mutation, the unloaded plugin's routes (or resolver bindings) are no longer reachable, and the `plugin_registry` record shows `is_loaded = false` with a populated `unloaded_at` timestamp.
- **SC-007**: A plugin loaded before a server process restart is automatically restored and operational after `restore_persisted_plugins()` completes during startup, verifiable by querying `plugins` and confirming `isLoaded: true`.
- **SC-008**: Release readiness gate: on the designated release-candidate build, the complete automated validation run must pass with 0 failed tests across the full suite and no open Sev-1/Sev-2 defects attributable to plugin-system behavior.
- **SC-009**: A `loadPlugin` or `unloadPlugin` mutation, or a `plugins` / `plugin(id:)` query, issued by a caller without the `platform_admin` role is rejected with an authorisation error; plugin state remains unchanged and protected plugin metadata is not returned.
- **SC-010**: A plugin whose manifest declares an `abi_version` with a major component different from the server's ABI major version (`1`) is rejected at load time with a version-mismatch error; no record is created or modified in `plugin_registry`.
- **SC-011**: A plugin whose `openApiFragmentJson` is not valid JSON is rejected at load time with a parse-error message; `nlohmann::json::parse` must fail on the fragment and the plugin must not be activated.
- **SC-012**: A request plugin that registers a path-template route (e.g., `GET /plugin/items/{id}`) is routable; a second load attempt registering an overlapping route is rejected with a prefix-overlap conflict error; and if the loaded plugin's `handleRequest()` throws, the plugin is automatically unloaded and the caller receives HTTP 500.
- **SC-013**: Resolver-plugin load outcomes for both schema-supplied and no-schema paths match **FR-021** in functional acceptance tests: valid inputs activate successfully, and invalid non-empty SDL is rejected with no partial activation.
- **SC-014**: Resolver-plugin conflict handling matches **FR-021** in functional acceptance tests: schema conflicts, already-bound fields, and missing target fields are rejected without partial activation.
- **SC-015**: A `loadPlugin` mutation providing a relative path, or an absolute path outside configured `plugins_root`, is rejected with a path-policy validation error and no plugin is loaded, persisted, or activated.
- **SC-016**: If a persisted plugin fails startup restore, the corresponding `plugin_registry` row records `last_restore_error`, increments `restore_attempts`, and updates `last_restore_attempt_at` for that attempt; on the next server startup, `restore_persisted_plugins()` attempts that plugin again unless it was manually unloaded or removed.
- **SC-017**: In acceptance tests with one or more active, currently connected `platform_admin` subscribers, each successful `loadPlugin` operation publishes exactly one lifecycle event with `eventType = LOADED` and each successful `unloadPlugin` operation publishes exactly one lifecycle event with `eventType = UNLOADED`; for each published event, every currently connected authorized subscriber receives that event exactly once, with delivery observed within 2 seconds of mutation completion in at least 95% of trials.
- **SC-018**: In negative-path acceptance tests (including load validation failure, load conflict rejection, unload-not-found, and idempotent unload of an already-unloaded plugin), lifecycle subscription event count remains unchanged (zero new events), and for successful-path events all required payload fields (`eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, `occurredAt`) are non-empty and match the affected plugin record.



