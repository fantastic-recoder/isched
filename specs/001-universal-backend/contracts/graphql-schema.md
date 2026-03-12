# GraphQL Schema Contract

**Purpose**: Defines the core GraphQL API contract for the Universal Application Server Backend.  
**Version**: 2.0.0  
**Compliance**: GraphQL specification and GraphQL over HTTP / `graphql-transport-ws` interoperability

## Core Schema

```graphql
scalar DateTime
scalar JSON
scalar UUID

type ServerInfo {
  version: String!
  startedAt: DateTime!
  activeTenants: Int!
  activeWebSocketSessions: Int!
  transportModes: [String!]!
}

type HealthComponent {
  name: String!
  status: String!
  details: JSON
}

type HealthStatus {
  status: String!
  timestamp: DateTime!
  components: [HealthComponent!]!
}

type ConfigurationSnapshot {
  id: UUID!
  tenantId: UUID!
  version: String!
  displayName: String!
  settings: JSON!
  schemaSdl: String!
  createdAt: DateTime!
  activatedAt: DateTime
  isActive: Boolean!
}

type ModelFieldDefinition {
  name: String!
  type: String!
  required: Boolean!
  unique: Boolean!
  defaultValue: JSON
}

type DataModelDefinition {
  id: UUID!
  tenantId: UUID!
  snapshotId: UUID!
  name: String!
  fields: [ModelFieldDefinition!]!
  indexes: JSON!
  constraints: JSON!
}

type User {
  id: UUID!
  email: String!
  displayName: String!
  tenantIds: [UUID!]!
  roles: [String!]!
  isActive: Boolean!
  createdAt: DateTime!
  lastLogin: DateTime
}

type Organization {
  id: UUID!
  name: String!
  domain: String
  subscriptionTier: String!
  userLimit: Int!
  storageLimit: Int!
  createdAt: DateTime!
}

type AuthPayload {
  accessToken: String!
  refreshToken: String!
  expiresAt: DateTime!
  user: User!
}

type ConfigurationApplyResult {
  configuration: ConfigurationSnapshot!
  warnings: [String!]!
}

type ConfigurationEvent {
  tenantId: UUID!
  version: String!
  activatedAt: DateTime!
}

type Query {
  hello: String!
  version: String!
  uptime: Int!
  serverInfo: ServerInfo!
  health: HealthStatus!

  activeConfiguration(tenantId: UUID!): ConfigurationSnapshot
  configurationHistory(tenantId: UUID!, limit: Int = 20): [ConfigurationSnapshot!]!
  dataModels(tenantId: UUID!, snapshotId: UUID): [DataModelDefinition!]!

  currentUser: User
  user(id: UUID!): User
  users(tenantId: UUID!, limit: Int = 20, offset: Int = 0): [User!]!

  organization(id: UUID!): Organization
  organizations(limit: Int = 20, offset: Int = 0): [Organization!]!
}

type Mutation {
  applyConfiguration(input: ApplyConfigurationInput!): ConfigurationApplyResult!
  activateConfiguration(snapshotId: UUID!): ConfigurationSnapshot!
  rollbackConfiguration(tenantId: UUID!, targetSnapshotId: UUID!): ConfigurationSnapshot!

  createDataModel(input: CreateDataModelInput!): DataModelDefinition!
  updateDataModel(id: UUID!, input: UpdateDataModelInput!): DataModelDefinition!
  deleteDataModel(id: UUID!): Boolean!

  register(input: RegisterInput!): AuthPayload!
  login(input: LoginInput!): AuthPayload!
  refreshToken(refreshToken: String!): AuthPayload!
  logout: Boolean!
}

type Subscription {
  configurationApplied(tenantId: UUID!): ConfigurationEvent!
  healthStatusChanged(tenantId: UUID): HealthStatus!
  modelChanged(tenantId: UUID!): DataModelDefinition!
}

input ModelFieldInput {
  name: String!
  type: String!
  required: Boolean = false
  unique: Boolean = false
  defaultValue: JSON
}

input DataModelInput {
  name: String!
  fields: [ModelFieldInput!]!
  indexes: JSON = []
  constraints: JSON = {}
}

input AuthConfigurationInput {
  allowRegistration: Boolean = false
  jwtIssuer: String
  tokenLifetimeSeconds: Int = 3600
}

input ApplyConfigurationInput {
  tenantId: UUID!
  displayName: String!
  settings: JSON = {}
  models: [DataModelInput!]!
  auth: AuthConfigurationInput
}

input CreateDataModelInput {
  tenantId: UUID!
  snapshotId: UUID!
  name: String!
  fields: [ModelFieldInput!]!
  indexes: JSON = []
  constraints: JSON = {}
}

input UpdateDataModelInput {
  name: String
  fields: [ModelFieldInput!]
  indexes: JSON
  constraints: JSON
}

input RegisterInput {
  tenantId: UUID!
  email: String!
  password: String!
  displayName: String!
}

input LoginInput {
  tenantId: UUID!
  email: String!
  password: String!
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
