#!/usr/bin/env bash
# Determinism gate: compiling Vulkan GLSL must reproduce tracked generated C blobs.
# No GPU required.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

GEN_DIR_REPO="$PROJECT_ROOT/src/renderers/vulkan/shaders/spirv/generated"
TMP_DIR="${1:-/tmp/vk_spirv_gate}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

"$PROJECT_ROOT/scripts/compile_shaders.sh" --generated-dir "$TMP_DIR" >/dev/null

for f in shader_data.c shader_binding.c; do
  [ -f "$GEN_DIR_REPO/$f" ] || fail "missing tracked $GEN_DIR_REPO/$f"
  [ -f "$TMP_DIR/$f" ] || fail "missing generated $TMP_DIR/$f"
  if ! cmp -s "$GEN_DIR_REPO/$f" "$TMP_DIR/$f"; then
    sha_repo="$(sha256sum "$GEN_DIR_REPO/$f" | awk '{print $1}')"
    sha_tmp="$(sha256sum "$TMP_DIR/$f" | awk '{print $1}')"
    fail "$f differs (repo=$sha_repo tmp=$sha_tmp). Run ./scripts/compile_shaders.sh and commit updated generated blobs."
  fi
  ok "$f matches"
done

echo "vk_shader_determinism_check: all checks passed"
