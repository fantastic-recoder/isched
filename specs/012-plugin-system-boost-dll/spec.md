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

1. **Given** a valid request-plugin shared library, **When** `loadPlugin` mutation is called with its library path and kind `REQUEST`, **Then** the plugin is loaded, its manifest is persisted to the `plugin_registry` table, and its declared routes are reachable under `/plugin/*`.
2. **Given** a request plugin is already loaded on a given route, **When** a second plugin attempts to register the same route (normalised as `METHOD /path` uppercase), **Then** loading is rejected with a conflict error and the second library is not activated.
3. **Given** a loaded request plugin, **When** an HTTP request arrives at its registered path, **Then** `HttpSession` dispatches the call to the plugin's `handleRequest` handler and returns the plugin's response to the client.
4. **Given** a loaded request plugin, **When** `unloadPlugin` mutation is called with its id, **Then** the plugin's routes are deregistered, the plugin is unloaded from memory, and the `plugin_registry` record is marked `is_loaded = false` with an `unloaded_at` timestamp.

---

### User Story 2 — Load and activate a resolver plugin at runtime (Priority: P1)

As a server administrator, I can load a shared library that implements GraphQL resolvers and have those resolvers registered in the live GraphQL executor without a server restart so I can extend the GraphQL API on the fly.

**Why this priority**: Resolver plugins allow the GraphQL schema surface to grow dynamically, which is the primary motivation for a plugin architecture in a GraphQL-first backend.

**Independent Test**: Can be fully tested by loading a resolver-plugin shared library, then executing a GraphQL query or mutation that targets a field registered by the plugin, and verifying a valid response is returned.

**Acceptance Scenarios**:

1. **Given** a valid resolver-plugin shared library, **When** `loadPlugin` mutation is called with its library path and kind `RESOLVER`, **Then** the plugin calls `registerResolvers(GqlExecutor&)` to attach its resolvers, its manifest is persisted, and the registered fields become queryable immediately.
2. **Given** a resolver plugin is already registered for a given scope and field name, **When** a second plugin attempts to register the same scope+fieldName combination, **Then** loading is rejected with a conflict error and the second library is not activated.
3. **Given** a loaded resolver plugin, **When** `unloadPlugin` mutation is called with its id, **Then** the plugin calls `unregisterResolvers(GqlExecutor&)` to remove its bindings, the library is unloaded, and the persisted record is marked unloaded.

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
3. **Given** a persisted plugin whose shared library file is missing at restart, **When** `restore_persisted_plugins()` attempts to load it, **Then** an error is logged, the record is marked as failed to restore, and the server continues startup without crashing.

---

### User Story 5 — Confidence through automated tests (Priority: P3)

As a delivery team member, I want a comprehensive Catch2 unit test suite covering the plugin system's core flows so regressions during future development are caught early.

**Why this priority**: The plugin system is intricate (conflict detection, dispatch, persistence, startup restore). Automated tests protect all of these invariants across refactors.

**Independent Test**: Can be fully tested by running `ctest` and verifying that the plugin system test suite passes all cases and that measured line/branch coverage on plugin system files meets the 80% threshold.

**Acceptance Scenarios**:

1. **Given** the plugin system test suite, **When** `ctest --output-on-failure` is run, **Then** all 17 plugin-system test cases pass (48 assertions) and the full test suite reports 53/53 passing.
2. **Given** the plugin system implementation files, **When** coverage is measured, **Then** line and branch coverage for `isched_PluginSystem.cpp` and related modified files reaches ≥ 80%.
3. **Given** the test suite, **When** reviewed, **Then** it covers: successful load/unload of both plugin kinds, conflict detection for routes and resolver bindings, HTTP dispatch to a loaded request plugin, `inject_loaded_plugin` for test-mode bypass of Boost.DLL loading, and startup restore behaviour.

### Edge Cases

