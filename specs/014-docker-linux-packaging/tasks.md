# Tasks: Dockerized Linux Package Builds

**Input**: `/home/groby/dev/isched/specs/014-docker-linux-packaging/` — `spec.md`, `plan.md`, `research.md`, `data-model.md`, `contracts/package-artifact-contract.md`, `quickstart.md`, and `threat-model.md`; repository constitution `.specify/memory/constitution.md` v2.3.0.
**Branch**: `014-docker-linux-packaging`
**Organization**: Setup → Foundation → US1 (P1) → US2 (P2) → US3 (P3) → Polish.
**Tests**: Explicitly required by the user, acceptance scenarios, and constitution. Test authoring precedes the corresponding implementation; passing automated coverage, full CTest, installed-runtime checks, and SDK consumer build/link/run are completion gates, not optional build-existence checks.

## Execution rules and design issues

- This document is an implementation plan, not implementation evidence. All tasks start unchecked. Paths below are relative to repository root `/home/groby/dev/isched`; this document's absolute path is `/home/groby/dev/isched/specs/014-docker-linux-packaging/tasks.md`.
- Preserve the existing dirty planning/context files and any unrelated changes. Do not rerun setup-plan, regenerate the plan, archive the feature, commit, push, merge, or publish releases. The parent handles the final artifact commit; **100% of the full CTest suite must be green before any eventual commit**.
- Initial production targets stay exactly Ubuntu 26.04 LTS x86_64 DEB and Fedora 45 x86_64 RPM. Each produces `isched-runtime` and `isched-devel`; the developer artifact role label is `developer`, while its CPack component is `devel`.
- Read `plan.md` and `research.md` for the recorded amd64 image digests, compiler package versions, Conan 2.34.0, Node.js 24.22.2 checksum, pnpm 10.30.3, CMake 3.22.6, and Ninja 1.12.1. These are supplied planning inputs, **not verified by task generation**. Missing image/package availability, unsupported Conan compiler settings, incompatible dependency recipes, or failed compilation are explicit blockers; do not float pins, downgrade targets, or change dependencies silently.
- The plan says there are no blockers, but OS repository snapshots/additional OS package pins, the Conan Python dependency lock, target compiler/recipe compatibility, the exact exported-header/link closure, and native fallback-version encoding remain implementation work requiring evidence. T001 records these qualifications without overwriting the plan.
- `quickstart.md` references `tools/package/validate-artifacts.sh` while the plan locates helpers in `packaging/scripts/`. Keep the implementation in the latter and provide the documented wrapper in T027.
- Canonical identity version remains `0.0.0+git.<short-sha>` for non-tag builds. DEB/RPM native version/release fields may require a deterministic format-valid encoding (RPM must not blindly receive an invalid `+` version). Preserve the canonical version and full revision in embedded/accompanying metadata; document and test any encoding, rather than changing version policy.
- The plan/contract/quickstart specify SDK compile/link, but the user additionally requires **run**. T011/T023/T030 cover execution of a client that actually calls a library symbol. No GraphQL/auth/UI runtime behavior changes are authorized.
- The build revision must equal the actual checked-out source. For PR validation, distinguish GitHub's merge SHA from head SHA and use the checkout revision consistently; do not label merge-checkout packages with an unrelated PR head SHA.
- SC-005 requires the project's single contributor to verify all four initial Ubuntu/Fedora target × runtime/developer role selections, identifying distribution, release, architecture, format, and role within one minute per selection from visible artifact metadata only. No recruitment is required. Hosted workflow/artifact evidence requires an authorized run and access. Record unavailable evidence as pending/blocking, never manufacture results or push merely to obtain it.
- A future-target experiment for US3 is temporary, uses a separately verified supported release and pins, and does not expand the committed initial production matrix. Unsupported/duplicate target definitions fail explicitly.

## Format and path conventions

Every task uses `- [ ] Tnnn [P?] [USn?] Description with file path`.
`[P]` means the task can run beside other eligible tasks on different files **after its listed prerequisites are complete**. Only story phases carry `[US1]`, `[US2]`, or `[US3]`.

