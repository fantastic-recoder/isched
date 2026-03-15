# GraphQL Schema Contract

**Purpose**: Defines the core GraphQL API contract for the Universal Application Server Backend.  
**Version**: 3.0.0  
**Compliance**: GraphQL specification and GraphQL over HTTP / `graphql-transport-ws` interoperability  
**Last updated**: 2026-03-15 (reconciled with Phase 6 implementation: T047–T052)

## Core Schema

```graphql
# ---------------------------------------------------------------------------
# Scalars
# ---------------------------------------------------------------------------
scalar DateTime
scalar JSON
scalar UUID

# ---------------------------------------------------------------------------
# Built-in server types
# ---------------------------------------------------------------------------

"""Server runtime information"""
type ServerInfo {
  version: String
  host: String
  port: Int
  status: String
  startedAt: Int
  activeTenants: Int
  activeWebSocketSessions: Int
  transportModes: [String]
}

"""Health information for a specific system component"""
type HealthComponent {
  name: String
  status: String
  details: HealthComponentDetails
}

"""Technical details for a health component"""
type HealthComponentDetails {
  database: String
  connectionPool: String
  error: String
  used: String
  max: String
}

"""Health status information"""
type HealthStatus {
  status: String
  timestamp: String
  components: [HealthComponent]
}

"""A persisted configuration snapshot for a tenant"""
type SnapshotRecord {
  id: String!
  tenantId: String!
  version: String!
  displayName: String
  schemaSdl: String!
  isActive: Boolean!
  createdAt: String!
  activatedAt: String
}

"""Result of a configuration mutation"""
type ConfigurationResult {
  success: Boolean!
  snapshotId: String
  errors: [String!]
}

# ---------------------------------------------------------------------------
# Performance metrics (T051)
# ---------------------------------------------------------------------------

"""Server-wide performance metrics (platform_admin only)"""
type ServerMetrics {
  requestsInInterval: Int!
  errorsInInterval: Int!
  totalRequestsSinceStartup: Int!
  totalErrorsSinceStartup: Int!
  activeConnections: Int!
  activeSubscriptions: Int!
  avgResponseTimeMs: Float!
  tenantCount: Int!
}

"""Per-tenant performance metrics"""
type TenantMetrics {
  organizationId: ID!
  requestsInInterval: Int!
  errorsInInterval: Int!
  totalRequestsSinceStartup: Int!
  totalErrorsSinceStartup: Int!
  avgResponseTimeMs: Float!
}

"""Advisory thread-pool and metrics configuration for an organization (T050, T051)"""
type TenantConfig {
  organizationId: ID!
  minThreads: Int!
  maxThreads: Int!
  metricsInterval: Int!
}

# ---------------------------------------------------------------------------
# RBAC, User, and Organization types (T047)
# ---------------------------------------------------------------------------

"""An authenticated user within an organization"""
type User {
  id: ID!
  email: String!
  displayName: String!
  roles: [String!]!
  isActive: Boolean!
  createdAt: String!
  lastLogin: String
}

"""A tenant organization"""
type Organization {
  id: ID!
  name: String!
  domain: String
  subscriptionTier: String!
  userLimit: Int!
  storageLimit: Int!
  createdAt: String!
}

"""Returned by the login mutation"""
type AuthPayload {
  """Signed JWT to include in subsequent Authorization: Bearer headers"""
  token: String!
  """ISO-8601 expiry time of the token"""
  expiresAt: String!
}

# ---------------------------------------------------------------------------
# Outbound-HTTP data sources (T048)
# ---------------------------------------------------------------------------

"""A registered outbound-HTTP data source"""
type DataSource {
  id: ID!
  name: String!
  baseUrl: String!
  """Authentication strategy: none | bearer_passthrough | api_key"""
  authKind: String!
  apiKeyHeader: String
  timeoutMs: Int!
  createdAt: String!
}

"""Error returned by RestDataSource when the upstream responds with a non-2xx status (T048-006)"""
type HttpError {
  statusCode: Int!
  message: String!
  url: String
}

# ---------------------------------------------------------------------------
# Subscription event payloads
# ---------------------------------------------------------------------------

"""Event payload when a configuration snapshot is activated"""
type ConfigurationActivatedEvent {
  tenantId: String!
  snapshotId: String!
  schemaSdl: String
  activatedAt: String!
}

"""Event payload when the server health status changes"""
type HealthChangedEvent {
  status: String!
  timestamp: String!
}

# ---------------------------------------------------------------------------
# Root Query
# ---------------------------------------------------------------------------

type Query {
  """Simple greeting message"""
  hello: String
  """Server version"""
  version: String
  """Server uptime in seconds"""
  uptime: Int
  """Server health status"""
  health: HealthStatus
  """Runtime server information"""
  serverInfo: ServerInfo

  """Active configuration snapshot for a tenant"""
  activeConfiguration(tenantId: String!): SnapshotRecord
  """Full configuration history for a tenant"""
  configurationHistory(tenantId: String!): [SnapshotRecord!]!

  """Currently authenticated user (null if unauthenticated)"""
  currentUser: User
  """Look up a user by ID (tenant_admin or platform_admin only)"""
  user(id: ID!): User
  """List all users in the caller's organization"""
  users: [User!]!

  """Look up an organization by ID"""
  organization(id: ID!): Organization
  """List organizations (platform_admin sees all; tenant_admin sees own org)"""
  organizations: [Organization!]!

  """Server-wide performance metrics (platform_admin only)"""
  serverMetrics: ServerMetrics
  """Per-tenant performance metrics"""
  tenantMetrics(organizationId: ID): TenantMetrics

  """List registered outbound-HTTP data sources"""
  dataSources(organizationId: ID): [DataSource!]!
}

# ---------------------------------------------------------------------------
# Root Mutation
# ---------------------------------------------------------------------------

type Mutation {
  """Create a new configuration snapshot (does not activate it)"""
  applyConfiguration(input: ApplyConfigurationInput!): ConfigurationResult!
  """Activate an existing snapshot by its ID"""
  activateSnapshot(id: String!): ConfigurationResult!
  """Roll back to the previous active snapshot for a tenant"""
  rollbackConfiguration(tenantId: String!): ConfigurationResult!

  """Authenticate and receive a signed JWT"""
  login(email: String!, password: String!, organizationId: ID): AuthPayload!
  """Revoke the caller's current session"""
  logout: Boolean!
  """Revoke a specific session by its ID (tenant_admin only)"""
  revokeSession(sessionId: ID!): Boolean!
  """Revoke all sessions for a user (tenant_admin only)"""
  revokeAllSessions(userId: ID!): Boolean!
  """Terminate all non-platform-admin sessions in an organization (platform_admin only)"""
  terminateAllSessions(organizationId: ID!): Boolean!

  """Create a new custom role"""
  createRole(input: CreateRoleInput!): Boolean!
  """Delete a custom role by id (built-in roles cannot be deleted)"""
  deleteRole(id: ID!): Boolean!

  """Create a new organization"""
  createOrganization(input: CreateOrganizationInput!): Organization!
  """Update an existing organization"""
  updateOrganization(id: ID!, input: UpdateOrganizationInput!): Organization!
  """Delete an organization"""
  deleteOrganization(id: ID!): Boolean!

  """Create a new user in the organization"""
  createUser(organizationId: ID!, input: CreateUserInput!): User!
  """Update an existing user"""
  updateUser(organizationId: ID!, id: ID!, input: UpdateUserInput!): User!
  """Delete a user"""
  deleteUser(organizationId: ID!, id: ID!): Boolean!

  """Register a new outbound-HTTP data source"""
  createDataSource(organizationId: ID!, input: CreateDataSourceInput!): DataSource!
  """Update an existing data source"""
  updateDataSource(organizationId: ID!, id: ID!, input: UpdateDataSourceInput!): DataSource!
  """Delete a data source"""
  deleteDataSource(organizationId: ID!, id: ID!): Boolean!

  """Update advisory thread-pool and metrics configuration (platform_admin only)"""
  updateTenantConfig(organizationId: ID!, minThreads: Int, maxThreads: Int, metricsInterval: Int): TenantConfig!
}

# ---------------------------------------------------------------------------
# Root Subscription
# ---------------------------------------------------------------------------

type Subscription {
  """Fires whenever a configuration snapshot is activated for a tenant"""
  configurationActivated(tenantId: String!): ConfigurationActivatedEvent!
  """Fires when the server health status changes; also delivers an initial snapshot"""
  healthChanged: HealthChangedEvent!
  """Fires with updated server-wide metrics at each interval boundary (platform_admin only)"""
  serverMetricsUpdated: ServerMetrics
  """Fires with updated tenant metrics at each interval boundary"""
  tenantMetricsUpdated(organizationId: ID!): TenantMetrics
}

# ---------------------------------------------------------------------------
# Input types
# ---------------------------------------------------------------------------

input ApplyConfigurationInput {
  tenantId: String!
  schemaSdl: String!
  displayName: String
  version: String
  resolverBindings: [ResolverBindingInput]
}

input ResolverBindingInput {
  fieldName: String!
  resolverKind: String!
  dataSourceId: String!
  pathPattern: String
  httpMethod: String
}

input CreateUserInput {
  email: String!
  password: String!
  displayName: String
  roles: [String!]
}

input UpdateUserInput {
  displayName: String
  roles: [String!]
  isActive: Boolean
}

input CreateOrganizationInput {
  name: String!
  domain: String
  subscriptionTier: String
  userLimit: Int
  storageLimit: Int
}

input UpdateOrganizationInput {
  name: String
  domain: String
  subscriptionTier: String
  userLimit: Int
  storageLimit: Int
}

input CreateDataSourceInput {
  name: String!
  baseUrl: String!
  """Authentication strategy: none | bearer_passthrough | api_key (default: none)"""
  authKind: String
  apiKeyHeader: String
  """Plain-text API key value (stored encrypted)"""
  apiKeyValue: String
  """Request timeout in milliseconds (default: 5000)"""
  timeoutMs: Int
}

input UpdateDataSourceInput {
  name: String
  baseUrl: String
  authKind: String
  apiKeyHeader: String
  apiKeyValue: String
  timeoutMs: Int
}

input CreateRoleInput {
  id: String!
  name: String!
  description: String
  """Scope: 'platform' (requires platform_admin) or 'tenant' (requires tenant_admin)"""
  scope: String!
}
```

