# Final validation

Status: pending integrated closeout changes and all acceptance evidence.

## Existing successful baseline

- Branch head `e9f8b0ff22c3e6ab4467e635591837d9fd375d6b`.
- PR merge/package revision `d7771ccdd9b20bd2e298f173907587a036373b53`.
- Hosted run 42707740390: Ubuntu/Fedora full Release CTest, 61/61 each;
  both native roles validated; clean runtime and external SDK passed.
- Pre-commit local command:
  `ctest --test-dir cmake-build-debug --output-on-failure`, 61/61.

## Remaining final checks

- Integrated implementation at the upcoming commit: on 2026-11-10,
  `python3 -m unittest discover -s tests/package -p 'test_*.py'` ran 67 tests,
  successful with 4 environment-dependent skips. Then
  `ctest --test-dir cmake-build-debug --output-on-failure` passed all 61/61
  tests without exclusions (154.57 seconds). These are host-safe/full Debug
  results, not a substitute for target/container execution.
- Obtain both-target successful hosted upload/download/checksum/native evidence
  at the final implementation revision.
- Controlled single-target failure: failed target uploads neither role;
  independent passing target publishes both; workflow fails overall.
- Run the real independently pinned additional-target pipeline; show initial
  target identities unchanged and production set still exactly two.
- Record four real contributor selections (SC-005).
- Review final threat model, requirement mapping, worktree and commit status.

The local optional `security_scan` target has repeatedly failed before scanning
because `cmake-build-debug/compile_commands.json` is missing. No clean scan
result is claimed. This is separate from the mandatory full CTest gate.