New packaging helpers belong in `packaging/scripts/`; SDK inputs in `packaging/sdk/`; fixtures/tests in `tests/package/`; evidence in `specs/014-docker-linux-packaging/checklists/`. Do not initialize a new project or introduce an application database/API.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish execution boundaries and a clean Docker source context without modifying the existing planning artifacts.

- [ ] T001 Record the design issues above, verification commands for supplied pins/compiler support, native-version encoding decision criteria, constitution justification for OS/build-tool dependencies, and human/hosted evidence prerequisites in `specs/014-docker-linux-packaging/checklists/implementation-readiness.md`; preserve `plan.md`, `research.md`, and dirty context files and stop affected work if a required pin cannot be verified.
- [ ] T002 Add or update `.dockerignore` to exclude host build trees, `node_modules`, UI dist/generated embedded assets, Conan caches, package output directories, application databases, credentials, and local state while retaining tracked build inputs and license sources; establish explicit revision/version build arguments so Git metadata is not required inside the clean Docker context.
- [ ] T003 [P] Create `packaging/README.md` documenting the directory layout, target/role contract, intended shared script CLI, evidence locations, no-host-install/no-seed rules, and no-commit execution boundary; reference the feature artifacts instead of duplicating or rewriting them.

**Checkpoint**: Clean context and implementation boundaries are documented.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared target identity and locked build inputs used by every story.
**Gate**: Finish this phase before story work; no target substitution on failure.

- [ ] T004 Define `packaging/targets.json` with exactly two initial tuples, distro/release/native and artifact architectures, format, role mapping, recorded image digest, Dockerfile path, explicit C/C++ compiler/version, and dated repository inputs or exact OS package pins; validate supplied references and reject unresolved pins rather than using latest aliases.
- [ ] T005 [P] Create `packaging/requirements-conan.lock` with Conan 2.34.0 and its exact Python dependency versions/hashes, pin any Python test/YAML-parser dependencies in `packaging/requirements-tests.lock`, and record interpreter compatibility and checksum-verified Node/pnpm bootstrap inputs in `packaging/tool-versions.json`; verify the research checksum/version inputs and retain CMake/Ninja tool-requires rather than implicitly selecting host tools.
- [ ] T006 [P] Generate the complete recipe-revision-locked build graph in `conan.lock` using `conanfile.txt` and add explicit Release/C++23/architecture/compiler/libstdc++ ABI host profiles in `packaging/conan/profiles/ubuntu-26.04` and `packaging/conan/profiles/fedora-45`; verify both target graphs with Conan 2.34.0 and report unsupported compiler versions or recipe incompatibilities instead of altering requirements silently.
- [ ] T007 [P] Implement `packaging/scripts/package_identity.py` to validate target tuples, role mapping, full source SHA, exact-at-HEAD `vMAJOR.MINOR.PATCH` tags, canonical fallback version, deterministic native DEB/RPM version/release encoding, and stable artifact/package filenames; emit machine-readable identity and fail on malformed, duplicate, or inconsistent values without shell evaluation of inputs.
- [ ] T008 Extend `configure.py` with narrowly scoped optional lockfile/profile/CMake-argument support needed by packaging while preserving ordinary debug/release behavior; apply explicit target compilers/C++23, activate Conan-generated build environment tools so pinned CMake/Ninja are actually used, and propagate Conan/configure/build failures rather than relying on best-effort profile detection for package builds.

**Checkpoint**: Both target graphs and build input identities are defined; unresolved tool/registry assumptions are tracked as blockers.

## Phase 3: User Story 1 — Build packages for the initial Linux targets (Priority: P1) — MVP

**Goal**: Build the UI/backend from clean target-specific Docker environments, run all tests, produce both native package roles, prove installability/runtime closure/SDK usability, and report independent failures without a false-green matrix.

**Independent Test**: Run the shared local Docker path for both initial targets and inspect a workflow run using that same path. Both targets must produce two validated native packages after Angular verification and full CTest. Install runtime packages in separate clean target containers with no build toolchain; assert parsed GraphQL health and embedded HTML plus a referenced UI asset. Install each SDK independently, provision only its declared Conan dependencies, and compile/link/run an external C++23 client using `isched::isched`. Inject one-target build/test/install/SDK/no-output failures: the target and overall workflow fail while the other target remains observable.

