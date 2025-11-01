# Feature Specification: Universal Application Server Backend

**Feature Branch**: `001-universal-backend`  
**Created**: 2025-11-01  
**Status**: Draft  
**Input**: User description: "The \"Isched\" universal application server backend should simplify web application development. The frontend developer should not need any other software to develop the frontend. A procedural configuration (for example in Python or Typescript) of the Isched is the only thing needed. Isched will adhere to the GraphQL specification to not \"reinvent the wheel\"."

## Clarifications

### Session 2025-11-01

- Q: Error Response Format for GraphQL Specification Compliance → A: Enhanced format with Isched-specific extensions: Include additional error metadata like `{"extensions": {"code": "VALIDATION_ERROR", "timestamp": "...", "requestId": "..."}}`
- Q: GraphQL Schema Bootstrapping for "Hello World" Test → A: Minimal built-in schema with basic queries including server health monitoring (client count, current configuration, uptime) similar to Spring Boot actuators
- Q: Multi-Tenant Process Isolation Strategy → A: Pre-allocated tenant process pool: Fixed number of tenant processes (configurable) with tenant assignment based on load balancing
- Q: Thread Pool Configuration Strategy → A: Adaptive thread pool sizing: Automatic thread pool adjustment based on tenant load patterns and response time metrics with configurable maximum threads
- Q: Database Connection Pooling Strategy for Multi-Tenant Architecture → A: Per-tenant database connections with pooling: Each tenant process maintains its own connection pool to tenant-specific SQLite databases
- Q: Python/TypeScript Runtime Architecture → A: Separated dynamic library with CLI executables: Python and TypeScript runtimes implemented as isched-cli-python and isched-cli-typescript CLI executables, loaded via dynamic library by main Isched server. Configuration scripts saved to disk and executed via spawn processes with shared memory for isolation and reduced attack surface.
- Q: IPC Communication Mechanism for CLI Process Coordination → A: IPC via shared memory segments with message queues for command/response coordination
- Q: Configuration Data Exchange Format → A: JSON configuration with version-controlled updates for rollback support
- Q: Dynamic Library Core Functionality → A: GraphQL resolver system with plugin support: Core data types, IPC utilities, built-in resolvers (REST API, SQL database), and binary plugin system for custom resolvers configurable via CLI
- Q: Memory Management Strategy for C++ Core Guidelines Compliance → A: Smart pointers mandatory: All resource management MUST use std::unique_ptr or std::shared_ptr instead of raw pointers (e.g., SQLite connections, file handles, dynamic allocations)
- Q: Documentation Generation Strategy for Build Process → A: Automated source code documentation with reference inclusion: Build process MUST generate comprehensive documentation from source code including API references, code examples, and inline source code snippets for complete developer reference

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Frontend Developer Setup (Priority: P1)

A frontend developer can configure and run a complete backend server using only Isched and a simple configuration script. They don't need to install databases, authentication services, or additional backend frameworks.

**Why this priority**: This is the core value proposition - eliminating the need for multiple backend services and complex setup procedures.

**Independent Test**: Can be fully tested by a frontend developer creating a basic configuration script and successfully running a GraphQL endpoint that serves data and handles authentication.

**Acceptance Scenarios**:

1. **Given** a frontend developer with only Isched installed, **When** they start Isched without any configuration script, **Then** they can immediately access a GraphQL endpoint with built-in health monitoring queries (hello, version, clientCount, uptime)
2. **Given** a running Isched server, **When** a frontend developer makes GraphQL queries, **Then** they receive properly formatted GraphQL responses according to the specification
3. **Given** a basic configuration, **When** the server starts, **Then** it automatically provides user authentication and basic data storage without additional setup

---

### User Story 2 - Procedural Configuration (Priority: P2)

Frontend developers can define their backend behavior through procedural scripts (Python or TypeScript) rather than complex configuration files or manual setup.

**Why this priority**: Procedural configuration provides flexibility and familiarity for developers while maintaining simplicity.

**Independent Test**: Can be tested by writing different configuration scripts that modify server behavior and verifying that the changes take effect without manual server configuration.

**Acceptance Scenarios**:

