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

## Build, validate, and stage

These commands are implemented. The workflow invokes the same interfaces for
each configured target; the artifact preparation step validates both roles and
all target evidence before staging either role:

Hosted and local Docker pipelines use three parallel native build jobs for
the producer, Conan dependencies/tools, and external SDK consumer. CTest and
frontend workers remain limited to one; runtime and SDK verification stages
remain sequential.

```text
python3 packaging/scripts/build_local.py --target TARGET --revision FULL_SHA --output DIR
python3 packaging/scripts/validate_packages.py --target TARGET --revision FULL_SHA --directory DIR
python3 tools/package/validate_artifacts.py --ubuntu DIR --fedora DIR --revision FULL_SHA
python3 packaging/scripts/prepare_artifacts.py --target TARGET --revision FULL_SHA --directory DIR --output-dir ARTIFACT_DIR
```

Docker context excludes `.git`, host build trees, dependency caches, generated
WebUI assets and application state. The caller must compute the revision from
`git rev-parse HEAD`, resolve an exact-at-HEAD conventional semver tag or the
canonical `0.0.0+git.<short-sha>` fallback, and supply **both**
`ISCHED_SOURCE_REVISION` and `ISCHED_PACKAGE_VERSION` as explicit build arguments.
A PR merge checkout must be labeled with its merge revision, not the PR head.
No environment credentials or Git authentication should be passed to Docker.
The build wrapper and artifact staging helper reject a mismatched revision.

## Downloading workflow artifacts

Artifacts are attached only to their GitHub Actions workflow run. Open the
repository's **Actions** tab, select the Dockerized Linux packages run for the
commit, and download the named artifact from that run's **Artifacts** section.
Choose by its complete name rather than by matrix order. Each archive contains
exactly one native package and `metadata.json`; `runtime` contains
`isched-runtime`, and `developer` contains `isched-devel`.

Names have the form
`isched-{role}-{version}-{distribution}-{distribution-version}-x86_64-{format}-{full-sha}`.
For example, the four initial choices are identifiable as:

```text
isched-runtime-{version}-ubuntu-26.04-x86_64-deb-{40-digit-sha}
isched-developer-{version}-ubuntu-26.04-x86_64-deb-{40-digit-sha}
isched-runtime-{version}-fedora-45-x86_64-rpm-{40-digit-sha}
isched-developer-{version}-fedora-45-x86_64-rpm-{40-digit-sha}
```

`{version}` is either the exact tagged `MAJOR.MINOR.PATCH` version or
`0.0.0+git.{first-12-sha-digits}`. Replace the placeholders with the values
visible in the artifact name. After extracting an artifact, inspect
`metadata.json` for `role`, `package_name`, canonical `version`,
`native_version`/`native_release`, full `source_revision`, target distribution
and release, `architecture`, `native_architecture`, `format`, `artifact_name`,
`package_filename`, and `package_sha256`. Verify that the extracted package's
bytes match its recorded checksum before inspecting native package metadata.

For example, from the extracted archive:

```sh
package="$(python3 -c 'import json; print(json.load(open("metadata.json"))["package_filename"])')"
expected="$(python3 -c 'import json; print(json.load(open("metadata.json"))["package_sha256"])')"
printf '%s  %s\n' "$expected" "$package" | sha256sum --check
```

Then inspect the package format and native identity with the matching tool:

```sh
dpkg-deb --field "$package" Package Version Architecture   # Ubuntu DEB
rpm -qp --qf '%{NAME} %{VERSION}-%{RELEASE} %{ARCH}\n' "$package"  # Fedora RPM
```

These are unsigned CI validation outputs, not signed packages or formal
release assets. The checksum detects package-byte changes relative to the
artifact's own metadata; neither the checksum nor the metadata is signed or
authenticates the workflow source. Treat downloads, especially pull-request
artifacts, as untrusted: inspect them and do not install or execute them with
elevated privileges. This workflow does not publish to a release, package
repository, or other privileged artifact consumer.

## Safety and evidence boundaries

- Native payload validation reads DEB tar entries and resolves RPM cpio hard
  links before checking notice hashes; formatted file listings are not paths.
- Conan notices retain package/source/recipe namespaces so same-named license
  files cannot overwrite each other or invalidate inventory checksums.
  Discovery excludes `test` and `tests` subtrees but retains production notices.
- Never install packages onto the host. Runtime/SDK verification uses fresh,
  exact-target containers, with disposable state and only owned processes.
- Never run `--seed`, ship application databases, or install service state.
- Runtime must serve the embedded WebUI and existing GraphQL health response.
  The verifier allowlists its exact loopback origin and sends it on GraphQL
  requests; HTTP rejection responses fail immediately with status/body details.
- SDK validation must compile, link **and run** an external client that calls an
  existing library symbol, outside the producer source/build/cache trees.
- Both roles must pass full target CTest, native checks and clean integrations
  before either is staged/uploaded. These unsigned CI outputs are not formal
  releases.
- Workflow downloads are limited to the owning run; hosted execution/download
  evidence needs authorization, and local simulations do not establish hosted
  artifact availability or behavior.
- SC-005 needs one real contributor's four target/role choices, correct within
  one minute **each**, from visible artifact metadata. Never synthesize timings.
- No commit, push, merge, archive or release is part of this execution. The
  parent owns the eventual artifact commit and its full-green CTest gate.
