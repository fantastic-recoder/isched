# Tasks: Dockerized Linux Package Builds

**Input**: `/home/groby/dev/isched/specs/014-docker-linux-packaging/` — `spec.md`, refreshed `plan.md`, `research.md`, `data-model.md`, `contracts/package-artifact-contract.md`, `quickstart.md`, and the feature `threat-model.md`.
**Branch**: `014-docker-linux-packaging`
**Organization**: Setup → Foundation → US1 (P1) → US2 (P2) → US3 (P3) → Polish.
**Tests**: Required by the feature specification, plan, and project constitution. Include automated contract/failure tests and target-level CTest/runtime/SDK verification; actual tests and human/hosted evidence are implementation gates, not inferred from file presence.

## Execution notes

- Tasks and paths are repository-relative. Implementation changes are in progress; checked implementation items below reflect code and host-side tests only, not unrun target compatibility, package builds, hosted runs, or usability evidence.
- Preserve the existing checked markers for T001–T004: their readiness record, Docker context exclusions, packaging README, and two pinned target definitions are present and supported by the recorded evidence. T002 is scoped to the `.dockerignore` boundary and documenting explicit identity inputs; actual revision/version argument transport remains an implementation task.
- Initial production targets remain exactly Ubuntu 26.04 LTS x86_64/amd64 DEB and Fedora 45 x86_64 RPM. Each target produces `isched-runtime` and `isched-devel` (artifact role `developer`, CPack component `devel`). Use the exact pins in `research.md`, `packaging/targets.json`, and `packaging/tool-versions.json`; do not silently substitute releases, compilers, dependencies, or moving aliases.
- Build/package orchestration, validation, and verification helpers are Python under `packaging/scripts/`; invoke subprocesses with argument lists, explicit timeouts/errors, and cleanup of owned resources. Do not add shell helper files.
- Package identity uses the actual checkout's full SHA (including a PR merge checkout), exact-at-HEAD `vMAJOR.MINOR.PATCH` tags only, and the canonical fallback `0.0.0+git.<first-12-sha-digits>`. Validate DEB/RPM-native version mapping and ordering; never infer identity from matrix order.
- Runtime and SDK packages are independent. Require fresh Angular test/build/embed output, full nonempty target Release CTest, native package checks, clean runtime install/smoke, and external SDK compile/link/run before a target can succeed or either role can be uploaded.
- Hosted workflow/artifact evidence requires a separately authorized hosted run. SC-005 requires the project's one contributor to select each of four target/role artifacts and identify distribution, release, architecture, format, and role within 60 seconds per selection from visible artifact metadata. Keep these tasks unchecked until actual evidence exists.
- A future-target exercise must use a separately verified, pinned compatible target in temporary configuration; do not expand the two-entry production matrix. If target compatibility or an authorized hosted experiment is unavailable, record the blocker and keep the affected acceptance evidence pending.
- Local target pipelines remain pending. `build_local.py` rejects dirty checkouts before export so packages cannot misrepresent their source as `HEAD`. The 2026-11-10 implementation owner is authorized to commit completed work only after the full Debug CTest gate; no push is authorized. T006/T020/T023/T024/T025 resource-bound corrections now enforce one-job producer/dependency/SDK builds and serial runtime→SDK verification; see `checklists/us1-validation.md`. These corrections do not complete T030.

## Format and path conventions

Every task uses `- [ ] Tnnn [P?] [USn?] Description with a concrete repository-relative path`. `[P]` means it can run alongside eligible tasks on different files after its actual prerequisites are complete. Only user-story phase tasks have `[US1]`, `[US2]`, or `[US3]` labels. Existing completed work uses its retained `[X]` marker.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish execution boundaries, target/source-context assumptions, and packaging layout.