1. **Given** a configuration script, **When** the developer defines data models, **Then** Isched automatically creates the corresponding GraphQL schema and database storage
2. **Given** a running server, **When** the configuration script is updated, **Then** the server reflects the changes without manual intervention
3. **Given** authentication requirements in the script, **When** the server starts, **Then** it automatically configures OAuth and JWT token handling

---

### User Story 3 - GraphQL Specification Compliance (Priority: P3)

All GraphQL interactions follow the official GraphQL specification exactly, ensuring compatibility with existing GraphQL tools and client libraries.

**Why this priority**: Standards compliance ensures interoperability and prevents vendor lock-in, allowing developers to use familiar GraphQL tools.

**Independent Test**: Can be tested by running standard GraphQL introspection queries and validating responses against the official GraphQL specification test suite.

**Acceptance Scenarios**:

1. **Given** a configured Isched server, **When** GraphQL introspection queries are executed, **Then** they return properly formatted schema information per the GraphQL spec
2. **Given** various GraphQL query types (queries, mutations, subscriptions), **When** they are executed, **Then** responses conform exactly to GraphQL specification formatting
3. **Given** GraphQL client libraries, **When** they connect to Isched, **Then** they can interact normally without compatibility issues

---

### Edge Cases

- Configuration errors cause immediate abort of Isched server boot-up. In case the configuration is changed on runtime a new instance of the Isched server process will be forked and the new instance checks the new configuration. In case the new instance boots up successfully, current connections in the old server will be severred after a retention period and sending an explanation message to the client applications. After the old connections are severed the old server terminates, the new server instance starts serving.
- When GraphQL queries exceed reasonable complexity limits the query should abort with a good description of the problem returned in the error message. The complexity metric should be configurable.
- When configuration scripts try to define conflicting authentication methods, the server will abort the boot-up, look on the first edge case.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow frontend developers to configure the entire backend using only procedural scripts (Python or TypeScript)
- **FR-002**: System MUST automatically provide user authentication and authorization without requiring external services
- **FR-003**: System MUST generate GraphQL schemas automatically based on configuration script definitions
- **FR-004**: System MUST provide embedded data storage that doesn't require separate database installation
- **FR-005**: System MUST start and run a complete backend server from a single configuration script execution
- **FR-006**: System MUST support both Python and TypeScript for configuration scripts through separate CLI executables (isched-cli-python, isched-cli-typescript)
- **FR-007**: System MUST provide basic user and organization management capabilities out of the box
- **FR-008**: System MUST provide enhanced GraphQL error responses with extensions containing error codes, timestamps, and request IDs for debugging and monitoring
- **FR-009**: System MUST provide a minimal built-in GraphQL schema for immediate testing with health monitoring queries (hello, version, clientCount, uptime, currentConfiguration) similar to Spring Boot actuators
- **FR-010**: System MUST serve thousands of clients using a pre-allocated tenant process pool with configurable thread pools per process
- **FR-011**: System MUST complete requests within 20 milliseconds on Ryzen 7/Intel i5 hardware under normal load conditions
- **FR-012**: System MUST provide adaptive thread pool sizing with automatic adjustment based on tenant load patterns and response time metrics, subject to configurable maximum thread limits
- **FR-013**: System MUST maintain per-tenant database connections with pooling, where each tenant process maintains its own connection pool to tenant-specific SQLite databases
- **FR-014**: System MUST execute configuration scripts in isolated processes using isched-cli-python and isched-cli-typescript executables with shared memory communication for security and performance
- **FR-015**: System MUST save GraphQL mutation-based configuration to script files on server work directory before execution by spawned CLI processes
- **FR-016**: System MUST use shared memory segments with message queues for IPC coordination between server and CLI processes to ensure efficient command/response handling
- **FR-017**: System MUST exchange configuration data and schema definitions using JSON format with version-controlled updates to support rollback and human-readable debugging
- **FR-018**: System MUST provide a GraphQL resolver system with built-in resolvers for REST API calls and SQL database operations
- **FR-019**: System MUST support binary plugin system for custom resolvers that can be loaded dynamically and configured via CLI executables
- **FR-020**: System MUST allow CLI processes to define, configure, and manage GraphQL resolvers in the server runtime
- **FR-021**: System MUST use smart pointers (std::unique_ptr, std::shared_ptr) for all resource management instead of raw pointers to ensure C++ Core Guidelines compliance and memory safety
- **FR-022**: System MUST generate comprehensive source code documentation as part of the build process, including API references, inline code examples, and embedded source code snippets for complete developer reference

