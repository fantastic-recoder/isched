# Phase 0 Research - 008-dod-mech-refactor

## Decision 1: Hot-path measurement strategy

- Decision: Use repeatable micro/macro measurements per selected subsystem using existing test executables plus Linux `perf stat` counters (cycles, instructions, branches, branch-misses) and wall-clock sampling under fixed inputs.
- Rationale: The repository already builds test executables through CMake/CTest on Linux. `perf stat` adds branch-level and CPU-level signals needed for branch-elimination decisions without introducing new runtime dependencies.
- Alternatives considered:
  - Introduce Google Benchmark: rejected for this pass because it adds a new dependency and integration overhead before baseline is established.
  - Rely only on wall-clock timing: rejected because branch/cycle-level effects are required by the feature constraints.

## Decision 2: Coverage gate enforcement for affected scope

- Decision: Enforce >=80% line and >=80% branch coverage for affected scope using compiler coverage flags and `gcovr` threshold checks scoped to touched backend/test paths.
- Rationale: Feature requirements mandate explicit line+branch thresholds; `gcovr` supports branch and line gates and can be constrained to pass scope.
- Alternatives considered:
  - Line-only coverage: rejected because branch coverage is mandatory.
  - Whole-repository coverage gate: rejected because the spec constrains gate to affected scope per pass.

## Decision 3: SoA transition strategy

- Decision: Migrate selected hot-loop state from object-centric layouts to index-aligned parallel arrays (SoA), preserving index consistency via shared logical record index.
- Rationale: Sequential iteration over contiguous arrays improves locality and makes cache behavior more predictable on hot loops.
- Alternatives considered:
  - Keep AoS and rely on compiler optimizations: rejected for selected hot paths where memory access pattern dominates.
  - Full global SoA rewrite in one change: rejected due to elevated regression risk; incremental pass-based migration is safer.

## Decision 4: Pointer-to-index replacement pattern

- Decision: Replace pointer cross-references in selected hot-path structures with index-based references into owning contiguous containers, with explicit invalid-index sentinel and guard-clause validation.
- Rationale: Index references avoid pointer chasing and improve data packing/locality while supporting deterministic bounds checks.
- Alternatives considered:
  - Raw pointer retention with pooling: rejected because pointer traversal keeps branchy/null-check-heavy behavior.
  - Opaque handle indirection layer: rejected for extra dispatch/lookup overhead on hot paths.

## Decision 5: Branch-elimination strategy on hot paths

- Decision: Apply branch elimination opportunistically using guard-clause early exits, branch consolidation, lookup-table/data-driven selection, and loop normalization when profiling indicates benefit.
- Rationale: Branch predictability and reduced conditional depth directly target mechanical-sympathy goals and FR-008.
- Alternatives considered:
  - Always rewrite conditionals into polymorphism: rejected where virtual dispatch or extra abstraction harms hot-path performance.
  - No branch refactoring: rejected because branch-cost evaluation is mandatory.

## Decision 6: Behavior-preservation validation

- Decision: Treat existing tests as non-regression contract and expand tests specifically for index validity, SoA alignment, and guard-clause behavior before pass acceptance.
- Rationale: The spec requires preserved behavior unless requirement updates are made in the same pass.
- Alternatives considered:
  - Rely only on manual validation: rejected as insufficient for refactor safety.

## Decision 7: Required pass artifacts and documentation rhythm

- Decision: Every pass emits a consistent artifact set: source diffs, test updates, coverage evidence, performance summary, and spec/docs update in the same branch.
- Rationale: FR-012 and acceptance scenarios require verifiable pass deliverables and incremental reviewability.
- Alternatives considered:
  - Batch documentation at end of initiative: rejected because it breaks pass-level acceptance criteria.

