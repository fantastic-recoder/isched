# Implementation Plan: Universal Application Server Backend

**Branch**: `001-universal-backend` | **Date**: 2025-11-01 | **Spec**: [Universal Application Server Backend](spec.md)
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

✅ **High Performance**: Multi-process architecture with 20ms response targets and tenant isolation
✅ **GraphQL Compliance**: PEGTL parser implementation ensures full GraphQL specification compliance
✅ **Security-First**: JWT/OAuth authentication with process isolation and binary plugin sandboxing
✅ **Test-Driven Development**: Comprehensive test strategy with unit, integration, and performance tests
✅ **Cross-Platform Portability**: C++23 with Conan dependencies for Linux primary, cross-platform support
✅ **C++ Core Guidelines**: Smart pointer usage (std::unique_ptr, std::shared_ptr) for all resource management, Doxygen documentation for all public APIs

**Post-Phase 1 Validation**: ✅ All constitutional requirements satisfied with detailed design including mandatory smart pointer usage and comprehensive documentation generation.

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
src/
├── main/
│   └── cpp/
│       └── isched/
│           ├── backend/                    # Universal backend implementation
│           │   ├── isched_server.hpp/cpp  # Doxygen documented
│           │   ├── isched_tenant_manager.hpp/cpp  
│           │   ├── isched_database.hpp/cpp
│           │   ├── isched_graphql_executor.hpp/cpp
│           │   ├── isched_resolver_system.hpp/cpp
│           │   └── isched_cli_coordinator.hpp/cpp
│           ├── cli/                        # Enhanced CLI with backend commands
│           │   └── isched_backend_commands.hpp/cpp
│           ├── runtime/                    # Shared dynamic library
│           │   ├── isched_runtime.hpp/cpp
│           │   ├── isched_ipc.hpp/cpp
│           │   └── isched_plugin_api.hpp/cpp
│           └── [existing files...]         # Current isched implementation
├── cli-python/                            # Python CLI executable
│   ├── isched_cli_python.cpp
│   └── python_script_executor.hpp/cpp
└── cli-typescript/                        # TypeScript CLI executable
    ├── isched_cli_typescript.cpp
    └── typescript_script_executor.hpp/cpp

docs/                                      # Generated documentation
├── api/                                   # API reference (Doxygen)
├── source/                                # Source code with examples
├── guides/                                # Developer guides
└── examples/                              # Complete working examples

tests/
├── backend/                               # Backend unit tests
├── integration/                           # Cross-component tests
├── performance/                           # 20ms response time validation
└── plugins/                              # Plugin system tests
```

**Structure Decision**: Extend existing Maven layout with backend, runtime library, separate CLI executables, and comprehensive documentation generation. Maintain isched::v0_0_1 namespace and smart pointer usage throughout.

## Complexity Tracking

> **No constitutional violations identified**

All requirements align with constitutional principles:

- High performance: 20ms response targets with multi-tenant architecture
- GraphQL compliance: PEGTL parser ensures specification adherence  
- Security-first: JWT/OAuth authentication with process isolation
- TDD: Comprehensive test strategy with performance validation
- Cross-platform: C++23 with Conan dependencies
- C++ Core Guidelines: Smart pointer usage and Doxygen documentation generation
