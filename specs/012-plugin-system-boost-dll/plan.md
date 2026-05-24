# Implementation Plan: Plugin System (Boost.DLL) — Gap Closure

**Branch**: `012-plugin-system-boost-dll` | **Date**: 2026-06-03 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification SP-012 (plugin system gaps identified post-initial implementation)

---

## Summary

The core plugin system (Boost.DLL loading, conflict detection, GraphQL mutations/queries, HTTP dispatch,
persistence, startup restore, 17-test suite) is **fully implemented and merged**. This plan covers the
five remaining gaps between the committed implementation and the complete FR set:

| Gap | FR | Status |
|-----|----|--------|
| Authorization guard on plugin mutations/queries | FR-017 | ❌ missing |
| ABI versioning in manifest + major-version check at load time | FR-018 | ❌ missing |
| OpenAPI fragment JSON validation at load time | FR-019 | ❌ missing |
| Path-template route conflict detection | FR-006 (updated) | ❌ missing |
| Crash containment for request + resolver plugin callables | FR-020 | ❌ missing |

---

## Technical Context

**Language/Version**: C++23 (required by constitution)
**Primary Dependencies**: Boost.DLL 1.84.0 (header-only + `Boost::filesystem` + `${CMAKE_DL_LIBS}`), nlohmann/json (already in use), spdlog (already in use), Catch2 (testing)
**Storage**: SQLite via `DatabaseManager` (`isched_system.db`, `plugin_registry` table — already implemented)
**Testing**: Catch2 unit tests; `inject_loaded_plugin()` test-injection helper (already implemented)
**Target Platform**: Linux server (cross-platform portability required per constitution)
**Project Type**: C++ backend service (embedded HTTP + GraphQL server)
**Performance Goals**: Plugin dispatch is not on the primary hot path; `O(n)` over loaded plugins acceptable for load/unload. Crash-guard wrapping introduces one try/catch per resolver invocation — acceptable per constitution guidance (neutral-to-positive vs. deep conditionals).
**Constraints**: Thread-safe access required (mutex already in place). Auto-unload during exception handling must itself be exception-safe. No plugin ABI breaks for existing plugin authors until ABI version `2.0.0`.
**Scale/Scope**: Plugin set is expected to remain small (tens, not thousands). All conflict detection at load time is acceptable O(n²) over plugin routes.

---

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **GraphQL-only external interface preserved** — No new REST endpoints introduced. All plugin management remains via `/graphql` mutations/queries. The `/plugin/*` HTTP dispatch prefix is an existing implementation for request-plugin payload routing, not a new public API surface.
- [x] **Language and build baseline preserved** — C++23, Conan-managed dependencies, CMake + Ninja. No new third-party dependencies required (all five gaps are solvable with existing libraries).
- [x] **WebUI changes** — None in scope; N/A.
- [x] **Frontend API calls** — None in scope; N/A.
- [x] **Browser JWT handling** — None in scope; N/A.
- [x] **Local Angular development** — None in scope; N/A.
- [x] **Test plan proves required coverage** — Each gap area has distinct test cases designed below covering both the happy path and the error/edge cases. See §Test Strategy per gap.
- [x] **Security-sensitive changes include threat-model updates** — FR-017 (authorization) is a security-sensitive change. A feature-scoped threat model section is included in this plan and a `docs/security-threat-model.md` entry is required before the branch is merged.
- [x] **Clean Code Principles** — Functions that grow beyond a single responsibility as a result of gap work (e.g., `load_request_plugin()`) must be split into focused helpers. The crash-containment design uses polymorphism (`IResolverRegistrar` interface) rather than conditional type-dispatch. The path-template conflict logic is extracted into focused free functions in an anonymous namespace.

**Constitution violations**: None identified. No complexity-tracking entries required.

---

## Project Structure

```text
specs/012-plugin-system-boost-dll/
├── plan.md              ← this file
├── research.md          ← Phase 0 output
├── data-model.md        ← Phase 1 output
├── quickstart.md        ← Phase 1 output
├── contracts/
│   └── graphql-plugin-api.md
└── tasks.md             ← Phase 2 output (speckit.tasks)
```

