# Research: Dockerized Linux Package Builds

**Date**: 2026-11-09  
**Feature**: [spec.md](spec.md)

## Decisions

### 1. Select current, supported target releases

**Decision**: Ubuntu 26.04 LTS (Resolute Raccoon), x86_64/`amd64`, and Fedora Linux 45, x86_64; build `.deb` and `.rpm`, respectively.

**Rationale**:

- Canonical's release-cycle page reports Ubuntu 26.04 LTS released in April 2026 with standard security maintenance through May 2031. It is the current LTS at the feature date and has an `amd64` supported architecture.
- Fedora Project announced Fedora Linux 45 general availability on 2026-11-03, making it the newest stable release on the planning date. Fedora's release-life-cycle policy describes approximately 13 months of support and support through one month after release N+2; selecting 45 avoids starting with the near-EOL Fedora 43 release.
- Ubuntu package metadata lists `g++-15` version `15.2.0-16ubuntu1` in the 26.04 (`resolute`) archive. Fedora's official package metadata lists `gcc-c++` version `16.2.1-2.fc45.1` for Fedora 45. Pin these versions in each image definition rather than selecting implicitly by `gcc`/`g++` aliases.
- Both compilers are suitable for the project's required C++23 baseline; the build must use explicit `CMAKE_CXX_STANDARD=23` and the real build/test matrix remains the proof of project compatibility.

**Alternatives considered**:

- Ubuntu 24.04 LTS has a longer elapsed history and broad familiarity, but 26.04 is already released and supported at this date, so starting on 24.04 would not use the current LTS.
- Fedora 43 is still temporarily active but approaches end of support when Fedora 45 arrives; it has a shorter useful runway than Fedora 45.
- Moving tags such as `ubuntu:latest`, `fedora:latest`, `ubuntu:resolute`, and `fedora:stable` were rejected because they do not encode fixed target identity.

### 2. Pin x86_64 container images by immutable manifest digest

**Decision**: Use the official Docker Hub images and platform-specific amd64 manifest references below. They were queried from the public Docker Registry v2 API on 2026-11-09; pin the full digest in each `FROM` reference and also specify `linux/amd64`.

| Target | Human-readable tag | amd64 manifest digest | Multi-platform index digest |
|---|---|---|---|
| Ubuntu 26.04 | `ubuntu:26.04` | `sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725` | `sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c` |
| Fedora 45 | `fedora:45` | `sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88` | `sha256:a7d72ba2745b100a3351087472e5ee7b2450af7cf331e7673007e3817612839c` |

**Rationale**: Docker documents that a digest identifies exact image content and that a platform-specific manifest digest identifies the platform variant. The platform-specific digests make the sole supported architecture explicit and immutable. Human-readable tags remain alongside digests for maintainability.

**Alternatives considered**:

- Pinning only `ubuntu:26.04`/`fedora:45` tags was rejected because tags can be retargeted.
- Pinning the index digest plus `--platform linux/amd64` is possible, but the platform-specific image digest narrows the build to the one agreed architecture.

**Reproducibility qualification**: A pinned base image does not pin subsequent `apt`/`dnf` repository results. Pin the toolchain packages above and the other image-installed tool packages (or use dated repository inputs). Pin Conan to 2.34.0 with its Python dependencies locked; pin Node.js 24.22.2 using the official Linux x64 tarball and verified SHA-256 below; pnpm is already fixed to 10.30.3 in `src/ui/package.json` and the lockfile. Retain the repository's exact Conan direct dependency versions and add `conan.lock` to freeze the resolved graph. This is a package-input reproducibility objective, not a claim that byte-for-byte identical output is guaranteed across all toolchains.

### 3. Reuse build-system inputs and explicitly remove starter-workflow defects

**Decision**: Reuse `configure.py release`, which performs Conan install, CMake configure, and CMake build; ensure the container has the pinned C/C++ toolchain, CMake, Ninja, Python, Node, and pnpm. Replace the starter `.github/workflows/cmake-multi-platform.yml` rather than adding a second workflow with overlapping `pull_request` and `push` triggers.

**Rationale**:

- `conanfile.txt` already declares exact direct Conan requirements, CMake 3.22.6 and Ninja 1.12.1 as tool-requires, and CMakeDeps/CMakeToolchain generators.
- `configure.py` already executes the appropriate `conan install` followed by CMake configure/build. Running its single `release` profile avoids building both Debug and Release for every target.
- The pinned build-tool baseline is CMake 3.22.6 and Ninja 1.12.1 from the repository's `[tool_requires]`; the distro compilers are GCC 15.2.0-16ubuntu1 (`g++-15`) and GCC 16.2.1-2.fc45.1 (`gcc-c++`).
- Conan 2.34.0 is a published release dated 2026-10-28. Angular's compatibility table accepts Node 24 for Angular 21; Node.js 24.22.2 is an LTS release dated 2026-10-28. Use its official Linux x64 archive only after checksum verification; pnpm remains at the repository-declared 10.30.3.
- The starter workflow does not install Conan or produce its referenced toolchain, has no C++ compiler entry for the intended Clang configuration, and passes both matrix-selected Release and a later hard-coded Debug build type. Replacing it removes these conflicting inputs and avoids duplicate jobs.
- The root CMake project builds and embeds Angular 21 assets when pnpm is present; root CMake targets already enforce Angular unit tests before the production UI build. The pnpm package manager version is declared as `pnpm@10.30.3`.

