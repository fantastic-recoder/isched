# Implementation Plan: Dockerized Linux Package Builds

**Branch**: `014-docker-linux-packaging` | **Date**: 2026-11-09 | **Spec**: [spec.md](spec.md)
**Input**: `/home/groby/dev/isched/specs/014-docker-linux-packaging/spec.md`
**Repository root**: `/home/groby/dev/isched`

## Summary

Replace the starter `.github/workflows/cmake-multi-platform.yml` in place with a Docker-based native-package workflow for pull requests and pushes to `master`. Exactly two initial targets, Ubuntu 26.04 LTS and Fedora 45 x86_64, each produce separately downloadable `isched-runtime` and `isched-devel` packages in DEB and RPM format respectively. Preserve the C++23/Conan/CMake/Ninja build and embedded Angular 21 WebUI. Add install/export integration for the existing static SDK, then require full target CTest, native package validation, clean runtime installation, and an external SDK compile/link/run before uploading either role.

This is a planning-only refresh. Existing uncommitted changes to packaging, tests, locks, `configure.py`, ignore rules, and `tasks.md` are inputs, not changes made or verified by this planning run. Existing Python-helper refinements are retained. The plan does not authorize implementation, commits, pushes, hosted workflow execution, or release publication.

## Technical Context

**Language/Version**: C++23; Python 3 packaging helpers; CMake and GitHub Actions YAML; existing Angular 21/strict TypeScript frontend. No new shell helper files.
**Primary Dependencies**: Conan 2.34.0; CMake 3.22.6 and Ninja 1.12.1 from Conan tool-requires; Node.js 24.22.2; pnpm 10.30.3; CPack DEB/RPM; Docker BuildKit/Buildx; GitHub Actions.
**Storage**: No new application persistence. Per-target output directories, JSON identity/evidence manifests, and workflow-run artifacts only.
**Testing**: Python packaging contract/failure tests; fresh Angular test/build/embed; full Release CTest per target with nonzero test discovery; native package inspection; clean runtime install/loader/UI/GraphQL checks; clean SDK compile/link/run; authorized hosted artifact checks and human SC-005 verification.
**Target Platform**: `linux/amd64`: Ubuntu 26.04 LTS (`amd64`, DEB), Fedora 45 (`x86_64`, RPM).
**Project Type**: Existing C++ server/static-library SDK with embedded WebUI; build/distribution infrastructure.
**Performance Goals**: No runtime hot-path or service behavior changes; no performance improvement claim. Independent matrix jobs preserve feedback when one target fails. Do not trade verification for faster CI.
**Constraints**: Exactly two initial target tuples and two roles each; pinned images/toolchains; Conan-managed C++ dependencies; lockfile-managed frontend dependencies; no secrets, seeded state, host package installation, release upload, or silent target/tool substitution.
**Scale/Scope**: Four native packages and four role-specific downloadable artifacts per passing source revision. Helpers use argument-list subprocess calls, explicit errors, timeouts, and cleanup of owned processes/temporary resources.

## Constitution Check

*GATE: Pass before Phase 0; re-evaluated after Phase 1. Constitution v2.3.0.*

- [x] High performance: no runtime hot-path changes or new abstractions; performance testing is required only if implementation scope later changes.
- [x] GraphQL-only HTTP/WebSocket `/graphql` contract and specification compliance preserved; no new application endpoint.
- [x] C++23, Conan-managed library dependencies, CMake + Ninja, and Linux portability retained.
- [x] Angular standards remain intact: no UI source changes; fresh existing tests/build/embed required, with no stale-header fallback in packaging.
- [x] No frontend REST transport, browser JWT persistence, auth, or local development proxy changes.
- [x] Test coverage and failure-path checks required before story completion; packaging success requires the complete target test suite and both role integrations.
- [x] Security-first: feature [threat-model.md](threat-model.md) and existing project summary in `/home/groby/dev/isched/docs/security-threat-model.md`; least-privilege, secret-free PR builds.
- [x] Clean code: small, focused Python helpers; shared target data and distro-specific Dockerfiles avoid large conditional chains; no gratuitous runtime dispatch.
- [x] Direct system dependencies justified only for the native packaging boundary: distro compiler/libc ABI, Python/venv, DEB/RPM/ELF tools, Docker/BuildKit. These are not replacements for Conan libraries. Node/pnpm and frontend packages retain the existing lockfile-based frontend build boundary.

**Pre-research result: PASS (design scope).** No constitution exception. This does not certify target builds, dependency locks, package usability, or hosted behavior.

## Phase 0 — Research

Update [research.md](research.md) using Decision / Rationale / Alternatives considered. Retain the selected releases and existing pin evidence, reconcile against current packaging inputs, and resolve the SDK, native-version, source-identity, dependency-boundary, and verification decisions.

