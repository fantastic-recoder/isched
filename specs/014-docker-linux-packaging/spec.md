# Feature Specification: Dockerized Linux Package Builds

**Feature Branch**: `014-docker-linux-packaging`  
**Created**: 2026-11-09  
**Status**: Draft — ready for planning  
**Input**: User description: "Add github actions which build packages for multiple platforms starting with ubuntu and fedora. The build should utilize docker."

## Clarifications

### Session 2026-11-09

- Initial targets: one explicitly pinned Ubuntu release and one explicitly pinned Fedora release, x86_64 only. Select and document exact release versions during planning.
- Package formats: Ubuntu `.deb` and Fedora `.rpm`.
- Package contents: separate runnable-server and developer/library distributions. The runtime distribution includes `isched_srv` with its embedded WebUI; the developer distribution provides library artifacts for development.
- Usability verification: the project currently has one contributor; SC-005 requires single-contributor verification of all initial target/role selections, not recruitment or a multi-contributor threshold.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build packages for the initial Linux targets (Priority: P1)

A contributor submits a change and the project automatically builds its distributable packages in Docker-based Ubuntu and Fedora environments. The contributor can tell whether each supported target completed or failed without confusing one target's result for another.

**Why this priority**: Building the requested initial targets is the primary value of the feature and provides feedback before changes are merged.

**Independent Test**: Run the package-build workflow against a change and verify that each configured Ubuntu and Fedora target produces a package or a target-specific failure result.

**Acceptance Scenarios**:

1. **Given** a change that builds successfully for every configured target, **When** the package workflow completes, **Then** the run reports success for each Ubuntu and Fedora target and produces an output for each.
2. **Given** a change that fails to build for one target, **When** the package workflow completes, **Then** the failed distribution, version, architecture, and package format are identifiable and the run does not report the entire matrix as successful.

---

### User Story 2 - Retrieve and identify a package (Priority: P2)

A maintainer or tester retrieves the package produced for a particular target and can determine which distribution, version, architecture, and package format it supports before using it.

**Why this priority**: Build outputs are useful only when people can retrieve the correct artifact and understand its compatibility.

**Independent Test**: Complete a successful workflow run, retrieve each output, and inspect its name and accompanying metadata to confirm that every target is unambiguous.

**Acceptance Scenarios**:

1. **Given** a successful workflow run, **When** a user opens its published build outputs, **Then** each target's package is available as a downloadable workflow artifact and is labeled with its distribution, version, architecture, and format.
2. **Given** multiple target outputs from one run, **When** a user selects an output, **Then** its identity does not depend on the order in which the matrix ran.

---

### User Story 3 - Extend the supported target set (Priority: P3)

A maintainer adds a future Linux distribution/version/architecture target while preserving the existing Ubuntu and Fedora build results and their identifying metadata.

**Why this priority**: Keeping target expansion straightforward supports the user's explicit goal of beginning with Ubuntu and Fedora and adding platforms later.

**Independent Test**: Add a representative additional target and verify that it produces a separately identified output while the original Ubuntu and Fedora outputs remain unchanged in meaning.

**Acceptance Scenarios**:

1. **Given** an additional target is added to the supported set, **When** the workflow runs, **Then** it reports and publishes a distinct output for that target without replacing or relabeling existing target outputs.

### Edge Cases

- A container image cannot be retrieved or started: the affected target must be reported as failed, not silently skipped or treated as successful.
- One target fails while another succeeds: results and artifacts remain attributable to their individual targets.
- A target produces no package or produces an output that does not match the selected package format: the target must fail validation and must not publish that output as a successful package.
- Two targets would otherwise produce identically named outputs: each output must still be distinguishable by its target identity.
- A package is built for an architecture different from the one declared by its target: it must not be presented as a valid artifact for that target.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST provide a GitHub Actions workflow that builds distributable packages for Ubuntu and Fedora using Docker-based build environments.
- **FR-002**: The supported build-target set MUST identify each target by distribution, distribution version, and architecture; these dimensions MUST remain individually visible in build results and artifact metadata.
- **FR-003**: Each configured target MUST produce a package in its selected package format. The initial matrix MUST contain exactly one explicitly pinned Ubuntu release and one explicitly pinned Fedora release, both x86_64. Planning MUST select and document the exact release versions before implementation; moving release aliases MUST NOT define target compatibility.
- **FR-004**: Ubuntu builds MUST produce native `.deb` packages and Fedora builds MUST produce native `.rpm` packages.
- **FR-005**: Each initial target MUST produce separate runnable-server and developer/library distributions in its native package format. The runtime distribution MUST deliver `isched_srv` with its embedded WebUI. The developer distribution MUST provide the library artifacts needed to develop against isched; planning MUST define its headers, libraries, dependency requirements, and build-system integration. Runtime and developer artifacts MUST be distinctly identified, with any dependency between them declared in package metadata. Additional archive formats are outside this initial scope.
- **FR-006**: A workflow run MUST report a clear success or failure for each configured target, and MUST NOT report overall success when any required target fails to build or validate its package.
- **FR-007**: A successful target output MUST be made available as a downloadable artifact of its workflow run. This feature does not publish or attach packages to a formal software release unless separately requested.
- **FR-008**: Every artifact and target result MUST be labeled so a user can identify its source revision, distribution, version, architecture, and package format without relying on matrix execution order.
- **FR-009**: The supported target set MUST be extendable to future platforms while preserving distinct, stable identification of the initial Ubuntu and Fedora outputs.
- **FR-010**: Package validation MUST confirm that each target produced an output matching its declared package format and target identity before that output is presented as successful.

### Key Entities *(include if feature involves data)*

- **Build target**: A supported combination of Linux distribution, distribution version, processor architecture, and selected package format.
- **Package artifact**: A distributable output associated with one build target, source revision, and package role (`runtime` or `developer`), including identifying metadata and its corresponding payload.
- **Workflow run**: A GitHub Actions execution that records the outcome of each configured build target and makes successful target artifacts available.

### Assumptions

- The workflow is intended to provide change-validation builds on pull requests and pushes to the repository's default branch; it is not a release-publication workflow.
- Docker is the required environment boundary for the distribution-specific build. Each package must be attributable to the container target whose distribution and version it declares.
- Ubuntu and Fedora are the initial distribution families, with one pinned release each and x86_64 only. Exact releases are an explicitly delegated planning decision.
- Repository inspection found no existing OS package-generation configuration or package-format convention. The agreed outputs are separate runtime and developer/library packages; the runtime includes `isched_srv` and its embedded Angular WebUI. Planning defines the developer payload and package dependencies without changing application functionality.
- The feature changes distribution of build outputs, not the application's GraphQL interface, UI behavior, or runtime features.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a passing workflow run, 100% of the agreed initial distribution/version/architecture targets produce both runtime and developer packages that pass the agreed format, role, and target-identity validation.
- **SC-002**: For every target in a workflow run, users can determine its success or failure and identify its distribution, version, architecture, and package format from that run without inspecting workflow internals.
- **SC-003**: Every successfully validated target package is downloadable from its workflow run, and no failed or invalid output is presented as a successful package.
- **SC-004**: A maintainer can add at least one future target while preserving distinguishable outputs and successful results for all existing initial targets.
- **SC-005**: In a single-contributor usability check, one contributor MUST correctly select the artifact for each named initial Ubuntu/Fedora target and runtime/developer role combination (four selections), and identify its distribution, distribution version, architecture, package format, and role within one minute per selection, using only the workflow run's visible artifact metadata.
