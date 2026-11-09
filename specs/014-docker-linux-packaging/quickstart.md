# Quickstart: Linux Package Build Validation

**Status**: Planning reference. The paths below describe the implementation this plan calls for and become executable when the follow-on implementation tasks are complete. No package Dockerfiles or package scripts are created by this planning phase.

## Prerequisites

- Docker with BuildKit/Buildx and permission to run containers.
- x86_64 host; network access to the pinned base images and locked dependency sources.
- Git checkout at the revision to validate.
- No GitHub token, registry credential, deploy key, or application credential is required.
- `act` is optional and is not required.

## Local package build

Build both targets directly with Docker BuildKit, exporting package outputs from the final scratch stage to distinct directories:

```bash
mkdir -p dist/ubuntu-26.04 dist/fedora-45

docker buildx build \
  --platform linux/amd64 \
  --file packaging/docker/ubuntu/Dockerfile \
  --output type=local,dest=dist/ubuntu-26.04 \
  .

docker buildx build \
  --platform linux/amd64 \
  --file packaging/docker/fedora/Dockerfile \
  --output type=local,dest=dist/fedora-45 \
  .
```

The Dockerfiles use the immutable base-image references in [research.md](research.md), a pinned Release build, the Conan lockfile, and exact tool versions. They should run the full CTest suite and validate both roles before exposing either package in the exported directory.

## Inspect package identity and payload

After implementation, use the native package tooling and the repository's package-validation helper:

```bash
tools/package/validate-artifacts.sh \
  --ubuntu dist/ubuntu-26.04 \
  --fedora dist/fedora-45 \
  --revision "$(git rev-parse HEAD)"
```

Expected output: two valid packages per directory. The report must show role, package version, source SHA, exact target release, native architecture, format, expected content paths, and validation status. The command must return non-zero for a missing/empty file, wrong extension or internal format, wrong architecture/release, missing required payload/license, source-revision mismatch, or unexpected seed/config data.

## Verify installed runtime in clean containers

Install the runtime package in a fresh container based on the same target release without build tools. Use a temporary data directory, do not seed, and confirm the server process starts and serves its existing embedded WebUI/GraphQL endpoint. Verify all ELF shared-library dependencies resolve using the target's native loader tooling. Remove the container and its temporary state after the check.

The test is deliberately not a permanent system service or a host installation. It verifies package-manager installation and runtime closure without relying on the packaging build container's compiler and development libraries.

## Verify the developer package

Install the `isched-devel` package in a fresh same-target container or disposable VM, provision the exact declared Conan dependencies using the shipped SDK dependency manifest and Conan 2, then configure a small CMake consumer that:

1. Includes the supported `<isched/isched.hpp>` header.
2. Calls `find_package(isched CONFIG REQUIRED)`.
3. Links the exported `isched::isched` target.
4. Compiles and links successfully without source-tree paths or build-directory artifacts.

This check is required to prove SDK usability; merely finding `libisched.a` and headers is not sufficient.

## GitHub Actions verification

Open a pull request or push to `master`. For each target, inspect the independent job result and verify:

- Build and full CTest both complete successfully.
- Both package roles pass target-native metadata/content and clean-install checks before artifact upload.
- Artifact names show role, project version, distribution, release, x86_64, format, and the full source SHA.
- A failure in either target remains visible without hiding the other target's outcome; the overall run is not green if either target fails.
- Artifacts are available only from that workflow run; there is no formal release publication.

## Single-contributor artifact selection (SC-005)

One contributor verifies all four named target/role combinations: Ubuntu 26.04 runtime, Ubuntu 26.04 developer, Fedora 45 runtime, and Fedora 45 developer. For each selection, use only the workflow run's visible artifact metadata to select the correct artifact and identify its distribution, distribution version, architecture, package format, and role within one minute. Record each selection's correctness and elapsed time in `checklists/artifact-usability.md`; all four must pass. No additional contributors or recruitment are required. This is a verification procedure, not a claim that the check has already passed.
