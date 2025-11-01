# HTTP API Contract

**Purpose**: Defines the REST endpoints and HTTP interface for the Universal Application Server Backend.
**Version**: 1.0.0
**Base URL**: `http://localhost:8080` (configurable)

## Core Endpoints

### Configuration Management

#### POST /api/v1/configuration
Create a new configuration script.

**Request**:
```json
{
  "language": "python" | "typescript",
  "content": "string",
  "tenant_id": "uuid"
}
```

**Response** (201 Created):
```json
{
  "id": "uuid",
  "language": "python",
  "content": "string",
  "version": "1.0.0",
  "tenant_id": "uuid",
  "created_at": "2025-11-01T10:00:00Z",
  "updated_at": "2025-11-01T10:00:00Z",
  "is_active": false
}
```

#### PUT /api/v1/configuration/{id}/activate
Activate a configuration script.

**Response** (200 OK):
```json
{
  "id": "uuid",
  "is_active": true,
  "activated_at": "2025-11-01T10:00:00Z"
}
```

#### GET /api/v1/configuration/{tenant_id}
Get active configuration for tenant.

**Response** (200 OK):
```json
{
  "id": "uuid",
  "language": "python",
  "content": "string",
  "version": "1.0.0",
  "is_active": true
}
```

### Authentication

#### POST /api/v1/auth/login
Authenticate user and create session.

**Request**:
```json
{
  "email": "user@example.com",
  "password": "password",
  "tenant_id": "uuid"
}
```

**Response** (200 OK):
```json
{
  "access_token": "jwt_token",
  "refresh_token": "refresh_token",
  "expires_at": "2025-11-01T11:00:00Z",
  "user": {
    "id": "uuid",
    "email": "user@example.com",
    "display_name": "John Doe"
  }
}
```

#### POST /api/v1/auth/refresh
Refresh access token.

**Request**:
```json
{
  "refresh_token": "refresh_token"
}
```

**Response** (200 OK):
```json
{
  "access_token": "new_jwt_token",
  "expires_at": "2025-11-01T12:00:00Z"
}
```

#### POST /api/v1/auth/logout
Logout and invalidate session.

**Request**:
```json
{
  "session_id": "uuid"
}
```

**Response** (204 No Content)

### GraphQL Endpoint

#### POST /graphql
Main GraphQL endpoint for all data operations.

**Request**:
```json
{
  "query": "query { users { id email displayName } }",
  "variables": {},
  "operationName": null
}
```

**Response** (200 OK):
```json
{
  "data": {
    "users": [
      {
        "id": "uuid",
        "email": "user@example.com",
        "displayName": "John Doe"
      }
    ]
  },
  "errors": null
}
```

#### GET /graphql?query={query}
GraphQL endpoint for GET requests (queries only).

**Response**: Same as POST format.

### Server Management

#### POST /api/v1/server/start
Start a server instance with configuration.

**Request**:
```json
{
  "tenant_id": "uuid",
  "config_script_id": "uuid",
  "port": 8080
}
```

**Response** (201 Created):
```json
{
  "id": "uuid",
  "tenant_id": "uuid",
  "config_script_id": "uuid",
  "port": 8080,
  "status": "starting",
  "started_at": "2025-11-01T10:00:00Z"
}
```

#### POST /api/v1/server/{id}/stop
Stop a running server instance.

**Response** (200 OK):
```json
{
  "id": "uuid",
  "status": "stopping",
  "stopped_at": "2025-11-01T10:00:00Z"
}
```

#### GET /api/v1/server/{tenant_id}/status
Get status of server instances for tenant.

**Response** (200 OK):
```json
{
  "instances": [
    {
      "id": "uuid",
      "status": "running",
      "port": 8080,
      "memory_usage": 256000000,
      "request_count": 1542
    }
  ]
}
```

### Health and Monitoring

#### GET /health
Health check endpoint.

**Response** (200 OK):
```json
{
  "status": "healthy",
  "timestamp": "2025-11-01T10:00:00Z",
  "version": "1.0.0",
  "uptime": 3661
}
```

