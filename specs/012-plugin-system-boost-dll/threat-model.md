# Threat Model: SP-012 Plugin System (Boost.DLL)

**Updated**: 2026-05-25

## Scope

GraphQL-only plugin administration (`loadPlugin`, `unloadPlugin`, `plugins`, `plugin`, `pluginLifecycleEvents`), runtime request/resolver plugin execution, persisted restore, and lifecycle event publication.

## Trust Boundaries

- External admin client -> GraphQL server (`/graphql` HTTP + WebSocket)
- GraphQL server -> local filesystem (`plugins_root`)
- GraphQL server -> SQLite plugin registry (`plugin_registry`)
- In-process server runtime -> untrusted native plugin DLL/SO code
- In-process lifecycle transition source -> subscription broadcaster

## Abuse Cases and Mitigations

| Threat | Attack Path | Mitigation | Residual Risk |
|---|---|---|---|
| Unauthorized plugin management | Non-admin user invokes plugin queries/mutations | `require_roles(platform_admin)` on `plugins`, `plugin(id)`, `loadPlugin`, `unloadPlugin` | Misconfigured role assignment can still grant broad power |
| Unauthorized lifecycle subscription | Non-admin user opens subscription stream | FR-025 pre-stream gate: `require_roles(platform_admin)` before stream establishment | None beyond RBAC correctness |
| Path traversal / symlink escape | Admin supplies `libraryPath` outside allowed root | FR-022 fail-fast absolute + canonical containment under configured `plugins_root` before load | Privileged admin can still load any native code under allowed root |
| Malformed plugin metadata | Plugin returns invalid `openApiFragmentJson` or ABI mismatch | FR-018 ABI major check + FR-019 strict JSON parse at load | ABI-compatible plugin can still be malicious |
| Route/resolver takeover | New plugin attempts conflicting route/resolver/schema ownership | Deterministic conflict checks; FR-021 schema/no-schema gates; fail without partial state | Logical plugin behavior may still be harmful even when non-conflicting |
| Runtime exception instability | Plugin request/resolver throws unexpectedly | FR-020 containment: catch + auto-unload + persisted unload state | Does not sandbox non-exception side effects inside plugin code |
| Event stream misinformation | Events emitted on failed/non-transition operations | FR-023/FR-024 publish only once after successful transition commit | Clock skew affects `occurredAt` interpretation only |
| Persistence poisoning on failed load | Partial DB writes despite load failure | Fail-fast order; persist only after successful activation path | DB corruption outside app control not addressed here |

## STRIDE Snapshot

- **Spoofing**: mitigated by existing auth/session validation and role checks.
- **Tampering**: constrained by path policy, schema/conflict checks, and controlled persistence APIs.
- **Repudiation**: lifecycle transitions and restore failures are logged with plugin id context.
- **Information Disclosure**: plugin metadata is admin-only; no new public API surface added.
- **Denial of Service**: malicious plugins can still consume resources; crash containment limits exception blast radius only.
- **Elevation of Privilege**: primary control is strict `platform_admin` gating.

## Required Operational Controls

1. Restrict write access to `plugins_root` to trusted operators only.
2. Keep `platform_admin` assignments tightly audited.
3. Treat plugin binaries as privileged software supply-chain artifacts.
4. Monitor lifecycle load/unload events and restore-failure warnings.
5. Re-run plugin-system regression and subscription integration suites before release.

## Residual Risks

- Native plugin code executes in-process with server privileges.
- A compromised admin account can intentionally load hostile code.
- Crash containment does not provide memory/process sandboxing.

## Validation Evidence

- `ctest --output-on-failure` (build/release) PASS (55/55).
- Plugin subscription integration suite PASS, including SC-017/SC-018 acceptance checks.
- Performance gate artifacts captured under `specs/012-plugin-system-boost-dll/benchmarks/` with <=5% p95 regressions.

