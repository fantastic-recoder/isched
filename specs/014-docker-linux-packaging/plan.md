# Implementation Plan: Dockerized Linux Package Builds

**Branch**: `014-docker-linux-packaging` | **Date**: 2026-11-09 | **Spec**: [spec.md](spec.md)
**Input**: Approved feature specification and checklist in `specs/014-docker-linux-packaging/`

## Summary

Replace the broken starter multi-platform CMake workflow with one Docker-based Linux package workflow for pull requests and pushes to `master`. It builds, tests, validates, and exports native `.deb` and `.rpm` packages for the pinned Ubuntu 26.04 LTS and Fedora 45 x86_64 targets. Each target produces separate `isched-runtime` and `isched-devel` packages, with artifact names and embedded metadata carrying the source revision, target, format, and role. CMake install/export rules will make the existing static `isched` library consumable as `isched::isched`; Conan 2 remains the SDK's transitive dependency manager.

## Technical Context

**Language/Version**: C++23; packaging/workflow glue uses Bash, Python, CMake, and YAML  
**Primary Dependencies**: Conan 2.34.0; CMake 3.22.6 and Ninja 1.12.1 from existing Conan tool-requires; Node.js 24.22.2; pnpm 10.30.3; CPack; Docker BuildKit; GitHub Actions  
**Storage**: N/A  
**Testing**: Existing Angular tests run through the CMake UI build; full CTest suite on each target; package metadata/content/architecture validation; clean-container install/run smoke test; installed-SDK CMake consumer compile/link smoke test.  
**Target Platform**: `linux/amd64` only: Ubuntu 26.04 LTS `.deb` and Fedora 45 `.rpm`  
**Project Type**: C++23 server and static-library SDK with embedded Angular 21 WebUI; native Linux packages and CI workflow  
**Performance Goals**: No runtime behavior or performance changes. Packaging adds no runtime service wrapper or always-on process.  
**Constraints**: Exactly two initial distribution/version/architecture targets and two package roles per target; use Docker; do not publish formal releases; execute without secrets or interactive input; validate before artifact upload; no `act` requirement.  
**Scale/Scope**: Four package files per successful revision (two roles × two targets), individually attributable to the full Git SHA and target dimensions.

## Constitution Check

*GATE: Pass before research; re-evaluated after design below.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only); no application interface changes.
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja).
- [x] No WebUI source changes; Angular code conventions are unaffected. Angular 21 build/test remains part of asset generation.
- [x] No frontend API, browser JWT, or local Angular proxy changes.
- [x] Test plan requires full CTest and package/SDK-specific checks before success; the repository's 100% CTest commit gate remains unchanged.
- [x] Security-sensitive CI/package supply-chain design has a feature threat model and a project-level summary entry.
- [x] Planned glue is limited to focused packaging/build functions; no hot-path runtime abstractions are introduced.

**Gate result**: PASS. No constitution exceptions or implementation blockers are identified. The library's missing install/export configuration is an implementation gap to close, not a reason to block the design.

## Design Decisions

1. **Targets**: Ubuntu 26.04 LTS (Resolute Raccoon), x86_64/`amd64`, `.deb`; Fedora 45, x86_64, `.rpm`. These are supported current releases as of 2026-11-09. The Docker base references use the observed platform-specific immutable image-manifest digests recorded in [research.md](research.md).
2. **Build orchestration**: Replace `.github/workflows/cmake-multi-platform.yml` in place rather than adding a second workflow with overlapping `master`/PR triggers. Use a two-entry, extensible target matrix with `fail-fast: false`; each job invokes the existing `configure.py release`/Conan+CMake flow with explicit distro compilers, then runs the full CTest suite and package checks. The workflow retains pull-request and default-branch push validation and adds no release publishing.
3. **Package implementation**: Add CMake install/export rules and use CPack's DEB and RPM generators with separate `runtime` and `devel` components. Keep the existing default static-library form (`libisched.a`) unless package integration proves an explicit change is needed; do not change runtime behavior to make packaging easier.
4. **SDK interface**: Export the supported CMake target as `isched::isched`, install the public-header include closure preserving `isched/...` paths, and ship CMake package config plus a Conan 2 consumer dependency manifest/instructions. The exported config resolves declared transitive dependencies through Conan-generated CMakeDeps targets. A clean consumer fixture must prove this works; the SDK must not silently claim to be self-contained if dependency provisioning is required.
5. **Version and artifact identity**: Prefer a project version only from a tag matching `vMAJOR.MINOR.PATCH`; otherwise use a non-release `0.0.0+git.<short-sha>` package version. Embed the full `github.sha` in package metadata and artifact identity. Naming pattern: `isched-{role}-{version}-{distro}-{distro-version}-x86_64-{deb|rpm}-{full-sha}`. Keep both role packages private to the workflow run as downloadable artifacts; do not publish them to a repository or formal release.
6. **Artifact publication**: Build and validate both role packages before uploading either role artifact for that target. Use independent matrix jobs, explicit artifact names, error-on-missing outputs, and no shared mutable package output. A failed build, test, or validation fails its target and the workflow; `fail-fast: false` preserves the other target's result.
7. **Reproducibility**: Pin the Docker base image to the recorded amd64 manifest digest; pin GCC (`g++-15` 15.2.0-16ubuntu1 on Ubuntu and `gcc-c++` 16.2.1-2.fc45.1 on Fedora); pin Conan 2.34.0 and its Python dependency set; use CMake 3.22.6/Ninja 1.12.1 from the existing Conan tool-requires and a committed Conan lockfile; pin Node.js 24.22.2 by its official tarball SHA-256 and pnpm 10.30.3 through the repository's packageManager/lockfile. Pin other OS tool packages or use dated repository inputs. Do not equate an immutable base-image digest alone with a bit-for-bit reproducible package: repository inputs, source revision, and all package inputs must be recorded/locked. Refresh pins through reviewed changes before a target loses support.
8. **Runtime payload and licensing**: Install `isched_srv` at `/usr/bin/isched_srv`; its Angular UI remains embedded. Runtime packages contain only the executable and required license/readme metadata; they do not install service state, a database, sample credentials, or `--seed` output. Install the SDK library, public headers, CMake package integration, and dependency manifest in standard GNUInstallDirs paths. Include `LICENSE.md` (MPL-2.0) and third-party/Angular license notices for distributed linked/embedded content.
9. **Dependency metadata**: Derive runtime shared-library requirements from each built target using native package tooling (`dpkg-shlibdeps`/DEB metadata and RPM automatic ELF dependency generation), then validate installed runtime closure in a clean target-release container. The developer package is not coupled to the runtime executable package; it documents and resolves the library's Conan-managed link dependencies. Do not hard-code a cross-distro dependency list without validation.
10. **Local verification**: Provide a direct Docker BuildKit path for each target and document installation/smoke checks. `act` is not required; Docker is available locally.