#### GET /metrics
Prometheus-compatible metrics endpoint.

**Response** (200 OK):
```text
# HELP isched_requests_total Total number of requests
# TYPE isched_requests_total counter
isched_requests_total{tenant="uuid",method="POST"} 1542

# HELP isched_memory_usage Current memory usage in bytes
# TYPE isched_memory_usage gauge
isched_memory_usage{tenant="uuid"} 256000000
```

### Configuration Script Endpoints

#### POST /api/v1/config/validate
Validate configuration script syntax.

**Request**:
```json
{
  "language": "python",
  "content": "from isched import define_model\n..."
}
```

**Response** (200 OK):
```json
{
  "valid": true,
  "errors": [],
  "warnings": [
    "Unused import detected on line 5"
  ]
}
```

#### GET /api/v1/config/schema/{tenant_id}
Get generated GraphQL schema for tenant.

**Response** (200 OK):
```json
{
  "schema": "type Query { ... }",
  "generated_at": "2025-11-01T10:00:00Z",
  "version": "1.0.0"
}
```

## Error Responses

### Standard Error Format

All error responses follow a consistent format:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Invalid configuration script syntax",
    "details": {
      "line": 15,
      "column": 22,
      "syntax_error": "Unexpected token ';'"
    },
    "timestamp": "2025-11-01T10:00:00Z"
  }
}
```

### Common Error Codes

- `VALIDATION_ERROR` (400): Invalid request data
- `AUTHENTICATION_REQUIRED` (401): Missing or invalid authentication
- `AUTHORIZATION_FAILED` (403): Insufficient permissions
- `RESOURCE_NOT_FOUND` (404): Requested resource doesn't exist
- `CONFLICT` (409): Resource conflict (e.g., duplicate email)
- `RATE_LIMITED` (429): Too many requests
- `INTERNAL_ERROR` (500): Server error
- `SERVICE_UNAVAILABLE` (503): Server temporarily unavailable

## Authentication

### JWT Token Format

All authenticated endpoints require JWT token in header:

```
Authorization: Bearer <jwt_token>
```

### JWT Claims

```json
{
  "sub": "user_uuid",
  "tenant_id": "tenant_uuid",
  "permissions": ["read:users", "write:config"],
  "exp": 1672531200,
  "iat": 1672527600,
  "iss": "isched-server"
}
```

### OAuth 2.0 Support

#### GET /api/v1/auth/oauth/{provider}
Initiate OAuth flow.

**Supported providers**: google, github, auth0

**Response** (302 Redirect):
```
Location: https://provider.com/oauth/authorize?client_id=...
```

#### GET /api/v1/auth/oauth/{provider}/callback
OAuth callback endpoint.

**Query Parameters**:
- `code`: Authorization code from provider
- `state`: CSRF protection state

**Response** (200 OK):
```json
{
  "access_token": "jwt_token",
  "refresh_token": "refresh_token",
  "user": {
    "id": "uuid",
    "email": "user@example.com",
    "display_name": "John Doe"
  }
}
```

## Rate Limiting

### Default Limits

- **General API**: 1000 requests per hour per IP
- **GraphQL**: 100 queries per minute per user
- **Authentication**: 10 login attempts per minute per IP
- **Configuration**: 10 updates per hour per tenant

### Rate Limit Headers

```
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 999
X-RateLimit-Reset: 1672531200
X-RateLimit-Retry-After: 60
```

## CORS Configuration

### Default CORS Settings

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-Tenant-ID
Access-Control-Max-Age: 86400
```

### Configurable CORS

CORS settings can be customized per tenant through configuration scripts.

## Content Types

### Supported Content Types

- `application/json` (default)
- `application/graphql` (for GraphQL queries)
- `text/plain` (for metrics endpoint)

### Request Headers

- `Content-Type`: Request content type
- `Accept`: Response content type preference
- `Authorization`: JWT authentication token
- `X-Tenant-ID`: Tenant identifier (optional, can be in JWT)
- `X-Request-ID`: Request tracking identifier