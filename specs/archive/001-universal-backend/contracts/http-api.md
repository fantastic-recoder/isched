# GraphQL Transport Contract

**Purpose**: Defines the allowed external transport surface for the Universal Application Server Backend.  
**Version**: 2.0.0  
**Base URL**: `http://localhost:8080` (configurable)

## Scope

The server exposes GraphQL as its only external interface.

- GraphQL over HTTP at `/graphql`
- GraphQL over WebSocket at `/graphql`

No REST configuration API, health API, metrics API, or scripting API is part of the supported contract.

## HTTP Transport

### POST /graphql

Primary endpoint for GraphQL queries and mutations.

**Request**:

```json
{
  "query": "query { serverInfo { version } }",
  "variables": {},
  "operationName": null
}
```

**Headers**:

- `Content-Type: application/json`
- `Authorization: Bearer <token>` when authentication is required

**Response** (200 OK):

```json
{
  "data": {
    "serverInfo": {
      "version": "1.0.0"
    }
  }
}
```

**Error Response** (GraphQL execution failure):

```json
{
  "data": null,
  "errors": [
    {
      "message": "Configuration snapshot is invalid",
      "extensions": {
        "code": "CONFIG_VALIDATION_ERROR",
        "timestamp": "2026-03-12T10:00:00Z",
        "requestId": "req-123"
      }
    }
  ]
}
```

### Non-standard `extensions` fields (FR-GQL-003)

When the server returns an error, the `extensions` object MAY contain the following non-standard fields:

| Field | Type | Description |
|---|---|---|
| `code` | `string` | Machine-readable error code identifying the error category (e.g. `CONFIG_VALIDATION_ERROR`, `UNAUTHORIZED`, `NOT_FOUND`, `INTERNAL_SERVER_ERROR`). Clients SHOULD use this field for programmatic error handling rather than parsing `message`. |
| `timestamp` | `string` | ISO-8601 UTC timestamp of when the error occurred. Useful for correlating errors with server-side logs. |
| `requestId` | `string` | Opaque identifier for the specific request that produced the error. Include this value when reporting bugs or contacting support; it allows server-side operators to locate the exact log entry. |

All three fields are optional and MAY be absent in any given error response. The `message` field (part of the GraphQL spec) is always present.
```

### GET /graphql

Optional support for query operations and introspection over HTTP GET.

**Request**:

```text
GET /graphql?query=query%20%7B%20hello%20%7D
```

**Constraints**:

- GET is allowed only for operations that are safe to expose as URL-encoded queries
- Mutations must use POST

## WebSocket Transport

### WS /graphql

Subscription endpoint for GraphQL over WebSocket.

**Protocol**: `graphql-transport-ws`

**Connection Example**:

```http
GET /graphql HTTP/1.1
Upgrade: websocket
Sec-WebSocket-Protocol: graphql-transport-ws
```

### Connection Init

Client message:

```json
{
  "type": "connection_init",
  "payload": {
    "authorization": "Bearer <token>"
  }
}
```

Server acknowledgement:

```json
{
  "type": "connection_ack"
}
```

### Subscribe

Client message:

```json
{
  "id": "sub-1",
  "type": "subscribe",
  "payload": {
    "query": "subscription { configurationApplied(tenantId: \"tenant-demo\") { tenantId version activatedAt } }",
    "variables": {}
  }
}
```

Server event message:

```json
{
  "id": "sub-1",
  "type": "next",
  "payload": {
    "data": {
      "configurationApplied": {
        "tenantId": "tenant-demo",
        "version": "42",
        "activatedAt": "2026-03-12T10:00:00Z"
      }
    }
  }
}
```

Completion message:

```json
{
  "id": "sub-1",
  "type": "complete"
}
```

### Ping/Pong

Server and client may exchange:

```json
{ "type": "ping" }
```

```json
{ "type": "pong" }
```

## Built-In Operational Surface

Operational concerns are exposed through GraphQL operations such as:

- `serverInfo`
- `health`
- `activeConfiguration`
- `configurationApplied` subscription

There are no separate REST endpoints for:

- `/health`
- `/metrics`
- `/api/v1/configuration`
- `/api/v1/auth/*`

Those concerns must be modeled in GraphQL.
