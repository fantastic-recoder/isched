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
T032 is complete: the controlled single-target failure evidence is recorded below.

## Historical execution boundary — 2026-11-10

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

## T032 resolved — 2026-11-14

The contributor authorized the controlled experiment, pushes, and subsequent
removal of the injected failure. Inspected hosted run:
[43638361963](https://github.com/fantastic-recoder/isched/actions/runs/43638361963),
source revision `56ae9b474b846766a803ee9bf253c399a0d3c5b7`.

| Named job | Conclusion | Completion (UTC) |
|---|---|---|
| Validate initial target matrix | success | 2026-11-14 20:36:16 |
| ubuntu-26.04 (deb) | intentional failure before package build | 2026-11-14 20:36:33 |
| fedora-45 (rpm) | success | 2026-11-14 21:11:19 |

Ubuntu's build step failed and staging plus both uploads were skipped. Fedora
continued independently, passing build/verification, both-role staging, and
both artifact uploads. The overall run is correctly marked failed because of
the injected Ubuntu failure; this is not a two-target passing build claim.
This satisfies T032's controlled failure-isolation requirement. The deliberate
failure guard is now removed while `fail-fast: false` remains in place.