| Target | Image input | Compiler input | Native architecture / format |
|---|---|---|---|
| Ubuntu 26.04 LTS | `ubuntu:26.04@sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725` | `/usr/bin/gcc-15`, `/usr/bin/g++-15`, `15.2.0-16ubuntu1` | `amd64` / DEB |
| Fedora 45 | `fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88` | `/usr/bin/gcc`, `/usr/bin/g++`, `16.2.1-2.fc45.1` | `x86_64` / RPM |

Use `/home/groby/dev/isched/packaging/targets.json` and `tool-versions.json` as the implementation's machine-readable inputs, not duplicated workflow constants. Their presence and recorded checks do not prove all transitive OS packages or Conan recipes work on both targets. Pin updates must be reviewed; unavailable pins fail closed.

**Phase 0 result**: Planning decisions resolved; no outstanding requirements clarification. Target compatibility and end-to-end execution remain explicit implementation gates below.

## Phase 1 — Design and Contracts

### Package payload and SDK integration

Use CMake install/export rules and CPack `runtime` / `devel` components. Artifact role `developer` maps to CPack `devel` and native package name `isched-devel`.

- Runtime: `/usr/bin/isched_srv` with fresh embedded WebUI, project license, applicable linked/embedded third-party notices, and usage documentation. No Node runtime, test binaries, build tools, service unit, database, seed output, or credential.
- Developer: `libisched.a` under GNUInstallDirs `${libdir}`, relocatable CMake config/targets under `${libdir}/cmake/isched`, dependency manifest/lock/profiles and instructions under `${datadir}/isched`, licenses, and the supported public header closure:
  - `${includedir}/isched/isched.hpp`
  - `${includedir}/isched/backend/isched_Server.hpp`
  - `${includedir}/isched/backend/isched_common.hpp`
  - `${includedir}/isched/shared/config/isched_config.hpp`
- Export `isched::isched` with C++23 and installed include usage requirements. The four-header closure above is the current supported umbrella API; do not install every private backend header or generated UI/schema header merely because it exists.
- Resolve the current static link graph through Conan CMakeDeps: PEGTL 3.2.8, cpp-httplib 0.18.3, OpenSSL 3.5.0, spdlog 1.12.0, fmt 10.2.1, nlohmann_json 3.11.3, jwt-cpp 0.7.0, SQLite 3.44.0, Boost 1.84.0 (`url`, `system`, `thread`, `program_options`, `filesystem` and transitive components), platformfolders 4.3.0, and platform loader/thread requirements. Keep Catch2, CMake, and Ninja out of the SDK runtime link interface.
- `ischedConfig.cmake` uses `find_dependency` for corresponding Conan-generated targets, including `openssl::openssl`, `Boost::*`, and the target names currently linked by `isched`. Export static transitive requirements without embedding producer source/build/Conan-cache paths. The consumer must resolve its own compatible Conan graph, compiler and ABI; the SDK is not self-contained.
- No runtime ↔ developer native package dependency: the static SDK does not need the executable. Derive runtime shared-object requirements using DEB shlibdeps and RPM ELF auto-requires; prove closure with clean installs.

### Identity and workflow

- Derive the full source revision from the actual checkout. For PR merge checkouts use the checked-out merge commit consistently, not a different PR head revision.
- Use only an unambiguous exact-at-HEAD `vMAJOR.MINOR.PATCH` tag; otherwise canonical version is `0.0.0+git.<first-12-sha-digits>`.
- DEB native version equals canonical version. RPM uses `Version=MAJOR.MINOR.PATCH, Release=1` for tagged builds; fallback uses `Version=0.0.0, Release=0.git.<first-12-sha-digits>`. Preserve canonical version/full SHA in embedded and accompanying metadata. Validate parsing and ordering natively rather than claiming `+` is categorically invalid for RPM.
- Artifact identity: `isched-{runtime|developer}-{canonical-version}-{distribution}-{release}-x86_64-{deb|rpm}-{full-sha}`. Each role artifact contains exactly its native package plus JSON identity/checksum metadata. Artifact visibility is that of the repository/workflow, not an assurance of private storage.
- Docker context excludes `.git` and generated/host state. A Python wrapper validates checkout identity and supplies `ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION` build arguments. Resolve tags outside Docker; no Git authentication or secret arguments.
- Matrix comes from validated target inputs, with exactly two initial entries and `fail-fast: false`; name jobs with distribution, version, architecture, and format. No `continue-on-error` or missing-output success path.
- Build Release via existing `configure.py release` with explicit Conan lock/profiles and Conan build environment activation. Existing optional packaging arguments are work in progress, not certified.
- Require pnpm and regenerate Angular assets; then run the full CTest suite, package, validate native metadata/payload, install/runtime-check and SDK-check. Stage outputs only after both roles pass.
- Use `pull_request` and push to `master`, read-only token permissions, SHA-pinned Actions, no publishing credentials, and no privileged Docker execution/socket mounting into build containers. Upload each role with explicit name and error-on-missing files only after all target gates pass.