```text
src/main/cpp/isched/backend/
├── isched_PluginSystem.hpp        ← add abi_version field, IResolverRegistrar interface
├── isched_PluginSystem.cpp        ← add ABI check, JSON validation, template routing,
│                                     crash containment, non-const dispatch_request
└── isched_GqlExecutor.cpp         ← add require_roles() calls for loadPlugin/unloadPlugin/plugins/plugin

src/test/cpp/isched/
└── isched_plugin_system_tests.cpp ← add 9–12 new test cases covering the 5 gaps

docs/
└── security-threat-model.md       ← add SP-012 plugin authorization section
```

---

## Fully Implemented (do not re-implement)

The following are complete and passing. They define the baseline for this plan.

| Component | File(s) | Status |
|-----------|---------|--------|
| `PluginKind`, `PluginManifest`, `PluginRoute`, `PluginResolverBinding`, `PluginHttpRequest/Response`, `PluginRecord`, `PluginOperationResult` | `isched_PluginSystem.hpp` | ✅ |
| `PluginManager`: constructor, `set_executor`, `load_plugin`, `unload_plugin`, `list_plugins`, `get_plugin`, `dispatch_request`, `export_openapi_document`, `restore_persisted_plugins`, `inject_loaded_plugin` | `isched_PluginSystem.cpp` | ✅ |
| Exact-path route conflict detection | `isched_PluginSystem.cpp` | ✅ |
| Resolver binding conflict detection | `isched_PluginSystem.cpp` | ✅ |
| `DatabaseManager`: `initialize_plugin_store`, `upsert_plugin_record`, `get_plugin_record`, `list_plugin_records` | `isched_DatabaseManager.hpp/.cpp` | ✅ |
| `GqlExecutor`: `plugins` query, `plugin(id)` query, `loadPlugin` mutation, `unloadPlugin` mutation, `unregister_resolver` | `isched_GqlExecutor.hpp/.cpp` | ✅ |
| HTTP dispatch at `/plugin/*` | `isched_Server.cpp` | ✅ |
| GraphQL schema types: `PluginRecord`, `PluginOperationResult`, `LoadPluginInput` | `isched_builtin_server_schema.graphql` | ✅ |
| 17 Catch2 unit tests, 48 assertions | `isched_plugin_system_tests.cpp` | ✅ |

---

## Gap 1 — Authorization (FR-017)

### Problem

`loadPlugin` and `unloadPlugin` resolvers in `setup_builtin_resolvers()` are registered without
`require_roles()` guards. Any authenticated caller can load or unload plugins.

### Architecture Decision

Add `require_roles("loadPlugin", {Role::PLATFORM_ADMIN})` and
`require_roles("unloadPlugin", {Role::PLATFORM_ADMIN})` immediately after the respective
`register_resolver(…)` calls in `isched_GqlExecutor.cpp`.

FR-017 mandates guards only on the two mutations. However, both `plugins` and `plugin(id)` queries
expose the server's full extension surface including library filesystem paths.
Decision: **also gate `plugins` and `plugin(id)` behind `PLATFORM_ADMIN`** — this matches the
pattern of every other administrative listing resolver in the codebase (`roles`, `users`,
`organizations`), satisfies the principle of least privilege, and adds no complexity.

### Component Design

