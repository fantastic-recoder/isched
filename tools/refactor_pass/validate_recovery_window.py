#!/usr/bin/env python3
"""Cross-artifact consistency checks for feature 009 recovery evidence."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


class ValidationError(Exception):
    """Raised when audit validation fails."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate recovery-window artifact consistency")
    parser.add_argument("--ledger", type=pathlib.Path, required=True, help="Path to improvement-ratio-ledger.json")
    parser.add_argument(
        "--artifacts-root",
        type=pathlib.Path,
        required=True,
        help="Root directory containing pass-XX artifact subdirectories",
    )
    parser.add_argument(
        "--decision",
        type=pathlib.Path,
        default=None,
        help="Optional compliance decision record to cross-check",
    )
    return parser.parse_args()


def load_json(path: pathlib.Path) -> dict[str, Any]:
    if not path.exists():
        raise ValidationError(f"Missing file: {path}")
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValidationError(f"Expected object JSON in {path}")
    return data


def expected_ratio(improving: int, completed: int) -> float:
    return round((improving / completed) * 100.0, 2)


def validate_ledger(ledger: dict[str, Any], artifacts_root: pathlib.Path) -> None:
    baseline = ledger.get("baseline")
    entries = ledger.get("entries")
    summary = ledger.get("currentSummary")

    if not isinstance(baseline, dict) or not isinstance(entries, list) or not isinstance(summary, dict):
        raise ValidationError("Ledger must contain baseline, entries, and currentSummary")

    improving = int(baseline.get("improvingCount", -1))
    completed = int(baseline.get("completedCount", -1))
    if improving != 1 or completed != 2:
        raise ValidationError("Ledger baseline must remain improving=1 and completed=2")

    for entry in entries:
        if not isinstance(entry, dict):
            raise ValidationError("Every ledger entry must be a JSON object")

        pass_id = entry.get("passId")
        if not isinstance(pass_id, str) or not pass_id:
            raise ValidationError("Ledger entry has invalid passId")

        artifact_path = artifacts_root / pass_id / "recovery-pass-artifact.json"
        artifact = load_json(artifact_path)

        if artifact.get("passId") != pass_id:
            raise ValidationError(f"Pass id mismatch between ledger and artifact for {pass_id}")

        artifact_classification = artifact.get("classification", {})
        if not isinstance(artifact_classification, dict):
            raise ValidationError(f"Invalid classification block in {artifact_path}")

        if artifact_classification.get("improvement") != entry.get("classification"):
            raise ValidationError(f"Classification mismatch for {pass_id}")

        if artifact_classification.get("regressionStatus") != entry.get("regressionStatus"):
            raise ValidationError(f"Regression status mismatch for {pass_id}")

        completed += 1
        if entry.get("classification") == "Improving":
            improving += 1

        if int(entry.get("completedCount", -1)) != completed:
            raise ValidationError(f"Completed denominator drift for {pass_id}")

        if int(entry.get("improvingCount", -1)) != improving:
            raise ValidationError(f"Improving numerator drift for {pass_id}")

        ratio = expected_ratio(improving, completed)
        if float(entry.get("improvementRatioPct", -1)) != ratio:
            raise ValidationError(f"Ratio drift for {pass_id}: expected {ratio}")

    if int(summary.get("improvingCount", -1)) != improving:
        raise ValidationError("Summary improvingCount mismatch")

    if int(summary.get("completedCount", -1)) != completed:
        raise ValidationError("Summary completedCount mismatch")

    ratio = expected_ratio(improving, completed)
    if float(summary.get("improvementRatioPct", -1)) != ratio:
        raise ValidationError("Summary improvementRatioPct mismatch")

    expected_compliance = ratio >= 90.0
    if bool(summary.get("sc002Compliant")) != expected_compliance:
        raise ValidationError("Summary sc002Compliant mismatch")


def validate_decision(decision: dict[str, Any], ledger: dict[str, Any]) -> None:
    summary = ledger["currentSummary"]
    if int(decision.get("numerator", -1)) != int(summary.get("improvingCount", -2)):
        raise ValidationError("Decision numerator does not match ledger summary")
    if int(decision.get("denominator", -1)) != int(summary.get("completedCount", -2)):
        raise ValidationError("Decision denominator does not match ledger summary")
    if float(decision.get("ratioPct", -1)) != float(summary.get("improvementRatioPct", -2)):
        raise ValidationError("Decision ratioPct does not match ledger summary")


def main() -> int:
    args = parse_args()

    try:
        ledger = load_json(args.ledger)
        validate_ledger(ledger, args.artifacts_root)

        if args.decision is not None:
            decision = load_json(args.decision)
            validate_decision(decision, ledger)

        print("PASS: recovery window artifacts are cross-consistent")
        return 0
    except ValidationError as err:
        print(f"FAILED: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

