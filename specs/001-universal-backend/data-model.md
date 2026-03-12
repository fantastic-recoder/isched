# Data Model: Universal Application Server Backend

**Phase**: 1 (Design & Contracts)  
**Created**: 2025-11-01  
**Updated**: 2026-03-12  
**Feature**: Universal Application Server Backend

## Implementation Context

**Namespace**: `isched::v0_0_1::backend`  
**File Prefix**: `isched_`  
**Database**: SQLite per tenant with file-per-tenant isolation  
**Schema Source**: Built-in GraphQL schema plus configuration-derived model metadata

## Core Entities

### Configuration Snapshot

**Purpose**: Represents a persisted, versioned backend configuration applied through GraphQL mutations.

**Attributes**:

- `id`: Unique snapshot identifier
- `tenant_id`: Associated tenant identifier
- `version`: Monotonic version label or sequence
- `display_name`: Human-readable label for the snapshot
- `settings`: JSON object with tenant runtime settings
- `schema_sdl`: Generated SDL fragment for tenant-defined schema
- `created_at`: Snapshot creation timestamp
- `activated_at`: Activation timestamp for the current snapshot
- `created_by`: User identifier that submitted the change
- `is_active`: Whether the snapshot is currently active

**Relationships**:

- One-to-many with Data Model Definition
- One-to-many with Resolver Definition
- Belongs-to Tenant Runtime

**Validation Rules**:

- Only one snapshot may be active per tenant at a time
- Snapshot must pass schema validation before activation
- Settings must satisfy tenant resource limits and auth rules

### Data Model Definition

**Purpose**: Represents tenant-defined model metadata that drives GraphQL schema generation and SQLite schema creation.

**Attributes**:

- `id`: Unique identifier
- `tenant_id`: Associated tenant identifier
- `snapshot_id`: Configuration snapshot identifier
- `name`: Model name
- `fields`: JSON array or object of field definitions
- `indexes`: JSON array of index definitions
- `relationships`: JSON array of model relationships
- `constraints`: JSON object of validation constraints
- `graphql_type_name`: Generated GraphQL object type name
- `table_name`: SQLite table name

**Relationships**:

- Belongs-to Configuration Snapshot
- May reference other Data Model Definition entities

**Validation Rules**:

- Name must be a valid GraphQL type name
- Field names must be unique within the model
- Relationship targets must reference existing models in the same snapshot

### Resolver Definition

**Purpose**: Represents built-in or configured resolver behavior mapped into the active GraphQL schema.

**Attributes**:

- `id`: Unique identifier
- `tenant_id`: Associated tenant
- `snapshot_id`: Owning configuration snapshot
- `field_path`: GraphQL path served by the resolver
- `resolver_kind`: Built-in, storage-backed, or outbound integration
- `configuration`: JSON configuration blob
- `requires_authentication`: Whether auth is required
- `is_subscription`: Whether resolver participates in subscription delivery

**Relationships**:

- Belongs-to Configuration Snapshot
- May reference Data Model Definition

**Validation Rules**:

- Resolver path must be unique within active schema scope
- Outbound integration configuration must be explicitly declared and validated

### GraphQL Schema

**Purpose**: Represents the effective GraphQL schema visible to a tenant at runtime.

**Attributes**:

- `tenant_id`: Associated tenant
- `built_in_sdl`: Built-in schema SDL
- `generated_sdl`: Tenant-generated SDL fragment
- `full_sdl`: Combined active schema SDL
- `introspection_result`: Cached introspection output
- `generated_at`: Timestamp of last schema generation
- `source_snapshot_id`: Snapshot used to generate the schema

**Relationships**:

- Generated-from Configuration Snapshot and Data Model Definition
- Belongs-to Tenant Runtime

**State Transitions**:

- Draft → Validating → Active → Superseded

### Authentication Context

**Purpose**: Represents user session and permission information for HTTP requests and WebSocket sessions.

**Attributes**:

- `id`: Session identifier
- `user_id`: Associated user identifier
- `tenant_id`: Tenant context
- `access_token_id`: Token identifier or fingerprint
- `permissions`: JSON array of granted permissions
- `issued_at`: Session creation timestamp
- `expires_at`: Session expiration timestamp
- `last_activity`: Last seen timestamp
- `transport_scope`: HTTP, WebSocket, or both