### Constitutional Requirements

**Performance Requirements** (Constitution Principle I):

- **FR-PERF-001**: Feature MUST maintain multi-tenant performance characteristics
- **FR-PERF-002**: Implementation MUST support cloud-to-embedded deployment scenarios  
- **FR-PERF-003**: Performance impact MUST be measured and documented

**GraphQL Compliance** (Constitution Principle II):

- **FR-GQL-001**: All GraphQL features MUST conform to [GraphQL specification](https://spec.graphql.org/)
- **FR-GQL-002**: Any non-standard extensions MUST be explicitly documented

**Security Requirements** (Constitution Principle III):

- **FR-SEC-001**: Authentication MUST use industry-standard protocols (OAuth, JWT)
- **FR-SEC-002**: Default configuration MUST be secure-by-default
- **FR-SEC-003**: Multi-tenant data isolation MUST be maintained

**Testing Requirements** (Constitution Principle IV):

- **FR-TEST-001**: Core functionality MUST follow TDD approach
- **FR-TEST-002**: Integration tests required for GraphQL endpoints
- **FR-TEST-003**: Performance regression tests MUST validate scalability

**Portability Requirements** (Constitution Principle V):

- **FR-PORT-001**: Code MUST compile on Linux with Conan dependencies
- **FR-PORT-002**: Cross-platform documentation MUST be provided

**C++ Core Guidelines Requirements** (Constitution Technical Standards):

- **FR-CPP-001**: All C++ code MUST adhere to [ISO C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- **FR-CPP-002**: Code reviews MUST verify guideline compliance
- **FR-CPP-003**: Any deviations MUST be explicitly justified and documented

### Key Entities

- **Configuration Script**: Python or TypeScript file that defines backend behavior, data models, and authentication rules, executed via isched-cli-python or isched-cli-typescript in isolated processes
- **Data Model**: User-defined data structures that automatically generate GraphQL types and database schemas
- **Authentication Context**: User session and permission information managed automatically by the server
- **GraphQL Schema**: Automatically generated schema based on configuration script definitions
- **Server Instance**: Running Isched server configured by the procedural script
- **Tenant Process Pool**: Pre-allocated pool of tenant processes with load balancing and adaptive thread management
- **Database Connection Pool**: Per-tenant connection pools managing access to tenant-specific SQLite databases
- **CLI Executables**: Separate isched-cli-python and isched-cli-typescript processes for executing configuration scripts with shared memory communication
- **Dynamic Library**: Shared isched runtime library loaded by server and CLI executables for common functionality
- **GraphQL Resolvers**: Built-in and plugin-based resolvers for data fetching, including REST API and SQL database resolvers
- **Binary Plugin System**: Dynamic loading system for custom resolver implementations configurable via CLI processes

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Frontend developers can create a working backend server in under 10 minutes using only a configuration script
- **SC-002**: System eliminates the need for 100% of external backend services (databases, auth servers, API frameworks) for typical web applications
- **SC-003**: All GraphQL responses pass 100% of official GraphQL specification compliance tests
- **SC-004**: Configuration script changes take effect in under 5 seconds without server restart
- **SC-005**: 95% of common web application backend requirements can be satisfied through configuration script alone
- **SC-006**: System serves thousands of concurrent clients with 95th percentile response times under 20 milliseconds on Ryzen 7/Intel i5 hardware

## Assumptions

- Frontend developers have basic programming knowledge in Python or TypeScript
- Applications built with Isched target typical web application use cases (CRUD operations, user management, basic business logic)
- GraphQL is the preferred API interface for modern web applications
- Embedded database performance is sufficient for most small to medium-scale applications
- Configuration through code is preferred over configuration files for developer experience
