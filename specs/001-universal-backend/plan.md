# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2025-11-01 | **Spec**: [Universal Application Server Backend](spec.md)
**Input**: Feature specification from `/specs/001-universal-backend/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Primary requirement: Create a universal application server backend that simplifies web application development by eliminating the need for external services. Frontend developers should only need Isched and a configuration script (Python or TypeScript) to run a complete backend. Technical approach: C++23 server with embedded SQLite database, PEGTL-based GraphQL parser, Restbed HTTP handling via CRTP pattern, comprehensive testing with TypeScript clients.

## Technical Context

**Language/Version**: C++23 (latest standard with advanced features)
**Primary Dependencies**: Conan-managed dependencies (restbed, spdlog, catch2, pegtl, nlohmann_json, sqlite3)
**Storage**: SQLite embedded database with ACID transaction support for searchable data
**Testing**: Catch2 for C++ unit/integration tests + TypeScript clients for server testing
**Target Platform**: Linux primary, cross-platform compatibility required
**Project Type**: High-performance C++ backend server with embedded database
**Performance Goals**: Massive parallel operation, cloud-to-embedded scalability
**Constraints**: Multi-tenant isolation, GraphQL spec compliance, security-first
**Scale/Scope**: Enterprise-grade multi-tenant backend eliminating external service dependencies
**GraphQL Parser**: PEGTL (Parsing Expression Grammar Template Library) for GraphQL parsing
**HTTP Handling**: Restbed library abstracted via CRTP (Curiously Recurring Template Pattern)
**JSON Processing**: nlohmann/json for JSON serialization/deserialization
**Build System**: CMake with Conan package management
**Logging**: spdlog for structured logging
**Configuration Languages**: Python and TypeScript for procedural configuration scripts

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

✅ **High Performance**: Implementation must consider performance impact on multi-tenant operation
✅ **GraphQL Compliance**: All GraphQL features must conform to official specification
✅ **Security-First**: Authentication/authorization must use industry standards
✅ **Test-Driven Development**: TDD mandatory for core functionality
✅ **Cross-Platform Portability**: Must build with Conan on Linux, documented elsewhere
✅ **C++ Core Guidelines**: All C++ code must adhere to ISO C++ Core Guidelines

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/main/cpp/isched/
├── core/                    # Core server functionality
│   ├── server.hpp/cpp      # Main server class
│   ├── configuration.hpp/cpp  # Configuration script handling
│   └── lifecycle.hpp/cpp   # Server lifecycle management
├── graphql/                # GraphQL implementation
│   ├── parser.hpp/cpp      # PEGTL-based GraphQL parser
│   ├── schema.hpp/cpp      # Schema generation and management
│   ├── executor.hpp/cpp    # Query execution engine
│   └── introspection.hpp/cpp # GraphQL introspection support
├── database/               # SQLite database layer
│   ├── connection.hpp/cpp  # Database connection management
│   ├── schema_builder.hpp/cpp # Automatic schema generation
│   ├── transaction.hpp/cpp # ACID transaction handling
│   └── query_builder.hpp/cpp # SQL query generation
├── http/                   # HTTP handling abstraction
│   ├── request_handler.hpp # CRTP base for HTTP handling
│   ├── restbed_adapter.hpp/cpp # Restbed implementation
│   └── response_builder.hpp/cpp # HTTP response construction
├── auth/                   # Authentication and authorization
│   ├── oauth_provider.hpp/cpp # OAuth implementation
│   ├── jwt_handler.hpp/cpp # JWT token management
│   └── user_manager.hpp/cpp # User and organization management
├── config/                 # Configuration script interfaces
│   ├── python_interface.hpp/cpp # Python configuration support
│   ├── typescript_interface.hpp/cpp # TypeScript configuration support
│   └── script_executor.hpp/cpp # Configuration script execution
└── utils/                  # Utility classes
    ├── json_serializer.hpp/cpp # nlohmann/json utilities
    ├── logger.hpp/cpp      # spdlog wrapper
    └── validation.hpp/cpp  # Input validation utilities

src/test/cpp/isched/
├── unit/                   # Unit tests (Catch2)
│   ├── graphql/           # GraphQL parser and executor tests
│   ├── database/          # Database layer tests
│   ├── auth/              # Authentication tests
│   └── config/            # Configuration handling tests
├── integration/           # Integration tests
│   ├── graphql_endpoint/  # End-to-end GraphQL tests
│   ├── auth_flow/         # Authentication flow tests
│   └── performance/       # Performance regression tests
└── typescript_clients/    # TypeScript test clients
    ├── basic_queries/     # Basic GraphQL query tests
    ├── auth_scenarios/    # Authentication test scenarios
    └── configuration/     # Configuration script examples
```

**Structure Decision**: Single project structure selected as Isched is a unified C++ backend server. The existing `src/main/cpp/isched/` and `src/test/cpp/isched/` directories will be extended with the new universal backend functionality. TypeScript clients are included in the test suite to validate server behavior from a frontend developer perspective.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitutional violations identified. All design decisions align with constitutional principles.

## Post-Design Constitution Re-Check

*GATE: Final validation after Phase 1 design completion.*

✅ **High Performance**: 
- CRTP pattern eliminates virtual function overhead
- SQLite provides excellent performance for embedded database scenarios
- Multi-threaded design supports massive parallel operation
- Memory-efficient design for cloud-to-embedded deployment

✅ **GraphQL Compliance**: 
- PEGTL parser ensures strict GraphQL specification compliance
- Complete introspection support implemented
- Standard error handling and response formatting
- No custom extensions without explicit documentation

✅ **Security-First**: 
- JWT authentication with industry-standard libraries
- OAuth 2.0 integration with major providers
- Multi-tenant data isolation through separate databases
- Secure-by-default configuration approach

✅ **Test-Driven Development**: 
- Comprehensive unit test coverage with Catch2
- Integration tests for all GraphQL endpoints
- TypeScript client tests for end-to-end validation
- Performance regression testing framework

✅ **Cross-Platform Portability**: 
- All dependencies available through Conan
- CMake build system ensures cross-platform compatibility
- Linux primary target with documented cross-platform support
- Embedded database eliminates external system dependencies

✅ **C++ Core Guidelines**: 
- RAII principles enforced throughout design
- Smart pointers for memory management
- Exception safety guaranteed
- Modern C++23 features utilized appropriately

**Final Assessment**: All constitutional requirements satisfied. Design ready for implementation.
