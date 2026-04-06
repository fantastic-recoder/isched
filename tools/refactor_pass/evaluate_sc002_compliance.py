#!/usr/bin/env python3
"""Evaluate SC-002 compliance status from the cumulative improvement-ratio ledger."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from datetime import datetime, timezone
from typing import Any


class DecisionError(Exception):
    """Raised when decision evaluation cannot continue."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate SC-002 compliance from ledger")
    parser.add_argument("--ledger", type=pathlib.Path, required=True, help="Path to improvement-ratio-ledger.json")
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        required=True,
        help="Path to compliance-decision-record.json",
    )
    parser.add_argument("--evaluated-at", type=str, default=None, help="Override ISO-8601 evaluation timestamp")
    return parser.parse_args()


def now_iso(override: str | None = None) -> str:
    if override:
        return override
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def load_json(path: pathlib.Path) -> dict[str, Any]:
    if not path.exists():
        raise DecisionError(f"Ledger not found: {path}")
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise DecisionError("Ledger must be a JSON object")
    return data


def evidence_ref_exists(ref: str, ledger_path: pathlib.Path) -> bool:
    candidate = ref.split("#", 1)[0].strip()
    if not candidate:
        return False

    path = pathlib.Path(candidate)
    if path.is_absolute():
        return path.exists()

    # Support both repository-relative and ledger-relative references.
    return path.exists() or (ledger_path.parent / path).exists()


def evaluate_status(ledger: dict[str, Any], ledger_path: pathlib.Path) -> tuple[str, list[str], str | None, list[str]]:
    entries = ledger.get("entries", [])
    if not isinstance(entries, list):
        raise DecisionError("Ledger entries must be an array")

    missing_evidence_passes: list[str] = []
    missing_files: list[str] = []
    unmitigated: list[str] = []
    evidence_set: list[str] = [str(ledger_path)]

    for entry in entries:
        if not isinstance(entry, dict):
            raise DecisionError("Ledger entry must be an object")

        pass_id = str(entry.get("passId", "<unknown>"))
        evidence_refs = entry.get("evidenceRefs")
        if not isinstance(evidence_refs, list) or not evidence_refs:
            missing_evidence_passes.append(pass_id)
        else:
            for ref in evidence_refs:
                ref_str = str(ref)
                evidence_set.append(ref_str)
                if not evidence_ref_exists(ref_str, ledger_path):
                    missing_files.append(f"{pass_id}:{ref_str}")

        if entry.get("regressionStatus") == "Unmitigated":
            unmitigated.append(pass_id)

    if missing_evidence_passes:
        note = "Missing evidence references for: " + ", ".join(missing_evidence_passes)
        return "Unresolved", unmitigated, note, sorted(set(evidence_set))

    if missing_files:
        note = "Missing evidence files for: " + ", ".join(missing_files)
        return "Unresolved", unmitigated, note, sorted(set(evidence_set))

    summary = ledger.get("currentSummary")
    if not isinstance(summary, dict):
        raise DecisionError("Ledger currentSummary is missing or invalid")

    ratio = float(summary.get("improvementRatioPct", 0.0))

    if ratio >= 90.0 and not unmitigated:
        return "Met", unmitigated, None, sorted(set(evidence_set))

    return "NotMet", unmitigated, None, sorted(set(evidence_set))


def main() -> int:
    args = parse_args()

    try:
        ledger = load_json(args.ledger)
        summary = ledger.get("currentSummary", {})
        if not isinstance(summary, dict):
            raise DecisionError("Ledger currentSummary is missing or invalid")

        numerator = int(summary.get("improvingCount", 0))
        denominator = int(summary.get("completedCount", 1))
        ratio = round((numerator / denominator) * 100.0, 2) if denominator else 0.0

        status, unmitigated, note, evidence_set = evaluate_status(ledger, args.ledger)
        ts = now_iso(args.evaluated_at)

        decision: dict[str, Any] = {
            "decisionId": f"sc002-{ts}",
            "featureRef": "009-raise-pass-improvement-ratio",
            "sc002Status": status,
            "numerator": numerator,
            "denominator": denominator,
            "ratioPct": ratio,
            "evaluatedAt": ts,
            "ledgerRef": str(args.ledger),
            "evidenceSet": evidence_set,
            "unmitigatedRegressionPasses": unmitigated,
        }
        if note is not None:
            decision["notes"] = note

        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", encoding="utf-8") as handle:
            json.dump(decision, handle, indent=2)
            handle.write("\n")

        print(f"PASS: SC-002 decision {status} ({numerator}/{denominator} = {ratio}%)")
        return 0
    except DecisionError as err:
        print(f"FAILED: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

