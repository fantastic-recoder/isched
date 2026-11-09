# Implementation readiness — Spec 014

## Execution boundary

Execution started 2026-11-09 at
`c0acb7bc9df46cecdd6fed5ce4062e079ac3553e`, branch
`014-docker-linux-packaging`, clean worktree. Constitution v2.3.0 and root
`AGENTS.md` apply. No extension hooks file exists. Requirements checklist:
16 completed / 16 total. Do not regenerate planning documents, commit, push,
trigger hosted runs, merge, archive, or publish releases.

## Qualifications to the planning PASS

The planning PASS is not implementation evidence. Required remaining decisions:

1. Verify both supplied amd64 image digests, actual OS identities and compiler
   package availability; pin the additional OS dependency closure or verified
   dated repositories. Never substitute releases, compilers or floating inputs.
2. Resolve and hash-lock Conan 2.34.0's Python dependencies, validate target
   interpreter compatibility and GCC 15/16 settings/recipe support, then lock the
   complete producer and SDK recipe graphs. Availability is not compatibility.
3. Inventory the public-header and static-link closure from `isched.hpp`.
   Installed exports must not embed producer/Conan-cache paths. An external
   client must call a real symbol, compile, link and run without the runtime
   package. OpenSSL/Boost components must be explicitly resolved.
4. Preserve canonical fallback version `0.0.0+git.<short-sha>`. Native DEB/RPM
   encodings must round-trip to that identity and full SHA in embedded metadata.
   Decide only after native parsing/ordering tests (including exact tagged
   versions); do not assume RPM accepts the canonical `+` representation.
5. Keep helpers under `packaging/scripts/` and supply the quickstart's
   `tools/package/validate_artifacts.py` compatibility wrapper (user refinement:
   Python helpers, argument-list subprocess calls, explicit resource ownership).
6. Require fresh Angular test/build/embed output; current warning/stale-header
   fallback is not acceptable for packaging. No GraphQL/auth/UI behavior changes.
7. Exclude test/tool-only dependencies from distributed license inventories;
   preserve linked/embedded notices and record legal-review uncertainty.
8. Use the actual checkout SHA throughout, including PR merge checkouts.

System dependencies are justified solely for the native build/package boundary:
distro compiler and libc/loader ABI, Python/venv for Conan/helpers, native
DEB/RPM/ELF tooling for format/dependency/install checks, and Docker/BuildKit for
isolation. Library dependencies remain Conan-managed; frontend dependencies stay
pnpm-lockfile-managed. These do not authorize replacing Conan libraries with
system development libraries or changing application behavior.

## Pin verification performed

Commands were run without credentials or repository uploads:

```sh
docker buildx imagetools inspect ubuntu:26.04@sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725
docker buildx imagetools inspect fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88
docker run --rm --platform linux/amd64 ubuntu:26.04@sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725 sh -ec 'cat /etc/os-release; dpkg --print-architecture; apt-get update -qq; apt-cache policy g++-15 gcc-15; apt-cache madison g++-15 gcc-15'
docker run --rm --platform linux/amd64 fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88 sh -ec 'cat /etc/os-release; uname -m; dnf --refresh list --showduplicates gcc-c++ gcc'
curl --fail --location https://nodejs.org/dist/v24.22.2/SHASUMS256.txt
curl --fail --location https://pypi.org/pypi/conan/2.34.0/json
conan list 'cmake/3.22.6#*' -r conancenter
conan list 'ninja/1.12.1#*' -r conancenter
```

All commands exited zero. Both manifests exist and containers start on amd64.
Ubuntu reports VERSION_ID=26.04; both gcc-15/g++-15 versions
`15.2.0-16ubuntu1` are available. Fedora reports VERSION_ID=45, x86_64;
`gcc`/`gcc-c++` `16.2.1-2.fc45.1` exist in `fedora`; a newer updates version
also exists and must **not** be silently selected.

The official Node checksum list agrees with research for the **tar.gz** archive:
`361718c444e49af2ca5a110c83ba6a8901143f23ab285ebbef0341f391f482a9`.
The archive itself has not yet been downloaded/checksum-verified.
PyPI confirms Conan 2.34.0 (Python >=3.8, not yanked), wheel SHA-256
`4e268bb9ad38614141c7274a769c7226122e3660e4d5eb545a0c7f7170b53272`.
Conan Center lists recipe revisions for CMake 3.22.6 and Ninja 1.12.1.
These checks do **not** prove dependency resolution, target builds or SDK ABI.

Host versions are not the required build pins: Conan 2.23.0, CMake 4.3.0,
Ninja 1.13.2, Node 20.19.5, pnpm 10.33.0. Packaging may not silently use them.
Docker Engine 29.9.2 and Buildx 0.38.1 are available. Missing-version validation
justifies an isolated pinned tool environment; no global upgrade is authorized.

## Required evidence gates

- Target-local gates: full Release CTest (nonzero tests), native metadata/payload,
  runtime installation/loader/health/UI/asset, and SDK compile/link/run on **both**
  clean target releases. No success claim without all results.
- Hosted gates T032/T038/T043: explicitly authorized run URLs, attributable target
  outcomes, retrieved packages/checksums and controlled failure isolation. No
  authorization has been supplied; no run is triggered by this execution.
- Human gate T039/SC-005: the project's one contributor selects all four initial
  target/role artifacts and records correct identity and elapsed time <=60 seconds
  per selection using only visible workflow artifact metadata. Evidence pending;
  automation/naming tests cannot satisfy it.
- Final full Debug CTest remains mandatory before the parent's eventual commit.
  Logs belong outside git; unavailable/failed prerequisites leave tasks unchecked.