### Tests for User Story 1

- [ ] T009 [P] [US1] Add native package contract tests in `tests/package/test_native_packages.py` for both formats/roles, required payloads/licenses, native version encoding, exact revision/release/architecture, derived dependency metadata, no role-to-role SDK dependency, and rejection of empty/missing/duplicate/misnamed/internal-format-mismatched/state-contaminated outputs; use disposable native packages and target-native tooling, not suffix-only fixtures.
- [ ] T010 [P] [US1] Add runtime integration coverage in `tests/package/test_runtime_install.sh` that invokes the clean-runtime verifier for each target, asserts native install succeeds without development packages, no unresolved ELF dependency, bounded startup, parsed existing GraphQL health response without errors, embedded UI HTML and referenced asset content, and cleanup/nonzero failure for install/start/health/UI/loader failures; use temporary data without `--seed`.
- [ ] T011 [P] [US1] Create external SDK fixtures in `tests/package/sdk-consumer/CMakeLists.txt` and `tests/package/sdk-consumer/main.cpp`, plus `tests/package/test_sdk_install.sh`, to include `<isched/isched.hpp>`, find installed `isched CONFIG REQUIRED`, link `isched::isched`, call a real existing library API and return a checked result, and run successfully without runtime package/source-tree/build-cache access; include missing-header/dependency/export and leaked-build-path failures.
- [ ] T012 [P] [US1] Add orchestration failure tests in `tests/package/test_build_pipeline.py` using controlled command adapters/fixtures for image pull/start, frozen frontend install/test/build, Conan/configure/build, CTest, package generation, validation, clean install, and SDK failures; assert nonzero exits, target attribution, and no exported successful packages at any failed gate.

### Implementation for User Story 1

