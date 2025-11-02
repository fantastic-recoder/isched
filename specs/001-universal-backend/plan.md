# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2025-11-02 | **Spec**: [Universal Application Server Backend](spec.md)
**Input**: Feature specification from `/specs/001-universal-backend/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Create a universal application server backend that eliminates external service dependencies for frontend developers. Technical approach: C++23 multi-tenant server with separated CLI runtimes (isched-cli-python, isched-cli-typescript), shared dynamic library, embedded SQLite databases, PEGTL GraphQL parser, binary plugin system for resolvers, shared memory IPC, mandatory smart pointer usage, and automated source code documentation generation as part of the build process.

## Technical Context

**Language/Version**: C++23 (advanced features for multi-process server applications)
**Primary Dependencies**: Conan-managed dependencies (pegtl, restbed, sqlite3, nlohmann_json, spdlog, jwt-cpp)
**Architecture**: Multi-process with separated CLI runtimes and shared dynamic library
**Storage**: SQLite embedded database per-tenant with connection pooling
**Memory Management**: Mandatory smart pointers (std::unique_ptr, std::shared_ptr) - no raw pointers
**Testing**: Catch2 for unit/integration tests + performance regression testing (20ms target)
**Documentation**: Automated source code documentation generation with code reference inclusion
**Target Platform**: Linux primary, cross-platform compatibility required
**Project Type**: High-performance C++ backend server with binary plugin system
**Performance Goals**: 20ms response times, thousands of concurrent clients, cloud-to-embedded scalability
**Constraints**: Multi-tenant isolation, GraphQL spec compliance, security-first, C++ Core Guidelines
**Scale/Scope**: Enterprise-grade multi-tenant backend with resolver plugins and CLI script execution
**IPC**: Shared memory segments with message queues for server-CLI coordination
**Configuration**: JSON format with version control for rollback support

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
<!--
Based on existing C++23 project structure with CMake and Conan dependencies.
The project follows standard C++ layout with separated CLI processes and shared library.
-->

```text
# Single C++23 project with multi-process architecture
src/
├── main/cpp/isched/
│   ├── backend/          # Core server implementation
│   │   ├── isched_server.hpp/cpp          # HTTP service foundation
│   │   ├── isched_tenant_manager.hpp/cpp  # Multi-tenant process management
│   │   ├── isched_database.hpp/cpp        # SQLite database layer
│   │   ├── isched_graphql.hpp/cpp         # GraphQL query engine
│   │   ├── isched_plugin.hpp/cpp          # Binary plugin system
│   │   └── isched_auth.hpp/cpp            # Authentication middleware
│   ├── cli/              # Separated CLI processes
│   │   ├── python/       # isched-cli-python runtime
│   │   └── typescript/   # isched-cli-typescript runtime
│   └── shared/           # Common components for dynamic library
│       ├── ipc/          # Shared memory IPC
│       ├── config/       # Configuration management
│       └── utils/        # Utility functions

tests/
├── unit/                 # Component unit tests
├── integration/          # Multi-component integration tests
└── performance/          # 20ms response time validation

build/                    # CMake build output (cmake-build-debug/)
conanfile.txt            # Conan dependency management
CMakeLists.txt           # Build system configuration
```

**Structure Decision**: Single C++23 project with multi-process architecture selected. This structure aligns with the existing CMake/Conan build system and supports the requirement for separated CLI processes while maintaining a shared dynamic library. The layout follows C++ Core Guidelines organization patterns and enables efficient binary plugin loading.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Multi-process architecture | CLI script isolation & security | Single process cannot isolate tenant scripts safely |
| Binary plugin system | Runtime GraphQL resolver loading | Static linking prevents dynamic schema updates |
| Per-tenant databases | Absolute data isolation guarantee | Shared database cannot prevent cross-tenant data leaks |

## Phase Completion Status

### ✅ Phase 0: Research & Technical Validation **COMPLETE**
- **research.md**: All technical unknowns resolved through implementation validation
- **Architecture Decisions**: Confirmed through working foundation components
- **Technology Stack**: Validated with actual C++23 implementation
- **Performance Requirements**: Benchmarked and confirmed achievable

### ✅ Phase 1: Design & Contracts **COMPLETE**  
- **data-model.md**: Multi-tenant database architecture and schema generation pipeline designed
- **contracts/graphql-schema.md**: Complete GraphQL API contract specification
- **contracts/http-api.md**: HTTP endpoint definitions for service integration
- **quickstart.md**: Step-by-step developer guide for frontend teams

### 🚧 Current Phase: Implementation Ready
**Status**: All planning phases complete. Foundation components implemented and tested.
**Next Action**: Proceed to `/speckit.tasks` command for detailed implementation task breakdown.

#### Implementation Foundation Completed
- ✅ **Server Foundation** (`isched_server.hpp/cpp`): Core HTTP service with lifecycle management
- ✅ **Tenant Management** (`isched_tenant_manager.hpp/cpp`): Multi-process tenant isolation  
- ✅ **Build System**: CMake + Conan integration with all dependencies resolved
- ✅ **Test Framework**: Comprehensive test coverage with Catch2 integration

#### Ready for Implementation
- 📋 **Database Management Layer**: SQLite integration with connection pooling
- 📋 **GraphQL Query Engine**: PEGTL parser integration with resolver system
- 📋 **Authentication Middleware**: JWT validation and session management
- 📋 **Plugin Loading System**: Binary plugin architecture for GraphQL resolvers
- 📋 **CLI Process Integration**: IPC coordination for script execution

**Planning Phase Status**: **COMPLETE** ✅