- [x] T001 Record supplied-pin verification, unresolved compatibility/version-encoding questions, native dependency justification, and hosted/human evidence prerequisites in `specs/014-docker-linux-packaging/checklists/implementation-readiness.md`; preserve planning artifacts and do not claim unrun implementation checks.
- [x] T002 Update `.dockerignore` to exclude host build/dependency/generated state, package outputs, databases, credentials, and local state while retaining source/build inputs and licenses; document in `.dockerignore` and `packaging/README.md` that callers must pass the checked-out revision and canonical package version because `.git` is excluded.
- [x] T003 [P] Document the packaging layout, initial target/role contract, intended Python entry points, evidence locations, and no-host-install/no-seed/no-publish boundaries in `packaging/README.md`.

**Checkpoint**: Shared boundaries and current pin evidence are recorded; completed markers above are historical repository work, not proof of package builds.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Freeze reproducible inputs and provide the shared configuration/identity foundation. Complete this phase before story implementation.

- [x] T004 Define the exactly-two-target tuples, image/compiler pins, architectures/formats, role mapping, and OS package inputs in `packaging/targets.json`; preserve recorded verification in `specs/014-docker-linux-packaging/checklists/implementation-readiness.md` and fail closed on unresolved pins.
- [X] T005 [P] Complete pinned bootstrap/test dependencies and tool metadata in `packaging/requirements-conan.lock`, `packaging/requirements-tests.lock`, and `packaging/tool-versions.json`; verify Conan/Node/pnpm checksums and installability/interpreter compatibility for the two target Python versions without relying on host tools.
- [X] T006 [P] Generate the full recipe-revision-locked C++ dependency graph in `conan.lock` and explicit Release/C++23/architecture/compiler/libstdc++ ABI profiles in `packaging/conan/profiles/ubuntu-26.04` and `packaging/conan/profiles/fedora-45`; resolve both graphs with the pinned Conan/compiler inputs or record an explicit compatibility blocker without changing pins.
- [X] T007 [P] Complete `packaging/scripts/package_identity.py` and `tests/package/test_package_identity.py` for target/role validation, full checkout SHA, exact semver-tag selection, canonical fallback, native DEB/RPM version/release mapping and ordering, stable filenames, and malformed/duplicate/mismatched identity rejection.
- [X] T008 Extend `configure.py` with narrowly scoped packaging-only lock/profile/CMake argument and Conan build-environment support; prove ordinary debug/release behavior remains intact and package builds fail on Conan/configure/build errors rather than falling back to host-selected tools.

**Checkpoint**: Both pinned target tool/dependency graphs and the reusable identity/build configuration are validated before US1 begins.

## Phase 3: User Story 1 — Build packages for the initial Linux targets (Priority: P1) — MVP

**Goal**: Build the application and both native package roles inside pinned Ubuntu/Fedora Docker environments, validate clean runtime and SDK use, and surface independent target failures without false success.

**Independent Test**: Run the local Docker package path for each initial target. Each must produce exactly one valid runtime and developer native package after fresh Angular verification and nonzero full Release CTest; install runtime in a clean same-release environment and verify loader closure, existing GraphQL health, embedded UI and asset; install the SDK without the runtime package and compile/link/run an external client. Inject target-specific build/test/package/install/SDK failures and confirm they cannot be reported as success.

### Tests for User Story 1

- [X] T009 [P] [US1] Add native package fixtures and tests in `tests/package/test_native_packages.py` for DEB/RPM format, role, native identity/version/architecture/release, required payload and notices, checksums/dependencies, no invented SDK→runtime dependency, and rejection of missing/empty/duplicate/misnamed/state-contaminated packages using native inspection rather than suffix-only checks.
- [X] T010 [P] [US1] Add clean runtime integration tests in `tests/package/test_runtime_install.py` for both target releases: package-manager installation without development tools, resolved ELF loader dependencies, bounded unprivileged start, parsed existing GraphQL health response, embedded UI plus referenced asset, disposable data without `--seed`, and cleanup/nonzero outcomes for injected failures.
- [X] T011 [P] [US1] Add external SDK consumer fixtures in `tests/package/sdk-consumer/CMakeLists.txt`, `tests/package/sdk-consumer/main.cpp`, and `tests/package/test_sdk_install.py`; test installed `find_package(isched CONFIG REQUIRED)`, `<isched/isched.hpp>`, `isched::isched`, a real out-of-line symbol, and successful run without runtime package, producer tree, build tree, or Conan cache.
- [X] T012 [P] [US1] Add controlled build-pipeline failure tests in `tests/package/test_build_pipeline.py` for image startup, frozen frontend install/test/build/embed, Conan/configure/build, full/empty CTest, package generation/validation, runtime install, and SDK consumption; assert actionable target attribution, nonzero status, and no successful package export at a failed gate.
- [X] T013 [P] [US1] Inventory the public header closure from `src/main/cpp/isched/isched.hpp` into `packaging/sdk/public-headers.cmake`; preserve the `isched/...` layout for the four supported headers and record public third-party includes while excluding private/generated schema/UI headers.

