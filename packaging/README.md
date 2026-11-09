# Linux native packaging

Spec 014's implementation is in progress. This document does **not** claim that
packages, clean-install checks, or hosted artifacts have passed.

## Contract and layout

The initial targets are exactly Ubuntu 26.04 LTS amd64 DEB and Fedora 45 x86_64
RPM, both built with Docker `linux/amd64`. Each target must produce independently
installable `isched-runtime` and `isched-devel` packages. The artifact role for
`isched-devel` is `developer`; its CPack component is `devel`.

- `docker/{ubuntu,fedora}/`: target-specific, digest-pinned build definitions.
- `scripts/`: shared build, identity, validation, and clean-install helpers.
- `sdk/`: supported public headers, CMake export inputs, Conan consumer graph.
- `conan/profiles/`: explicit Release/C++23/compiler/ABI profiles.
- `targets.json`: verified target inputs (not floating release aliases).
- `tests/package/` (at repository root): native contract, failure, runtime, and
  external SDK consumer fixtures.

The authoritative requirements are
[the specification](../specs/014-docker-linux-packaging/spec.md),
[the implementation tasks](../specs/014-docker-linux-packaging/tasks.md), and
[the package contract](../specs/014-docker-linux-packaging/contracts/package-artifact-contract.md).
Evidence and unresolved gates live in
[`checklists/`](../specs/014-docker-linux-packaging/checklists/).

## Intended shared interfaces — not yet executable

Do not invoke these paths until their implementation tasks are verified:

```text
python3 packaging/scripts/build_local.py --target TARGET --revision FULL_SHA --output DIR
python3 packaging/scripts/validate_packages.py --target TARGET --revision FULL_SHA --directory DIR
python3 tools/package/validate_artifacts.py --ubuntu DIR --fedora DIR --revision FULL_SHA
```

Docker context excludes `.git`, host build trees, dependency caches, generated
WebUI assets and application state. The caller must compute the revision from
`git rev-parse HEAD`, resolve an exact-at-HEAD conventional semver tag or the
canonical `0.0.0+git.<short-sha>` fallback, and supply **both**
`ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION` as explicit build arguments.
A PR merge checkout must be labeled with its merge revision, not the PR head.
No environment credentials or Git authentication should be passed to Docker.
The eventual shared build wrapper must reject a mismatched revision.

## Safety and evidence boundaries

- Never install packages onto the host. Runtime/SDK verification uses fresh,
  exact-target containers, with disposable state and only owned processes.
- Never run `--seed`, ship application databases, or install service state.
- Runtime must serve the embedded WebUI and existing GraphQL health response.
- SDK validation must compile, link **and run** an external client that calls an
  existing library symbol, outside the producer source/build/cache trees.
- Both roles must pass full target CTest, native checks and clean integrations
  before either is staged/uploaded. No package is an unsigned formal release.
- Hosted execution/download evidence needs authorization; local simulations do
  not establish hosted publication behavior.
- SC-005 needs one real contributor's four target/role choices, correct within
  one minute **each**, from visible artifact metadata. Never synthesize timings.
- No commit, push, merge, archive or release is part of this execution. The
  parent owns the eventual artifact commit and its full-green CTest gate.
