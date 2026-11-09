# Package and Workflow Artifact Contract

## Supported matrix

| Distribution | Release | Build platform | Native format | Package architecture |
|---|---|---|---|---|
| Ubuntu | 26.04 LTS | `linux/amd64` | DEB (`.deb`) | `amd64` |
| Fedora | 45 | `linux/amd64` | RPM (`.rpm`) | `x86_64` |

The matrix contains exactly these two entries initially. A target addition must add a complete distribution/release/architecture/format identity, pinned build inputs, and validation without relabeling existing targets.

## Package roles and payload contract

### Runtime package

- Package name: `isched-runtime`.
- Required executable: `/usr/bin/isched_srv`.
- The server includes the embedded Angular WebUI; no separate UI runtime package or Node server is required.
- Native package metadata declares automatically derived runtime library requirements. The release-specific install smoke test proves that those requirements are sufficient.
- Include the project license and applicable third-party/Angular notices and package documentation.
- Must not contain a generated database, bootstrap administrator, credential, secret, signing key, development source tree, Conan cache, test binary, or build tool.

### Developer/library package

- Package name: `isched-devel`.
- Required library: the existing `isched` static library (`libisched.a`) unless a separately reviewed technical reason changes that design.
- Required headers: the supported public-header closure, installed under `include/isched` with source-relative include layout preserved.
- Required build integration: relocatable CMake config and exported target `isched::isched`.
- Required dependency integration: package-provided Conan 2 dependency manifest/instructions for the exact library link graph; installed CMake config resolves the corresponding Conan CMakeDeps targets. It must not refer to source or build paths from the container.
- Include license and third-party notices.
- Does not require the `isched-runtime` executable package; package metadata must not invent that dependency.

## Version, source and filenames

- A project version is taken only from an exact Git tag matching `vMAJOR.MINOR.PATCH`. Non-tag builds use a conspicuously non-release version `0.0.0+git.<short-sha>`.
- Store the full source commit SHA in package metadata and accompanying artifact metadata.
- Artifact name format: `isched-{role}-{version}-{distro}-{distro-version}-x86_64-{deb|rpm}-{full-sha}`.
- Package-manager filenames must remain valid in each format, and the uploaded artifact name must be unique without relying on matrix order.
- Artifacts are immutable workflow-run downloads. This contract does not define a release asset, repository publication, signing key, or package-repository lifecycle.

## Workflow contract

- Triggers: `pull_request` and pushes to `master`.
- No `pull_request_target`, release event, protected-environment job, or release publication.
- Each matrix target runs independently with `fail-fast: false`.
- Each target configures/builds using Conan 2 + CMake + Ninja in its pinned Docker image and runs full CTest.
- Both package roles must build and validate before either role artifact is uploaded for that target.
- The target fails on a build/test/package/install/SDK check failure. A missing package is an error; no target may silently skip.
- Overall workflow success requires both target jobs to succeed.
- Artifact files and their names expose source revision, distribution, release, architecture, format, and role.

## Validation and integrity requirements

Before upload, validate native internal package format (not just suffix), architecture, release identity, source SHA, role/package name/version, complete required payload, license files, and package non-emptiness.

For runtime, install into a clean target-release environment without build toolchain packages, resolve dynamic dependencies, and start with a disposable data path without `--seed`. For developer, install into a clean environment and compile/link the minimal external CMake consumer using only the installed package and its documented Conan dependency provisioning.

On any failed validation, do not upload either package for that target. Preserve per-target failure logs and the other matrix target's independent result.
