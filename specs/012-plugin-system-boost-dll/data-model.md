# Data Model: Plugin System Clarification Alignment (SP-012)

**Phase 1 output** | Branch: `012-plugin-system-boost-dll` | Date: 2026-06-04

This document captures the Phase 1 design model aligned to clarified requirements, with emphasis on:

- FR-021 optional resolver-schema load flow
- FR-022 library path policy under configured `plugins_root`
- FR-023/FR-024 lifecycle subscription publish and payload guarantees
- FR-025 lifecycle subscription authorization timing
- SC-013 / SC-014 / SC-015 acceptance behavior
- SC-017 / SC-018 lifecycle subscription acceptance behavior

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

## Modified Entity: LoadPluginInput (validation semantics)

**GraphQL input**: `libraryPath`, `kind`, optional `pluginId`

### Validation Rules (FR-022)

| Field | Rule | Failure Outcome |
|------|------|-----------------|
| `libraryPath` | Must be absolute path | Validation error; no load attempt |
| `libraryPath` | Canonicalized path must be contained within canonicalized `plugins_root` | Validation error; no load/persist/activation |
| `libraryPath` | Canonicalization must succeed | Validation error; no load/persist/activation |

> This validation occurs before Boost.DLL loading and before any in-memory or persisted plugin mutation.

---

## New Entity: PluginLifecycleEvent

**GraphQL subscription payload type** used for plugin lifecycle administration updates.

| Field | Type | Source mapping | Validation |
|---|---|---|---|
| `eventType` | `PluginLifecycleEventType` (`LOADED`\|`UNLOADED`) | Derived from successful lifecycle transition kind | Must be present |
| `pluginId` | `String` | `PluginManifest.id` / `PluginRecord.id` | Non-empty |
| `pluginKind` | `PluginKind` | `PluginManifest.kind` / `PluginRecord.kind` | Must map to `REQUEST` or `RESOLVER` |
| `pluginName` | `String` | `PluginManifest.name` / `PluginRecord.name` | Non-empty |
| `libraryPath` | `String` | `PluginManifest.libraryPath` / `PluginRecord.libraryPath` | Absolute path under `plugins_root` for load-originated events |
| `occurredAt` | `String` timestamp | Server transition commit time (UTC) | Non-empty ISO-8601 timestamp |

Payload is emitted only when a lifecycle state transition has completed successfully.

---

## New Entity: PluginLifecycleSubscription

Represents an authorized admin subscriber stream attached to the plugin lifecycle event source.

| Property | Type | Rule |
|---|---|---|
| `subscriberPrincipal` | auth context principal | Must hold `platform_admin` |
| `streamState` | `PendingAuth` \| `Established` \| `Rejected` \| `Closed` | `Rejected` is terminal for unauthorized callers |
| `lastDeliveredEvent` | `PluginLifecycleEvent?` | Optional bookkeeping for tests/diagnostics |

### Authorization timing rule (FR-025)

`require_roles(platform_admin)` executes while stream is in `PendingAuth`. If auth fails, transition directly to `Rejected`; no stream cursor/channel is established.

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

### Optional Schema Supply Contract (FR-021)

`ResolverPlugin::graphqlSchema()` can return:

1. **Valid non-empty SDL string** -> parse + merge flow is required before resolver registration.
2. **empty string sentinel** -> no schema merge; plugin may register resolvers only on existing schema fields.

Invalid SDL, schema conflicts, field-not-found, or resolver-conflict conditions reject load with no partial activation.

---

## Unchanged Entities

The following are fully implemented and require no data-model modifications:

- `PluginKind` — unchanged
- `PluginRecord` — unchanged (persisted JSON includes `abi_version` transparently via `manifestJson`)
- `PluginHttpRequest` / `PluginHttpResponse` — unchanged
- `PluginOperationResult` — unchanged
- `PluginManager` public interface — `dispatch_request` loses `const`; no other signature changes
- `plugin_registry` SQLite table — unchanged (ABI version stored inside `manifest_json` TEXT column)
- Existing GraphQL management types (`PluginRecord`, `PluginOperationResult`, `LoadPluginInput`) — unchanged

