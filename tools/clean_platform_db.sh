#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/clean_platform_db.sh [--full] [--data-dir <path>] [--help]

Options:
  --full             Also remove tenant databases (<data-dir>/tenants)
  --data-dir <path>  Server data dir (same value as isched_srv --data-dir)
  --help             Show this help text

Defaults:
  1) --data-dir value (if provided)
  2) ISCHED_DATA_DIR env var (if set)
  3) ${XDG_DATA_HOME:-$HOME/.local/share}/isched
EOF
}

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
ISCHED_DATA_DIR_DEFAULT="$DATA_HOME/isched"
ISCHED_DATA_DIR="${ISCHED_DATA_DIR:-$ISCHED_DATA_DIR_DEFAULT}"
FULL_WIPE=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --full)
      FULL_WIPE=true
      shift
      ;;
    --data-dir)
      if [[ $# -lt 2 || -z "${2:-}" ]]; then
        echo "ERROR: --data-dir requires a value" >&2
        usage
        exit 2
      fi
      ISCHED_DATA_DIR="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown argument '$1'" >&2
      usage
      exit 2
      ;;
  esac
done

mkdir -p "$ISCHED_DATA_DIR"
ISCHED_DATA_DIR="$(cd "$ISCHED_DATA_DIR" && pwd)"

SYSTEM_DB="$ISCHED_DATA_DIR/isched_system.db"
SYSTEM_DB_WAL="$ISCHED_DATA_DIR/isched_system.db-wal"
SYSTEM_DB_SHM="$ISCHED_DATA_DIR/isched_system.db-shm"
SYSTEM_DB_JOURNAL="$ISCHED_DATA_DIR/isched_system.db-journal"

echo "Cleaning platform DB under: $ISCHED_DATA_DIR"

# Optional backup of the system DB before cleanup.
cp -a "$SYSTEM_DB" "$SYSTEM_DB.bak.$(date +%s)" 2>/dev/null || true

# Remove platform DB + SQLite sidecars.
rm -f "$SYSTEM_DB" "$SYSTEM_DB_WAL" "$SYSTEM_DB_SHM" "$SYSTEM_DB_JOURNAL"

if [[ "$FULL_WIPE" == true ]]; then
  rm -rf "$ISCHED_DATA_DIR/tenants"
  echo "Also removed tenant databases."
fi

echo "Done. If isched is still running, restart it to enter seed/bootstrap mode."
