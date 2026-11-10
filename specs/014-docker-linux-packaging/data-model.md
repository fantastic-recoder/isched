# Data Model: Dockerized Linux Package Builds

This feature models build outputs and workflow evidence; it does not add application-domain persistence.

## Entities

### Build Target

One immutable supported build tuple.

| Field | Type | Constraints |
|---|---|---|
| `distribution` | enum/string | `ubuntu` or `fedora`; explicit, never inferred from matrix order |
| `distribution_version` | string | `26.04` for Ubuntu or `45` for Fedora; exact release, not `latest`/`stable` |
| `architecture` | enum/string | `x86_64` in artifact identity; `amd64` for Ubuntu package metadata and `x86_64` for RPM metadata |
| `package_format` | enum | `.deb` for Ubuntu; `.rpm` for Fedora |
| `container_image` | OCI reference | Official image name and pinned amd64 manifest digest |
| `compiler` | toolchain identity | Explicit C and C++ compiler and pinned package version |

**Validation**: The supported initial target set has exactly two entries. Package payload architecture and target release metadata must agree with the corresponding tuple.

### Package Role

The two independently installable package components built for each target.

| Role | Package identity | Required payload |
|---|---|---|
| `runtime` | `isched-runtime` | `/usr/bin/isched_srv`, `LICENSE.md`, third-party/Angular notices and package documentation; no database, secrets, seed state, or deployment config |
| `developer` | `isched-devel` | `libisched.a`, supported public headers preserving the `isched/...` include layout, exported CMake config/targets for `isched::isched`, Conan 2 dependency manifest/instructions, licenses/notices |

**Relationships**: Both roles are produced by one build target and one source revision. The SDK package does not depend on the runtime executable package; its third-party link dependencies are resolved through the project-approved Conan 2 integration.

### Package Artifact

One native installable package plus its uploaded workflow artifact.

| Field | Type | Constraints |
|---|---|---|
| `role` | enum | `runtime` or `developer` |
| `package_name` | string | `isched-runtime` or `isched-devel` |
| `version` | string | Version from an unambiguous exact-at-checkout `vMAJOR.MINOR.PATCH` tag; otherwise `0.0.0+git.<first-12-sha-digits>` |
| `native_version` / `native_release` | strings | DEB version equals canonical version, no separate release; tagged RPM uses semver / `1`, fallback RPM uses `0.0.0` / `0.git.<first-12-sha-digits>` |
| `source_revision` | string | Full Git SHA, embedded in metadata and artifact identity |
| `distribution` / `distribution_version` | string | Inherited from Build Target |
| `architecture` | string | Native package metadata checked against x86_64 |
| `format` | enum | Native DEB or RPM only |
| `payload_manifest` | file list | Role-specific required files; no path outside expected package prefixes |
| `license_payload` | file list | Project MPL-2.0 license plus relevant linked/embedded dependency notices |
| `package_sha256` | lowercase hex string | 64 digits computed from the final package bytes and included in accompanying metadata |
| `validation_state` | enum | `built` → `validated` → `uploaded`; a failed validation cannot transition to `uploaded` |

**Artifact identity**: `isched-{role}-{version}-{distro}-{distro-version}-x86_64-{format}-{full-sha}`. Artifact names do not depend on matrix ordering.

### Workflow Run

One GitHub Actions run tied to one source revision and trigger.

| Field | Type | Constraints |
|---|---|---|
| `source_revision` | full Git SHA | Same value in package metadata and artifact names |
| `trigger` | enum | Pull request or push to default branch `master` |
| `target_results` | collection | Exactly one independent result per configured Build Target |
| `target_status` | enum | `pending`, `building`, `validating`, `succeeded`, `failed` |
| `uploaded_artifacts` | collection | Separate runtime and developer artifacts for a target, present only after both package files validate |

**State transitions**: A target moves from `pending` to `building`, then `validating`; it becomes `succeeded` only after build, complete nonempty CTest, both-role package validation, clean runtime install/smoke, SDK consumer compile/link/run, and both artifact uploads succeed. Any stage, including upload, may transition the target to `failed`. With `fail-fast: false`, one target's failure does not suppress the other target's result. The overall run succeeds only when all required targets succeed. Both role validations precede either upload; upload operations are not transactional, so an upload error can leave one validated artifact downloadable while the target remains failed.

### SDK Interface

The developer role installs `isched.hpp`, `backend/isched_Server.hpp`,
`backend/isched_common.hpp`, and `shared/config/isched_config.hpp` beneath
`${includedir}/isched`, plus `libisched.a`, relocatable config/targets for
`isched::isched`, and the consumer Conan manifest/lock/profiles.

**Relationships**: The exported target requires C++23, public Boost.URL headers,
and the library's static transitive link closure. Exact dependency versions and
target names are specified in `plan.md`; Catch2 and producer build paths are
not part of the installed SDK link interface. Consumer dependencies are resolved
under its own compatible Conan environment, not the producer cache.

## Package identity invariants

1. Every package is produced for one source SHA, target tuple, format, and role.
2. Runtime and developer packages are separate package-manager objects and separate downloadable artifacts.
3. Package filename extension, internal package format, package metadata architecture, and matrix target must agree.
4. Runtime payload contains the embedded-UI server executable; the WebUI is not a separate runtime web service.
5. Developer payload supplies the library, public header closure, build-system import, and dependency-resolution instructions.
6. No artifact contains initialized runtime data, a seeded administrator, user credentials, signing keys, or workflow secrets.
7. Invalid or missing package outputs are failures and cannot be uploaded as successful artifacts.
8. Embedded and accompanying identity agree with the actual checked-out source SHA, including PR merge checkouts. Build arguments transport that identity into a Git-free Docker context.
9. Canonical and native version fields round-trip without changing source identity; native format parsing and ordering require tests.