## Project Structure

### Documentation (this feature)

```text
specs/014-docker-linux-packaging/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── threat-model.md
└── contracts/
    └── package-artifact-contract.md
```

### Planned source and packaging changes

```text
.github/workflows/cmake-multi-platform.yml  # replace starter matrix workflow in place
docs/security-threat-model.md               # add a planning-stage feature summary
CMakeLists.txt                              # package/install configuration and version plumbing
src/main/cpp/isched/CMakeLists.txt           # public include/export and install targets
conan.lock                                   # lock complete Conan graph for package builds
packaging/
├── docker/
│   ├── ubuntu/Dockerfile
│   └── fedora/Dockerfile
├── sdk/                                     # consumer dependency manifest/config inputs
└── scripts/                                 # focused package identity/validation helpers
```

**Structure Decision**: Keep packaging isolated under `packaging/`, reuse the current source build and the existing workflow path, and add CMake packaging rules at the actual library/executable targets. Do not create a second workflow that competes with the starter's existing PR and `master` triggers.

## Validation Design

- Every matrix entry identifies one fixed distro release, x86_64 architecture, compiler family/version, and native package format.
- Configure and build the Release profile using the pinned Conan 2.34.0, CMake 3.22.6, Ninja 1.12.1, Node.js 24.22.2, and pnpm 10.30.3; reject a missing or unpinned build input.
- Run all registered CTest cases with `ctest --output-on-failure`; retain the repository's full-suite 100% gate.
- Generate both component packages. Inspect native package metadata and payload to confirm package name/role, version, architecture, expected format, source revision, executable/library/header/config paths, and license files.
- Validate the DEB/RPM by native metadata/listing tools and reject empty, duplicate, wrong-format, wrong-architecture, or misidentified files.
- Install the runtime package into a clean container of its exact target release without build tools; inspect dynamic dependency resolution and start `isched_srv` with a disposable temporary data directory, without seeding or persisted credentials.
- Install the developer package in a clean consumer environment, provision Conan dependencies from the shipped manifest, configure a tiny CMake client that includes the supported header and links `isched::isched`, then compile/link.
- Upload separate role artifacts only after all target validation passes. Verify the full revision and all target dimensions are visible in artifact names/metadata.
- Verify SC-005 with the project's single contributor: select each of the four named Ubuntu/Fedora target × runtime/developer role artifacts and correctly identify distribution, release, architecture, format, and role within one minute per selection using visible workflow artifact metadata only. No recruitment or multi-contributor threshold is required.

## Post-Design Constitution Check

- [x] Build language, C++23, Conan 2, CMake, and Ninja baselines remain intact.
- [x] No GraphQL, runtime auth, JWT persistence, REST client, or UI source behavior is changed.
- [x] Tests cover full CTest plus clean installed-runtime and SDK behavior before the target is successful.
- [x] Feature and project-level threat model updates are included.
- [x] No performance-sensitive runtime changes are designed.

**Post-design gate result**: PASS. No complexity exception is required.

## Complexity Tracking

No constitution violations or exceptions.