- [ ] T013 [P] [US1] Inventory the supported public include closure starting at `src/main/cpp/isched/isched.hpp` through `backend/isched_Server.hpp`, `backend/isched_common.hpp`, and shared configuration headers in `packaging/sdk/public-headers.cmake`; preserve `isched/...` layout, document public third-party include requirements, and exclude generated UI/internal schema headers unless genuinely required by the public API.
- [ ] T014 [US1] Update `src/main/cpp/isched/CMakeLists.txt` to install the existing static `isched` archive and supported headers as `devel`, install `isched_srv` as `runtime`, export namespace `isched::` target `isched`, and express C++23, relocatable BUILD_INTERFACE/INSTALL_INTERFACE include paths and complete static-library link requirements without absolute source/build/Conan-cache paths; preserve default linkage and runtime behavior.
- [ ] T015 [US1] Add `packaging/sdk/ischedConfig.cmake.in` with the actual `find_dependency` names/components needed by the exported link graph, CMake package/version initialization and exported targets import; resolve through consumer-generated Conan CMakeDeps targets, including OpenSSL/Boost components, without requiring Catch2 or the producer build tree.
- [ ] T016 [US1] Add `packaging/sdk/conanfile.txt`, `packaging/sdk/conan.lock`, and `packaging/sdk/README.md` for the exact public/link transitive dependency graph, matching target compiler/ABI/options and consumer build environment activation; exclude producer-only test dependencies, explicitly declare required development tooling and system-link requirements, and explain why the SDK does not depend on `isched-runtime`.
- [ ] T017 [US1] Implement `packaging/scripts/collect_licenses.py` to inventory linked Conan dependencies and embedded Angular production notices, collect `LICENSE.md` plus relevant third-party notices for each role, and fail on missing inventories/notices; distinguish distributed content from test/tool-only dependencies and record maintainer review needs without inventing license conclusions.
- [ ] T018 [US1] Add `packaging/cmake/Package.cmake` configuring CPack DEB/RPM component packages, valid target-native package metadata/version fields, full-revision/target/role metadata, GNUInstallDirs payload paths, license/docs components and distinct output names; derive DEB shared-library dependencies using `dpkg-shlibdeps` and RPM ELF requirements, and do not invent a developer-to-runtime dependency or ship service/data/seed state.
- [ ] T019 [US1] Wire packaging opt-in, identity inputs, GNUInstallDirs, SDK config/version generation, header/export installation, licenses and CPack into `CMakeLists.txt`; ensure required dependency discovery matches the actual library graph and package mode fails if pnpm or clean UI generation is unavailable rather than accepting the current warning/stale generated-header fallback.
- [ ] T020 [US1] Implement `packaging/scripts/build-target.sh` to validate inputs/tool versions, select explicit Conan profiles and lockfile, perform `pnpm install --frozen-lockfile`, run `configure.py release` in the pinned Conan build environment, require fresh Angular test/build/embed outputs, execute the complete Release CTest suite without regex filtering and reject zero registered tests, then generate both components and licenses; every failed stage exits nonzero with target-qualified diagnostics.
- [ ] T021 [US1] Implement `packaging/scripts/validate-packages.sh` using `dpkg-deb`/RPM native tools and ELF inspection to enforce T009's format/name/version/revision/role/release/native-architecture/required-path/license/dependency checks, exactly one package per role, explicit payload allowlists and no source/cache/test/tool/database/credential/seed payload; report validation results and never infer identity from execution order.
- [ ] T022 [US1] Implement `packaging/scripts/verify-runtime.sh` for fresh same-release verification stages without compiler/Conan/Node/development packages: install `isched-runtime` using the native package manager and declared dependencies, check loader closure, start `/usr/bin/isched_srv` with isolated disposable data, assert existing `/graphql` health JSON and embedded UI/asset bytes with bounded waits, preserve failure diagnostics, and terminate only the owned process and clean temporary state.
- [ ] T023 [US1] Implement `packaging/scripts/verify-sdk.sh` to install only `isched-devel` in a fresh same-target consumer environment, provision locked dependencies from the installed manifest with the matching compiler ABI, configure/build/link/run T011's client through installed `find_package`/`isched::isched`, test the documented installation layout and a relocated staged SDK prefix, and reject source/build/cache path leakage or accidental runtime-package requirements.
- [ ] T024 [P] [US1] Create `packaging/docker/ubuntu/Dockerfile` with the verified Ubuntu 26.04 amd64 digest and pinned compiler/repository/tool inputs, clean build stage invoking shared helpers, separate minimal runtime and SDK-consumer verification stages, and a final scratch export of exactly two validated DEBs plus evidence only after all gates pass; do not mount host state/secrets/socket or use privileged Docker.
- [ ] T025 [P] [US1] Create `packaging/docker/fedora/Dockerfile` with the verified Fedora 45 amd64 digest and pinned compiler/repository/tool inputs, clean build stage invoking shared helpers, separate minimal runtime and SDK-consumer verification stages, and a final scratch export of exactly two validated RPMs plus evidence only after all gates pass; keep parity with Ubuntu and fail unresolved compiler/recipe pins explicitly.
- [ ] T026 [US1] Implement `packaging/scripts/build-local.sh` accepting a validated target and output directory, checking the actual checkout full revision/tag, passing identity safely to `docker buildx build --platform linux/amd64`, exporting from the validated scratch stage to isolated fresh target paths, rejecting stale/preexisting successful outputs, and preserving BuildKit failure codes; use this exact path from local documentation and CI.
- [ ] T027 [US1] Add `tools/package/validate-artifacts.sh` as the compatibility wrapper for quickstart's `--ubuntu`, `--fedora`, and `--revision` arguments, dispatching native checks through the matching Docker environment/shared `packaging/scripts/validate-packages.sh` rather than requiring both distro tools on the host; reject missing target directories and metadata/revision mismatches.
- [ ] T028 [US1] Register lightweight helper/contract/failure coverage in `tests/package/CMakeLists.txt` and `CMakeLists.txt`, and provide explicit container integration entry points for native/install/SDK tests; keep package-producing builds from recursively spawning Docker builds in CTest while requiring full core/helper CTest followed by both clean-container integrations before package success.
- [ ] T029 [US1] Replace `.github/workflows/cmake-multi-platform.yml` in place with PR and `master` Docker packaging jobs using exactly two complete named target entries and `fail-fast: false`, shared local runner, checkout-consistent full SHA, SHA-pinned Actions, least-privilege read permissions, no secrets/shared writable PR cache/privileged trigger/release publishing, and target-qualified always-visible summaries/logs; do not add uploads until the both-role gates in US2 are wired.
- [ ] T030 [US1] Run both clean Docker builds through `packaging/scripts/build-local.sh` and T027's wrapper, run all package tests/full CTest and both runtime/SDK integrations, and record package listings/dependency resolution/health/UI/consumer-run/toolchain evidence in `specs/014-docker-linux-packaging/checklists/us1-validation.md`; reject host-generated UI assets or masked runtime dependencies and leave failures blocked rather than marking completion.
- [ ] T031 [US1] Exercise T009–T012's negative cases, including absent/wrong-format/architecture/revision/role/license payloads and one-target build/test/install/SDK failures, and record nonzero results/no successful exports/remaining-target evidence in `specs/014-docker-linux-packaging/checklists/us1-failure-validation.md`; include malformed inputs and stale-output contamination.
- [ ] T032 [US1] Inspect an authorized run of `.github/workflows/cmake-multi-platform.yml` for independent initial-target results and a controlled one-target failure/other-target continuation, recording run URLs/job conclusions/overall failure in `specs/014-docker-linux-packaging/checklists/us1-workflow-validation.md`; do not push/create a run without authorization, and report this gate pending if hosted evidence is unavailable.