- A shared library exports neither `isched_create_request_plugin` nor `isched_create_resolver_plugin`: loading is rejected with a descriptive error.
- A plugin's `manifest()` returns an id that is empty or already present in the registry under a different library path: conflict is detected and loading is rejected.
- Two plugins attempt to register routes that are the same after normalisation (e.g., different casing): the second is rejected.
- Two plugins attempt to register the same GraphQL field under the same scope: the second is rejected.
- A plugin shared library file exists but is not a valid shared library: Boost.DLL throws at load time; the exception is caught, logged, and a user-visible error is returned via GraphQL.
- `unloadPlugin` is called for an id that does not exist in `plugin_registry`: a not-found error is returned.
- `unloadPlugin` is called for a plugin that is already unloaded: the operation is idempotent and returns success without re-unloading.
- A resolver plugin's `unregisterResolvers` throws during unload: the exception is caught, the library is still unloaded, and the error is recorded.
- `restore_persisted_plugins()` is called when the `plugin_registry` table does not exist yet: the table is created on first use by `DatabaseManager`.
- A caller without the `platform_admin` role issues `loadPlugin` or `unloadPlugin`: the mutation is rejected with an authorisation error before any plugin state is mutated.
- A plugin manifest's `abi_version` major component does not match the server's ABI major version (e.g., plugin declares `"2.0.0"` when server is `1.x.x`): loading is rejected with a version-mismatch error identifying both versions.
- A plugin manifest's `openApiFragmentJson` is not valid JSON (e.g., truncated or empty string): loading is rejected with a parse-error message; no partial state is created.
- A request plugin registers a path-template route (e.g., `GET /plugin/items/{id}`) and a second plugin tries to register an overlapping static or templated route (e.g., `GET /plugin/items/latest`): the second load is rejected with a prefix-overlap conflict error.
- A request plugin's `handleRequest()` throws an uncaught exception: the plugin is automatically unloaded, the exception is logged at ERROR level, and the caller receives HTTP 500.
- A resolver plugin's registered resolver callable throws an uncaught exception: the plugin is automatically unloaded, the exception is logged at ERROR level, and the GraphQL response contains an error node.

## Clarifications

### Session 2026-06-03

