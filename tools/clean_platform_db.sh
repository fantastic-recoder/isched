#!/usr/bin/env bash
set -euo pipefail

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
ISCHED_DATA_DIR="$DATA_HOME/isched"

FULL_WIPE=false
if [[ "${1:-}" == "--full" ]]; then
  FULL_WIPE=true
fi

echo "Cleaning platform DB under: $ISCHED_DATA_DIR"
mkdir -p "$ISCHED_DATA_DIR"

# Optional backup of system DB before cleanup.
cp -a "$ISCHED_DATA_DIR/isched_system.db" \
  "$ISCHED_DATA_DIR/isched_system.db.bak.$(date +%s)" 2>/dev/null || true

# Remove platform DB + sqlite sidecars.
rm -f "$ISCHED_DATA_DIR/isched_system.db" \
      "$ISCHED_DATA_DIR/isched_system.db-wal" \
      "$ISCHED_DATA_DIR/isched_system.db-shm"

if [[ "$FULL_WIPE" == true ]]; then
  rm -rf "$ISCHED_DATA_DIR/tenants"
  echo "Also removed tenant databases."
fi

echo "Done. Next server start will be in seed/bootstrap mode."
