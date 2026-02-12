#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SPIRV_DIR="$REPO_ROOT/src/renderers/vulkan/shaders/spirv"
METADATA_FILE="$SPIRV_DIR/shader_permutations.json"
CACHE_HASH="$SPIRV_DIR/shader_hash.txt"

usage() {
  cat <<'EOF'
Usage: shader_worker_pool.sh [--force]

This helper keeps shader permutations cached by invoking scripts/compile_vulkan_shaders.py
when the metadata hash or git HEAD changes.
EOF
  exit 1
}

FORCE=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --force)
      FORCE=1
      shift
      ;;
    --help|-h)
      usage
      ;;
    *)
      break
      ;;
  esac
done

read_metadata() {
  python3 <<'PY'
import json
import pathlib

path = pathlib.Path("$METADATA_FILE")
if not path.exists():
    print()
    print()
    return

data = json.loads(path.read_text())
print(data.get("shader_hash", ""))
print(data.get("git_sha", ""))
PY
}

print_metadata_summary() {
  python3 <<'PY'
import json
import pathlib

path = pathlib.Path("$METADATA_FILE")
if not path.exists():
    print("shader_worker_pool: shader metadata missing")
    return

data = json.loads(path.read_text())
total = data.get("task_count") or len(data.get("tasks", []))
print(f"shader_worker_pool: {total} shader permutations recorded in metadata")
PY
}

current_sha="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo "unknown")"
mapfile -t metadata < <(read_metadata)
metadata_hash="${metadata[0]:-}"
metadata_git_sha="${metadata[1]:-}"
cache_hash=""
if [ -f "$CACHE_HASH" ]; then
  cache_hash="$(cat "$CACHE_HASH")"
fi

if [ "$FORCE" -eq 0 ] && [ -n "$metadata_hash" ] && [ -n "$metadata_git_sha" ] && [ "$metadata_git_sha" = "$current_sha" ] && [ "$cache_hash" = "$metadata_hash" ]; then
  echo "shader_worker_pool: shader cache already up-to-date (hash $metadata_hash)"
  exit 0
fi

echo "shader_worker_pool: regenerating shader cache"
python3 "$SCRIPT_DIR/compile_vulkan_shaders.py" --metadata-out "$METADATA_FILE"
mapfile -t metadata < <(read_metadata)
metadata_hash="${metadata[0]:-}"
if [ -z "$metadata_hash" ]; then
  echo "shader_worker_pool: failed to read shader hash after compilation"
  exit 1
fi

mkdir -p "$(dirname "$CACHE_HASH")"
printf "%s\n" "$metadata_hash" > "$CACHE_HASH"
echo "shader_worker_pool: captured hash $metadata_hash"

print_metadata_summary
