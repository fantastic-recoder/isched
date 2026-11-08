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

---

## Decision 6 — Resolver plugin schema is an explicit two-path load flow (FR-021)

**Decision**: Treat resolver loading as two mutually exclusive paths:

1. **Schema-Supplied Path**: `graphqlSchema()` returns non-empty SDL -> parse -> syntax validate -> conflict check against core + loaded plugin schema -> merge schema -> register resolvers.
2. **No-Schema Path**: `graphqlSchema()` returns empty string sentinel -> skip merge -> allow resolver registration only for already-defined schema fields with no existing resolver binding.

**Rationale**: The clarification explicitly allows optional schema supply. Modeling this as two explicit branches removes ambiguity and makes rollback behavior deterministic.

**Alternatives considered**:
- Always require SDL schema: rejected (contradicts clarified requirement).
- Always attempt merge from manifest metadata (`graphqlSchemaSdl`) only: rejected (runtime contract clarifies `graphqlSchema()` method flow).

---

## Decision 7 — Conflict checks are staged and fail-fast before activation (FR-021, SC-013, SC-014)

**Decision**: Apply fail-fast validation gates in this order for resolver plugins:

1. Path policy + plugin identity validation
2. ABI + manifest JSON validation
3. Optional schema parse/validate/merge checks (only when schema is supplied)
4. Resolver binding preflight checks (field exists / no existing resolver)
5. Resolver registration
6. Persistence + in-memory activation

Any failure before step 6 returns a descriptive error and leaves runtime/persisted state unchanged.

**Rationale**: This prevents partial registration or partial schema mutation, directly satisfying the no-partial-state expectations in FR-021 and SC-014.

**Alternatives considered**:
- Register resolvers first, then validate schema: rejected (can produce partial mutable state and complicated rollback).
- Persist first, then validate/activate: rejected (violates SC-015/SC-014 no-state-mutation-on-failure semantics).

---

## Decision 8 — `plugins_root` policy uses canonical containment checks (FR-022, SC-015)

**Decision**: Validate `LoadPluginInput.libraryPath` with canonicalized filesystem paths before dynamic loading:

- Require input path to be absolute.
- Resolve both `libraryPath` and configured `plugins_root` using canonical/weakly-canonical semantics.
- Reject if resolved library path is not contained by resolved root path (prefix containment on path components, not string-only comparison).
- Reject on resolution failures (nonexistent path, permission error, malformed path) as path-policy validation errors.

**Rationale**: Canonical containment prevents bypasses via `..`, symlink indirection, or path normalization tricks while preserving clear operator behavior.

**Alternatives considered**:
- Absolute-path-only check: rejected (does not enforce root containment).
- Raw string prefix check: rejected (unsafe with symlinks and equivalent-path variants).

---

## Decision 9 — Contract-level error surface must distinguish schema and path-policy failures

**Decision**: Keep GraphQL response shape unchanged (`PluginOperationResult`) but standardize error categories/messages for:

- Path-policy violations (relative/outside-root/unresolvable path)
- Schema parse errors (invalid SDL)
- Schema merge conflicts (duplicate/conflicting type/field definitions)
- No-schema registration failures (`field not found`, existing resolver conflict)

**Rationale**: Deterministic errors are necessary for admin automation and for testing SC-013/SC-014/SC-015.

**Alternatives considered**:
- Generic `load failed` error: rejected (insufficient for operator troubleshooting and acceptance criteria verification).

---

## Decision 10 — Keep management interface GraphQL-only

**Decision**: No new REST management API is introduced. All load/unload/list/get management operations remain GraphQL resolvers guarded by `platform_admin` role checks.

**Rationale**: Required by constitution and existing feature contract; avoids transport divergence.

**Alternatives considered**:
- Add HTTP admin endpoint under `/plugin/admin/*`: rejected (constitution violation and duplicate surface).

---

## Decision 11 — Lifecycle event publication occurs only on successful state transitions (FR-023/FR-024)

**Decision**: Publish plugin lifecycle subscription events from the same mutation transaction boundary that marks runtime+persisted lifecycle transitions as successful:

- `LOADED` publishes once after `loadPlugin` successfully activates plugin state and persists `is_loaded = true`.
- `UNLOADED` publishes once after `unloadPlugin` successfully deactivates plugin state and persists `is_loaded = false`.
- Any failure before transition completion (auth/validation/conflict/not-found/idempotent unload) publishes nothing.

**Rationale**: Transition-sourced emission guarantees exact-once semantics per successful lifecycle transition and naturally enforces "no event on failure" constraints in FR-024/SC-018.

**Alternatives considered**:
- Emit at mutation entry: rejected (would emit false-positive events on later failure paths).
- Emit from DB trigger polling: rejected (extra operational complexity and weaker in-process ordering guarantees).

---

## Decision 12 — Lifecycle subscription auth gate executes before stream establishment (FR-025)

**Decision**: Apply `require_roles(..., platform_admin)` at subscription resolver entry before creating/returning any event stream cursor.

**Rationale**: FR-025 requires unauthorized callers to be rejected before stream establishment. Resolver-entry gating is the earliest consistent point across GraphQL transports.

**Alternatives considered**:
- Check auth after stream allocation: rejected (violates FR-025 timing requirement).
- Rely on perimeter auth only: rejected (insufficient defense-in-depth and inconsistent with existing field-level auth policy).

---

## Decision 13 — SC-017 latency measurement method uses mutation-complete to event-receive delta

**Decision**: For lifecycle subscription latency acceptance, measure `(subscriber_received_timestamp - mutation_completion_timestamp)` per successful load/unload trial, then compute p95 over >=20 trials. Pass condition: p95 <= 2000ms with >=95% of successful events inside 2s.

**Rationale**: This directly matches operator-visible end-to-end responsiveness from admin action completion to subscriber observability.

**Alternatives considered**:
- Measure broker enqueue time only: rejected (does not represent client-observed latency).
- Single-run timing check: rejected (insufficient statistical confidence for 95th-percentile target).

