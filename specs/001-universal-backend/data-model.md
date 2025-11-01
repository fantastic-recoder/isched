# Data Model: Universal Application Server Backend

**Phase**: 1 (Design & Contracts)
**Created**: 2025-11-01
**Updated**: 2025-11-01 (Phase 1 Implementation Details)
**Feature**: Universal Application Server Backend

## Implementation Context

**Namespace**: `isched::v0_0_1::backend`
**File Prefix**: `isched_backend_`
**Database**: SQLite per-tenant with file-per-tenant isolation
**Schema Generation**: Automatic from configuration scripts

## Core Entities

### Configuration Script

**Purpose**: Represents a procedural configuration script that defines backend behavior.

**Attributes**:
- `id`: Unique identifier (UUID)
- `language`: Script language (Python, TypeScript)
- `content`: Script source code
- `version`: Script version for change tracking
- `tenant_id`: Associated tenant identifier
- `created_at`: Script creation timestamp
- `updated_at`: Last modification timestamp
- `is_active`: Whether script is currently active

**Relationships**:
- One-to-many with DataModel (script defines multiple data models)
- One-to-many with AuthenticationRule (script defines auth rules)
- Belongs-to Tenant (multi-tenant isolation)

**Validation Rules**:
- Language must be 'python' or 'typescript'
- Content must be valid syntax for specified language
- Only one active script per tenant at a time

### Data Model

**Purpose**: Represents a user-defined data structure from configuration script.

**Attributes**:
- `id`: Unique identifier
- `name`: Model name (e.g., "User", "Product")
- `fields`: JSON representation of field definitions
- `indexes`: Database index definitions
- `constraints`: Data validation constraints
- `config_script_id`: Associated configuration script
- `graphql_type`: Generated GraphQL type definition
- `sql_schema`: Generated SQLite table schema

**Relationships**:
- Belongs-to ConfigurationScript
- Many-to-many with DataModel (relationships between models)

**Validation Rules**:
- Name must be valid GraphQL type name
- Fields must include type information
- Required fields must be specified

### GraphQL Schema

**Purpose**: Automatically generated GraphQL schema based on data models.

**Attributes**:
- `id`: Unique identifier
- `tenant_id`: Associated tenant
- `schema_definition`: Complete GraphQL SDL
- `introspection_result`: Cached introspection response
- `generated_at`: Schema generation timestamp
- `is_current`: Whether this is the active schema

**Relationships**:
- Belongs-to Tenant
- Generated-from multiple DataModel entities

**State Transitions**:
- Draft → Validating → Active → Superseded

### Authentication Context

**Purpose**: Represents user session and permission information.

**Attributes**:
- `id`: Session identifier
- `user_id`: Associated user identifier
- `tenant_id`: Tenant context
- `jwt_token`: Encrypted JWT token
- `refresh_token`: Token refresh capability
- `permissions`: JSON array of granted permissions
- `expires_at`: Session expiration timestamp
- `created_at`: Session creation time
- `last_activity`: Last request timestamp

**Relationships**:
- Belongs-to User
- Belongs-to Tenant

**Validation Rules**:
- JWT token must be valid and unexpired
- Permissions must be valid for tenant
- Session must not exceed maximum duration

### User

**Purpose**: Represents system users with authentication capabilities.

**Attributes**:
- `id`: Unique user identifier (UUID)
- `email`: User email address (unique)
- `password_hash`: Encrypted password storage
- `display_name`: User-friendly display name
- `tenant_id`: Primary tenant association
- `oauth_providers`: JSON array of linked OAuth providers
- `is_active`: Account status
- `created_at`: Account creation timestamp
- `last_login`: Last successful login

**Relationships**:
- Belongs-to Tenant (primary)
- Many-to-many with Tenant (can belong to multiple tenants)
- One-to-many with AuthenticationContext (multiple sessions)

**Validation Rules**:
- Email must be valid email format
- Password must meet minimum security requirements
- Display name must be non-empty

### Organization

**Purpose**: Represents tenant organizations for multi-tenant isolation.