### Validation and acceptance

1. Contract tests reject missing/empty/duplicate packages, wrong internal format/suffix, role, release, revision, version, architecture and ELF class/machine; reject private payload/state and missing notices.
2. Verify target `/etc/os-release`, `linux/amd64`, exact toolchain/build-tool versions, lock inputs, and fresh WebUI test/build/embed. Nonzero full Release CTest must pass on both targets.
3. Install runtime into a fresh exact-release container without the build toolchain. Verify loader closure, start unprivileged with isolated writable data, check existing GraphQL health and embedded UI plus an asset, and clean up without `--seed`.
4. Install SDK into a clean target consumer environment without the runtime package. Provision Conan dependencies from shipped inputs, `find_package(isched CONFIG REQUIRED)`, include `<isched/isched.hpp>`, link `isched::isched`, call a real out-of-line symbol such as `is_lifecycle_trace_enabled()`, and run. No producer tree/cache path may be required.
5. Test one-target failure isolation and upload suppression. Hosted runs/download checks need separate authorization; a local simulation is not hosted evidence.
6. Temporarily exercise an additional independently pinned supported target for US3 without expanding the committed initial matrix or relabeling its outputs.
7. SC-005: one real contributor selects each of the four target/role artifacts and identifies all required dimensions in ≤60 seconds per selection from visible workflow metadata. Record actual timings; automation cannot replace this human check.

Detailed interfaces and entities are in [contracts/package-artifact-contract.md](contracts/package-artifact-contract.md) and [data-model.md](data-model.md). Local procedures are in [quickstart.md](quickstart.md).

## Project Structure

### Documentation (this feature)

```text
/home/groby/dev/isched/specs/014-docker-linux-packaging/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── threat-model.md
├── contracts/package-artifact-contract.md
├── checklists/implementation-readiness.md  # existing execution evidence; preserve
└── tasks.md                              # existing; not regenerated by planning
```

### Source Code (repository root; implementation scope, not this run's edits)

```text
/home/groby/dev/isched/
├── .github/workflows/cmake-multi-platform.yml
├── CMakeLists.txt
├── src/main/cpp/isched/CMakeLists.txt
├── configure.py
├── conan.lock
├── packaging/
│   ├── targets.json
│   ├── tool-versions.json
│   ├── requirements-*.lock
│   ├── conan/profiles/{ubuntu-26.04,fedora-45}
│   ├── docker/{ubuntu,fedora}/Dockerfile
│   ├── sdk/
│   └── scripts/                           # Python build/identity/validation/smoke helpers
├── tests/package/                         # contract/failure and SDK fixtures
├── tools/package/validate_artifacts.py    # wrapper for shared Python validators
└── docs/security-threat-model.md
```

**Structure Decision**: Extend existing build targets, isolate packaging inputs/helpers, and replace the starter workflow rather than adding overlapping workflows. Existing packaging files are partial implementation; paths shown above include future files. Update `/home/groby/dev/isched/.github/agents/copilot-instructions.md` through the repository agent-context script, preserving manual additions.

## Post-Design Constitution Check

**PASS for design.** All pre-research checks still hold. No application behavior, public GraphQL schema, browser storage, or hot-path change is designed. Conan remains the C++ dependency boundary; native tool/ABI dependencies have explicit justification. Feature/project threat models already exist. SDK usability, secure artifact handling, and both-role verification are required completion gates, not optional follow-up.

## Phase 2 — Handoff and Evidence Gates

Planning stops here; do not execute or regenerate existing `tasks.md`. That task list and implementation-readiness notes predate this refresh; their historical references to earlier plan gaps are not current design conclusions. Preserve task completion markers and reconcile them only in a separately requested task-management/implementation pass.

**No blocking requirements clarification or constitution violation remains.** Implementation readiness is conditional on:

- Proving target Python dependency locks, GCC 15/16 Conan settings/recipe compatibility, complete producer/consumer locks, and resolvable OS dependency closure on both pinned targets.
- Proving the installed SDK export and native version encodings against actual tools; file presence or public package availability alone is not sufficient.
- Building/testing/installing both roles on both releases and collecting authorized hosted/failure-isolation evidence.
- Obtaining the real contributor's four artifact-selection results and reviewing redistribution notices.

These are required verification gates; they were not run or marked passed during this planning refresh. If a pinned input or target graph fails, stop affected implementation and report the blocker rather than changing the agreed releases or library versions silently.

## Complexity Tracking

No constitution violations or exceptions. Native packaging/build-tool dependencies are justified above; no new runtime architecture or application persistence is introduced.
