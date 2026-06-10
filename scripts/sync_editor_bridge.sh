#!/usr/bin/env bash
# Copy EDITOR_BRIDGE entity defs into examples/radiant (manual sync checkpoint).
# Full defs are maintained in examples/radiant/scripts/entities_idtech3_bridge.def
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEF="${ROOT}/examples/radiant/scripts/entities_idtech3_bridge.def"
BRIDGE="${ROOT}/docs/EDITOR_BRIDGE.md"

if [[ ! -f "$DEF" ]]; then
  echo "Missing $DEF" >&2
  exit 1
fi

echo "[sync_editor_bridge] entity defs: $DEF"
echo "[sync_editor_bridge] contract doc: $BRIDGE"
echo "[sync_editor_bridge] verify misc_billboard / misc_decal keys match EDITOR_BRIDGE.md table"
grep -E '^/\*QUAKED misc_' "$DEF" | sed 's/^/  /' || true
echo "[sync_editor_bridge] run install_radiant_gamepack.sh on your mod to deploy"