**File**: `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
**Function**: `GqlExecutor::setup_builtin_resolvers()` — after each of the four resolver registrations:

```cpp
require_roles("plugins",      {std::string(Role::PLATFORM_ADMIN)});
require_roles("plugin",       {std::string(Role::PLATFORM_ADMIN)});
require_roles("loadPlugin",   {std::string(Role::PLATFORM_ADMIN)});
require_roles("unloadPlugin", {std::string(Role::PLATFORM_ADMIN)});
```

No changes to headers, schemas, or other files.

### Test Strategy

- **T-SEC-01**: `loadPlugin` resolver called without `platform_admin` role → authorization error returned, no plugin state changed.
- **T-SEC-02**: `unloadPlugin` resolver called without `platform_admin` role → authorization error, plugin remains loaded.
- **T-SEC-03**: `plugins` query called without `platform_admin` role → authorization error.
- **T-SEC-04**: `loadPlugin` succeeds for caller with `platform_admin` role (regression guard).

---

## Gap 2 — ABI Versioning (FR-018)

### Problem

`PluginManifest` has no `abi_version` field. There is no check at load time that the plugin's declared
ABI major version matches the server's current plugin ABI major (`1`).

### Architecture Decision

- Add `String abi_version` to `PluginManifest` (default `"1.0.0"`).
- Add module-scoped constants in `isched_PluginSystem.hpp`:
  ```cpp
  inline constexpr std::string_view PLUGIN_ABI_VERSION = "1.0.0";
  inline constexpr int PLUGIN_ABI_MAJOR = 1;
  ```
- Add a focused free function `extract_semver_major(std::string_view) -> std::optional<int>` in the
  anonymous namespace of `isched_PluginSystem.cpp`.
- Add a focused private helper
  `validate_abi_version(const PluginManifest&) -> std::optional<String>`
  returning an error string on mismatch or `std::nullopt` on success.
- Call `validate_abi_version` in both `load_request_plugin()` and `load_resolver_plugin()` after
  obtaining the manifest, before conflict checks.
- Update `manifest_to_json()` / `manifest_from_json()` to include `abi_version`.

**Insertion point in load path**:
```
manifest = plugin->manifest()  →  validate_abi_version()  →  conflict checks  →  persist
```

### Test Strategy

- **T-ABI-01**: Plugin with `abi_version = "1.0.0"` loads successfully.
- **T-ABI-02**: Plugin with `abi_version = "1.5.3"` (minor/patch differ) loads successfully.
- **T-ABI-03**: Plugin with `abi_version = "2.0.0"` (major differs) is rejected; error identifies both versions.
- **T-ABI-04**: Plugin with `abi_version = "0.9.0"` (major `0` ≠ `1`) is rejected.
- **T-ABI-05**: Plugin with `abi_version = ""` or `"not-semver"` is rejected (unparseable major).
- **T-ABI-06**: `abi_version` survives JSON round-trip via `manifest_to_json` / `manifest_from_json`.

Note: Tests T-ABI-01…T-ABI-05 should exercise `validate_abi_version` directly or via a thin accessor,
since `inject_loaded_plugin` bypasses load-time validation.

---

## Gap 3 — OpenAPI Fragment Validation at Load Time (FR-019)

### Problem

`openApiFragmentJson` is accepted without JSON validation at load time. Invalid JSON is silently skipped
during `export_openapi_document()`. The spec requires the plugin load to be **rejected** if the fragment
field contains non-parseable content.

### Architecture Decision

Add validation in `load_request_plugin()` after manifest retrieval. Extract into a focused helper:
`validate_openapi_fragment(const String&) -> std::optional<String>`.

**Empty string rule**: An empty `openApiFragmentJson` is treated as "no fragment provided" and is
always accepted. Only non-empty strings that fail `nlohmann::json::parse` are rejected.

> **Rationale**: The spec says reject if "not valid JSON or not parseable." Empty string is not
> parseable as JSON, but `openApiFragmentJson` is an optional field (resolver plugins never set it,
> many request plugins may omit it). Treating `""` as "absent" avoids breaking all
> plugins that set `openApiFragmentJson = ""`. Non-empty strings are always intentional and must be valid.

**Existing test update required**: Test `"Invalid JSON OpenAPI fragment is silently skipped"` currently
verifies that `export_openapi_document` silently skips an invalid fragment on an already-injected plugin.
After this change the load-time validation is the enforcement point. The test should be updated or
split: one sub-case verifies load-time rejection; a second verifies that `export_openapi_document` still
skips fragments that are invalid at export time (for plugins injected via the test helper which bypasses validation).

### Component Design

**File**: `isched_PluginSystem.cpp` — anonymous-namespace helper:
```cpp
// Returns error message on failure, nullopt on success.
std::optional<String> validate_openapi_fragment(const String& fragment_json);
```

Called in `load_request_plugin()` after manifest is obtained.

### Test Strategy

- **T-OAI-01**: `load_plugin` with valid JSON `openApiFragmentJson` succeeds; fragment appears in `export_openapi_document`.
- **T-OAI-02**: `load_plugin` with `openApiFragmentJson = "not valid json"` fails; `result.errors` contains parse-error message; no plugin loaded.
- **T-OAI-03**: `load_plugin` with `openApiFragmentJson = ""` succeeds.
- **T-OAI-04**: `load_plugin` with `openApiFragmentJson = "{}"` succeeds (valid empty JSON object).
- **T-OAI-05 (updated)**: Existing silent-skip test is updated to verify load-time rejection for bad fragments; validation context restored for the `inject_loaded_plugin` path.

---

## Gap 4 — Path-Template Route Conflict Detection (FR-006 updated)

### Problem

`route_conflicts_with_loaded()` does exact normalized-key string comparison only. Template routes
(`/plugin/items/{id}`) are stored by literal string; prefix-overlap conflicts go undetected.
`dispatch_request()` likewise does exact match and never dispatches to template routes.

### Architecture Decision

Introduce a path-template conflict subsystem as focused free functions in the anonymous namespace of
`isched_PluginSystem.cpp`. No public API changes.

**Conflict rule**: Two routes with the same HTTP method conflict when they have the same segment count
AND every position's segment pair satisfies: both are identical, OR at least one is a template segment
(`{…}`). This covers exact duplicates, template-vs-static, and template-vs-template collisions.

**Dispatch rule**: Same logic applied where the incoming request path is always concrete (no `{…}`);
a route segment of `{…}` matches any single path segment in the request.

**Segment-count mismatch is never a conflict**: `/plugin/a` and `/plugin/a/b` are always distinct.

> **Alternative considered**: Radix tree / trie. Rejected — plugin count stays small (tens of routes);
> O(n²) per load and O(n) per request are acceptable. Profiling evidence required to justify the added
> structural complexity of a trie.

### Component Design

**File**: `isched_PluginSystem.cpp` — anonymous namespace:

```cpp
bool is_template_segment(std::string_view segment);
std::vector<std::string_view> split_path_segments(std::string_view path);
bool routes_overlap(const PluginRoute& a, const PluginRoute& b);
bool route_matches_request(const PluginRoute& route,
                           std::string_view method, std::string_view target);
