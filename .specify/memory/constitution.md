<!--
Sync Impact Report:
- Version change: 1.3.0 → 2.0.0
- Modified principles: None
- Added sections: None
- Removed sections: None
- Templates requiring updates:
  ✅ .specify/templates/plan-template.md (Constitution Check + technical context wording updated for C++23 baseline)
  ✅ .specify/templates/tasks-template.md (setup task examples updated for C++23 + Conan/CMake baseline)
  ✅ .specify/templates/spec-template.md (reviewed; no language-version wording change required)
  ✅ .specify/templates/constitution-template.md (reviewed; generic placeholders remain valid)
  ✅ `.specify/templates/commands/*.md` (not present; no updates required)
  ✅ README.md (reviewed; already aligned to C++23)
- Follow-up TODOs: None
-->

# Isched Constitution

## Core Principles

### I. High Performance (NON-NEGOTIABLE)

All code MUST be designed for high-performance multi-tenant operation. Performance considerations are mandatory for every feature implementation. Code MUST support operation from cloud server hardware down to embedded hardware without degradation of core functionality.

**Rationale**: Isched targets massive parallel operation across diverse hardware environments. Performance is not optional but fundamental to the project's value proposition.

### II. GraphQL Specification Compliance (NON-NEGOTIABLE)

All GraphQL implementations MUST strictly conform to the official [GraphQL specification](https://spec.graphql.org/). Any deviation requires explicit documentation and justification. Custom extensions MUST be clearly marked as non-standard.

**Rationale**: Specification compliance ensures interoperability and prevents vendor lock-in. Clients should be able to rely on standard GraphQL behavior.

### III. Security-First

Authentication and authorization mechanisms MUST be implemented using industry-standard protocols (OAuth, JWT). All user data handling MUST follow security best practices. Default configurations MUST be secure-by-default.

**Rationale**: As a "batteries included" backend, Isched eliminates the need for additional authentication services. Security cannot be an afterthought in a multi-tenant environment.

### IV. Test-Driven Development

TDD remains the preferred engineering practice for all core functionality. Automated tests covering the intended behavior MUST exist before a story or core capability is considered complete. Planning artifacts and task lists are not required to place test tasks before implementation tasks, but the final delivered change MUST include the necessary passing verification coverage. Integration tests are required for GraphQL endpoints, database transactions, and authentication flows. Performance tests MUST validate scalability claims.

**Rationale**: High-performance multi-tenant systems require bulletproof reliability. TDD ensures functionality works correctly under all supported conditions.

### V. Cross-Platform Portability

Code MUST compile and run on Linux, with clear documentation for other platforms. Dependencies MUST be manageable via Conan. Build system MUST support both high-end development environments and resource-constrained embedded targets.

**Rationale**: Supporting cloud-to-embedded deployment requires portable code. Build complexity must not prevent adoption on target platforms.

## Technical Standards

**Language**: C++23 standard compliance required. Contributions that lower the language level are prohibited unless the constitution is amended first.

**C++ Core Guidelines**: All C++ code MUST adhere to the [ISO C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). Code reviews MUST verify guideline compliance. Deviations require explicit justification and documentation in code comments.

**Dependencies**: All external dependencies MUST be managed via Conan. Direct system dependencies require justification and documentation.

**Database**: Embedded database implementation MUST support ACID transactions. Multi-tenant data isolation is mandatory.

**Threading**: Multi-threaded code MUST be thread-safe. Shared state requires explicit synchronization strategy documentation.

**Memory Management**: RAII principles mandatory. Memory leaks in multi-tenant environment are critical bugs.

## Development Workflow

**Code Review**: All changes require review with explicit verification of constitution compliance.

**Performance Testing**: Changes affecting core paths require performance regression testing.

**Documentation**: Public APIs require comprehensive documentation. Security-related features require both a feature-scoped threat model in the relevant `specs/[feature]/` directory and a summarized project-wide threat model entry in `docs/security-threat-model.md`.

**Versioning**: Semantic versioning with special attention to GraphQL schema breaking changes.

## Governance

This constitution supersedes all other development practices. All pull requests and code reviews MUST verify compliance with these principles.

Amendments require: (1) Documentation of proposed changes, (2) Impact analysis on existing codebase, (3) Migration plan for any breaking changes, (4) Approval through standard review process.

Constitution violations are critical issues requiring immediate resolution.

**Version**: 2.0.0 | **Ratified**: 2025-11-01 | **Last Amended**: 2026-04-04
