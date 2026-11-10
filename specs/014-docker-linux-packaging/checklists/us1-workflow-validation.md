# Hosted target results

User authorized PR execution and completion work. Passing run:
[42707740390](https://github.com/fantastic-recoder/isched/actions/runs/42707740390).

| Named job | Conclusion | Completion (UTC) |
|---|---|---|
| Validate initial target matrix | success | 2026-11-09 22:00:10 |
| fedora-45 (rpm) | success | 2026-11-09 22:26:12 |
| ubuntu-26.04 (deb) | success | 2026-11-09 22:31:22 |

Workflow uses `fail-fast: false`, no `continue-on-error`, and per-target
nonzero failures. Historical failures are linked in us1-failure-validation.md.
T032 remains pending a controlled failure of one target while the other
finishes successfully; history with both jobs failing does not meet that gate.

## Current execution boundary — 2026-11-10

Local T030/T031 passed after the approved Perl-only correction at package
source `410fd0342dbc9f59f7556c0ff8c4b33231182a95`; see the local and negative
validation checklists. These do not replace T032 hosted evidence.

**T032 remains blocked and unchecked**. Current user authorization explicitly
excludes pushes, hosted triggers and uploads. No authorized controlled
single-target failing run with the other target succeeding was supplied.
No new hosted run was triggered or inspected by this recovery execution;
the historical run above is not a new verification of the corrected source.
To resume, supply authorized passing/failing hosted run evidence or explicitly
authorize the bounded hosted experiment and its publication boundary.
Later hosted downloads and the contributor's four actual SC-005 selections
are still separate requirements. Do not close the feature or skip phase
dependencies while those gates remain unavailable.