```

- `route_conflicts_with_loaded()`: Replace inner `candidate == normalize_route_key(existing)` with `routes_overlap(route, existing)`.
- `dispatch_request()`: Replace normalized-key equality with `route_matches_request(route, request.method, request.target)`.

No changes to `isched_PluginSystem.hpp` public API.

### Test Strategy

- **T-RT-01**: Exact-path route registers and dispatches correctly (regression).
- **T-RT-02**: Two plugins with identical routes — second load rejected.
- **T-RT-03**: `/plugin/items/{id}` vs `/plugin/items/latest` → conflict detected, second load rejected.
- **T-RT-04**: `/plugin/items/{id}` vs `/plugin/items/{name}` (same structure, different param) → conflict detected.
- **T-RT-05**: `/plugin/items/{id}` vs `/plugin/orders/{id}` → no conflict, both load.
- **T-RT-06**: Dispatch `GET /plugin/items/42` to plugin with `GET /plugin/items/{id}` → dispatched successfully.
- **T-RT-07**: `/plugin/a` and `/plugin/a/b` (different segment counts) → no conflict, both load.
- **T-RT-08**: `GET /plugin/x` and `POST /plugin/x` (different methods) → no conflict, both load.
- **T-RT-09**: Trailing-slash and double-slash path normalisation edge cases.

---

## Gap 5 — Crash Containment (FR-020)

### Problem

Uncaught exceptions from `RequestPlugin::handleRequest()` or from resolver callables registered in
`GqlExecutor` propagate uncaught, potentially crashing the server process.

### Architecture Decision — Two Separate Mechanisms

#### 5a — Request Plugin Crash Containment

`dispatch_request()` wraps `handleRequest()` in try/catch. On exception:
1. Log plugin ID, exception type, and message at ERROR.
2. Auto-unload the plugin (deregister routes, mark `is_loaded = false` in DB).
3. Return HTTP 500 response.

`dispatch_request()` becomes **non-`const`** to permit `unload_plugin()` internally.

Extract unload-on-crash into:
```cpp
PluginHttpResponse crash_unload_request_plugin(const String& plugin_id,
                                               const std::exception& ex) noexcept;