**Checkpoint**: US1 MVP is complete only with both native roles, full test/clean-install/SDK-run gates, and attributable target failure evidence. Downloadable publication is the next increment.

## Phase 4: User Story 2 — Retrieve and identify a package (Priority: P2)

**Goal**: Publish independently identifiable role artifacts only from successful, fully validated target outputs.

**Independent Test**: Retrieve all four artifacts from an authorized passing run. Inspect visible names and accompanying/native metadata for role, canonical/native version, full checkout revision, exact distro/release, x86_64/native architecture and format without job-order knowledge; compare downloaded checksums. In a one-target or one-role validation failure, neither role of that failed target is uploaded, the other successful target remains downloadable, and the overall run fails. Separately, one contributor must correctly select each of the four named target/role artifacts and identify distribution, release, architecture, format, and role within one minute per selection using visible artifact metadata only.

### Tests for User Story 2

- [ ] T033 [P] [US2] Add `tests/package/test_artifact_identity.py` covering tag/fallback/native version mapping, full revision/role/target naming, metadata/checksum correspondence, reversed matrix ordering, collisions and wrong revisions, malformed inputs, duplicate files and both-role publication eligibility; distinguish untrusted PR/run context from release identity.
- [ ] T034 [P] [US2] Add `tests/package/test_workflow_contract.py` validating `.github/workflows/cmake-multi-platform.yml` triggers/read permissions/SHA-pinned Actions/exact two-entry matrix/fail-fast setting and upload gating, with failed/skipped/missing-output states unable to satisfy target success and no `continue-on-error`, privileged downstream artifact consumer, or release job.

### Implementation for User Story 2

- [ ] T035 [US2] Implement `packaging/scripts/prepare-artifacts.py` consuming validated package identities/evidence to create separate role staging directories and accompanying `artifact.json` with canonical/native version, full source revision, target/role, package filename/SHA-256, validation status and workflow-run/PR-or-push context; require both roles' successful build/test/install/SDK evidence before staging either and reject duplicates or stale metadata.
- [ ] T036 [US2] Update `.github/workflows/cmake-multi-platform.yml` to stage/downloadable-upload runtime and developer separately through T035 only after the target's complete gate succeeds, use full-dimension immutable artifact names and `if-no-files-found: error`, retain logs without presenting failed outputs as packages, and make upload/staging failures fail the target and overall workflow.
- [ ] T037 [US2] Document artifact selection/download/checksum and native metadata inspection, version encoding, SDK/runtime independence, and untrusted unsigned validation-output status in `packaging/README.md`; state that artifacts are workflow-run downloads only and show names for both targets and roles without a fixed execution order.
- [ ] T038 [US2] Retrieve four artifacts from an authorized successful workflow and inspect metadata/native contents/checksums, then inspect a controlled failed-target run for no failed-target role artifacts and continued successful-target publication; record URLs/artifact identities/results in `specs/014-docker-linux-packaging/checklists/us2-validation.md`, reporting lack of hosted access as a pending gate rather than substituting mocked retrieval for real evidence.
- [ ] T039 [US2] Conduct SC-005's single-contributor selection check for all four named initial Ubuntu/Fedora target × runtime/developer role combinations using only visible workflow artifact metadata; record selected artifacts, distribution, release, architecture, format, role, correctness, and elapsed time per selection in `specs/014-docker-linux-packaging/checklists/artifact-usability.md`, require all four correct within one minute each, and flag missing evidence or any failing selection without requiring recruitment.

