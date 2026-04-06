# 009 Recovery Artifacts

This directory stores auditable evidence for feature `009-raise-pass-improvement-ratio`.

## Layout

- `pass-03` ... `pass-10`: per-pass evidence folders for each recovery pass.
- `improvement-ratio-ledger.json`: cumulative SC-002 ratio ledger carrying baseline `1/2` forward.
- `passes-rollup.md`: reviewer-friendly timeline from baseline to recovery threshold.
- `templates/`: JSON templates for per-pass artifacts, ledger records, and final decision records.

## Baseline Rules

- Baseline is immutable: `improvingCount=1`, `completedCount=2`.
- Denominator must include every completed pass in the recovery window.
- SC-002 is recovered once cumulative ratio is `>=90%` (minimum `9/10`).

