# Data Model: Plugin System Gap Closure (SP-012)

**Phase 1 output** | Branch: `012-plugin-system-boost-dll`

This document describes only the data model **changes** required by the five gap areas. The base model
(all entities listed in spec.md §Key Entities) is already implemented; see plan.md §Fully Implemented.

---

## Modified Entity: PluginManifest

**File**: `src/main/cpp/isched/backend/isched_PluginSystem.hpp`

### Added Field

| Field | Type | Default | Validation |
|-------|------|---------|------------|
| `abi_version` | `String` | `"1.0.0"` | Must be parseable as semver; major component must equal `PLUGIN_ABI_MAJOR` (currently `1`) |

### Updated `PluginManifest` struct

```cpp
struct PluginManifest {
    String id;
    PluginKind kind{PluginKind::Request};
    String name;
    String version;
    String description;
    String libraryPath;
    String abi_version{"1.0.0"};          // ← NEW (FR-018)
    String openApiFragmentJson;
    std::vector<PluginRoute> requestRoutes;
    std::vector<PluginResolverBinding> resolverBindings;
};
```

### JSON Serialisation (manifest_to_json / manifest_from_json)

Add `"abi_version"` key to both serialisation and deserialisation paths.

---

## Modified Entity: PluginRoute (conflict semantics only)

**File**: `src/main/cpp/isched/backend/isched_PluginSystem.hpp` — struct unchanged.

`PluginRoute.path` now supports path-template segments using `{param}` syntax (e.g., `/plugin/items/{id}`). The struct fields are unchanged; the template semantics are enforced at conflict-detection and dispatch time only.

### Template Segment Rules

- A segment is a template segment if it matches the regex `\{[^/]+\}`.
- Template segments are wildcards — they match any single concrete path segment.
- Two routes with the same method and segment count conflict if at least one template segment is involved at any position that would otherwise prevent distinction.

---

## New Constants

**File**: `src/main/cpp/isched/backend/isched_PluginSystem.hpp`

```cpp
/// Current plugin ABI version advertised by this server build.
inline constexpr std::string_view PLUGIN_ABI_VERSION = "1.0.0";
/// Major component of PLUGIN_ABI_VERSION. Plugins must match this.
inline constexpr int PLUGIN_ABI_MAJOR = 1;
```

---

## New Interface: IResolverRegistrar

**File**: `src/main/cpp/isched/backend/isched_PluginSystem.hpp`

Thin abstract interface allowing `PluginManager` to intercept resolver registrations for crash-guard
wrapping without depending on `GqlExecutor` internals.

```cpp
/// Abstract registrar interface passed to ResolverPlugin::registerResolvers.
/// Implemented by GqlExecutor (direct pass-through) and CrashSafeRegistrar (crash-guard wrapper).
class IResolverRegistrar {
public:
    virtual ~IResolverRegistrar() = default;

    using ResolverPath     = std::vector<std::string>;
    using ResolverFunction = std::function<nlohmann::json(
        const nlohmann::json&,   // parent
        const nlohmann::json&,   // args
        const ResolverCtx&)>;    // context

    virtual void register_resolver(const ResolverPath& scope,
                                   const std::string& field_name,
                                   ResolverFunction resolver) = 0;
    virtual void unregister_resolver(const ResolverPath& scope,
                                     const std::string& field_name) = 0;
};
```

> **Dependency note**: `IResolverRegistrar` uses `ResolverCtx` from `GqlExecutor`. The type aliases
> should reference the concrete `GqlExecutor` type aliases to avoid duplication. A forward declaration
> of `ResolverCtx` or a `using` alias may be needed in `isched_PluginSystem.hpp`.

---

## Modified Interface: ResolverPlugin

**File**: `src/main/cpp/isched/backend/isched_PluginSystem.hpp`

```cpp
class ResolverPlugin : public Plugin {
public:
    virtual void registerResolvers(IResolverRegistrar& registrar) = 0;    // changed: GqlExecutor& → IResolverRegistrar&
    virtual void unregisterResolvers(IResolverRegistrar& registrar) = 0;  // changed: GqlExecutor& → IResolverRegistrar&
};
```

**Migration impact**: All plugin authors (and test stubs) must update their override signatures.
The only current callsite is `StubResolverPlugin` in the test file.

---

## Unchanged Entities

The following are fully implemented and require no data-model modifications:

- `PluginKind` — unchanged
- `PluginRecord` — unchanged (persisted JSON includes `abi_version` transparently via `manifestJson`)
- `PluginHttpRequest` / `PluginHttpResponse` — unchanged
- `PluginOperationResult` — unchanged
- `PluginManager` public interface — `dispatch_request` loses `const`; no other signature changes
- `plugin_registry` SQLite table — unchanged (ABI version stored inside `manifest_json` TEXT column)
- GraphQL schema (`PluginRecord`, `PluginOperationResult`, `LoadPluginInput`) — unchanged

---

## State Transitions

### Plugin Load Path (updated)

```
loadPlugin mutation
  │
  ├─ [FR-017] require_roles check → REJECTED if caller lacks platform_admin
  │
  ├─ Boost.DLL load → factory call → manifest()
  │
  ├─ [FR-018] validate_abi_version(manifest)
  │     └─ major mismatch → REJECTED
  │
  ├─ [FR-019] validate_openapi_fragment(manifest.openApiFragmentJson)  [request only]
  │     └─ non-empty invalid JSON → REJECTED
  │
  ├─ conflict detection (routes / resolver bindings)
  │     └─ conflict → REJECTED (now uses template-aware routes_overlap)
  │
  ├─ [FR-020] CrashSafeRegistrar wraps callables  [resolver only]
  │
  └─ persist + activate → SUCCESS
```

### Plugin Crash Unload Path (new — FR-020)

```
handleRequest() throws  OR  resolver callable throws
  │
  ├─ catch (const std::exception& ex)
  ├─ spdlog::error(plugin_id, ex.what())
  ├─ unload_plugin(plugin_id) [noexcept path]
  │     ├─ unregisterResolvers / deregister routes
  │     └─ DB: is_loaded = false, unloaded_at = now
  │
  └─ return HTTP 500  OR  return GraphQL error node
```

