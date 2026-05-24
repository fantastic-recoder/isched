# Research: Plugin System Gap Closure (SP-012)

**Phase 0 output for plan.md** | Generated: 2026-06-03

All clarification questions for SP-012 were resolved in the 2026-06-03 session (documented in spec.md §Clarifications). No NEEDS CLARIFICATION items remain. This document records the research findings that informed the five architecture decisions in plan.md.

---

## Decision 1 — Authorization Pattern (FR-017)

**Decision**: Use the existing `require_roles(field_name, roles)` mechanism already implemented in `GqlExecutor`. Gate `loadPlugin`, `unloadPlugin`, `plugins`, and `plugin(id)` behind `Role::PLATFORM_ADMIN`.

**Rationale**: `require_roles` is the established project-wide access control pattern (used on `serverMetrics`, `organizations`, `deleteRole`, etc.). It is OR-based, evaluated at resolver dispatch time, and requires no schema changes. Adding four `require_roles` calls is the minimal, consistent change.

**Alternatives considered**:
- GraphQL directive-based auth (`@auth(roles: ["platform_admin"])`): would require schema changes and a directive-aware executor extension. Rejected — disproportionate complexity for four fields.
- Middleware-level check in `isched_Server.cpp`: would not apply to WebSocket GQL transport. Rejected.

---

## Decision 2 — ABI Versioning Strategy (FR-018)

**Decision**: Embed `abi_version` as a semver string in `PluginManifest`. At load time, extract and compare only the major component. Server ABI version constant is `"1.0.0"` (major `1`). Minor/patch differences are accepted silently.

**Rationale**: Semantic versioning with major-only compatibility check is the industry standard for library ABIs. `nlohmann::json` is already present in the codebase — no new dependency needed to parse the version string. The semver major-only rule matches the clarified requirement from the spec.

**Alternatives considered**:
- Integer ABI version (not a string): Simpler parsing, but the spec explicitly says "semantic versioning string (e.g., `1.0.0`)". Rejected to match spec.
- Full semver compatibility check (major.minor): The spec says "minor and patch differences are accepted". Rejected.

---

## Decision 3 — OpenAPI Fragment Validation Timing (FR-019)

**Decision**: Validate `openApiFragmentJson` at load time using `nlohmann::json::parse` with exceptions disabled. Treat empty string as "absent" (always valid). Reject any non-empty, non-parseable string with a descriptive error.

**Rationale**: The spec requires rejection at load time to prevent "partially-loaded state". `nlohmann::json::parse(..., nullptr, false)` is already in use in `PluginSystem.cpp` for the same purpose in `manifest_from_json`. No new API needed. The empty-string-as-absent rule prevents breaking resolver plugins (which legitimately omit the field) and aligns with how `export_openapi_document` already handles the field.

**Alternatives considered**:
- Full OpenAPI 3.x semantic validation: Too deep — spec explicitly says "does not further validate their OpenAPI 3.x semantic correctness". Rejected.
- Validate only on export: Current behaviour — spec says load-time rejection is required. Rejected.

---

## Decision 4 — Path Template Routing Algorithm (FR-006 updated)

**Decision**: Segment-by-segment comparison for both conflict detection and dispatch. A template segment is any segment matching `{…}`. Two routes with the same method conflict if they have the same segment count and every position's pair is either exactly equal or at least one is a template segment. The same rule drives dispatch matching.

**Rationale**: Simple, readable, easily testable. Segment count as a discriminator avoids a common source of false conflicts. The spec says "prefix-overlap" means a static path overlapping with a parameterised path's static prefix — the segment comparison rule captures this: `/a/{id}` conflicts with `/a/latest` because segment 0 `a==a` (equal) and segment 1 `{id}` (template) matches `latest` (static).

The spec also says "the static portion of a parameterised path overlaps with an already-registered static or templated path under the same method." The chosen algorithm satisfies this: two routes with the same method and same segment count that have at least one template segment at any position will always conflict.

**Alternatives considered**:
- Radix tree / trie router (e.g., cpp-httplib or Boost.Beast route matching): Correct but adds structural complexity. The plugin set is bounded and small; O(n²) at load is negligible. Rejected pending profiling evidence.
- Regex-based template matching: More flexible, harder to audit, adds a regex dependency. Rejected.
- Longest-prefix-match semantics: Overly complex; the spec describes a simpler overlap rule. Rejected.

**Limitation documented**: Two routes that differ only by template parameter name (e.g., `GET /a/{itemId}` vs `GET /a/{orderId}`) are correctly flagged as conflicts — they are structurally indistinguishable from the server's routing perspective. This is the correct behaviour per spec.

---

## Decision 5 — Crash Containment Mechanism (FR-020)

**Decision (request plugins)**: Make `dispatch_request` non-const; wrap `handleRequest()` in try/catch; extract a `noexcept` helper `crash_unload_request_plugin`.

**Decision (resolver plugins)**: Introduce `IResolverRegistrar` abstract interface. `CrashSafeRegistrar` implements it by wrapping each registered callable with a try/catch that auto-unloads on exception. `ResolverPlugin::registerResolvers` is updated to accept `IResolverRegistrar&`.

**Rationale**: Resolver callables are invoked inside `GqlExecutor::execute()`. The only practical interception point without modifying `GqlExecutor` internals is at registration time. The `IResolverRegistrar` interface provides a clean polymorphic seam; `CrashSafeRegistrar` is a single-responsibility adapter.  This uses the "prefer polymorphism" principle from the constitution.

The `ResolverPlugin` interface change (from `GqlExecutor&` to `IResolverRegistrar&`) is a necessary C++ API break. It is appropriate now because no external plugins exist at ABI version `1.0.0`.

**Alternatives considered**:
- Post-registration wrapping via `GqlExecutor::replace_resolver`: Would require `GqlExecutor` to expose its internal resolver map. Rejected — increases coupling and violates encapsulation.
- `std::set_terminate` handler: Global; cannot identify the offending plugin. Rejected.
- Signal handlers (`SIGSEGV`) for true crash containment: Platform-dependent, unreliable for C++ exceptions. Rejected — spec says "uncaught exception", not OS-level crash.
- Making `GqlExecutor` a template parameter for the executor type (CRTP): Solves the problem but complicates the entire plugin type hierarchy. Rejected in favour of the runtime interface.