## GraphQL Introspection System

The server MUST implement the full GraphQL introspection system as specified in the GraphQL specification. All meta-types and meta-fields below are required and must reflect the **currently active schema**.

### Introspection meta-fields (available on root Query type)

```graphql
# Available on every query root — returns full schema description
__schema: __Schema!

# Returns the named type or null if not found
__type(name: String!): __Type

# Available on every object, interface, and union selection set
__typename: String!
```

### Introspection meta-types

```graphql
type __Schema {
  description: String
  types: [__Type!]!
  queryType: __Type!
  mutationType: __Type
  subscriptionType: __Type
  directives: [__Directive!]!
}

type __Type {
  kind: __TypeKind!
  name: String
  description: String
  # OBJECT and INTERFACE only
  fields(includeDeprecated: Boolean = false): [__Field!]
  # OBJECT only
  interfaces: [__Type!]
  # INTERFACE and UNION only
  possibleTypes: [__Type!]
  # ENUM only
  enumValues(includeDeprecated: Boolean = false): [__EnumValue!]
  # INPUT_OBJECT only
  inputFields(includeDeprecated: Boolean = false): [__InputValue!]
  # NON_NULL and LIST only
  ofType: __Type
  # SCALAR only (for custom scalars with `@specifiedBy`)
  specifiedByURL: String
}

enum __TypeKind {
  SCALAR
  OBJECT
  INTERFACE
  UNION
  ENUM
  INPUT_OBJECT
  LIST
  NON_NULL
}

type __Field {
  name: String!
  description: String
  args(includeDeprecated: Boolean = false): [__InputValue!]!
  type: __Type!
  isDeprecated: Boolean!
  deprecationReason: String
}

type __InputValue {
  name: String!
  description: String
  type: __Type!
  defaultValue: String
  isDeprecated: Boolean!
  deprecationReason: String
}

type __EnumValue {
  name: String!
  description: String
  isDeprecated: Boolean!
  deprecationReason: String
}

type __Directive {
  name: String!
  description: String
  locations: [__DirectiveLocation!]!
  args(includeDeprecated: Boolean = false): [__InputValue!]!
  isRepeatable: Boolean!
}

enum __DirectiveLocation {
  QUERY
  MUTATION
  SUBSCRIPTION
  FIELD
  FRAGMENT_DEFINITION
  FRAGMENT_SPREAD
  INLINE_FRAGMENT
  VARIABLE_DEFINITION
  SCHEMA
  SCALAR
  OBJECT
  FIELD_DEFINITION
  ARGUMENT_DEFINITION
  INTERFACE
  UNION
  ENUM
  ENUM_VALUE
  INPUT_OBJECT
  INPUT_FIELD_DEFINITION
}
```