### Implementation for User Story 1

- [x] T014 [US1] Add install/export rules in `src/main/cpp/isched/CMakeLists.txt` for `isched_srv` runtime and the existing static `isched` library/public headers in the `devel` component; export `isched::isched` with C++23 and relocatable build/install include/link requirements, retaining default linkage and no producer/cache paths.
- [x] T015 [US1] Add `packaging/sdk/ischedConfig.cmake.in` and generated package config/version support for `isched::isched`; resolve the actual Conan CMakeDeps target names and required OpenSSL/Boost components without Catch2 or producer paths.
- [x] T016 [US1] Add `packaging/sdk/conanfile.txt`, `packaging/sdk/conan.lock`, and `packaging/sdk/README.md` for the exact static link dependency closure and compatible consumer ABI/tool activation; ship dependency resolution instructions, exclude test-only dependencies, and explain SDK independence from `isched-runtime`.
- [x] T017 [US1] Implement `packaging/scripts/collect_licenses.py` to inventory linked Conan and embedded Angular production notices and assemble role-specific license payloads; fail on missing inputs, exclude test/tool-only dependencies, and leave legal interpretation for maintainer review.
- [x] T018 [US1] Configure CPack component DEB/RPM packaging in `packaging/cmake/Package.cmake` with native package metadata, role-specific paths/names, license/docs, embedded identity, and derived runtime requirements (`dpkg-shlibdeps`/RPM ELF auto-requires); prohibit role confusion, service/data/seed state, and an unsupported SDK-to-runtime dependency.
- [X] T019 [US1] Wire opt-in packaging, GNUInstallDirs, SDK config/version generation, header export, license collection, and CPack into root `CMakeLists.txt`; make package mode fail unless fresh Angular test/build/embed output is available, with no stale generated-header fallback.
- [X] T020 [US1] Implement `packaging/scripts/build_target.py` to apply the selected target's pinned tools/profile/lock, run frozen pnpm install and fresh Angular test/build/embed, invoke `configure.py release`, run the complete Release CTest suite and reject zero discovered tests, then build both package roles and collect notices with target-qualified failures.
- [X] T021 [US1] Implement `packaging/scripts/validate_packages.py` using native DEB/RPM/ELF tools to verify exactly one package per role, internal format, filename, native metadata/version/release/architecture, source SHA/target/role, payload allowlist/notices/dependencies/checksum, and absence of source/cache/test/tool/database/credential/seed content.
- [X] T022 [US1] Implement `packaging/scripts/verify_runtime.py` to install `isched-runtime` in a fresh exact-release container without build tools, validate declared/loader dependencies, start `/usr/bin/isched_srv` unprivileged with temporary data, check existing GraphQL health and embedded UI/asset responses, bound waits, preserve failure diagnostics, and clean only owned processes/state.
- [X] T023 [US1] Implement `packaging/scripts/verify_sdk.py` to install only `isched-devel` in a fresh same-target consumer environment, provision the shipped Conan dependency inputs, configure/build/link/run T011's external client against the installed relocatable SDK, and fail on missing dependencies, producer-path leakage, or accidental runtime-package dependence.
- [X] T024 [P] [US1] Create `packaging/docker/ubuntu/Dockerfile` from the exact pinned Ubuntu 26.04 amd64 image/tool inputs; run shared build/validation helpers, separate minimal runtime and SDK verification stages, and export only the two validated DEBs plus evidence after every gate, without host state/secrets/socket mounts or privileged execution.
- [X] T025 [P] [US1] Create `packaging/docker/fedora/Dockerfile` from the exact pinned Fedora 45 amd64 image/tool inputs; run the same shared gates and export only the two validated RPMs plus evidence, failing explicitly on unavailable package/compiler/recipe pins and retaining parity with Ubuntu.
- [X] T026 [US1] Implement `packaging/scripts/build_local.py` with the contract CLI, actual checkout SHA/tag/version validation, explicit `ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION` BuildKit arguments, `linux/amd64` and target Dockerfile selection, isolated fresh output directories, stale-output rejection, and faithful Docker failure status.
- [X] T027 [US1] Add the documented compatibility wrapper `tools/package/validate_artifacts.py` for `--ubuntu`, `--fedora`, and `--revision`; dispatch to the shared per-target native validator and reject missing directories or mismatched metadata without duplicating validation policy.
- [X] T028 [US1] Register Python helper/contract tests in `tests/package/CMakeLists.txt` and root `CMakeLists.txt`, and define explicit target-container runtime/SDK integration entry points that do not recursively start Docker package builds from CTest.
- [X] T029 [US1] Replace `.github/workflows/cmake-multi-platform.yml` with pull-request and `master` push validation using the exact two named targets, `fail-fast: false`, shared local build path, checkout-consistent SHA, SHA-pinned Actions, read-only permissions, and visible target-qualified outcomes; do not upload artifacts before US2's both-role staging gate.
- [ ] T030 [US1] Run helper/contract tests, full CTest, and the Ubuntu and Fedora local Docker package pipelines through `packaging/scripts/build_local.py` and `tools/package/validate_artifacts.py`; record exact tool, package, native metadata, runtime, GraphQL/UI/asset, and SDK compile/link/run evidence in `specs/014-docker-linux-packaging/checklists/us1-validation.md`.
- [ ] T031 [US1] Execute the negative package and orchestration cases from T009–T012, including wrong format/architecture/revision/role/release, missing notices, contaminated payload, no output, and one-target build/test/runtime/SDK failure; record target status and suppressed outputs in `specs/014-docker-linux-packaging/checklists/us1-failure-validation.md`.
- [ ] T032 [US1] Inspect an authorized hosted run of `.github/workflows/cmake-multi-platform.yml` to confirm both initial target results are independently visible and a controlled single-target failure does not hide the other result; record run URLs and conclusions in `specs/014-docker-linux-packaging/checklists/us1-workflow-validation.md`, or leave pending with an authorization/access blocker.

