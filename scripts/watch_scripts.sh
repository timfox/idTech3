#!/usr/bin/env bash
# External script watch helper (pairs with com_scriptWatch 1 in-engine).
# Usage: ./scripts/watch_scripts.sh <mod_dir>
# Requires: inotifywait (inotify-tools) or falls back to 2s polling.
set -euo pipefail

MOD="${1:?mod directory}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WATCH="${MOD}/scripts"

if [[ ! -d "$WATCH" ]]; then
  echo "No scripts/ under $MOD" >&2
  exit 1
fi

echo "[watch_scripts] mod=$MOD (use com_scriptWatch 1 in-engine for automatic reload)"
echo "[watch_scripts] or run: script_reload <path> after saves"

if command -v inotifywait >/dev/null 2>&1; then
  while inotifywait -r -e modify,create,delete,move "$WATCH" 2>/dev/null; do
    echo "[watch_scripts] change detected — run: script_reload (tracked scripts)"
  done
else
  echo "[watch_scripts] inotifywait not found; polling every 2s"
  find "$WATCH" -type f -printf '%T@ %p\n' | sort > /tmp/idtech3_watch_a.$$
  while sleep 2; do
    find "$WATCH" -type f -printf '%T@ %p\n' | sort > /tmp/idtech3_watch_b.$$
    if ! cmp -s /tmp/idtech3_watch_a.$$ /tmp/idtech3_watch_b.$$; then
      mv /tmp/idtech3_watch_b.$$ /tmp/idtech3_watch_a.$$
      echo "[watch_scripts] change detected — run: script_reload"
    else
      rm -f /tmp/idtech3_watch_b.$$
    fi
  done
fi