```

Update `isched_Server.cpp` call site to remove `const` qualifier from the call if needed.

#### 5b — Resolver Plugin Crash Containment

Resolver callables are invoked inside `GqlExecutor::execute()` — outside `PluginManager`'s direct
control. The only clean interception point is **at registration time**.

**Design**: Introduce `IResolverRegistrar` as a thin abstract interface in `isched_PluginSystem.hpp`:

```cpp
class IResolverRegistrar {
public:
    virtual ~IResolverRegistrar() = default;
    using ResolverPath     = GqlExecutor::ResolverPath;
    using ResolverFunction = GqlExecutor::ResolverFunction;
    virtual void register_resolver(const ResolverPath& scope,
                                   const std::string& field_name,
                                   ResolverFunction resolver) = 0;
    virtual void unregister_resolver(const ResolverPath& scope,
                                     const std::string& field_name) = 0;
};
```

Change `ResolverPlugin::registerResolvers(GqlExecutor&)` →
`registerResolvers(IResolverRegistrar&)`.  
Change `ResolverPlugin::unregisterResolvers(GqlExecutor&)` →
`unregisterResolvers(IResolverRegistrar&)`.

`GqlExecutor` gains a thin adapter method or implements `IResolverRegistrar` for use when no crash
guard is needed (e.g., unload path, test injection).

`PluginManager::load_resolver_plugin()` constructs a `CrashSafeRegistrar`
(in-file class, not exposed in the header) and passes it to `registerResolvers()`.

`CrashSafeRegistrar`:
- Holds: `GqlExecutor&`, `String plugin_id`, `PluginManager&` callback reference.
- In `register_resolver()`: delegates to `GqlExecutor` with the original callable **wrapped** in a
  try/catch. On exception: `spdlog::error(…)`, call `crash_unload_resolver_plugin(plugin_id, ex)`,
  return a GraphQL error JSON node (`{"errors":[{"message":"plugin crash…"}]}`).
- In `unregister_resolver()`: delegates directly (no crash guard needed on unload).

`crash_unload_resolver_plugin(plugin_id, ex)` is a `noexcept` helper that:
- Iterates the plugin's `resolverBindings` and calls `m_executor->unregister_resolver()` for each.
- Removes the plugin from `m_loaded_plugins`.
- Updates the DB record to `is_loaded = false`.
- Catches and logs any secondary exceptions.

> **ABI note**: Changing `ResolverPlugin::registerResolvers(GqlExecutor&)` to
> `registerResolvers(IResolverRegistrar&)` is a C++ API break for plugin authors. This is correct to
> do now — no external plugins are deployed at ABI version `1.0.0`. Document in changelog.

> **Clean Code note**: `CrashSafeRegistrar` is a single-responsibility class: wrap callables with
> crash guards. `crash_unload_resolver_plugin` is a focused noexcept cleanup function. Both align with
> the "small and focused functions" and "prefer polymorphism" principles.

### Component Design Summary

| File | Change |
|------|--------|
| `isched_PluginSystem.hpp` | Add `IResolverRegistrar`; update `ResolverPlugin` signatures; declare `crash_unload_request_plugin` and `crash_unload_resolver_plugin` private |
| `isched_PluginSystem.cpp` | Add `CrashSafeRegistrar` (anon ns); make `dispatch_request` non-const; add crash helpers |
| `isched_GqlExecutor.hpp` | Implement `IResolverRegistrar` via adapter (or have `GqlExecutor` implement the interface directly for non-guarded paths) |
| `isched_Server.cpp` | Update `dispatch_request` call site if const removal causes compile error |
| `isched_plugin_system_tests.cpp` | Update `StubResolverPlugin` to use `IResolverRegistrar&` |

### Test Strategy

- **T-CR-01**: `dispatch_request()` where `handleRequest()` throws → returns HTTP 500 `PluginHttpResponse`, plugin absent from `list_plugins()`.
- **T-CR-02**: After crash-unload of a request plugin, the route is available for a second plugin to claim (conflict map cleared).
- **T-CR-03**: Resolver plugin's wrapped callable throws → `GqlExecutor::execute()` returns a response with a GraphQL error node, does not throw, plugin absent from `list_plugins()`.
- **T-CR-04**: Double-crash safety — crash-unload on an already-absent plugin ID does not throw or corrupt state.

---

## Implementation Sequence

```
Phase A — Additive helpers (no existing behaviour changed, safe to commit independently)
  A1. Add abi_version field + constants + extract_semver_major + validate_abi_version helper
  A2. Add validate_openapi_fragment helper
  A3. Add is_template_segment, split_path_segments, routes_overlap, route_matches_request