**Checkpoint**: Both local target pipelines have passed complete build, test, native package, runtime and SDK gates. Hosted workflow evidence remains a separate authorized gate; US1 does not claim workflow artifact downloads.

## Phase 4: User Story 2 — Retrieve and identify a package (Priority: P2)

**Goal**: Make each successfully validated runtime/developer package separately downloadable and unambiguously identifiable by source revision, target, format, and role.

**Independent Test**: From an authorized successful hosted run, retrieve all four artifacts, inspect `metadata.json`, checksums, and native package identity, and identify role/distribution/release/architecture/format independent of matrix order. Verify a failed target uploads neither role while the other successful target's outcome remains distinguishable and the overall workflow fails. Perform SC-005's one-contributor four-selection timing exercise using only visible workflow artifact metadata.

### Tests for User Story 2

- [x] T033 [P] [US2] Add artifact identity/checksum tests in `tests/package/test_artifact_identity.py` for full SHA and role/target/format/version names, native/canonical version correspondence, exact `metadata.json` fields/checksum, reversed matrix order, collisions, duplicate files, stale/wrong revision, and both-role eligibility.
- [x] T034 [P] [US2] Add workflow contract tests in `tests/package/test_workflow_contract.py` for triggers, least-privilege permissions, SHA-pinned Actions, exactly two initial targets, fail-fast behavior, both-role validation-before-upload, unique names and error-on-missing files; reject `continue-on-error`, privileged artifact consumers, and release publication.

### Implementation for User Story 2

