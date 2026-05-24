# GraphQL Plugin API Contract (SP-012)

**Contract type**: GraphQL mutations and queries
**Schema file**: `src/main/resources/isched_builtin_server_schema.graphql`
**Branch**: `012-plugin-system-boost-dll`

---

## Overview

All plugin management operations are exposed exclusively through the `/graphql` endpoint.
No REST endpoints are defined for plugin management (the `/plugin/*` HTTP prefix is the
*dispatch target* for loaded request-plugin payloads, not a management API).

**Authorization**: All four operations require the caller to hold the `platform_admin` role
(enforced via `require_roles` in `GqlExecutor::setup_builtin_resolvers()`).

---

## Types (already implemented, no changes)

```graphql
enum PluginKind {
  REQUEST
  RESOLVER
}

type PluginRecord {
  id: String!
  kind: String!
  name: String!
  version: String!
  description: String
  libraryPath: String!
  manifestJson: String!
  isLoaded: Boolean!
  loadedAt: String
  unloadedAt: String
}

type PluginOperationResult {
  success: Boolean!
  pluginId: String
  errors: [String!]!
  plugin: PluginRecord
}

input LoadPluginInput {
  libraryPath: String!
  kind: String!          # "request" or "resolver"
  pluginId: String       # optional override; derived from library stem if omitted
}
```

---

## Queries

### `plugins(kind: String): [PluginRecord!]!`

Returns all currently loaded (in-memory) plugin records. Optionally filter by kind.

**Authorization**: `platform_admin` required.

**Example**:
```graphql
query {
  plugins {
    id kind name version isLoaded loadedAt
  }
}
```

**Behaviour**:
- Returns in-memory loaded plugins only (not unloaded historical records).
- Optional `kind` filter accepts `"request"` or `"resolver"`.
- Returns empty array if no plugins loaded.

---

### `plugin(id: String!): PluginRecord`

Returns a single loaded plugin by its id, or `null` if not found / not loaded.

**Authorization**: `platform_admin` required.

**Example**:
```graphql
query {
  plugin(id: "my-plugin") {
    id kind name version manifestJson isLoaded
  }
}
```

---

## Mutations

### `loadPlugin(input: LoadPluginInput!): PluginOperationResult!`

Loads a shared library as a plugin. The library must be accessible on the server filesystem.

**Authorization**: `platform_admin` required.

**Validation sequence** (new in gap-closure work):
1. ABI version check: manifest's `abi_version` major must equal server's `PLUGIN_ABI_MAJOR` (currently `1`).
2. OpenAPI fragment check (request plugins): `openApiFragmentJson`, if non-empty, must be valid JSON.
3. Route/resolver conflict check: No overlap with already-loaded plugins.

**Success response**:
```json
{
  "loadPlugin": {
    "success": true,
    "pluginId": "my-plugin",
    "errors": [],
    "plugin": { "id": "my-plugin", "isLoaded": true, ... }
  }
}
```

**Failure response** (example: ABI mismatch):
```json
{
  "loadPlugin": {
    "success": false,
    "pluginId": null,
    "errors": ["ABI version mismatch: plugin declares '2.0.0', server requires major '1'"],
    "plugin": null
  }
}
```

**Failure response** (example: invalid OpenAPI fragment):
```json
{
  "loadPlugin": {
    "success": false,
    "pluginId": null,
    "errors": ["Plugin openApiFragmentJson is not valid JSON"],
    "plugin": null
  }
}
```

---

### `unloadPlugin(pluginId: String!): PluginOperationResult!`

Unloads a loaded plugin. Idempotent if the plugin is not loaded (returns success without change).

**Authorization**: `platform_admin` required.

**Post-condition**: Plugin's routes/resolver bindings are deregistered; `plugin_registry` record updated with `is_loaded = false` and `unloaded_at` timestamp.

**Example**:
```graphql
mutation {
  unloadPlugin(pluginId: "my-plugin") {
    success errors
  }
}
```

---

## Error Codes (as returned in `errors` array)

| Error message (prefix) | Condition |
|------------------------|-----------|
| `"ABI version mismatch: plugin declares '…', server requires major '…'"` | FR-018: major version mismatch |
| `"Plugin openApiFragmentJson is not valid JSON"` | FR-019: invalid OpenAPI fragment |
| `"Request plugin route conflicts with an already loaded plugin"` | FR-006: route overlap |
| `"Resolver plugin binding conflicts with an already loaded plugin"` | FR-007: resolver overlap |
| `"Plugin id already loaded: …"` | Duplicate ID already in memory |
| `"Plugin not loaded: …"` | `unloadPlugin` called for unknown id |
| `"Request plugin factory returned null"` | Boost.DLL loaded but factory returned null |
| `"Resolver plugin factory returned null"` | Boost.DLL loaded but factory returned null |
| `"Plugin persistence unavailable"` | DatabaseManager not configured |
| Authorization error (framework-level) | Caller lacks `platform_admin` role (FR-017) |

---

## Crash Containment Behaviour (FR-020)

If a loaded plugin's handler or resolver callable throws after activation:

- **Request plugin crash**: `dispatch_request()` returns `PluginHttpResponse{status: 500, body: "Plugin error"}`. The plugin is automatically unloaded (routes deregistered, DB updated).
- **Resolver plugin crash**: `GqlExecutor::execute()` returns a response with a GraphQL error node. The plugin is automatically unloaded (resolver bindings deregistered, DB updated).

These automatic unloads are transparent to the GraphQL client — no special schema type is needed.

