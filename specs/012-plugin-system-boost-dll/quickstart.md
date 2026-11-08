# Quickstart: SP-012 Clarification Alignment

**Branch**: `012-plugin-system-boost-dll`

This quickstart covers implementation work needed to align runtime behavior with clarified requirements for optional resolver schema flow, strict `plugins_root` path policy, and lifecycle admin subscription semantics.

---

## Prerequisites

```bash
cd /home/groby/dev/isched
git checkout 012-plugin-system-boost-dll
cmake --preset conan-release -S . -B build/release
cmake --build build/release --parallel
cd build/release && ctest --output-on-failure
```

---

## Step 1 — Enforce `plugins_root` path policy (FR-022, SC-015)

Implement validation before any dynamic loading in `PluginManager::load_plugin(...)` path:

1. Reject relative `libraryPath`.
2. Canonicalize configured `plugins_root` and incoming `libraryPath`.
3. Reject if canonical library path is outside canonical root.
4. Return validation error in `PluginOperationResult.errors` with no state mutation.

Suggested touch points:

- `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- config/root retrieval code used by `PluginManager`

---

## Step 2 — Implement resolver optional schema branch (FR-021)

For resolver plugins, branch on `graphqlSchema()` output:

- **Non-empty SDL**:
  - Parse SDL.
  - Validate syntax.
  - Detect schema conflicts against core + loaded plugin schemas.
  - Merge schema before resolver registration.
- **Empty SDL string sentinel**:
  - Skip merge.
  - Permit resolver registration only for existing schema fields with no existing resolver.

Any parse/conflict/registration error must fail load without partial activation.

Suggested touch points:

- `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- schema helpers used during resolver registration

---

## Step 3 — Preserve conflict and rollback guarantees (SC-013, SC-014)

Ensure failure paths are fail-fast and side-effect safe:

- No DB write for rejected loads.
- No in-memory route/resolver registration retained on failure.
- No partial schema merge retained when merge/registration fails.

---

## Step 4 — Keep GraphQL contract and auth guards aligned

Confirm resolver setup still enforces:

- `loadPlugin` + `unloadPlugin` restricted to `platform_admin`.
- `plugins` + `plugin(id)` restricted to `platform_admin`.
- `pluginLifecycleEvents` subscription restricted to `platform_admin` **before** stream establishment.
- Plugin management remains `/graphql` only.

---

## Step 5 — Implement lifecycle event publish pipeline (FR-023/FR-024)

Define publish points and mapping in plugin lifecycle transitions:

1. Publish one `LOADED` event only after successful load activation + persistence.
2. Publish one `UNLOADED` event only after successful unload deactivation + persistence.
3. Do not publish for validation/auth/conflict/not-found/idempotent-no-transition paths.
4. Map payload fields from affected plugin record: `eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, `occurredAt`.

---

## Step 6 — Add/Update tests

Update `isched_plugin_system_tests.cpp` (and related auth tests) to cover:

1. **SC-015**: reject relative path and outside-root path.
2. **SC-013(a)**: schema supplied + valid SDL succeeds.
3. **SC-013(b)**: invalid SDL rejected, no partial state.
4. **SC-013(c)**: no-schema flow registers only existing fields.
5. **SC-014**: schema conflicts and no-schema field conflicts reject with no partial state.
6. **SC-017**: successful load/unload each emits exactly one lifecycle event.
7. **SC-018 negative paths**: failed load, conflict reject, unload-not-found, already-unloaded idempotent path emit zero new events.
8. **SC-018 payload checks**: assert all required fields are non-empty and match plugin record values.
9. **SC-017 latency method**: record `(event_received_ts - mutation_completed_ts)` over N>=20 successful trials; compute p95 and assert <=2000ms with >=95% trials within target.

---

## Step 7 — Run performance regression gate (constitution)

Capture baseline and post-change benchmark results for core paths under equivalent hardware/configuration:

1. `PluginManager::dispatch_request` p95 latency
2. `GqlExecutor::execute` for plugin resolver path p95 latency
3. `PluginManager::restore_persisted_plugins` startup duration

Pass criteria: each post-change metric must be **<= 5% regression** vs baseline.

Record raw numbers in feature notes so gate evidence is reviewable.

Reference harness command:

```bash
./tools/benchmarks/plugin_system_perf.sh \
  --build-dir /home/groby/dev/isched/build/release \
  --output /home/groby/dev/isched/specs/012-plugin-system-boost-dll/benchmarks/post-change.json \
  --iterations 20
```

---

## Step 8 — Update security threat-model docs (constitution)

Security documentation is complete only when both are updated:

1. Feature-scoped threat-model update under `specs/012-plugin-system-boost-dll/`
2. Project summary update in `docs/security-threat-model.md`

---

## Step 9 — Verification checklist (operator runbook)

Use this checklist when validating SP-012 behavior on a release candidate:

- [ ] FR-021 schema-supplied path: load resolver plugin with non-empty SDL and confirm queryability.
- [ ] FR-021 no-schema path: load resolver plugin with empty SDL and confirm only pre-existing fields are allowed.
- [ ] FR-022 path policy: verify relative/outside-root/canonicalization-failure paths are rejected before load.
- [ ] FR-023 transition emission: verify successful `loadPlugin` emits exactly one `LOADED` event and successful `unloadPlugin` emits exactly one `UNLOADED` event.
- [ ] FR-024 payload mapping: verify event payload fields map to affected plugin record (`eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, `occurredAt`).
- [ ] FR-025 auth timing: verify unauthorized `pluginLifecycleEvents` subscription attempts fail before stream establishment.
- [ ] SC-017 latency: confirm p95 lifecycle delivery <= 2000ms with >=95% trials within 2s.
- [ ] SC-018 negative paths: confirm failed load/unload non-transition paths publish zero new events.

Record outcomes in `validation-report.md` and keep benchmark artifacts under `benchmarks/`.

Current evidence artifacts:

- `specs/012-plugin-system-boost-dll/benchmarks/baseline.json`
- `specs/012-plugin-system-boost-dll/benchmarks/post-change.json`
- `specs/012-plugin-system-boost-dll/benchmarks/perf-gate-report.md`
- `specs/012-plugin-system-boost-dll/validation-report.md`

---

## Done Criteria

Implementation is ready for `/speckit.tasks` execution when:

- FR-021 and FR-022 behaviors are implemented and test-backed.
- FR-023/FR-024/FR-025 lifecycle subscription behavior is implemented and test-backed.
- SC-013, SC-014, SC-015, SC-017, and SC-018 scenarios are automated.
- Performance benchmark gate passes with <=5% regression on relevant core paths.
- Both feature-scoped and project-level threat-model docs are updated.
- No constitution gate regressions are introduced.