- [x] T035 [US2] Implement `packaging/scripts/prepare_artifacts.py` to consume only validated target evidence and stage one native package plus contract-compliant `metadata.json` per role with full identity and final-package SHA-256; require both roles' build/test/install/SDK gates before staging either and reject duplicates/stale evidence.
- [x] T036 [US2] Add separate runtime/developer artifact uploads to `.github/workflows/cmake-multi-platform.yml` after T035, with immutable full-dimension names, `if-no-files-found: error`, target-specific logs/results, and failure propagation for staging/upload; do not present invalid or failed-target packages as successful outputs.
- [x] T037 [US2] Document workflow-run-only download, artifact naming/selection, metadata/checksum/native package inspection, and unsigned validation-output status in `packaging/README.md`; include examples for both targets and roles that do not rely on matrix order or imply formal release publication.
- [ ] T038 [US2] Using separately authorized hosted runs, retrieve and inspect all four artifacts from a passing run and a controlled failed-target run, verify package checksums/metadata/target isolation and no failed-target role upload, and record URLs/identities/results in `specs/014-docker-linux-packaging/checklists/us2-validation.md`; if unauthorized or inaccessible, record the blocker and leave the gate pending.
- [ ] T039 [US2] Have the project's single contributor perform SC-005 on all four initial target/role artifacts using only visible workflow metadata; record selected artifact, correctness, target dimensions, role, and elapsed seconds in `specs/014-docker-linux-packaging/checklists/artifact-usability.md` and require four correct selections at no more than 60 seconds each.

**Checkpoint**: Validated packages are retrievable and identifiable, with hosted publication and the actual human selection gate evidenced rather than assumed.

## Phase 5: User Story 3 — Extend the supported target set (Priority: P3)

**Goal**: Demonstrate data-driven support for a separately identified additional Linux target without relabeling or changing the committed Ubuntu/Fedora identities.

**Independent Test**: Add a temporary, independently pinned and compatibility-verified target definition; validate/build/test/package it through the shared target path and show a distinct output while the original two identities and outputs remain unchanged. Reject incomplete, duplicate, unsupported, or unverified inputs, then restore the committed production target set to exactly two entries. A schema-only fixture is not SC-004 evidence.

### Tests for User Story 3

- [x] T040 [P] [US3] Add `tests/package/test_target_extension.py` with immutable snapshots of the Ubuntu/Fedora identities, order-independent three-target identity/matrix behavior, and duplicate/incomplete/unsupported input rejection using a separate `tests/package/fixtures/additional-target.json`; gate real execution on verified target inputs.

### Implementation for User Story 3

- [x] T041 [US3] Generalize `packaging/scripts/package_identity.py`, `packaging/scripts/build_local.py`, and `.github/workflows/cmake-multi-platform.yml` to consume validated target records and reusable format handlers, retaining exactly two default production entries and failing unsupported builders/architectures/formats rather than adding silent distro-specific fallbacks.
- [x] T042 [P] [US3] Document the target-addition checklist in `packaging/docs/adding-targets.md`, covering release/image digest, compiler/OS package pins, locks/ABI, native format, runtime/SDK/license gates, identity snapshots, and how to keep experiments out of `packaging/targets.json`'s initial two-target production set.
- [ ] T043 [US3] Select a separately pinned compatible target in `tests/package/fixtures/additional-target.json`, run the isolated additional-target build/test/package/runtime/SDK experiment, prove original target identity snapshots remain unchanged and restore the two-entry `packaging/targets.json`; record evidence or compatibility/authorization blockers in `specs/014-docker-linux-packaging/checklists/us3-validation.md`.

**Checkpoint**: A real additional target experiment demonstrates extension while the committed initial matrix remains exactly Ubuntu 26.04 and Fedora 45.

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Close docs, repository agent context, security/clean-code review, final regression, and evidence handoff.

