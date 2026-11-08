# GraphQL Plugin API Contract (SP-012)

**Contract type**: GraphQL queries, mutations, and subscriptions  
**Schema source**: `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`  
**Branch**: `012-plugin-system-boost-dll`

---

## Overview

Plugin management remains GraphQL-only at `/graphql` (HTTP + WebSocket).  
No REST management endpoint is introduced.

`/plugin/*` remains request-plugin dispatch space, not admin API surface.

**Authorization**: `plugins`, `plugin(id)`, `loadPlugin`, `unloadPlugin`, and `pluginLifecycleEvents` require `platform_admin` via `require_roles`.

---

## Core Types

```graphql
enum PluginKind {
  REQUEST
  RESOLVER
}

enum PluginLifecycleEventType {
  LOADED
  UNLOADED
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
  kind: String!
  pluginId: String
}

type PluginLifecycleEvent {
  eventType: PluginLifecycleEventType!
  pluginId: String!
  pluginKind: PluginKind!
  pluginName: String!
  libraryPath: String!
  occurredAt: String!
}
```

---

## Queries

### `plugins(kind: String): [PluginRecord!]!`

Returns all known plugin records from persisted registry, optionally filtered by kind.

### `plugin(id: String!): PluginRecord`

Returns the plugin record for the given id, or `null` if missing.

### Discovery response semantics

- `plugins` returns `[]` when no plugin records are persisted.
- `plugin(id:)` returns `null` (not an error) when the id is unknown.
- `manifestJson` must include `abi_version` and `openApiFragmentJson` keys for management visibility.

---

## Mutations

### `loadPlugin(input: LoadPluginInput!): PluginOperationResult!`

Loads and activates a plugin if all validation stages succeed.

#### Validation order

1. **Authorization** (`platform_admin`)  
2. **Path policy** (FR-022): `libraryPath` must be absolute and resolve under configured `plugins_root`  
3. **Dynamic load/factory + manifest extraction**  
4. **Manifest checks** (ABI / OpenAPI JSON / identity)  
5. **Conflict checks** (routes/resolvers)  
6. **Resolver schema flow** (FR-021, resolver plugins only):
   - **Schema supplied** (`graphqlSchema()` non-empty): parse SDL -> validate -> detect conflicts -> merge -> register resolvers
   - **No schema supplied** (`graphqlSchema()` returns empty string sentinel): register resolvers only to existing schema fields with no existing resolver
7. **Persist + activate**

If any stage fails, response is `success: false`, with no partial activation/persistence.

### `unloadPlugin(pluginId: String!): PluginOperationResult!`

Unloads plugin routes/resolvers and marks persisted record unloaded.

---

## Subscriptions

### `pluginLifecycleEvents: PluginLifecycleEvent!`

Dedicated admin subscription delivering plugin load/unload lifecycle transitions.

#### Auth timing (FR-025)

- `require_roles(platform_admin)` is evaluated before stream establishment.
- Unauthorized callers receive authorization error and no stream is created.

#### Publish timing (FR-023)

- Emit exactly one `LOADED` event after a successful `loadPlugin` state transition completes (activation + persistence).
- Emit exactly one `UNLOADED` event after a successful `unloadPlugin` state transition completes (deactivation + persistence).
- Emit nothing for failed operations or idempotent non-transition outcomes.

#### Payload mapping (FR-024)

- `eventType`: from transition (`LOADED`/`UNLOADED`)
- `pluginId`: plugin id from manifest/record
- `pluginKind`: plugin kind enum mapped to GraphQL `PluginKind`
- `pluginName`: plugin manifest/record name
- `libraryPath`: plugin manifest/record library path
- `occurredAt`: server transition commit timestamp (UTC)

---

## Resolver Schema Flow Contract (FR-021)

### A) Schema-supplied flow

- Input plugin returns non-empty SDL from `graphqlSchema()`.
- SDL must parse successfully.
- Type/field conflicts against core schema or loaded plugin schemas reject the load.
- On success, merged schema becomes active before resolver registration.

### B) No-schema flow

- Plugin returns an empty string from `graphqlSchema()`.
- No schema merge is attempted.
- Resolver registration is allowed only for fields already present in active schema and currently unclaimed by another resolver.
- Field-missing or resolver-conflict rejects load.

---

## Path Policy Contract (FR-022, SC-015)

`loadPlugin` MUST reject before load attempt when:

- `libraryPath` is relative
- `libraryPath` is absolute but outside configured `plugins_root`
- canonicalization/normalization fails

Rejected requests MUST NOT load, persist, or activate plugin state.

---

## Error Surface (`PluginOperationResult.errors`)

Representative error categories/messages:

| Error prefix | Condition |
|---|---|
| `authorization error` | caller lacks `platform_admin` |
| `path policy validation failed` | relative/outside-root/unresolvable `libraryPath` |
| `ABI version mismatch` | manifest major differs from server ABI major |
| `openApiFragmentJson is not valid JSON` | invalid fragment JSON |
| `invalid GraphQL SDL` | `graphqlSchema()` returned unparsable SDL |
| `schema conflict` | duplicate/conflicting type/field during merge |
| `field not found` | no-schema flow targets non-existent field |
| `resolver already registered` | no-schema or merge flow conflicts at target field |
| `route conflict` | request route overlap/conflict |

Subscription authorization failure surface:

| Error prefix | Condition |
|---|---|
| `authorization error` | caller lacks `platform_admin` for `pluginLifecycleEvents` |

Discovery authorization failure surface:

| Error prefix | Condition |
|---|---|
| `authorization error` | caller lacks `platform_admin` for `plugins` or `plugin(id:)` |

---

## Examples

### Success (schema-supplied resolver plugin)

```graphql
mutation {
  loadPlugin(input: {
    libraryPath: "/opt/isched/plugins/inventory_resolver.so"
    kind: "resolver"
  }) {
    success
    pluginId
    errors
  }
}
```

### Failure (outside `plugins_root`)

```json
{
  "loadPlugin": {
    "success": false,
    "pluginId": null,
    "errors": ["path policy validation failed: libraryPath must resolve under plugins_root"],
    "plugin": null
  }
}
```

### Failure (no-schema flow field missing)

```json
{
  "loadPlugin": {
    "success": false,
    "pluginId": null,
    "errors": ["field not found: Query.dynamicField"],
    "plugin": null
  }
}
```

### Discovery list example (empty)

```graphql
query {
  isched {
    plugins {
      id
      kind
      manifestJson
    }
  }
}
```

Returns `plugins: []` when the registry is empty.

### Discovery by id example (not found)

```graphql
query {
  isched {
    plugin(id: "missing-plugin") {
      id
      manifestJson
    }
  }
}
```

Returns `plugin: null` when no persisted record exists.

### Subscription example (authorized admin)

```graphql
subscription {
  pluginLifecycleEvents {
    eventType
    pluginId
    pluginKind
    pluginName
    libraryPath
    occurredAt
  }
}
```