> GraphQL schema is additionally extended with lifecycle subscription types/field required by FR-023/FR-024/FR-025.

---

## State Transitions

### Plugin Load Path (updated)

```
loadPlugin mutation
  │
  ├─ [FR-022] validate_library_path_under_plugins_root(libraryPath)
  │     └─ invalid/relative/outside-root/unresolvable → REJECTED
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
  ├─ [FR-021 resolver only] schema branch
  │     ├─ schema supplied → parse SDL → conflict check → merge
  │     │     └─ parse/conflict/merge failure → REJECTED (no partial state)
  │     └─ no schema supplied → require existing target fields + no existing resolvers
  │           └─ field-missing/conflict → REJECTED
  │
  ├─ [FR-020] CrashSafeRegistrar wraps callables  [resolver only]
  │
  ├─ persist + activate
  ├─ [FR-023/FR-024] publish lifecycle event `LOADED` (exactly one)
  │     └─ payload mapped from activated plugin + transition timestamp
  └─ SUCCESS
```

### Plugin Unload Path (updated)

```text
unloadPlugin mutation
  │
  ├─ [FR-017] require_roles check → REJECTED if caller lacks platform_admin
  ├─ locate plugin record
  │    ├─ missing plugin id → REJECTED (no event)
  │    └─ already unloaded (idempotent) → SUCCESS (no transition, no event)
  ├─ unregister routes/resolvers + unload handle
  ├─ persist `is_loaded = false`, `unloaded_at = now`
  ├─ [FR-023/FR-024] publish lifecycle event `UNLOADED` (exactly one)
  │     └─ payload mapped from deactivated plugin + transition timestamp
  └─ SUCCESS
```

### Resolver Plugin Load State Machine (FR-021)

```text
DISCOVERED
  -> (graphqlSchema() empty string) NO_SCHEMA_VALIDATION
  -> (graphqlSchema() non-empty) SDL_PARSE

SDL_PARSE
  -> (invalid SDL) REJECTED
  -> SDL_CONFLICT_CHECK

SDL_CONFLICT_CHECK
  -> (conflict) REJECTED
  -> SDL_MERGED

NO_SCHEMA_VALIDATION / SDL_MERGED
  -> RESOLVER_PRECHECK (field exists + resolver unclaimed)
  -> (precheck fail) REJECTED
  -> REGISTERED
  -> PERSISTED_ACTIVE
```

### Path Policy Validation State Machine (FR-022, SC-015)

```text
INPUT_RECEIVED
  -> ABSOLUTE_CHECK
  -> (relative path) REJECTED
  -> CANONICALIZE_ROOT_AND_LIBRARY
  -> (canonicalization error) REJECTED
  -> CONTAINMENT_CHECK_UNDER_ROOT
  -> (outside root) REJECTED
  -> LOAD_ALLOWED
```

### Lifecycle Subscription Auth + Stream State Machine (FR-025)

```text
SUBSCRIBE_REQUEST_RECEIVED
  -> PENDING_AUTH
  -> require_roles(platform_admin)
  -> (unauthorized) REJECTED (error returned; no stream established)
  -> (authorized) STREAM_ESTABLISHED
  -> EVENT_DELIVERY_LOOP
  -> STREAM_CLOSED
```

### Lifecycle Event Emission Rules (FR-023/FR-024, SC-017/SC-018)

```text
TRANSITION_ATTEMPT
  -> (fails before lifecycle state transition) NO_EVENT
  -> (successful load transition) EMIT_LOADED_EXACTLY_ONCE
  -> (successful unload transition) EMIT_UNLOADED_EXACTLY_ONCE
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