**Checkpoint**: US2 adds four retrievable, distinct artifacts with fail-closed publication and verified single-contributor evidence for all four target/role selections.

## Phase 5: User Story 3 — Extend the supported target set (Priority: P3)

**Goal**: Demonstrate a data-driven target addition without relabeling, replacing, or suppressing the original outputs.

**Independent Test**: Use an isolated temporary matrix/target definition containing the two originals and one separately verified supported additional release/architecture/format tuple. Build/test/install/SDK-verify it and show a distinct artifact while original identity snapshots remain unchanged. Reject duplicate/incomplete/unsupported definitions. Restore the production matrix to exactly the two agreed initial targets.

### Tests for User Story 3

- [ ] T040 [P] [US3] Add `tests/package/test_target_extension.py` with immutable original-target identity snapshots, order-independent three-target generation, duplicate/incomplete/unsupported tuple rejection, and a temporary additional-target fixture definition outside `packaging/targets.json`; require verified inputs before enabling its real build and never count schema-only simulation as SC-004 build evidence.

### Implementation for User Story 3

- [ ] T041 [US3] Generalize target loading/matrix emission in `packaging/scripts/package_identity.py`, `packaging/scripts/build-local.sh`, and `.github/workflows/cmake-multi-platform.yml` around validated `packaging/targets.json` records and shared format handlers; keep the default exactly two targets, permit an explicitly supplied temporary test target set, and fail unsupported builders/architectures/format combinations rather than adding hidden distro branches or altering original names.
- [ ] T042 [P] [US3] Add `packaging/docs/adding-targets.md` describing required tuple/image/compiler/repository/lock/ABI/native-format/runtime/SDK/license gates, digest refresh policy, and temporary representative-target selection/verification procedure; explain how to preserve the original identity snapshots and keep experimental support out of the initial production matrix.
- [ ] T043 [US3] Select and verify one supported additional target with complete pinned inputs in `tests/package/fixtures/additional-target.json`, run an isolated three-target build/publication experiment with full CTest/runtime/SDK checks, and record distinct output plus unchanged original identities and restored two-entry production configuration in `specs/014-docker-linux-packaging/checklists/us3-validation.md`; require authorization for hosted evidence and block SC-004 if only fixture/schema tests are available.

**Checkpoint**: Target expansion is proven, not merely described, and the committed initial matrix is unchanged.

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Close documentation, constitution/threat-model evidence, full regression gates and handoff without commits.