- [ ] T044 Update `specs/014-docker-linux-packaging/quickstart.md` and `packaging/README.md` with executable local build/validation, native inspection, runtime install and SDK compile/link/run instructions, pin/blocker guidance, and hosted-vs-local verification boundaries; retain Docker as the required path and do not claim current commands have passed.
- [ ] T045 [P] Update `.github/agents/copilot-instructions.md` through the repository's agent-context script to reflect the settled packaging paths, test/evidence gates, and no-host/no-seed/no-secret rules; preserve existing manual additions and verify only the intended generated section changed.
- [ ] T046 [P] Reconcile implemented controls and evidence in `specs/014-docker-linux-packaging/threat-model.md` and the feature entry in `docs/security-threat-model.md`, including workflow triggers/permissions, immutable pins, cache boundaries, BuildKit output scoping, payload validation, runtime/SDK checks, no secrets/seed state, notices, and residual risks; preserve unrelated summary content.
- [ ] T047 Perform the constitution and clean-code review of `packaging/scripts/`, `packaging/cmake/Package.cmake`, `packaging/sdk/`, `configure.py`, and `.github/workflows/cmake-multi-platform.yml`; record findings/resolutions in `specs/014-docker-linux-packaging/checklists/implementation-review.md`, covering focused helpers, data-driven dispatch, C++23/RAII fixtures, justified system dependencies, and no runtime/API/UI changes or gratuitous hot-path abstraction.
- [ ] T048 Run the complete `cmake-build-debug` CTest suite with `ctest --output-on-failure` and no exclusions/zero-test success, both target Release/full-CTest package flows, all package/runtime/SDK/failure/extension checks, and record exact revision/commands/counts/outcomes in `specs/014-docker-linux-packaging/checklists/final-validation.md`; leave handoff blocked on any failure and do not commit, push, or merge.
- [ ] T049 Trace FR-001–FR-010 and SC-001–SC-005 to actual implementation/automated/hosted/human evidence in `specs/014-docker-linux-packaging/checklists/completion-evidence.md`; distinguish pending gates, verify the production matrix and role artifacts, and record dirty-file/test status without claiming unavailable evidence complete.

## Dependencies & Execution Order

### Phase and story graph

```text
Setup T001–T003
  └─ Foundation T004–T008
       └─ US1 T009–T032 (MVP: validated local packages and target CI results)
            └─ US2 T033–T039 (downloadable artifacts and measured selection)
                 └─ US3 T040–T043 (temporary real target extension)
                      └─ Polish T044–T049 (final evidence and handoff)
```

US1 depends on the shared foundation. US2 follows US1's validated package/workflow contract so it can safely stage and retrieve artifacts. US3's real experiment uses the shared builder and artifact identity established by earlier stories; its committed initial target set must not change. Story phases are separately testable increments, but these real implementation/evidence prerequisites are intentional.

### Task prerequisites

A listed prerequisite is a real dependency; phase gates also apply. `[P]` does not override any edge or same-file ownership.

| Task(s) | Prerequisite(s) |
|---|---|
| T001 | None |
| T002, T003 | T001 |
| T004 | T002, T003 |
| T005, T006, T007 | T004 |
| T008 | T005, T006, T007 |
| T009–T013 | Foundation complete (T004–T008) |
| T014 | T013 |
| T015 | T014 |
| T016 | T006, T013 |
| T017 | T016 |
| T018 | T007, T014–T017 |
| T019 | T018 |
| T020 | T008, T019 |
| T021 | T009, T018 |
| T022 | T010, T021 |
| T023 | T011, T014–T016, T021 |
| T024, T025 | T005, T006, T020–T023 |
| T026 | T012, T024, T025 |
| T027 | T021, T026 |
| T028 | T009–T012, T019, T027 |
| T029 | T026, T028 |
| T030 | T027–T029 |
| T031 | T030 |
| T032 | T029, T031; authorized hosted run |
| T033, T034 | US1 complete |
| T035 | T033, T030 |
| T036 | T034, T035 |
| T037 | T036 |
| T038 | T036, T037; authorized hosted passing/failing runs |
| T039 | T038; one contributor performs all four real selections |
| T040, T042 | US2 complete |
| T041 | T040 |
| T043 | T041, T042; compatible pinned target, and authorization if hosted evidence is used |
| T044–T046 | US3 complete |
| T047 | T044–T046 |
| T048 | T047 and all required local/target checks available |
| T049 | T048; hosted/human gates evidenced or explicitly recorded as blockers |

