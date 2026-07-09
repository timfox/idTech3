#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_ROOT="$(mktemp -d)"
trap 'rm -rf "$OUT_ROOT"' EXIT

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

command -v cmake >/dev/null 2>&1 || fail "cmake not in PATH"
command -v unzip >/dev/null 2>&1 || fail "unzip not in PATH"

"$ROOT/scripts/asset_pipeline.sh" "$ROOT/examples/demo_game/mod" \
  --output-root "$OUT_ROOT/out" \
  --release-root "$OUT_ROOT/release" \
  --skip-shaders

COOK_ROOT="$OUT_ROOT/out/idtech3_demo"
STAGE="$COOK_ROOT/stage"
MANIFEST="$COOK_ROOT/manifest.txt"
HOT_RELOAD="$COOK_ROOT/hot_reload.cfg"
PK3="$COOK_ROOT/package/idtech3_demo.pk3"
RELEASE_PK3="$OUT_ROOT/release/idtech3_demo/idtech3_demo.pk3"

[[ -d "$STAGE" ]] || fail "missing cooked stage dir"
[[ -f "$MANIFEST" ]] || fail "missing manifest"
[[ -f "$HOT_RELOAD" ]] || fail "missing hot reload cfg"
[[ -f "$PK3" ]] || fail "missing package pk3"
[[ -f "$RELEASE_PK3" ]] || fail "missing copied release pk3"

grep -q '^mod_name=idtech3_demo$' "$MANIFEST" || fail "manifest missing mod_name"
grep -q '^shader_compile=skipped$' "$MANIFEST" || fail "manifest missing shader status"
grep -q '^stage_files_begin$' "$MANIFEST" || fail "manifest missing staged file list"
grep -q 'reloadTtf' "$HOT_RELOAD" || fail "hot reload cfg missing reloadTtf"
grep -q 'script_reload' "$HOT_RELOAD" || fail "hot reload cfg missing script_reload"

for staged in \
  "$STAGE/scripts/demo_bootstrap.shader" \
  "$STAGE/scripts/demo_sprites.shader" \
  "$STAGE/animgraph/idle_run.txt" \
  "$STAGE/fonts/Inter_28pt-Regular.ttf" \
  "$STAGE/fonts/demo_console_sdf.png" \
  "$STAGE/gfx/demo/bootstrap_white.png" \
  "$STAGE/loc/en.loc"; do
  [[ -f "$staged" ]] || fail "missing staged file: $staged"
done

listing="$(unzip -l "$PK3")"
for needle in \
  scripts/demo_bootstrap.shader \
  scripts/demo_sprites.shader \
  fonts/Inter_28pt-Regular.ttf \
  fonts/demo_console_sdf.png \
  gfx/demo/bootstrap_white.png \
  loc/en.loc; do
  [[ "$listing" == *"$needle"* ]] || fail "pk3 missing $needle"
done

echo "OK: asset pipeline cooked and packaged demo assets"
