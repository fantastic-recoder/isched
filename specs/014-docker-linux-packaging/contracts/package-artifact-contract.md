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
- Required headers below `${includedir}/isched`: `isched.hpp`, `backend/isched_Server.hpp`, `backend/isched_common.hpp`, and `shared/config/isched_config.hpp`, preserving relative layout. Do not silently export private/generated backend headers.
- Required build integration: relocatable CMake config and exported target `isched::isched`.
- Required dependency integration: package-provided Conan 2 dependency manifest/instructions for the exact library link graph; installed CMake config resolves the corresponding Conan CMakeDeps targets. It must not refer to source or build paths from the container.
- Include license and third-party notices.
- Does not require the `isched-runtime` executable package; package metadata must not invent that dependency.
- The exported target requires C++23 and public Boost.URL usage requirements; the Conan manifest/config resolves the static link closure listed in `plan.md`, including OpenSSL, Boost filesystem and platform loader/thread dependencies. Test-only Catch2 is not part of that link interface.

## Version, source and filenames

- A project version is taken only from an exact Git tag matching `vMAJOR.MINOR.PATCH`. Non-tag builds use a conspicuously non-release version `0.0.0+git.<short-sha>`.
- `<short-sha>` is the first 12 lowercase hex digits of the full checkout SHA. Reject ambiguous exact-at-HEAD semver tags.
- DEB native version equals canonical version. Tagged RPM uses semver `Version` and `Release=1`; snapshot RPM uses `Version=0.0.0` and `Release=0.git.<short-sha>`. Embedded metadata retains the canonical version regardless of native encoding. Native parsing and ordering must be tested.
- Store the full source commit SHA in package metadata and accompanying artifact metadata.
- Artifact name format: `isched-{role}-{version}-{distro}-{distro-version}-x86_64-{deb|rpm}-{full-sha}`.
- Package-manager filenames must remain valid in each format, and the uploaded artifact name must be unique without relying on matrix order.
- Artifacts are immutable workflow-run downloads. This contract does not define a release asset, repository publication, signing key, or package-repository lifecycle.

Each role artifact contains exactly one native package and `metadata.json`.
The same identity object is embedded under
`${datadir}/isched/{runtime|developer}/package-identity.json` in the package,
excluding the self-referential package checksum. Required identity fields:
`role`, `package_name`, `version`, `native_version`, `native_release`,
`source_revision`, `target`, `distribution`, `distribution_version`,
`architecture`, `native_architecture`, `format`, `artifact_name`,
`package_filename`. Accompanying `metadata.json` adds `package_sha256` computed
from final package bytes. Use `native_release=""` for DEB. Unknown/missing
identity fields, invalid SHA/checksum, or disagreement with native metadata,
filename, target or embedded identity must fail validation.

## Workflow contract

Packaging orchestration and validation entry points are Python:
`packaging/scripts/build_local.py`, `build_target.py`, `validate_packages.py`,
`verify_runtime.py`, `verify_sdk.py`, and
`tools/package/validate_artifacts.py`. Invoke them with `python3`. Commands use
argument lists without shell evaluation; runtime and container checks must clean
up their owned processes and temporary resources on success, error or timeout.

- Local build CLI: `python3 packaging/scripts/build_local.py --target TARGET --revision FULL_SHA --output DIR`. Targets come from `packaging/targets.json`. Verify revision equals checkout, resolve version, and supply explicit Docker build arguments `ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION`; `.git` is not in the build context.
- Per-target validator CLI: `python3 packaging/scripts/validate_packages.py --target TARGET --revision FULL_SHA --directory DIR`.
- Aggregate wrapper CLI: `python3 tools/package/validate_artifacts.py --ubuntu DIR --fedora DIR --revision FULL_SHA`. Delegate to the shared validators; do not maintain a second validation policy.
- All helpers return zero only on success and nonzero with actionable errors on invalid input, timeout, build/test or validation failure. They must not evaluate inputs as shell code.
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

For runtime, install into a clean target-release environment without build toolchain packages, resolve dynamic dependencies, and start unprivileged with a disposable writable data path without `--seed`. Verify the existing GraphQL health response, embedded UI and a referenced asset. For developer, install into a clean environment without the runtime package and compile/link/run an external CMake consumer calling a real out-of-line library symbol, using only the installed package and its documented Conan dependency provisioning.

On any failed validation, do not upload either package for that target. Preserve per-target failure logs and the other matrix target's independent result.

Both-role validation is a publication prerequisite, not a transactional upload
guarantee. If uploading the second validated role fails, the first can remain
downloadable; mark the target/run failed and do not claim a complete target.
