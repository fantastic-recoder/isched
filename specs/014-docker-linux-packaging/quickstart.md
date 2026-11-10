# Quickstart: Linux Package Build Validation

**Status**: Implementation reference refreshed 2026-11-10. Both initial target
build/test/native/runtime/SDK pipelines passed hosted run 42707740390. Artifact
publication, the additional-target experiment and contributor timing remain
separate gates; see `checklists/completion-evidence.md`.

**Repository root**: `/home/groby/dev/isched`. Commands below run from this
absolute checkout path; change it explicitly when using a different checkout.

## Prerequisites

- Docker with BuildKit/Buildx and permission to run containers.
- x86_64 host; network access to the pinned base images and locked dependency sources.
- Git checkout at the revision to validate.
- No GitHub token, registry credential, deploy key, or application credential is required.
- `act` is optional and is not required.

## Local package build

Use the shared Python wrapper from a clean checkout to validate the actual
checkout revision, derive its canonical version and build each target with
Docker BuildKit, exporting packages to distinct directories:

```bash
cd /home/groby/dev/isched
REVISION="$(git rev-parse HEAD)"

python3 /home/groby/dev/isched/packaging/scripts/build_local.py \
  --target ubuntu-26.04 --revision "$REVISION" \
  --output /home/groby/dev/isched/dist/ubuntu-26.04

python3 /home/groby/dev/isched/packaging/scripts/build_local.py \
  --target fedora-45 --revision "$REVISION" \
  --output /home/groby/dev/isched/dist/fedora-45
```

The wrapper invokes target Dockerfiles with `--platform linux/amd64`, explicit
`ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION` build arguments, and
BuildKit local output export. `.git` is excluded from Docker context; do not
expect `git describe` to work inside it. The revision is the checked-out commit,
including GitHub's merge commit for a PR merge checkout.

The Dockerfiles use the immutable base-image references in [research.md](research.md), a pinned Release build, the Conan lockfile, and exact tool versions. They must require fresh Angular test/build/embed output, run the full nonempty CTest suite and validate both roles before exposing either package in the exported directory. Missing pins or incompatible recipes are errors, not permission to change releases/tool versions.

## Inspect package identity and payload

Use native package tooling in disposable containers and the repository's
package-validation helper (which requires matching native inspection tools):

```bash
cd /home/groby/dev/isched
python3 /home/groby/dev/isched/tools/package/validate_artifacts.py \
  --ubuntu /home/groby/dev/isched/dist/ubuntu-26.04 \
  --fedora /home/groby/dev/isched/dist/fedora-45 \
  --revision "$(git rev-parse HEAD)"
```

Expected output: two valid packages per directory. The report must show role, package version, source SHA, exact target release, native architecture, format, expected content paths, and validation status. The command must return non-zero for a missing/empty file, wrong extension or internal format, wrong architecture/release, missing required payload/license, source-revision mismatch, or unexpected seed/config data.

Each uploaded role contains one package plus `metadata.json`, including its
SHA-256 and the identity fields defined in the contract. Compare that metadata
with the package's embedded identity and native metadata. Snapshot RPM
`Version=0.0.0, Release=0.git.<12-sha-digits>` must map back to canonical
`0.0.0+git.<12-sha-digits>`; DEB uses the canonical version directly.

## Verify installed runtime in clean containers

Install the runtime package in a fresh container based on the same target release without build tools. Run the server unprivileged with a temporary writable data directory, do not seed, and confirm the server process starts and serves its existing embedded WebUI, a referenced asset and GraphQL health response. Verify all ELF shared-library dependencies resolve using the target's native loader tooling. Remove owned processes, the container and its temporary state after the check, including on failure/timeout.

The test is deliberately not a permanent system service or a host installation. It verifies package-manager installation and runtime closure without relying on the packaging build container's compiler and development libraries.

The shared Docker build already performs this check. To repeat against an
exported package, use the actual full-identity filename:

```bash
python3 packaging/scripts/verify_runtime.py --target ubuntu-26.04 \
  --package dist/ubuntu-26.04/<runtime-package>.deb \
  --evidence /tmp/ubuntu-runtime-verification.json
```

Use `fedora-45` and its RPM for Fedora. The helper configures and sends an
exact loopback Origin for GraphQL, and reports HTTP rejections explicitly.

## Verify the developer package

Install the `isched-devel` package in a fresh same-target container or disposable VM, provision the exact declared Conan dependencies using the shipped SDK dependency manifest and Conan 2, then configure a small CMake consumer that:

1. Includes the supported `<isched/isched.hpp>` header.
2. Calls `find_package(isched CONFIG REQUIRED)`.
3. Links the exported `isched::isched` target.
4. Compiles and links successfully without source-tree paths or build-directory artifacts.
5. Calls an existing out-of-line symbol, for example
   `isched::v0_0_1::backend::is_lifecycle_trace_enabled()`, and runs successfully
   without installing the `isched-runtime` executable package.

The installed public header closure is `isched.hpp`,
`backend/isched_Server.hpp`, `backend/isched_common.hpp` and
`shared/config/isched_config.hpp` beneath `${includedir}/isched`. Use the
shipped Conan manifest/lock/profiles and compatible compiler/ABI for the static
link dependencies enumerated in `plan.md`; do not rely on producer Conan cache
paths. This check is required to prove SDK usability; merely finding
`libisched.a` and headers, or compiling without calling a library symbol, is not
sufficient.

```bash
python3 packaging/scripts/verify_sdk.py --target fedora-45 \
  --package dist/fedora-45/<developer-package>.rpm \
  --consumer tests/package/sdk-consumer \
  --evidence /tmp/fedora-sdk-verification.json
```

The helper installs/activates pinned CMake before building dependencies and
configures the external consumer with `CMAKE_BUILD_TYPE=Release`, matching the
shipped Conan profiles. See `packaging/sdk/README.md` for downstream commands.
These helper commands create disposable containers; do not install on the host.

## GitHub Actions verification

Inspect a hosted run caused by an approved pull request or push to `master`.
The user authorized hosted builds and remaining completion work. For each
target, inspect the independent job result and verify:

- Build and full CTest both complete successfully.
- Both package roles pass target-native metadata/content and clean-install checks before artifact upload.
- Artifact names show role, project version, distribution, release, x86_64, format, and the full source SHA.
- A failure in either target remains visible without hiding the other target's outcome; the overall run is not green if either target fails.
- Artifacts are available only from that workflow run; there is no formal release publication.

Controlled invalid fixtures must prove target-specific failure and suppression
of both uploads after validation failure. Upload failure itself must fail the
target even if one already validated role artifact was uploaded. For US3, use a
temporary, separately pinned target experiment without changing the two-entry
initial production matrix.

## Single-contributor artifact selection (SC-005)

One contributor verifies all four named target/role combinations: Ubuntu 26.04 runtime, Ubuntu 26.04 developer, Fedora 45 runtime, and Fedora 45 developer. For each selection, use only the workflow run's visible artifact metadata to select the correct artifact and identify its distribution, distribution version, architecture, package format, and role within one minute. Record each selection's correctness and elapsed time in `checklists/artifact-usability.md`; all four must pass. No additional contributors or recruitment are required. This is a verification procedure, not a claim that the check has already passed.

## Completion boundary

`checklists/us1-validation.md` records passed hosted build and clean-integration
gates. The earlier implementation-readiness notes are historical, not current
authorization limits. Do not equate naming tests with actual artifact downloads,
identity fixtures with a real additional-target run, or automated selection
with SC-005 contributor evidence. Archive/merge only after all required gates.
