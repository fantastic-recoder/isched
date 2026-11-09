# Threat Model: Dockerized Linux Package Builds

**Status**: Planning-stage mitigations; implementation and workflow evidence are not yet available.  
**Scope**: GitHub Actions PR and `master` validation; Docker-based Ubuntu/Fedora package builds; DEB/RPM and SDK artifact production and download.

## Assets

- GitHub repository contents, branch protection, and workflow permissions.
- Workflow-run artifact store and package identity metadata.
- GitHub-hosted runner and Docker daemon during a build.
- Package-manager metadata and package payload integrity.
- Build inputs: project source, pinned container images, Conan graph, CMake/Ninja, Node/pnpm dependencies.
- Project and third-party license notices.
- Application data and credentials, which must not enter the build artifact.

## Trust boundaries

1. Untrusted pull-request code and build scripts → GitHub-hosted runner/workflow.
2. Workflow runner/Docker daemon → per-target build container and its package repositories.
3. Conan/npm/package registries and downloaded dependencies → build environment.
4. Built package/container output → native package validation and clean target-release install check.
5. Validated workflow artifact → contributor/maintainer download and later local installation.

## Threats, mitigations, and residual risks

| Threat | Attack path / impact | Planned mitigation | Residual risk |
|---|---|---|---|
| PR code steals credentials or writes to repository | Malicious or compromised PR modifies build steps to exfiltrate secrets or mutate refs/releases | Use `pull_request`, not `pull_request_target`; provide no repository secrets; set minimum `GITHUB_TOKEN` read permission; no publishing, signing, or protected-environment jobs | PR code can still use network access and consume build resources; hosted runner and builder vulnerabilities remain possible |
| Untrusted build abuses Docker host | Dockerfile/build scripts attempt host filesystem access, socket access, privilege escalation, or lateral movement | Use ephemeral GitHub-hosted runner; do not mount host Docker socket into the build; do not use `--privileged`; do not bind-mount broad host paths or pass secrets/build args; export only the package output from BuildKit | Docker daemon/buildkit and kernel attack surface cannot be eliminated; keep hosted runner and Docker engine managed/current |
| Mutable or substituted build inputs | Tags or repositories change compiler/base/dependency versions, producing unexpected or compromised packages | Pin distro release plus per-architecture base-image digest; pin toolchain and build tools; lock Conan transitive graph; pin Node/pnpm and use pnpm lockfile; record source SHA and tool versions; review digest/lock refreshes | Upstream repositories and package registries remain trusted external inputs; updates require deliberate reviewed refreshes |
| Dependency cache poisoning | PR writes shared cache content later trusted by default-branch builds | Avoid cross-PR writable caches for native package inputs initially; if caching is later introduced, use immutable keys scoped to trust boundary and lockfile hashes | Repeated clean dependency resolution costs more time/network |
| Wrong/malformed package presented as successful | Build emits wrong extension, format, arch, role, target release, or empty content | Inspect native metadata and payload; compare against explicit matrix tuple and source SHA; fail on missing/duplicate/unexpected outputs; upload only after both roles validate | Validators may have bugs; continue checking installed packages and artifact metadata |
| Runtime package has undeclared dependencies | Package works only in the build image because toolchain/dev packages mask missing runtime dependencies | Derive DEB/RPM runtime dependency metadata from built ELF files and install/smoke-test in a clean target-release container with no build toolchain | Differences from downstream systems on the same release can remain; packages only claim the named distro release/architecture |
| SDK claims false usability or leaks build paths | Exported config references Conan cache/source paths or omits static-library link dependencies | Relocatable exported target; declared Conan CMakeDeps graph; installed-consumer compile/link test outside the source/build tree | SDK usage depends on compatible Conan 2 and target-ABI-compatible dependencies, which must be documented |
| Artifact used as a trusted release without review | Workflow package from a PR is mistaken for a release or used by a later privileged job | Label every artifact with PR/run context, version, full source SHA, target/role; no release publication; do not execute workflow artifacts in downstream privileged workflows | Users can still download and install an unreviewed PR build; artifacts are validation outputs, not signed releases |
| Credentials or seed state included in packages | Packaging copies a local database, test fixture, default credential, or secret | Fresh Docker build context and explicit payload allowlist; never run `--seed`; package content scan rejects database/credential/seed paths; runtime smoke uses temporary disposable data only | A future payload change can reintroduce a sensitive file unless content validation remains mandatory |
| License obligations omitted | Static linked dependencies or embedded Angular content are redistributed without notices | Include project `LICENSE.md`, Conan dependency license inventory, and Angular generated third-party notices in relevant package payloads | License expression/obligation interpretation requires maintainer review; tooling inventory is not legal advice |

## STRIDE snapshot

- **Spoofing**: package identity is tied to repository workflow run, full source SHA, target, and role; artifact names do not use matrix order.
- **Tampering**: immutable image digests, lockfiles, native format checks, and package content/metadata validation constrain substitution.
- **Repudiation**: the workflow run and full revision are retained in artifact identity and package metadata.
- **Information disclosure**: builds receive no secrets; no local host data mounts; package payload excludes application state and credentials.
- **Denial of service**: one target cannot hide the other (`fail-fast: false`); hosted job/resource limits remain the platform control.
- **Elevation of privilege**: least-privilege token, no privileged pull-request trigger, no Docker socket mount into build steps, no signing/release secrets.

## Required implementation evidence

1. Review trigger and permissions on the final workflow; confirm no secrets, privileged context, or release job.
2. Inspect Docker build invocations and stages for privilege, mounts, secret propagation, and output scoping.
3. Verify immutable platform image refs and locked tool/dependency inputs.
4. Prove package format/architecture/target/revision checks reject a deliberately invalid local fixture.
5. Prove clean runtime install/start and installed SDK consumer checks for both targets.
6. Inspect runtime and developer package payloads for databases, credentials, seed files, build outputs, and notices.
7. Keep workflow artifacts as untrusted build evidence; do not consume them in a privileged `workflow_run` pipeline.

## Out of scope

Formal release publication, repository hosting, package signing keys, deployment/service management, changing auth/API/runtime behavior, persistent JWT handling, REST clients, or seeded runtime state.