**Alternatives considered**:

- Keep the starter workflow and add a package workflow: rejected because the old workflow's missing Conan setup and C++ compiler/build-type conflict would remain a competing required check.
- Rewrite all Conan/CMake commands in workflow YAML: rejected in favor of the existing tested configurator, with only explicit container environment/toolchain values added.

### 3a. Pin frontend and Conan build tools

**Decision**: Use Conan 2.34.0, Node.js 24.22.2, and pnpm 10.30.3; retain CMake 3.22.6 and Ninja 1.12.1 as the existing Conan tool-requires. Verify the official Node.js Linux x64 tarball against SHA-256 `361718c444e49af2ca5a110c83ba6a8901143f23ab285ebbef0341f391f482a9`.

**Rationale**: Conan 2.34.0 is the latest published Conan 2 documentation/release at planning time (2026-10-28). Angular 21 accepts Node 24; the selected Node 24 release is LTS and is available before the feature date. Exact tool versions remove implicit dependence on current aliases while retaining the existing project build tools.

**Alternatives considered**:

- Floating `conan`/Node/pnpm latest versions: rejected as non-reproducible.
- Replacing Angular or its package manager: rejected as outside scope.

### 4. Use CPack components and create an actual consumable CMake SDK

**Decision**: Add CMake install/export configuration for the current `isched` static library and `isched_srv`; use CPack DEB/RPM component packaging to make separate runtime and developer/library packages. Export `isched::isched`, install the supported public header closure, and provide a CMake config plus Conan 2 dependency manifest/instructions. Keep the SDK's dependencies explicitly Conan-managed and verify them from an external consumer project.

**Rationale**:

- The current source CMake project declares `isched` with `add_library` and `isched_srv` with `add_executable`, but contains no install/export rules. The default CMake library type is static in this repository; the existing Release build contains `libisched.a`.
- `src/main/cpp/isched/isched.hpp` describes itself as the complete public API convenience include and includes `backend/isched_Server.hpp`. The server header includes `isched_common.hpp`, `isched_config.hpp`, and Boost.URL, so headers and transitive dependencies must be treated as a coherent SDK interface.
- `target_link_libraries(isched PRIVATE ...)` and source-tree-relative include paths are not a usable installed consumer contract by themselves. Exported targets must carry installed include paths and resolved dependencies through a package config with `find_dependency`/Conan CMakeDeps integration.
- Native packages can carry target-native package metadata while CMake/Conan remain the project-native development integration. The developer package is independent from the runtime executable package; consumers receive a clear Conan dependency path, not an unsupported promise that the static archive is self-contained.

**Alternatives considered**:

- Copy `isched_srv` and `libisched.a` by path without CMake install/export rules: rejected because it leaves header locations, target usage requirements, and transitive link dependencies undefined.
- Change the library to shared or statically bundle every dependency: rejected as unnecessary runtime/build behavior change and an unverified change to licensing/linkage properties.
- Depend on `isched-runtime` from `isched-devel`: rejected because the static SDK can be used without the server executable; no direct role-to-role dependency is warranted.

### 5. Derive version, metadata, runtime dependencies, and file layout from verifiable inputs

**Decision**:

- Accept a project version only from an exact `vMAJOR.MINOR.PATCH` Git tag. For ordinary branch/PR builds use a clearly non-release version, `0.0.0+git.<short-sha>`, and record the full source SHA in the package metadata and artifact name.
- Install the server at `/usr/bin/isched_srv`; install the SDK in GNUInstallDirs locations (`include/isched`, `${libdir}`, and `${libdir}/cmake/isched`), plus the Conan manifest and usage/license docs below `${datadir}/isched`.
- Use `MPL-2.0` as the project's license metadata, include `LICENSE.md`, and retain third-party and Angular license notices for the binary's embedded/linked dependencies.
- Derive runtime shared-object dependencies from the actual output with native package tooling, then prove resolution in the clean target container. Do not ship generated application data or bootstrap credentials.

**Rationale**: No canonical project version variable, package version file, install prefix, package metadata, or package format convention exists today. `git describe` in the current repository resolves to a non-semantic archival tag, which is not an appropriate product version. The fallback keeps CI artifacts distinguishable without presenting an untagged commit as a release.