**Attributes**:
- `id`: Unique organization identifier (UUID)
- `name`: Organization name
- `domain`: Organization domain (optional)
- `subscription_tier`: Service tier level
- `configuration_limit`: Maximum active configurations
- `user_limit`: Maximum users allowed
- `storage_limit`: Data storage limit
- `created_at`: Organization creation timestamp

**Relationships**:
- One-to-many with User (organization members)
- One-to-many with ConfigurationScript (organization configurations)

**Validation Rules**:
- Name must be unique across system
- Limits must be positive integers
- Domain must be valid domain format if provided

### Server Instance

**Purpose**: Represents a running Isched server instance with specific configuration.

**Attributes**:
- `id`: Instance identifier
- `tenant_id`: Associated tenant
- `config_script_id`: Active configuration script
- `port`: HTTP server port
- `status`: Instance status (starting, running, stopping, stopped)
- `pid`: Process identifier
- `started_at`: Instance start timestamp
- `memory_usage`: Current memory consumption
- `request_count`: Total requests served

**Relationships**:
- Belongs-to Tenant
- Uses ConfigurationScript
- One-to-many with RequestLog (request history)

**State Transitions**:
- Starting → Running → Stopping → Stopped
- Running → Restarting → Running (for configuration updates)

## Database Schema Considerations

### Multi-Tenant Data Isolation

**Strategy**: Separate SQLite database files per tenant with shared schema structure.

**Benefits**:
- Strong data isolation
- Independent backup/restore per tenant
- Simplified access control

**File Structure**:
```
data/
├── tenants/
│   ├── {tenant_id_1}/
│   │   ├── main.db          # Primary tenant data
│   │   ├── config.db        # Configuration scripts
│   │   └── sessions.db      # Authentication sessions
│   └── {tenant_id_2}/
│       └── ...
└── system/
    ├── organizations.db     # System-wide organization data
    └── users.db            # User account information
```

### ACID Transaction Requirements

**Configuration Changes**: All schema modifications must be atomic.

**Data Operations**: User data operations must support rollback.

**Session Management**: Authentication state changes must be consistent.

### Index Strategy

**Performance Indexes**:
- Users: email, tenant_id
- ConfigurationScript: tenant_id, is_active
- AuthenticationContext: user_id, expires_at
- DataModel: config_script_id, name

**Full-Text Search**: Enable FTS5 for searchable text fields in user-defined models.

## GraphQL Schema Generation

### Automatic Type Generation

**Process**:
1. Parse DataModel field definitions
2. Generate GraphQL object types
3. Create Query/Mutation resolvers
4. Build introspection schema
5. Validate against GraphQL specification

**Type Mapping**:
- String → GraphQL String
- Integer → GraphQL Int
- Float → GraphQL Float
- Boolean → GraphQL Boolean
- Date → GraphQL DateTime (custom scalar)
- JSON → GraphQL JSON (custom scalar)
- Array → GraphQL List
- Object → Nested GraphQL type

### Query Complexity Analysis

**Limits**:
- Maximum query depth: 10 levels
- Maximum field count: 100 fields
- Maximum array size: 1000 items
- Timeout: 30 seconds per query

**Cost Calculation**: Based on field count, depth, and estimated database operations.

## Error Handling

### Configuration Script Errors

**Syntax Errors**: Immediate validation failure with line number and error description.

**Runtime Errors**: Graceful degradation with error logging and fallback to previous configuration.

**Schema Conflicts**: Detailed conflict resolution guidance for developers.

### Database Errors

**Connection Failures**: Automatic retry with exponential backoff.

**Constraint Violations**: User-friendly error messages with suggested corrections.

**Disk Space**: Graceful handling with cleanup suggestions.

### GraphQL Errors

**Parse Errors**: Standard GraphQL error format with precise location information.

**Validation Errors**: Detailed explanation of schema violations.

**Execution Errors**: Field-level error reporting with partial results when possible.

## C++ Implementation Classes

### Core Backend Classes

**Memory Management**: All classes follow C++ Core Guidelines with mandatory smart pointer usage - no raw pointers for ownership.

**Documentation**: All public APIs documented with Doxygen comments, source code examples included in generated documentation.