- Q: Should `loadPlugin` / `unloadPlugin` be access-controlled, and if so to which role? → A: Restrict `loadPlugin` and `unloadPlugin` to the `platform_admin` role via `require_roles`.
- Q: How should plugin ABI / versioning compatibility be enforced? → A: Use semantic versioning in the manifest (`abi_version` field); reject plugins whose major version is incompatible with the server's current plugin ABI major version (initial ABI version: `1.0.0`).
- Q: What should happen when `openApiFragmentJson` is invalid or unparseable? → A: Validate the OpenAPI fragment JSON with `nlohmann::json::parse` at load time; if invalid or not parseable, reject the plugin load with a descriptive error.
- Q: What types of route paths should plugins be allowed to register, and how should conflicts be detected? → A: Support path templates (e.g., `/plugin/items/{id}`) with prefix-overlap conflict detection. Exact paths are always supported. Parameterised segments (e.g., `{id}`) denote variable path components. Two routes conflict if one is an exact sub-match or prefix-overlap of the other.
- Q: How should plugin crashes (uncaught exceptions from handlers) be contained? → A: If a request plugin's `handleRequest()` or a resolver plugin's registered resolver throws an uncaught exception, automatically unload the offending plugin and return HTTP 500 (request plugins) or a GraphQL error (resolver plugins). Log the exception details at ERROR level.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST define a `PluginKind` enumeration with values `Request` and `Resolver` to distinguish the two supported plugin categories.
- **FR-002**: Every plugin MUST expose a `manifest()` method returning a `PluginManifest` containing: `id` (unique string), `name`, `version`, `description`, `libraryPath`, `abi_version` (semver string), `openApiFragmentJson`, `requestRoutes` (for request plugins), and `resolverBindings` (for resolver plugins).
- **FR-003**: Request plugins MUST implement `handleRequest(PluginHttpRequest) -> std::optional<PluginHttpResponse>` for HTTP dispatch.
- **FR-004**: Resolver plugins MUST implement `registerResolvers(GqlExecutor&)` and `unregisterResolvers(GqlExecutor&)` for GraphQL executor integration.
- **FR-005**: Plugin shared libraries MUST export factory functions with C linkage: `isched_create_request_plugin` for request plugins and `isched_create_resolver_plugin` for resolver plugins.
- **FR-006**: The `PluginManager` MUST detect and reject loading of a request plugin whose routes conflict with any already-registered route. Route conflict detection MUST normalise keys as `METHOD /path` in uppercase for exact-path routes. Additionally, path template routes (using `{param}` segment syntax, e.g., `/plugin/items/{id}`) MUST be registered and matched; two routes conflict if one is an exact duplicate or if a prefix-overlap exists between a templated and an exact route (i.e., the static portion of a parameterised path overlaps with an already-registered static or templated path under the same method).
- **FR-007**: The `PluginManager` MUST detect and reject loading of a resolver plugin whose field bindings (scope + fieldName) conflict with any already-registered binding.
- **FR-008**: Request plugin HTTP traffic MUST be dispatched at the `/plugin/*` path prefix inside `HttpSession::handle_request()`.
- **FR-009**: The `PluginManager` MUST expose a `loadPlugin` operation and an `unloadPlugin` operation, both accessible as GraphQL mutations.
- **FR-010**: The GraphQL schema MUST expose a `plugins` query returning all persisted `PluginRecord` entries and a `plugin(id: String!)` query returning a single record by id.
- **FR-011**: Plugin state MUST be persisted in a `plugin_registry` SQLite table in `isched_system.db` with columns: `id`, `kind`, `name`, `version`, `description`, `library_path`, `manifest_json`, `is_loaded`, `loaded_at`, `unloaded_at`.
- **FR-012**: On server startup, `PluginManager::restore_persisted_plugins()` MUST be called to reload all plugins whose `is_loaded` flag is `true` in `plugin_registry`.
- **FR-013**: The `PluginManager` MUST provide an `inject_loaded_plugin()` method that accepts a pre-constructed plugin instance and registers it without invoking the shared-library loader, enabling unit-test scenarios that bypass dynamic library loading.
- **FR-014**: `GqlExecutor` MUST expose an `unregister_resolver` method to support resolver plugin unloading.
- **FR-015**: The `DatabaseManager` MUST expose plugin-registry CRUD methods for persisting and querying `PluginRecord` entries.
- **FR-016**: The unit test suite MUST achieve ≥ 80% line and branch coverage on plugin system implementation files.
- **FR-017**: The `loadPlugin` and `unloadPlugin` GraphQL mutations MUST be protected by a `require_roles` guard restricting invocation to callers holding the `platform_admin` role. Requests from callers without this role MUST be rejected with an authorisation error before any plugin load or unload logic executes.
- **FR-018**: Every plugin manifest MUST include an `abi_version` field containing a semantic-versioning string (e.g., `"1.0.0"`). At load time the `PluginManager` MUST compare the plugin manifest's `abi_version` major component against the server's current plugin ABI major version (initial value: **`1`**, full version **`1.0.0`**). If the major versions differ the plugin MUST be rejected with a descriptive error message identifying the version mismatch; minor and patch differences are accepted.
- **FR-019**: The `openApiFragmentJson` field in a plugin manifest MUST be validated at load time by attempting to parse it with `nlohmann::json::parse`. If the string is not valid JSON or is not parseable, the plugin load MUST be rejected with a descriptive error message; partially-loaded state MUST NOT result from this failure.
- **FR-020**: If a request plugin's `handleRequest()` throws an uncaught exception, or a resolver plugin's registered resolver callable throws an uncaught exception, the `PluginManager` MUST (a) catch the exception, (b) automatically unload the offending plugin (deregistering its routes or resolver bindings and marking `is_loaded = false` in the registry), (c) log the exception type and message at ERROR level, and (d) return an HTTP 500 response to the caller (for request plugins) or a GraphQL error node (for resolver plugins). This automatic unload MUST itself be exception-safe.

### Key Entities *(include if feature involves data)*