- [ ] T044 Update `specs/014-docker-linux-packaging/quickstart.md` and `packaging/README.md` with executable shared local commands, revision/output handling, both-target native validation and runtime/SDK build/link/run instructions, pinned input refresh/blocker guidance, and optional `act` guidance explaining Linux Docker prerequisites and hosted artifact-service limitations; keep direct Docker sufficient and never treat `act` as hosted retrieval evidence.
- [ ] T045 [P] Reconcile implemented mitigations with `specs/014-docker-linux-packaging/threat-model.md` and the existing feature summary in `docs/security-threat-model.md`, recording evidence for triggers/permissions/pins/cache isolation/BuildKit output scoping/invalid payload rejection/runtime closure/SDK path hygiene/no seed or credentials/license inventory and residual risks; preserve unrelated dirty summary content.
- [ ] T046 Perform a focused constitution/clean-code review of `packaging/scripts/`, `packaging/cmake/Package.cmake`, `packaging/sdk/`, `configure.py`, and `.github/workflows/cmake-multi-platform.yml`, recording findings/resolutions in `specs/014-docker-linux-packaging/checklists/implementation-review.md`; verify small single-purpose helpers, data-driven target dispatch, C++23/RAII fixture compliance, documented system dependencies and no runtime/API/UI behavior changes or gratuitous hot-path abstractions.
- [ ] T047 Run the complete updated `cmake-build-debug` CTest suite with `ctest --output-on-failure` (no regex exclusions, skips-as-success, or zero-test success), both clean target Release/full-CTest package pipelines and all native/runtime/SDK/failure/extension checks after final changes, and record exact revision/commands/test counts/outcomes in `specs/014-docker-linux-packaging/checklists/final-validation.md`; leave the handoff blocked on any failure and do not commit, push, or merge.
- [ ] T048 Produce FR-001–FR-010 and SC-001–SC-005 evidence/remaining-blocker traceability in `specs/014-docker-linux-packaging/checklists/completion-evidence.md`, distinguishing implemented/automated/hosted/human gates, confirming production has exactly two targets/four valid role artifacts, and recording final dirty-file status for the parent's eventual full-CTest-green artifact commit; do not claim completion for unavailable hosted or human evidence.

## Dependencies & Execution Order

### Phase and story graph

```text
Setup T001–T003
  └─ Foundation T004–T008
       └─ US1 T009–T032 (MVP: tested, installable local packages + CI results)
            └─ US2 T033–T039 (validated role artifact retrieval/selection)
                 └─ US3 T040–T043 (temporary real target expansion)
                      └─ Polish T044–T048 (final gates and parent handoff)
```

US1 has no story prerequisite beyond Foundation. US2 genuinely depends on US1's validated packages/workflow. US3's real publication experiment depends on US1's builders and US2's artifact contract; its fixture/doc authoring may be prepared earlier, but its completion gate cannot. Stories are independently **testable increments**, not falsely advertised as dependency-free.

### Task prerequisites

Every row is a dependency, not merely list order. A comma means all named prerequisites. Phase gates apply in addition to these within-phase edges. `[P]` never overrides a dependency or same-file serialization.

| Task(s) | Prerequisite(s) |
|---|---|
| T001 | None |
| T002, T003 | T001 |
| T004 | T002, T003 |
| T005, T006, T007 | T004 |
| T008 | T005, T006, T007 |
| T009, T010, T011, T012, T013 | Foundation complete (T004–T008) |
| T014 | T013 |
| T015 | T014 |
| T016 | T013, T006 |
| T017 | T016 |
| T018 | T007, T014, T015, T016, T017 |
| T019 | T018 |
| T020 | T008, T019 |
| T021 | T009, T018 |
| T022 | T010, T021 |
| T023 | T011, T014, T015, T016, T021 |
| T024, T025 | T002, T004, T005, T006, T020, T021, T022, T023 |
| T026 | T012, T024, T025 |
| T027 | T021, T026 |
| T028 | T009, T010, T011, T012, T019, T027 |
| T029 | T026, T028 |
| T030 | T027, T028, T029 |
| T031 | T030 |
| T032 | T029, T031; authorized hosted run |
| T033, T034 | US1 complete |
| T035 | T033, T030 |
| T036 | T034, T035 |
| T037 | T036 |
| T038 | T036, T037; authorized hosted passing/failing runs |
| T039 | T038; the project's single contributor performs all four target/role selections |
| T040 | US2 complete |
| T041 | T040 |
| T042 | US2 complete; existing shared interface contract |
| T043 | T040, T041, T042; verified extra target inputs and authorized publication experiment |
| T044, T045 | US3 complete |
| T046 | T044, T045 |
| T047 | T046 |
| T048 | T047; hosted/human evidence or explicit blocker accounting |

### Parallel opportunities and examples