**Alternatives considered**:

- Reuse any available Git tag as the package version: rejected because the observed repository tag is not a product semver tag.
- Invent a new release version: rejected because this feature has no formal release publication scope or approved semantic version.
- Hard-code a list of SONAME dependencies: rejected because package requirements should be derived and verified from each built binary.

### 6. Keep CI artifacts unprivileged, independently attributable, and local-testable

**Decision**: Use `pull_request` (not `pull_request_target`) and push to `master`; set only required read permissions; use no repository secrets; pin third-party Actions by full commit SHA; keep `fail-fast: false`; upload separate immutable runtime and developer artifacts only after both packages pass validation. For local checks, use Docker BuildKit directly; do not require `act`.

**Rationale**:

- GitHub's Actions security guidance recommends least-privilege `GITHUB_TOKEN` permissions and warns against checking out untrusted pull-request code in privileged trigger contexts.
- A package build necessarily executes source and build scripts from a PR. The workflow does not need write access, publishing credentials, signing secrets, or protected environment credentials.
- Per-target jobs and stable names make failures and outputs independent from matrix execution order. Artifacts remain workflow-run downloads, not release assets.
- Docker and Buildx are available locally; `act` is absent, so a documented native Docker build/install path has fewer prerequisites.

**Alternatives considered**:

- `pull_request_target`, write-scoped token, or release/publish jobs: rejected because they add privilege and are outside requested scope.
- One shared artifact name or one matrix job with fail-fast behavior: rejected because names can collide and the first failure can obscure other target results.
- Require `act`: rejected; direct Docker is sufficient for the build/package path, while hosted Actions-specific artifact behavior is tested in GitHub.

## Public research references

1. Canonical Ubuntu release cycle and support status: <https://ubuntu.com/about/release-cycle?product=ubuntu&release=ubuntu&version=26.04+LTS>
2. Ubuntu 26.04 release notes: <https://documentation.ubuntu.com/release-notes/26.04/>
3. Ubuntu 26.04 GCC package metadata: <https://packages.ubuntu.com/resolute/gcc-15>
4. Ubuntu 26.04 x86_64 `g++-15` package metadata: <https://packages.ubuntu.com/resolute/amd64/g++-15>
5. Fedora Linux 45 announcement (2026-11-03): <https://fedoraproject.org/magazine/?article=announcing-fedora-linux>
6. Fedora release life cycle: <https://fedoraproject.org/wiki/Fedora_Release_Life_Cycle>
7. Fedora 45 `gcc-c++` package metadata: <https://packages.fedoraproject.org/pkgs/gcc/gcc-c++/fedora-45.html>
8. Docker image digests and platform-specific manifest guidance: <https://docs.docker.com/dhi/explore/security-concepts/digests/>
9. GitHub Actions secure-use guidance: <https://docs.github.com/actions/learn-github-actions/security-hardening-for-github-actions>
10. Angular version compatibility (Angular 21 supports Node `^20.19.0 || ^22.12.0 || ^24.0.0`): <https://angular.dev/reference/versions>
11. Node.js 24.22.2 LTS release: <https://nodejs.org/en/blog/release/v24.22.2>; official checksums: <https://nodejs.org/dist/v24.22.2/SHASUMS256.txt>
12. Conan 2.34.0 release notes: <https://docs.conan.io/2.34/changelog.html>
13. CMake 3.22 CPack component support: <https://cmake.org/cmake/help/v3.22/module/CPackComponent.html>

## Repository evidence

- `.github/workflows/cmake-multi-platform.yml`: current starter workflow; lacks Conan installation, lacks a matching Clang C++ compiler setting, and overrides Release with Debug.
- `configure.py`, `conanfile.txt`: current Conan 2 + CMake/Ninja build path and exact direct dependency declarations.
- `CMakeLists.txt`, `src/main/cpp/isched/CMakeLists.txt`: C++23 and embedded Angular build; current library/executable targets lack install/export configuration.
- `src/main/cpp/isched/isched.hpp`, `backend/isched_Server.hpp`: documented public API include and header dependency evidence.
- `src/ui/package.json`, `pnpm-lock.yaml`: Angular package scripts and pinned `pnpm@10.30.3`.
- `src/docker/Dockerfile`: historical Ubuntu 20.04 development container; it is not a package build workflow and includes an interactive shell/default legacy compiler profile, so it is not reused as-is.
- `README.md`: runtime data path is user-local by default; `--data-dir` allows isolated tests; `--seed` is explicitly a runtime operation and must not be used by packaging.
- Existing local Release build: `isched_srv` is an x86_64 ELF; its observed dynamic closure is the C++ runtime, math, GCC support, libc, and loader. Each target must still derive and test its own package requirements.
