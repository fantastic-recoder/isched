# pass-01 summary: gql executor selection hot-loop

## Stage 1: Analysis findings

- Target hotspot: `GqlExecutor::process_field_selection` and `GqlExecutor::process_sub_selection` in repeated selection traversal.
- Bottleneck pattern: nested recursive traversal with repeated node-type branching for every selection level.
- Risk: refactor must preserve resolver dispatch ordering, null-propagation semantics, and error-path behavior.

## Stage 2: Data layout plan

### Before

- Traversal uses recursive call chains over AST nodes.
- Per-node processing repeatedly checks string node types while hopping between stack frames.

### After

- Build a contiguous `FieldNodeList` batch from each selection set using index-based traversal.
- Dispatch resolved fields from contiguous storage with one tight loop (`process_field_nodes`).
- Keep nested field semantics unchanged by not descending into field-owned sub-selection children at collection time.

### Index mapping

- Queue index (`idx`) drives traversal over pending nodes.
- Field dispatch index follows collected order from `FieldNodeList`.

## Stage 3: Logic refactor notes

- Added `collect_field_nodes` and `process_field_nodes` helpers.
- Replaced recursive selection traversal with flattened batch dispatch in hot paths.
- Preserved resolver behavior for default resolution, RBAC checks, and structured error reporting.

## Stage 4 and 5 evidence links

- ctest output: `specs/008-dod-mech-refactor/artifacts/pass-01/ctest-green.txt`
- coverage output: `specs/008-dod-mech-refactor/artifacts/pass-01/coverage.txt`
- baseline perf: `specs/008-dod-mech-refactor/artifacts/pass-01/perf-baseline.txt`
- post-pass perf: `specs/008-dod-mech-refactor/artifacts/pass-01/perf-post.txt`
- perf summary: `specs/008-dod-mech-refactor/artifacts/pass-01/performance-summary.md`

## Current gate outcome snapshot

- Relevant ctest suites: PASS.
- Affected-scope coverage gate: FAIL (`line=32.3%`, `branch=14.2%`, threshold `80/80`).
- Artifact schema validation: FAIL (schema requires `coverageGate.passed=true`).