- **Setup**: After T001, T002 (`.dockerignore`) and T003 (`packaging/README.md`) can proceed together.
- **Foundation**: After T004, T005 (Python/tool pins), T006 (Conan lock/profiles), and T007 (identity helper) use different files and can proceed together. T008 waits for their outputs.
- **US1 example — foundation complete**: Author T009 native contract tests, T010 runtime tests, T011 SDK fixture, T012 failure tests, and T013 public-header inventory concurrently. Then serialize target CMake export/config wiring; T016/T017's SDK/license branch can run alongside T014/T015 once the inventory is settled. After shared helpers pass, T024 Ubuntu and T025 Fedora Dockerfiles run in parallel. Actual target builds use isolated output directories and may run concurrently when resources permit.
- **US2 example — US1 complete**: T033 artifact identity tests and T034 workflow contract tests can be authored concurrently. T035 staging implementation follows T033; T036 owns workflow edits and waits for both. Do not edit workflow YAML concurrently with US1 or US3.
- **US3 example — US2 complete**: T040 target-extension tests and T042 target-maintenance documentation can run concurrently; T041 generalization follows T040 and T043 waits for both branches plus verified extra inputs.
- **Polish**: T044 quickstart/local docs and T045 threat-model evidence touch different files. T046 review, T047 final tests and T048 handoff are serial gates.

## Requirements and entity mapping

| Requirement / entity | Primary task coverage |
|---|---|
| FR-001, FR-003, FR-004; initial Build Target | T004–T006, T020, T024–T030 |
| FR-002, FR-008; target/revision identity | T007, T018, T021, T029, T033, T035–T038 |
| FR-005; Runtime and Developer Package Roles | T009–T011, T013–T025, T030 |
| FR-006; Workflow Run status and failure isolation | T012, T020–T032, T034, T036, T038 |
| FR-007; downloadable Package Artifact | T035–T038 |
| FR-009; stable target extension | T040–T043 |
| FR-010; built → validated → uploaded transition | T009–T012, T021–T023, T030–T031, T033–T036 |
| SC-001; both roles × both targets pass | T030, T038, T047 |
| SC-002; target outcome visibility | T029, T032, T036, T038 |
| SC-003; valid-only retrievable artifacts | T034–T038 |
| SC-004; demonstrated additional target | T040–T043 |
| SC-005; one contributor, four correct target/role selections within one minute each | T039 |
| Supply-chain/runtime/SDK/license threat mitigations | T002, T004–T006, T013–T025, T029, T031, T034–T037, T045 |
| Full CTest before completion/eventual commit | T020, T028, T030, T043, T047–T048 |

## Implementation Strategy

### MVP first

Complete Setup and Foundation, then US1. Demonstrate both Docker targets producing runtime and SDK packages after clean UI/backend builds, full CTest, native payload checks, installed GraphQL/UI smoke and external SDK compile/link/run. Stop on pin/toolchain/test blockers; a copied executable or archive is not an MVP. US1 does not yet claim downloadable artifacts or full feature completion.

### Incremental delivery

1. US1 supplies validated local packages and CI target results.
2. US2 adds fail-closed separate uploads, actual retrieval/checksum evidence, and single-contributor selection measurement for all four initial target/role combinations.
3. US3 proves one temporary future target without changing the agreed production target set.
4. Polish closes threat-model/review/regression/evidence gaps. Parent receives uncommitted changes and exact test status; no implementation task instructs a commit.

### Readiness and completion policy

Task generation is complete when this file is structurally validated. Implementation can begin at T001; availability/compatibility/encoding decisions are early explicit gates. Story completion requires its independent test and all mandatory evidence. Docker-only local success does not prove hosted matrix/upload behavior; automated artifact names do not prove SC-005's four single-contributor selections. Mark unavailable hosted access, missing selection evidence, or failed pins/builds/tests as blockers in the evidence files, retain unchecked work, and do not silently reduce scope. No additional contributors are required.

## Task totals

| Phase | Tasks |
|---|---:|
| Setup | 3 |
| Foundation | 5 |
| US1 (P1) | 24 |
| US2 (P2) | 7 |
| US3 (P3) | 4 |
| Polish | 5 |
| **Total** | **48** |

**Parallel-marked tasks**: 16. Actual parallel execution is limited by the prerequisite table and shared-file ownership.
