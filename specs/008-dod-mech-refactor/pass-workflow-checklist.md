# Refactor Pass Workflow Checklist

A pass is accepted only when each stage is completed in order.

## Stage Order

- [ ] Stage 1 complete: Analyze hot path and capture baseline metrics.
- [ ] Stage 2 complete: Document before/after data layout and index mapping.
- [ ] Stage 3 complete: Apply logic refactor for selected hot loop.
- [ ] Stage 4 complete: Relevant tests green and affected-scope coverage >=80 line/branch.
- [ ] Stage 5 complete: Post-pass metrics captured and docs/performance summary finalized.

## Mandatory Artifacts

- [ ] `affected-scope.txt`
- [ ] `perf-baseline.txt`
- [ ] `pass-summary.md`
- [ ] `ctest-green.txt`
- [ ] `coverage.txt`
- [ ] `perf-post.txt`
- [ ] `performance-summary.md`
- [ ] `refactor-pass-artifact.json`
- [ ] `schema-validation.txt`

## Acceptance Gates

- [ ] `ctest --output-on-failure` passes for relevant suites.
- [ ] Coverage gate script reports line >=80 and branch >=80 for affected scope.
- [ ] Artifact JSON validates against `contracts/refactor-pass-artifact.schema.json`.
- [ ] No unmitigated selected-metric regression in post-pass comparison.

