# GraphQL HTTP Endpoint — Startup and Built-in Queries

**Updated**: 2026-03-14

## Overview

The isched server exposes a **single external interface**: GraphQL over HTTP (POST)
and WebSocket at `/graphql`. No REST endpoints are served — health, metrics, and
configuration are all accessible through GraphQL queries and mutations.

## Starting the Server

```cpp
#include <isched/backend/isched_Server.hpp>
using namespace isched::v0_0_1::backend;

Server::Configuration config;
config.port = 8080;           // default
config.host = "localhost";    // default
config.max_threads = 16;
config.enable_introspection = true;

auto server = Server::create(config);
server->start();
// ... server is now serving at http://localhost:8080/graphql
server->stop();
```

## HTTP Transport

All GraphQL requests MUST be sent as **HTTP POST** to `/graphql` with
`Content-Type: application/json` and a JSON body:

```json
{
  "query": "{ hello version }",
  "variables": {}
}
```

The response is also JSON and always contains at least one of `data` or `errors`.
It also carries an `extensions` object with observability fields:

```json
{
  "data": { "hello": "Hello, GraphQL!", "version": "0.0.1" },
  "extensions": {
    "requestId": "req-1",
    "endpoint": "/graphql",
    "executionTimeMs": 0,
    "timestamp": 1710000000000
  }
}
```

### Error responses

| HTTP status | Reason |
|-------------|--------|
| `200 OK` | GraphQL execution completed (may still contain `errors`) |
| `400 Bad Request` | Request body is not valid JSON, or `query` field is absent/empty |

## Built-in Schema Queries

The built-in schema is loaded at startup. The following Query fields are always available:

| Field | Type | Description |
|-------|------|-------------|
| `hello` | `String` | Returns `"Hello, GraphQL!"` — connectivity test |
| `version` | `String` | Server version string (`"0.0.1"`) |
| `uptime` | `Int` | Seconds since the server started |
| `clientCount` | `Int` | Current active connection count |
| `health` | `HealthStatus` | Overall health and component breakdown |
| `info` | `AppInfo` | Application and build information |
| `metrics` | `ServerMetrics` | Runtime performance metrics |
| `env` | `Environment` | Safe-to-expose environment variables |
| `configprops` | `Configuration` | Server configuration snapshot |

The following Mutation field is available:

| Field | Type | Description |
|-------|------|-------------|
| `echo(message: String)` | `String` | Returns the `message` argument unchanged |

### Example: health check

```graphql
{ health { status components { name status } } }
```

```json
{
  "data": {
    "health": {
      "status": "UP",
      "components": { "database": {"status": "UP"}, "memory": {"status": "UP"} }
    }
  }
}
```

### Example: connectivity test

```graphql
{ hello version uptime }
```

```json
{
  "data": {
    "hello": "Hello, GraphQL!",
    "version": "0.0.1",
    "uptime": 3
  }
}
```

## GraphQL Variable Support

Variables are passed in the `variables` field of the request body:

```json
{
  "query": "query($msg: String) { echo(message: $msg) }",
  "variables": { "msg": "ping" }
}
```

## Security Notes

- No REST health, management, or metrics endpoints are served.
- Introspection is enabled by default but can be disabled via
  `config.enable_introspection = false` (recommended for production).
- Query complexity is bounded by `config.max_query_complexity` (default: 1000).