### Built-in types always present in introspection

The following types MUST appear in `__schema { types }` regardless of whether they are referenced in the active tenant schema:

| Type | Kind |
|---|---|
| `String` | `SCALAR` |
| `Int` | `SCALAR` |
| `Float` | `SCALAR` |
| `Boolean` | `SCALAR` |
| `ID` | `SCALAR` |
| `__Schema` | `OBJECT` |
| `__Type` | `OBJECT` |
| `__TypeKind` | `ENUM` |
| `__Field` | `OBJECT` |
| `__InputValue` | `OBJECT` |
| `__EnumValue` | `OBJECT` |
| `__Directive` | `OBJECT` |
| `__DirectiveLocation` | `ENUM` |

### Built-in directives always present in `__schema { directives }`

| Directive | Locations | Args |
|---|---|---|
| `@skip` | `FIELD`, `FRAGMENT_SPREAD`, `INLINE_FRAGMENT` | `if: Boolean!` |
| `@include` | `FIELD`, `FRAGMENT_SPREAD`, `INLINE_FRAGMENT` | `if: Boolean!` |
| `@deprecated` | `FIELD_DEFINITION`, `ARGUMENT_DEFINITION`, `INPUT_FIELD_DEFINITION`, `ENUM_VALUE` | `reason: String` |
| `@specifiedBy` | `SCALAR` | `url: String!` |

### `ofType` chain for wrapped types

A field declared as `[String!]!` MUST be represented via nested `ofType` as:

```
NON_NULL
  ofType: LIST
    ofType: NON_NULL
      ofType: SCALAR (name: "String")
```

### Introspection accuracy requirement

Introspection results MUST be kept in sync with the active schema. When a configuration snapshot is activated and the schema changes, the next `__schema` query MUST reflect the updated type set.

## Contract Notes

- Built-in operational concerns such as health and server info are modeled directly in GraphQL.
- Configuration is mutation-driven and versioned; scripts are not part of the API contract.
- WebSocket subscriptions are limited to GraphQL subscription semantics and do not expose a separate event API.
- Full GraphQL introspection is required for standard tool interoperability (GraphiQL, Apollo Sandbox, Altair, codegen clients).