Phase B — Wire helpers into load/dispatch paths (changes existing behaviour, requires test updates)
  B1. Wire validate_abi_version into load_request_plugin + load_resolver_plugin
  B2. Wire validate_openapi_fragment into load_request_plugin
  B3. Replace route-conflict check with routes_overlap; replace dispatch with route_matches_request
  B4. Update manifest_to_json / manifest_from_json for abi_version
  B5. Update existing "Invalid JSON fragment" test to match new load-time rejection behaviour

Phase C — Interface evolution (coordinated changes, highest coupling)
  C1. Add IResolverRegistrar to isched_PluginSystem.hpp
  C2. Update ResolverPlugin::registerResolvers/unregisterResolvers signatures
  C3. Add GqlExecutor IResolverRegistrar adapter
  C4. Update StubResolverPlugin in test file
  C5. Implement CrashSafeRegistrar; wire into load_resolver_plugin
  C6. Make dispatch_request non-const; add crash_unload_request_plugin

Phase D — Authorization (trivial, isolated, last to avoid locking out running tests)
  D1. Add four require_roles() calls in GqlExecutor::setup_builtin_resolvers()

Phase E — Test suite completion
  E1. Add T-SEC-01…04, T-ABI-01…06, T-OAI-01…05, T-RT-01…09, T-CR-01…04
  E2. Run coverage; verify ≥ 80% line + branch on plugin system files

Phase F — Documentation
  F1. Add SP-012 plugin authorization entry to docs/security-threat-model.md
```

---

## Risk Register

| # | Risk | Likelihood | Severity | Mitigation |
|---|------|------------|----------|------------|
| R1 | `IResolverRegistrar` interface addition changes `ResolverPlugin` public ABI | Medium | High | No external plugins deployed yet; document as intentional correction in ABI `1.0.0` changelog |
| R2 | `dispatch_request` const→non-const propagation breaks call sites | Low | Medium | Grep all call sites before the change; `isched_Server.cpp` is the only known consumer |
| R3 | `crash_unload_resolver_plugin` throws during crash handling (double exception) | Low | High | Helper is `noexcept`; secondary exceptions are caught and logged internally |
| R4 | Template route matching incorrectly flags non-conflicting routes | Medium | Medium | Negative test cases T-RT-05, T-RT-07, T-RT-08, T-RT-09 cover critical non-conflict paths |
| R5 | Default `abi_version = "1.0.0"` in `PluginManifest` causes existing tests to require explicit field | Low | Low | Default is valid; existing stubs (e.g. `make_request_manifest`) simply do not set the field and pass with the default |
| R6 | Empty `openApiFragmentJson = ""` incorrectly rejected | Low | High | Plan explicitly treats `""` as absent; `validate_openapi_fragment` only parses non-empty strings |
| R7 | `require_roles` change breaks existing integration tests querying plugins without `platform_admin` auth | Medium | Low | Audit `plugins`/`plugin` callers in test suite; update to provide `platform_admin` token or use `inject_loaded_plugin` path that bypasses GqlExecutor |
| R8 | Coverage drops below 80% on new crash-path branches | Low | Medium | All new branches exercised via `StubRequestPlugin`/`StubResolverPlugin` throwing stubs in Catch2 tests |
| R9 | Path-segment edge cases (trailing slash, empty segments) cause false conflicts | Low | Low | `split_path_segments` drops empty segments (normalises both stored routes and request targets) |

---

## Threat Model Note (FR-017 — Constitution §Documentation)

**Feature-scoped (SP-012)**:
Plugin loading executes arbitrary shared-library code on the server host.
- **Attack path**: Compromised `platform_admin` credentials → `loadPlugin` mutation → `boost::dll::shared_library` constructor → OS `dlopen` → arbitrary code execution as the server process user.
- **Mitigations in place**: (1) `loadPlugin`/`unloadPlugin` gated to `platform_admin` role (FR-017); (2) library path must be a pre-existing file on the server filesystem — no remote fetch or upload mechanism; (3) ABI version check (FR-018) prevents malformed manifests from activating incompatible code; (4) crash containment (FR-020) limits blast radius of a buggy/malicious plugin to automatic unload + HTTP 500, not server crash.
- **Residual risk**: A malicious plugin can trivially bypass containment by spawning threads or forking. Mitigation is operational: restrict write access to directories from which the server loads plugins.

**Project-level update required**: `docs/security-threat-model.md` must receive a "Plugin System (SP-012)" section covering the above attack path and mitigations before the branch is merged.
