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
