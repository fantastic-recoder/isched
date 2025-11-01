# Research: Universal Application Server Backend

**Phase**: 0 (Outline & Research)
**Created**: 2025-11-01
**Feature**: Universal Application Server Backend

## Technical Decisions

### Decision: C++23 Language Standard

**Rationale**: C++23 provides advanced features like modules, coroutines, and improved template metaprogramming that are beneficial for high-performance server applications. The constitution requires C++20 minimum, so C++23 represents an advancement while maintaining compatibility.

**Alternatives considered**:

- C++20: Minimum constitutional requirement, but lacks some modern features
- C++26: Too new, limited compiler support

**Implementation impact**: Requires recent compiler versions (GCC 13+, Clang 16+) in build environment.

### Decision: PEGTL for GraphQL Parsing

**Rationale**: PEGTL (Parsing Expression Grammar Template Library) is a header-only C++ library that provides excellent performance and type safety for parsing. It's already in the project dependencies and well-suited for GraphQL syntax parsing.

**Alternatives considered**:
- Hand-written parser: More complex, error-prone
- ANTLR C++: Requires code generation, external dependencies
- Boost.Spirit: Heavy dependency, complex syntax

**Implementation impact**: GraphQL grammar needs to be expressed as PEGTL rules.

### Decision: SQLite for Embedded Database

**Rationale**: SQLite provides ACID transactions, is embedded (no external process), has excellent performance for read-heavy workloads, and supports full-text search. Perfect match for "batteries included" philosophy.

**Alternatives considered**:
- PostgreSQL embedded: Too heavyweight for embedded use
- RocksDB: Key-value only, lacks SQL and ACID properties
- LMDB: Memory-mapped but lacks SQL interface

**Implementation impact**: SQLite3 needs to be added to Conan dependencies.

### Decision: Restbed with CRTP Abstraction

**Rationale**: Restbed is lightweight and performant for C++ HTTP services. CRTP (Curiously Recurring Template Pattern) allows abstraction without runtime overhead, making HTTP handling swappable in the future.

**Alternatives considered**:
- Boost.Beast: More complex API, larger dependency
- cpp-httplib: Simpler but less performant for high-concurrency scenarios
- Custom HTTP implementation: Reinventing the wheel

**Implementation impact**: Design interface template for HTTP handlers using CRTP pattern.

### Decision: Python and TypeScript Configuration Support

**Rationale**: Python and TypeScript are widely known by frontend developers. Both languages support procedural scripting and can be embedded in C++ applications using available libraries.

**Alternatives considered**:
- JavaScript only: More universal but lacks TypeScript's type safety
- YAML/JSON configuration: Static, doesn't support procedural configuration
- Lua scripting: Less familiar to frontend developers

**Implementation impact**: Need embedding libraries for Python (pybind11) and TypeScript (V8 or similar).

### Decision: nlohmann/json for JSON Processing

**Rationale**: Already in project dependencies, excellent C++ JSON library with intuitive API and good performance. Required for GraphQL response formatting.

**Alternatives considered**:
- RapidJSON: Faster but more complex API
- Boost.JSON: Additional Boost dependency
- Custom JSON implementation: Unnecessary complexity

**Implementation impact**: Use existing dependency, extend with GraphQL-specific serialization helpers.

### Decision: JWT for Authentication Tokens

**Rationale**: JWT (JSON Web Tokens) are industry standard for stateless authentication, required by constitution for security compliance. Works well with OAuth flows.

**Alternatives considered**:
- Session cookies: Stateful, doesn't scale well
- Custom token format: Non-standard, security risks
- API keys only: Limited authentication scenarios

**Implementation impact**: Need JWT library (consider jwt-cpp) for token generation and validation.

### Decision: Smart Pointer Memory Management

**Rationale**: C++ Core Guidelines mandate RAII and prohibit raw pointer ownership. Smart pointers (std::unique_ptr, std::shared_ptr) provide automatic memory management, exception safety, and clear ownership semantics essential for multi-tenant server applications.