- **PluginKind**: Enumeration with values `Request` and `Resolver` classifying the two plugin categories.
- **PluginManifest**: Value struct carrying all metadata for a plugin instance — `id` (unique string key), `name`, `version`, `description`, `libraryPath`, `abi_version` (semver string declaring the plugin's ABI version, e.g., `"1.0.0"`), `openApiFragmentJson` (OpenAPI 3.x JSON fragment for the plugin's routes — must be valid JSON, validated at load time), `requestRoutes` (list of route descriptors, supporting both exact paths and path-template paths using `{param}` segment syntax), `resolverBindings` (list of scope+fieldName pairs for resolver plugins).
- **Plugin**: Abstract base interface requiring all plugins to implement `manifest() -> PluginManifest`.
- **RequestPlugin**: Extends `Plugin`; adds `handleRequest(PluginHttpRequest) -> std::optional<PluginHttpResponse>` for processing HTTP calls dispatched to the plugin's registered routes.
- **ResolverPlugin**: Extends `Plugin`; adds `registerResolvers(GqlExecutor&)` to attach resolvers at load time and `unregisterResolvers(GqlExecutor&)` to detach them at unload time.
- **PluginRecord**: Persisted representation of a plugin's registry entry, mapping 1:1 to a `plugin_registry` database row; tracks `isLoaded`, `loadedAt`, and `unloadedAt` in addition to manifest data.
- **PluginManager**: Central orchestrator responsible for load/unload lifecycle, conflict detection for both route and resolver bindings, HTTP dispatch to request plugins, OpenAPI fragment aggregation, and database persistence. Exposes `inject_loaded_plugin()` for test injection.
- **plugin_registry** (SQLite table): Durable store for all plugin records in `isched_system.db`. Schema: `id TEXT PRIMARY KEY`, `kind TEXT`, `name TEXT`, `version TEXT`, `description TEXT`, `library_path TEXT`, `manifest_json TEXT`, `is_loaded INTEGER`, `loaded_at TEXT`, `unloaded_at TEXT`.
- **PluginOperationResult** (GraphQL type): Represents the outcome of a `loadPlugin` or `unloadPlugin` mutation — includes a success flag and an optional error message.
- **LoadPluginInput** (GraphQL input type): Input for the `loadPlugin` mutation — carries `libraryPath` and `kind`.

### Assumptions

- Boost.DLL is the shared-library loading mechanism; it is header-only within Boost 1.84.0 and requires `Boost::filesystem` and the platform dynamic-linker library (`${CMAKE_DL_LIBS}`) as link dependencies.
- Plugin shared libraries are pre-compiled and accessible on the server's filesystem; the administrator provides absolute paths via the GraphQL mutation.
- The `plugin_registry` table is created on first use by `DatabaseManager`; schema initialisation is handled transparently alongside other system schema setup.
- OpenAPI fragment generation from plugin manifests requires a valid JSON string: the plugin author provides the `openApiFragmentJson` value, and the server validates it with `nlohmann::json::parse` at load time, rejecting the plugin if the value is not parseable. The server aggregates valid fragments but does not further validate their OpenAPI 3.x semantic correctness.
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
- **SC-008**: Before release, `ctest --output-on-failure` reports 0 failures across the full test suite.
- **SC-009**: A `loadPlugin` or `unloadPlugin` mutation issued by a caller without the `platform_admin` role is rejected with an authorisation error and leaves all plugin state unchanged.
- **SC-010**: A plugin whose manifest declares an `abi_version` with a major component different from the server's ABI major version (`1`) is rejected at load time with a version-mismatch error; no record is created or modified in `plugin_registry`.
- **SC-011**: A plugin whose `openApiFragmentJson` is not valid JSON is rejected at load time with a parse-error message; `nlohmann::json::parse` must fail on the fragment and the plugin must not be activated.
- **SC-012**: A request plugin that registers a path-template route (e.g., `GET /plugin/items/{id}`) is routable; a second load attempt registering an overlapping route is rejected with a prefix-overlap conflict error; and if the loaded plugin's `handleRequest()` throws, the plugin is automatically unloaded and the caller receives HTTP 500.