### Parallel opportunities and examples

- **Setup/Foundation**: After T001, T002 and T003 use different files. After T004, T005 (bootstrap locks), T006 (Conan graph/profiles), and T007 (identity helper/tests) can proceed independently; T008 waits for all three.
- **US1**: After Foundation, author T009–T013 in parallel. Then SDK/CMake changes are ordered T014→T015, while T016/T017 form the separate dependency/license branch. Once shared gates/helpers are ready, Ubuntu T024 and Fedora T025 Dockerfiles can be authored independently. Do not run same-target builds into shared output paths.
- **US2**: After US1, T033 and T034 can be authored in parallel; T035 follows identity tests, and T036 owns workflow edits only after both-role staging is ready. T037 follows the final artifact interface.
- **US3**: After US2, T040 and T042 touch separate test/documentation files and can proceed in parallel; T041 follows extension tests and T043 waits for compatible pins and the builder/documentation paths.
- **Polish**: Once US3 is complete, T045 and T046 can be done in parallel on distinct files; T044 is local procedure documentation. T047 and then T048/T049 are serial review/validation/evidence gates.

## Requirements and entity mapping

| Requirement/entity | Primary task coverage |
|---|---|
| FR-001, FR-003, FR-004; initial Build Targets | T004–T006, T020, T024–T032 |
| FR-002, FR-008; source/target identity | T007, T026, T033, T035–T038 |
| FR-005; Runtime and Developer Package Roles / SDK Interface | T009–T011, T013–T023, T030 |
| FR-006; independent Workflow Run results | T012, T029–T032, T034, T036, T038 |
| FR-007; downloadable Package Artifacts | T033–T039 |
| FR-009; stable extensible target set | T040–T043 |
| FR-010; validated-before-upload transition | T009, T012, T018, T021–T023, T030–T036 |
| SC-001 | T020–T023, T030, T048 |
| SC-002 | T029, T032, T036, T038 |
| SC-003 | T021, T030–T031, T035–T038 |
| SC-004 | T040–T043 |
| SC-005 | T039 (human timing evidence only) |
| Security, license, and clean-code controls | T002, T005–T006, T017–T025, T029, T034–T037, T045–T047 |

## Implementation Strategy

### MVP first — User Story 1

Complete Setup and Foundation, then US1. Demonstrate both pinned Docker targets producing valid runtime and SDK packages after fresh UI/backend builds, full nonempty CTest, native payload checks, clean GraphQL/UI runtime smoke, and an external SDK compile/link/run. Stop and report pin/toolchain/recipe/ABI blockers; a copied executable/archive is not an MVP. US1 does not claim downloadable artifact verification or completion of hosted/human acceptance gates.

### Incremental delivery

1. Setup + Foundation: stabilize target inputs and shared identity/build helpers.
2. US1: deliver independently validated local target packages and target-specific workflow outcomes.
3. US2: add separate fail-closed workflow downloads, checksum/metadata inspection, authorized hosted evidence, and the four human artifact-selection measurements.
4. US3: demonstrate an independently pinned additional target temporarily, preserving the original production identities.
5. Polish: update procedures/context, review security and clean-code evidence, run final full gates, and report any unavailable hosted or human checks as blockers. Do not commit or publish as part of these tasks.

### Conditional evidence gates

- Target Python/Conan/compiler/OS dependency compatibility, native DEB/RPM version parsing, complete package builds, both clean runtime installs, and SDK consumer runs require actual target execution; existing lock/profile/file presence is not proof.
- Hosted workflow and artifact-download/failure-isolation evidence requires explicit authorization and real workflow-run URLs; local workflow simulation is not hosted evidence.
- SC-005 requires one real contributor's four artifact selections and recorded timings; automation and naming tests cannot satisfy it.
- US3 requires an additional independently pinned compatible target; fixture/schema tests alone do not satisfy SC-004. Preserve unchecked tasks and record blockers where gates cannot be performed.
