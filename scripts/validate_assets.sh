#!/usr/bin/env bash
# Lightweight asset reference checks for mod trees (no full asset DB).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MOD="${1:-$ROOT/examples/demo_game/mod}"
status=0

if [[ "$MOD" != /* ]]; then
  MOD="$ROOT/$MOD"
fi

LOC_DIR=""
for candidate in \
  "$MOD/loc" \
  "$(cd "$MOD/.." 2>/dev/null && pwd)/loc" \
  "$ROOT/examples/demo_game/loc"; do
  if [[ -d "$candidate" ]]; then
    LOC_DIR="$candidate"
    break
  fi
done

fail() { echo "validate_assets: $*" >&2; status=1; }

[[ -d "$MOD" ]] || fail "mod dir missing: $MOD"
[[ -f "$MOD/default.cfg" || -f "$MOD/autoexec.cfg" ]] || fail "missing default.cfg or autoexec.cfg"

if [[ -f "$MOD/gameinfo.txt" ]]; then
  grep -q '^title "' "$MOD/gameinfo.txt" || fail "gameinfo.txt missing title"
fi

if [[ -f "$MOD/sound/soundevents.txt" ]]; then
  while read -r _ bus _ path _; do
    [[ "$path" =~ ^# ]] && continue
    [[ -z "${path:-}" ]] && continue
    if [[ ! -f "$MOD/$path" && ! -f "$ROOT/base/$path" ]]; then
      echo "validate_assets: note — sound not in mod tree (may live in base/): $path"
    fi
  done < <(grep -v '^[[:space:]]*#' "$MOD/sound/soundevents.txt" || true)
fi

if [[ -f "$MOD/readme_demo.txt" ]]; then
  for shader in demo_bootstrap.shader demo_sprites.shader; do
    [[ -f "$MOD/scripts/$shader" ]] || fail "missing scripts/$shader"
  done

  [[ -f "$MOD/animgraph/idle_run.txt" ]] || fail "missing animgraph/idle_run.txt"
fi

if [[ -n "$LOC_DIR" && -f "$LOC_DIR/en.loc" ]]; then
  "$ROOT/scripts/check_loc_keys.sh" "$LOC_DIR" || status=1
fi

echo "validate_assets: OK ($MOD)"
exit "$status"
