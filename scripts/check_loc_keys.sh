#!/usr/bin/env bash
# Verify .loc files have key=value lines (no empty keys). Usage: check_loc_keys.sh [dir...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
status=0

check_file() {
  local f="$1"
  local line n=0
  while IFS= read -r line || [[ -n "$line" ]]; do
    n=$((n + 1))
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ "$line" =~ ^[[:space:]]*$ ]] && continue
    if [[ ! "$line" =~ ^[^=]+=.+ ]]; then
      echo "check_loc_keys: $f:$n invalid line: $line" >&2
      status=1
    fi
  done < "$f"
}

scan() {
  local dir="$1"
  if [[ ! -d "$dir" ]]; then
    return 0
  fi
  while IFS= read -r -d '' f; do
    check_file "$f"
  done < <(find "$dir" -name '*.loc' -print0 2>/dev/null || true)
}

if [[ $# -gt 0 ]]; then
  for d in "$@"; do scan "$d"; done
else
  scan "$ROOT/examples/demo_game/loc"
  scan "$ROOT/examples/demo_game/mod"
fi

exit "$status"
