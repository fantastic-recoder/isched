# GraphQL Schema Contract

**Purpose**: Defines the core GraphQL API contract for the Universal Application Server Backend.
**Version**: 1.0.0
**Compliance**: GraphQL Specification (https://spec.graphql.org/)

## Core Schema

```graphql
# Scalar Types
scalar DateTime
scalar JSON
scalar UUID

# Configuration Management
type ConfigurationScript {
  id: UUID!
  language: ConfigurationLanguage!
  content: String!
  version: String!
  tenantId: UUID!
  createdAt: DateTime!
  updatedAt: DateTime!
  isActive: Boolean!
  dataModels: [DataModel!]!
  authRules: [AuthenticationRule!]!
}

enum ConfigurationLanguage {
  PYTHON
  TYPESCRIPT
}

type DataModel {
  id: UUID!
  name: String!
  fields: JSON!
  indexes: [String!]!
  constraints: JSON!
  configScriptId: UUID!
  graphqlType: String!
  sqlSchema: String!
}

# User Management
type User {
  id: UUID!
  email: String!
  displayName: String!
  tenantId: UUID!
  oauthProviders: [String!]!
  isActive: Boolean!
  createdAt: DateTime!
  lastLogin: DateTime
}

type Organization {
  id: UUID!
  name: String!
  domain: String
  subscriptionTier: String!
  configurationLimit: Int!
  userLimit: Int!
  storageLimit: Int!
  createdAt: DateTime!
  users: [User!]!
  configurations: [ConfigurationScript!]!
}

# Authentication
type AuthenticationContext {
  id: UUID!
  userId: UUID!
  tenantId: UUID!
  permissions: [String!]!
  expiresAt: DateTime!
  createdAt: DateTime!
  lastActivity: DateTime!
}

type AuthenticationRule {
  id: UUID!
  name: String!
  type: AuthenticationType!
  configuration: JSON!
  isRequired: Boolean!
}

enum AuthenticationType {
  OAUTH2
  JWT
  BASIC_AUTH
  API_KEY
}

# Server Management
type ServerInstance {
  id: UUID!
  tenantId: UUID!
  configScriptId: UUID!
  port: Int!
  status: ServerStatus!
  startedAt: DateTime!
  memoryUsage: Int!
  requestCount: Int!
}

enum ServerStatus {
  STARTING
  RUNNING
  STOPPING
  STOPPED
  RESTARTING
}

# Dynamic Schema Support
type GeneratedSchema {
  id: UUID!
  tenantId: UUID!
  schemaDefinition: String!
  introspectionResult: JSON!
  generatedAt: DateTime!
  isCurrent: Boolean!
}

# Root Query Type
type Query {
  # Configuration Management
  configurationScript(id: UUID!): ConfigurationScript
  configurationScripts(tenantId: UUID!, isActive: Boolean): [ConfigurationScript!]!
  
  # User Management
  currentUser: User
  user(id: UUID!): User
  users(tenantId: UUID!, limit: Int = 10, offset: Int = 0): [User!]!
  
  # Organization Management
  organization(id: UUID!): Organization
  organizations(limit: Int = 10, offset: Int = 0): [Organization!]!
  
  # Server Management
  serverInstance(id: UUID!): ServerInstance
  serverInstances(tenantId: UUID!): [ServerInstance!]!
  
  # Schema Introspection
  generatedSchema(tenantId: UUID!): GeneratedSchema
  
  # Dynamic Data Queries (generated at runtime based on configuration)
  # These will be dynamically added based on user-defined data models
}

# Root Mutation Type
type Mutation {
  # Configuration Management
  createConfigurationScript(input: CreateConfigurationScriptInput!): ConfigurationScript!
  updateConfigurationScript(id: UUID!, input: UpdateConfigurationScriptInput!): ConfigurationScript!
  activateConfigurationScript(id: UUID!): ConfigurationScript!
  deleteConfigurationScript(id: UUID!): Boolean!
  
  # User Management
  createUser(input: CreateUserInput!): User!
  updateUser(id: UUID!, input: UpdateUserInput!): User!
  deleteUser(id: UUID!): Boolean!
  
  # Organization Management
  createOrganization(input: CreateOrganizationInput!): Organization!
  updateOrganization(id: UUID!, input: UpdateOrganizationInput!): Organization!
  deleteOrganization(id: UUID!): Boolean!
  
  # Authentication
  authenticate(input: AuthenticationInput!): AuthenticationContext!
  refreshToken(refreshToken: String!): AuthenticationContext!
  logout(sessionId: UUID!): Boolean!
  
  # Server Management
  startServerInstance(input: StartServerInstanceInput!): ServerInstance!
  stopServerInstance(id: UUID!): Boolean!
  restartServerInstance(id: UUID!): ServerInstance!
  
  # Dynamic Data Mutations (generated at runtime based on configuration)
  # These will be dynamically added based on user-defined data models
}

# Root Subscription Type
type Subscription {
  # Configuration Changes
  configurationScriptUpdated(tenantId: UUID!): ConfigurationScript!
  
  # Server Status
  serverInstanceStatusChanged(tenantId: UUID!): ServerInstance!
  
  # Real-time Data Updates (generated at runtime based on configuration)
  # These will be dynamically added based on user-defined data models
}

# Input Types
input CreateConfigurationScriptInput {
  language: ConfigurationLanguage!
  content: String!
  tenantId: UUID!
}

input UpdateConfigurationScriptInput {
  content: String
  version: String
}

input CreateUserInput {
  email: String!
  password: String!
  displayName: String!
  tenantId: UUID!
}

input UpdateUserInput {
  email: String
  displayName: String
  isActive: Boolean
}

input CreateOrganizationInput {
  name: String!
  domain: String
  subscriptionTier: String!
}

input UpdateOrganizationInput {
  name: String
  domain: String
  subscriptionTier: String
  configurationLimit: Int
  userLimit: Int
  storageLimit: Int
}

input AuthenticationInput {
  email: String
  password: String
  oauthToken: String
  oauthProvider: String
  tenantId: UUID!
}

input StartServerInstanceInput {
  tenantId: UUID!
  configScriptId: UUID!
  port: Int = 8080
}

# Error Types
type Error {
  code: String!
  message: String!
  field: String
  details: JSON
}

# Response wrapper for operations that can fail
union ConfigurationScriptResult = ConfigurationScript | Error
union UserResult = User | Error
union OrganizationResult = Organization | Error
union AuthenticationResult = AuthenticationContext | Error
union ServerInstanceResult = ServerInstance | Error
```

## Dynamic Schema Extension

The schema above represents the core management API. At runtime, additional types, queries, and mutations will be dynamically generated based on user configuration scripts:

### Dynamic Type Generation

```graphql
# Example: If user defines a "Product" model in their configuration
type Product {
  id: UUID!
  name: String!
  price: Float!
  description: String
  categoryId: UUID!
  createdAt: DateTime!
  updatedAt: DateTime!
}

# Corresponding queries would be added:
extend type Query {
  product(id: UUID!): Product
  products(limit: Int = 10, offset: Int = 0, filter: ProductFilter): [Product!]!
}

# Corresponding mutations would be added:
extend type Mutation {
  createProduct(input: CreateProductInput!): Product!
  updateProduct(id: UUID!, input: UpdateProductInput!): Product!
  deleteProduct(id: UUID!): Boolean!
}
```

### Configuration Script Interface

Configuration scripts will use a standardized interface to define data models:

```python
# Python configuration example
from isched import define_model, define_auth

# Define a data model
Product = define_model('Product', {
    'name': {'type': 'string', 'required': True},
    'price': {'type': 'float', 'required': True, 'min': 0},
    'description': {'type': 'string', 'max_length': 1000},
    'category_id': {'type': 'uuid', 'required': True}
})

# Define authentication rules
define_auth({
    'oauth_providers': ['google', 'github'],
    'jwt_secret': 'auto-generated',
    'session_timeout': 3600
})
```

```typescript
// TypeScript configuration example
import { defineModel, defineAuth } from 'isched';

// Define a data model
const Product = defineModel('Product', {
  name: { type: 'string', required: true },
  price: { type: 'number', required: true, min: 0 },
  description: { type: 'string', maxLength: 1000 },
  categoryId: { type: 'uuid', required: true }
});

// Define authentication rules
defineAuth({
  oauthProviders: ['google', 'github'],
  jwtSecret: 'auto-generated',
  sessionTimeout: 3600
});
```

## Compliance Requirements

### GraphQL Specification Compliance

1. **Introspection**: Full introspection support as per GraphQL spec
2. **Error Handling**: Standard GraphQL error format
3. **Type System**: Proper scalar, object, interface, union, and enum types
4. **Validation**: Query validation according to GraphQL rules
5. **Execution**: Proper field resolution and execution order

### Security Considerations

1. **Query Depth Limiting**: Maximum 10 levels of nesting
2. **Query Complexity Analysis**: Cost-based query analysis
3. **Rate Limiting**: Configurable request rate limits
4. **Authentication**: JWT-based authentication for protected fields
5. **Authorization**: Field-level permission checking

### Performance Requirements

1. **Response Time**: < 100ms for simple queries, < 1s for complex queries
2. **Concurrent Users**: Support 1000+ concurrent connections
3. **Memory Usage**: Efficient memory usage for large result sets
4. **Caching**: Intelligent caching of introspection results and frequent queries