**Alternatives considered**:
- Raw pointers: Violates C++ Core Guidelines, memory leak risks
- Manual RAII wrappers: Reinventing existing standard library functionality
- Garbage collection: Not available in C++, performance overhead

**Implementation impact**: All resource management (SQLite connections, file handles, dynamic allocations) must use appropriate smart pointers.

### Decision: Doxygen for Source Code Documentation Generation

**Rationale**: Doxygen is the de facto standard for C++ documentation generation, supports inline comments, generates comprehensive API references, and can include source code snippets. Integration with CMake build system is well-established.

**Alternatives considered**:
- Sphinx with Breathe: More complex setup, Python dependency
- GitBook/GitLab Pages: Manual documentation, no automatic API generation
- Custom documentation system: Unnecessary complexity

**Implementation impact**: Add Doxygen comments to all public APIs, configure CMake to generate documentation during build, include source code examples in generated docs.

## Configuration Script Integration Research

### Python Integration Approach

**Method**: Embed Python interpreter using pybind11 for C++ interoperability.

**Benefits**:
- Native C++ binding support
- Excellent performance
- Type safety between C++ and Python

**Challenges**:
- Python interpreter overhead
- Memory management between Python and C++
- Threading considerations for multi-tenant operation

### TypeScript Integration Approach

**Method**: Use V8 JavaScript engine with TypeScript compilation pipeline.

**Benefits**:
- High performance V8 engine
- TypeScript provides type safety
- Good sandbox capabilities for multi-tenant isolation

**Challenges**:
- V8 integration complexity
- TypeScript compilation step needed
- Memory isolation between tenants

## GraphQL Schema Generation Strategy

### Automatic Schema Generation

**Approach**: Parse configuration scripts to extract data model definitions, automatically generate corresponding GraphQL schema and SQLite table schemas.

**Key components**:
1. Configuration script parser (per language)
2. Abstract data model representation
3. GraphQL schema generator
4. SQLite schema generator
5. Query resolver generator

**Benefits**:
- Zero-configuration experience for developers
- Automatic consistency between data storage and GraphQL API
- Type safety throughout the stack

## Multi-Tenant Architecture Research

### Tenant Isolation Strategy

**Database isolation**: Separate SQLite databases per tenant with shared schema structure.

**Configuration isolation**: Separate configuration script execution contexts per tenant.

**Memory isolation**: Tenant-specific object pools and memory allocators.

**Benefits**:
- Strong isolation guarantees
- Independent scaling per tenant
- Simplified backup and recovery

**Challenges**:
- Resource management complexity
- Inter-tenant communication limitations

## Performance Optimization Research

### High-Performance Considerations

**Connection pooling**: Reuse HTTP connections and database connections efficiently.

**Query optimization**: GraphQL query complexity analysis and optimization.

**Memory management**: Custom allocators for high-frequency objects.

**Caching strategy**: In-memory caching for frequently accessed data.

**Threading model**: Async I/O with thread pool for CPU-intensive operations.

## Security Research

### OAuth 2.0 Integration

**Flow support**: Authorization Code, Client Credentials, and Resource Owner Password flows.

**Provider integration**: Support for common OAuth providers (Google, GitHub, Auth0).

**Token management**: Secure storage and refresh token handling.

### Data validation and sanitization

**Input validation**: GraphQL query validation and SQL injection prevention.

**Output sanitization**: Prevent XSS in JSON responses.

**Rate limiting**: Query complexity limits and request rate limiting.

## Dependencies Research

### Additional Conan Dependencies Required

- `sqlite3/3.44.0`: SQLite database engine
- `pybind11/2.11.1`: Python-C++ bindings
- `jwt-cpp/0.6.0`: JWT token handling
- `v8/10.8.168`: JavaScript/TypeScript engine (optional, if TypeScript support needed)

### Additional Build Dependencies Required

- `doxygen/1.9.8`: Source code documentation generation
- `graphviz`: Dependency graphs and diagrams for Doxygen
- CMake FindDoxygen module: Integration with build system

### Dependency Compatibility

All proposed dependencies are compatible with:
- C++23 standard
- CMake build system
- Conan package manager
- Linux target platform
- Cross-compilation requirements