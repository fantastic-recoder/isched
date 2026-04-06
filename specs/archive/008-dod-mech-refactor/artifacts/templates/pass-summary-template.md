# Refactor Pass Summary Template

## Pass Metadata

- Pass ID:
- Date:
- Owner:
- Scope:

## Stage 1: Analyze Hot Paths

- Workload ID:
- Baseline command:
- Hot-path findings:
- Selected bottlenecks:

## Stage 2: Redefine Data Layout

### Before Layout

- Representation:
- Traversal pattern:
- Locality pain points:

### After Layout

- Representation:
- Traversal pattern:
- Index mapping rules:
- Guard/sentinel rules:

## Stage 3: Refactor Logic

- Branch-elimination changes:
- Stateless/system-style operations:
- Behavior-preservation notes:

## Stage 4: Tests and Coverage

- Relevant ctest suites:
- ctest evidence path:
- Coverage command:
- Coverage evidence path:
- Line coverage (%):
- Branch coverage (%):

## Stage 5: Performance and Documentation

- Baseline evidence path:
- Post-pass evidence path:
- Selected metric comparison:
- Non-regression decision:
- Docs/spec updates:

## Risks and Follow-ups

- Residual risks:
- Deferred optimizations:
- Rollback strategy:

