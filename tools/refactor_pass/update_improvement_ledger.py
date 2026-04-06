#!/usr/bin/env python3
"""Append recovery-pass results to the 009 cumulative improvement-ratio ledger."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from datetime import datetime, timezone
from typing import Any

BASELINE_IMPROVING: int = 1
BASELINE_COMPLETED: int = 2
DEFAULT_BASELINE: dict[str, Any] = {
    "improvingCount": BASELINE_IMPROVING,
    "completedCount": BASELINE_COMPLETED,
    "sourceRef": "specs/008-dod-mech-refactor/artifacts/passes-rollup.md#baseline-1-2",
}


class LedgerError(Exception):
    """Raised when the ledger input/output state is invalid."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Update cumulative improvement-ratio ledger")
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        required=True,
        help="Path to improvement-ratio-ledger.json",
    )
    parser.add_argument(
        "--pass-artifact",
        dest="pass_artifacts",
        type=pathlib.Path,
        action="append",
        default=[],
        help="Path to one recovery-pass-artifact.json file (repeat for multiple passes)",
    )
    parser.add_argument(
        "--updated-at",
        type=str,
        default=None,
        help="Override ISO-8601 timestamp used in updatedAt/recordedAt",
    )
    parser.add_argument(
        "--print-only",
        action="store_true",
        help="Print resulting ledger JSON to stdout without writing the ledger file",
    )
    return parser.parse_args()


def now_iso(override: str | None = None) -> str:
    if override:
        return override
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def load_json(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise LedgerError(f"Expected JSON object in {path}")
    return data


def ratio_pct(improving: int, completed: int) -> float:
    return round((improving / completed) * 100.0, 2)


def bootstrap_ledger() -> dict[str, Any]:
    return {
        "baseline": DEFAULT_BASELINE.copy(),
        "entries": [],
        "currentSummary": {
            "improvingCount": BASELINE_IMPROVING,
            "completedCount": BASELINE_COMPLETED,
            "improvementRatioPct": ratio_pct(BASELINE_IMPROVING, BASELINE_COMPLETED),
            "sc002Compliant": False,
        },
        "updatedAt": now_iso(),
    }


def ensure_baseline(ledger: dict[str, Any]) -> None:
    baseline = ledger.get("baseline")
    if not isinstance(baseline, dict):
        raise LedgerError("Ledger baseline is missing or invalid")

    if baseline.get("improvingCount") != BASELINE_IMPROVING or baseline.get("completedCount") != BASELINE_COMPLETED:
        raise LedgerError("Baseline is immutable and must remain improving=1/completed=2")

    if not baseline.get("sourceRef"):
        baseline["sourceRef"] = DEFAULT_BASELINE["sourceRef"]


def parse_pass_artifact(path: pathlib.Path) -> dict[str, Any]:
    if not path.exists():
        raise LedgerError(f"Pass artifact not found: {path}")

    artifact = load_json(path)
    pass_id = artifact.get("passId")
    classification = artifact.get("classification", {})

    if not isinstance(classification, dict):
        raise LedgerError(f"Invalid classification object in {path}")

    improvement = classification.get("improvement")
    regression_status = classification.get("regressionStatus")
    counts_toward_recovery = bool(classification.get("countsTowardRecovery", False))

    if not pass_id or not isinstance(pass_id, str):
        raise LedgerError(f"Missing/invalid passId in {path}")
    if improvement not in {"Improving", "NonImproving", "Blocked"}:
        raise LedgerError(f"Invalid improvement classification in {path}")
    if regression_status not in {"None", "Mitigated", "Unmitigated"}:
        raise LedgerError(f"Invalid regression status in {path}")

    evidence_refs = [str(path)]
    for key in ("metricEvidence", "gates", "artifacts"):
        if key in artifact:
            evidence_refs.append(f"{path}#{key}")

    return {
        "passId": pass_id,
        "classification": improvement,
        "regressionStatus": regression_status,
        "countsTowardRecovery": counts_toward_recovery,
        "evidenceRefs": evidence_refs,
    }


def recompute_summary(ledger: dict[str, Any]) -> None:
    baseline = ledger["baseline"]
    entries = ledger.get("entries", [])
    if not isinstance(entries, list):
        raise LedgerError("Ledger entries must be an array")

    improving = int(baseline["improvingCount"])
    completed = int(baseline["completedCount"])

    for entry in entries:
        if not isinstance(entry, dict):
            raise LedgerError("Ledger entry must be an object")
        completed += 1
        if entry.get("classification") == "Improving":
            improving += 1

    ledger["currentSummary"] = {
        "improvingCount": improving,
        "completedCount": completed,
        "improvementRatioPct": ratio_pct(improving, completed),
        "sc002Compliant": ratio_pct(improving, completed) >= 90.0,
    }


def append_entries(ledger: dict[str, Any], pass_artifacts: list[pathlib.Path], timestamp: str) -> None:
    entries = ledger.setdefault("entries", [])
    if not isinstance(entries, list):
        raise LedgerError("Ledger entries must be an array")

    seen_passes = {entry.get("passId") for entry in entries if isinstance(entry, dict)}

    for artifact_path in pass_artifacts:
        parsed = parse_pass_artifact(artifact_path)
        pass_id = parsed["passId"]

        if pass_id in seen_passes:
            raise LedgerError(f"Pass already exists in ledger: {pass_id}")

        prev_summary = ledger.get("currentSummary") or {}
        improving = int(prev_summary.get("improvingCount", ledger["baseline"]["improvingCount"]))
        completed = int(prev_summary.get("completedCount", ledger["baseline"]["completedCount"]))

        completed += 1
        if parsed["classification"] == "Improving":
            improving += 1

        entry = {
            "passId": pass_id,
            "classification": parsed["classification"],
            "regressionStatus": parsed["regressionStatus"],
            "improvingCount": improving,
            "completedCount": completed,
            "improvementRatioPct": ratio_pct(improving, completed),
            "evidenceRefs": parsed["evidenceRefs"],
            "recordedAt": timestamp,
        }
        entries.append(entry)
        seen_passes.add(pass_id)

        ledger["currentSummary"] = {
            "improvingCount": improving,
            "completedCount": completed,
            "improvementRatioPct": ratio_pct(improving, completed),
            "sc002Compliant": ratio_pct(improving, completed) >= 90.0,
        }


def main() -> int:
    args = parse_args()

    try:
        if args.ledger.exists():
            ledger = load_json(args.ledger)
        else:
            ledger = bootstrap_ledger()

        ensure_baseline(ledger)
        recompute_summary(ledger)

        ts = now_iso(args.updated_at)
        if args.pass_artifacts:
            append_entries(ledger, args.pass_artifacts, ts)

        # Recompute from baseline + full entry list to guard against denominator manipulation.
        recompute_summary(ledger)
        ledger["updatedAt"] = ts

        output = json.dumps(ledger, indent=2, sort_keys=False)
        if args.print_only:
            print(output)
            return 0

        args.ledger.parent.mkdir(parents=True, exist_ok=True)
        with args.ledger.open("w", encoding="utf-8") as handle:
            handle.write(output)
            handle.write("\n")

        print(
            "PASS: ledger updated "
            f"({ledger['currentSummary']['improvingCount']}/{ledger['currentSummary']['completedCount']} "
            f"= {ledger['currentSummary']['improvementRatioPct']}%)"
        )
        return 0
    except LedgerError as err:
        print(f"FAILED: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

