<!--
Sync Impact Report:
- Version change: 2.1.0 → 2.2.0
- Modified principles:
  - VI. Angular WebUI Engineering (NON-NEGOTIABLE) → VI. Angular WebUI Engineering (NON-NEGOTIABLE) (expanded with explicit app-owned state rule)
- Added sections:
  - None
- Removed sections: None
- Templates requiring updates:
  ✅ .specify/templates/plan-template.md (Constitution Check now forbids async-pipe-driven app-owned UI state)
  ✅ .specify/templates/spec-template.md (frontend constitutional requirements now forbid async-pipe-driven app-owned UI state)
  ✅ .specify/templates/tasks-template.md (WebUI implementation/testing tasks now include signal-backed template-state verification)
  ✅ `.specify/templates/commands/*.md` (not present; no updates required)
  ✅ AGENTS.md (Angular guidance now explicitly forbids async-pipe-driven app-owned UI state)
  ✅ .github/copilot-instructions.md (Angular guidance now explicitly forbids async-pipe-driven app-owned UI state)
  ✅ README.md (reviewed; no constitution reference change required)
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

Authentication and authorization mechanisms MUST be implemented using industry-standard protocols (OAuth, JWT). All user data handling MUST follow security best practices. Default configurations MUST be secure-by-default. Browser clients MUST treat JWTs as sensitive credentials: access tokens MUST NOT be persisted in `localStorage`, `sessionStorage`, or IndexedDB; secure `HttpOnly` `SameSite` cookies are the default transport, and any exception MUST be documented with compensating controls.

**Rationale**: As a "batteries included" backend, Isched eliminates the need for additional authentication services. Security cannot be an afterthought in a multi-tenant environment.

### IV. Test-Driven Development

TDD remains the preferred engineering practice for all core functionality. Automated tests covering the intended behavior MUST exist before a story or core capability is considered complete. Planning artifacts and task lists are not required to place test tasks before implementation tasks, but the final delivered change MUST include the necessary passing verification coverage. Integration tests are required for GraphQL endpoints, database transactions, and authentication flows. Performance tests MUST validate scalability claims.

**Rationale**: High-performance multi-tenant systems require bulletproof reliability. TDD ensures functionality works correctly under all supported conditions.

### V. Cross-Platform Portability

Code MUST compile and run on Linux, with clear documentation for other platforms. Dependencies MUST be manageable via Conan. Build system MUST support both high-end development environments and resource-constrained embedded targets.

**Rationale**: Supporting cloud-to-embedded deployment requires portable code. Build complexity must not prevent adoption on target platforms.

### VI. Angular WebUI Engineering (NON-NEGOTIABLE)

All Isched WebUI work MUST use modern Angular conventions. State management MUST be signal-first, with RxJS used only where stream semantics are required. Application-owned UI state rendered by templates MUST be sourced from signals or signal-derived view models; async-pipe-driven template state from long-lived component-owned observables is prohibited unless adapting immutable third-party stream APIs, and any exception MUST be documented in the feature spec. New UI building blocks MUST be standalone components/directives/pipes by default. Templates MUST use modern control-flow syntax (`@if`, `@for`, `@switch`) for new code. Forms MUST use typed reactive forms. TypeScript compilation MUST stay in strict mode (including strict template checking). UI code MUST prefer zoneless- and `OnPush`-compatible patterns where feasible, avoiding reliance on implicit Zone.js side effects. All frontend API access MUST go through GraphQL at `/graphql` (HTTP and WebSocket), with no REST fallbacks.

**Rationale**: The new WebUI is a first-class product surface. Enforcing modern Angular patterns preserves long-term maintainability, predictable performance, and architectural consistency with Isched's GraphQL-only platform contract.

## Technical Standards

**Language**: C++23 standard compliance required. Contributions that lower the language level are prohibited unless the constitution is amended first.

**C++ Core Guidelines**: All C++ code MUST adhere to the [ISO C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). Code reviews MUST verify guideline compliance. Deviations require explicit justification and documentation in code comments.

**Dependencies**: All external dependencies MUST be managed via Conan. Direct system dependencies require justification and documentation.

**Database**: Embedded database implementation MUST support ACID transactions. Multi-tenant data isolation is mandatory.

**Threading**: Multi-threaded code MUST be thread-safe. Shared state requires explicit synchronization strategy documentation.

**Memory Management**: RAII principles mandatory. Memory leaks in multi-tenant environment are critical bugs.

### Angular WebUI

Frontend work MUST use Angular standalone APIs and strict TypeScript settings. State consumed by templates for application-owned UI behavior MUST come from signals (or signal-derived selectors/view models); async-pipe-driven template state over component-owned observables is disallowed except when bridging immutable third-party stream contracts with explicit documentation. New templates MUST use `@if`, `@for`, and `@switch` instead of legacy structural-directive microsyntax unless integrating immutable third-party code. Typed reactive forms are mandatory for user input flows, particularly authentication and administration screens. Local development MUST use an Angular dev-server proxy that forwards `/graphql` HTTP and WebSocket traffic to the backend origin; frontend source code MUST avoid hard-coded backend hostnames for dev mode.

## Development Workflow

**Code Review**: All changes require review with explicit verification of constitution compliance.

**Performance Testing**: Changes affecting core paths require performance regression testing.

**Documentation**: Public APIs require comprehensive documentation. Security-related features require both a feature-scoped threat model in the relevant `specs/[feature]/` directory and a summarized project-wide threat model entry in `docs/security-threat-model.md`.

**Versioning**: Semantic versioning with special attention to GraphQL schema breaking changes.

**Frontend Verification**: Any feature touching `src/ui/` MUST include automated frontend verification (unit/component and relevant integration coverage), plus checks that GraphQL-only transport and JWT handling rules remain compliant.

## Governance

This constitution supersedes all other development practices. All pull requests and code reviews MUST verify compliance with these principles.

Amendments require: (1) Documentation of proposed changes, (2) Impact analysis on existing codebase and templates, (3) Migration plan for any breaking changes, and (4) Approval through standard review process.

Versioning policy for this constitution follows semantic versioning: MAJOR for backward-incompatible governance changes or principle removals/redefinitions, MINOR for new principles or materially expanded guidance, PATCH for non-semantic clarifications.

Compliance reviews are mandatory at plan, implementation, and review time; any non-compliance MUST be tracked as a blocking issue or approved via explicit, documented exception.

Constitution violations are critical issues requiring immediate resolution.

**Version**: 2.2.0 | **Ratified**: 2025-11-01 | **Last Amended**: 2026-04-05