**Relationships**:

- Belongs-to User
- Belongs-to Tenant Runtime

**Validation Rules**:

- Session must be unexpired
- Permissions must be valid for the tenant and operation

### Subscription Session

**Purpose**: Represents an active GraphQL WebSocket connection and its subscription registry.

**Attributes**:

- `id`: Connection identifier
- `tenant_id`: Tenant context
- `user_id`: Optional authenticated user identifier
- `protocol`: Expected to be `graphql-transport-ws`
- `connected_at`: Connection timestamp
- `last_heartbeat`: Last ping or pong timestamp
- `active_subscriptions`: Map of subscription IDs to execution descriptors
- `connection_state`: Connecting, Open, Closing, Closed

**Relationships**:

- May reference Authentication Context
- Belongs-to Tenant Runtime

**Validation Rules**:

- Protocol must match supported GraphQL WebSocket protocol
- Subscription IDs must be unique per connection

### User

**Purpose**: Represents a system user with authentication and tenant-scoped permissions.

**Attributes**:

- `id`: Unique user identifier
- `email`: User email address
- `password_hash`: Password hash or credential record
- `display_name`: User-friendly display name
- `tenant_ids`: Array of tenant associations
- `roles`: Array of role identifiers
- `is_active`: Account state
- `created_at`: Account creation timestamp
- `last_login`: Last successful login timestamp

**Relationships**:

- One-to-many with Authentication Context
- Many-to-many with Tenant Runtime

### Organization

**Purpose**: Represents a tenant or organization boundary in the multi-tenant system.

**Attributes**:

- `id`: Unique organization identifier
- `name`: Organization name
- `domain`: Optional organization domain
- `subscription_tier`: Service tier
- `user_limit`: Maximum users
- `storage_limit`: Tenant storage limit
- `created_at`: Creation timestamp

**Relationships**:

- One-to-many with User
- One-to-many with Configuration Snapshot

### Tenant Runtime

**Purpose**: Represents live in-process state for a tenant.

**Attributes**:

- `tenant_id`: Tenant identifier
- `active_snapshot_id`: Current active configuration snapshot
- `database_path`: SQLite file path
- `connection_pool_size`: Current pool size
- `worker_quota`: Scheduler or worker allocation
- `active_http_requests`: In-flight HTTP operations
- `active_websocket_sessions`: Active subscription session count
- `status`: Starting, Running, Reconfiguring, Degraded, Stopped

**Relationships**:

- One-to-one with active GraphQL Schema
- One-to-many with Subscription Session
- One-to-many with Authentication Context

## Database Schema Considerations

### Tenant Isolation

**Strategy**: Store tenant data and metadata in tenant-scoped SQLite files or clearly isolated tenant partitions, with separate configuration history and application data.

**Benefits**:

- Easier recovery and backup per tenant
- Clearer operational boundaries
- Stronger protection against cross-tenant data leakage

### Atomic Configuration Activation

**Requirement**: Configuration snapshot writes, schema generation, migration decisions, and activation flags must occur as a transactionally consistent unit.

### Index Strategy

**Recommended indexes**:

- Configuration Snapshot: `tenant_id`, `is_active`, `version`
- Data Model Definition: `tenant_id`, `snapshot_id`, `name`
- Authentication Context: `tenant_id`, `user_id`, `expires_at`
- Subscription Session: `tenant_id`, `connected_at`

## Schema Generation Rules

### Type Generation

**Process**:

1. Read active configuration snapshot
2. Validate model definitions and resolver definitions
3. Generate SDL fragments for model types, queries, mutations, and subscriptions
4. Merge generated SDL with built-in schema
5. Validate combined schema and publish it atomically

### Query Complexity Controls

**Limits**:

- Maximum query depth: configurable per tenant
- Maximum field count: configurable per tenant
- Maximum subscription fan-out: configurable globally and per tenant
- Execution timeout: configurable for query and subscription operations

## Error Handling

### Configuration Errors

- Validation failures reject the candidate snapshot and preserve the active snapshot
- Migration failures reject activation and return structured GraphQL errors
- Subscription delivery failures are isolated to the connection or subscription where practical