```cpp
namespace isched::v0_0_1::backend {

/// @brief Main server entry point for Universal Application Server Backend
/// @details Manages tenant processes, HTTP server, and configuration lifecycle
/// @example
/// @code
/// server_config config{.port = 8080, .host = "0.0.0.0"};
/// isched_server server(config);
/// server.start();
/// @endcode
class isched_server {
public:
    explicit isched_server(const server_config& config);
    void start();
    void stop();
    void reload_config();
    
private:
    std::unique_ptr<tenant_manager> m_tenant_manager;
    std::unique_ptr<http_server> m_http_server;
    server_config m_config;
};

// Multi-tenant process pool management
class isched_tenant_manager {
public:
    void create_tenant(const tenant_id& id, const tenant_config& config);
    void remove_tenant(const tenant_id& id);
    tenant_process_pool& get_tenant_pool(const tenant_id& id);
    
private:
    std::unordered_map<tenant_id, std::unique_ptr<tenant_process_pool>> m_pools;
    std::shared_mutex m_pools_mutex;
};

// Per-tenant process pool with isolation
class tenant_process_pool {
public:
    explicit tenant_process_pool(const tenant_config& config);
    
    // Execute GraphQL query in isolated process
    future<graphql_result> execute_query(const graphql_query& query);
    
    // Execute configuration script
    future<config_result> execute_config(const config_script& script);
    
private:
    std::vector<std::unique_ptr<tenant_process>> m_processes;
    thread_pool m_thread_pool;
    database_pool m_db_pool;
};

// PEGTL-based GraphQL parser
class isched_graphql_parser {
public:
    parse_result parse_query(const std::string& query);
    validation_result validate_against_schema(const parsed_query& query, 
                                             const graphql_schema& schema);
    
private:
    using grammar = pegtl::seq<graphql_document>;
    std::unique_ptr<compiled_grammar> m_grammar;
};

// SQLite per-tenant database management
class isched_database {
public:
    explicit isched_database(const tenant_id& tenant, const database_config& config);
    
    void create_schema(const data_model_list& models);
    query_result execute_sql(const std::string& sql, const parameter_list& params);
    transaction begin_transaction();
    
private:
    sqlite3* m_connection;
    std::unordered_map<std::string, sqlite3_stmt*> m_prepared_statements;
    mutable std::shared_mutex m_mutex;
};

// Configuration script executor with subprocess isolation
class isched_config_executor {
public:
    execution_result execute_python(const python_script& script);
    execution_result execute_typescript(const typescript_script& script);
    
private:
    subprocess_pool m_python_pool;
    subprocess_pool m_typescript_pool;
    script_cache m_cache;
};

// REST and GraphQL-over-HTTP handler
class isched_rest_handler {
public:
    void register_route(const http_method& method, const std::string& path,
                       handler_function handler);
    
    void handle_graphql_request(const http_request& request, http_response& response);
    void handle_auth_request(const http_request& request, http_response& response);
    
private:
    route_table m_routes;
    auth_manager m_auth;
    graphql_executor m_graphql_executor;
};

} // namespace isched::v0_0_1::backend
```

### Data Model Mapping

**SQLite Schema Generation**:
```cpp
class schema_generator {
public:
    static std::string generate_create_table(const data_model& model);
    static std::string generate_indexes(const data_model& model);
    static migration_script generate_migration(const data_model& from, 
                                             const data_model& to);
};
```

**GraphQL Schema Generation**:
```cpp
class graphql_schema_generator {
public:
    static graphql_schema generate_schema(const std::vector<data_model>& models);
    static std::string generate_sdl(const graphql_schema& schema);
    static resolver_map generate_resolvers(const graphql_schema& schema);
};
```

### Multi-Tenant Isolation

**Process Isolation Strategy**:
```cpp
class tenant_process {
public:
    explicit tenant_process(const tenant_id& tenant);
    
    // Execute in isolated process space
    template<typename Result, typename Request>
    future<Result> execute(const Request& request);
    
private:
    process_handle m_process;
    ipc_channel m_channel;
    tenant_id m_tenant;
};
```

**Database Isolation Strategy**:
```cpp
class database_pool {
public:
    database_connection acquire_connection(const tenant_id& tenant);
    void release_connection(database_connection conn);
    
private:
    std::unordered_map<tenant_id, connection_pool> m_tenant_pools;
};
```