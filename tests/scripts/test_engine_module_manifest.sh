#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MANIFEST="${ROOT}/docs/ENGINE_MODULE_MANIFEST.md"

fail() { echo "FAIL: $*" >&2; exit 1; }

[ -f "$MANIFEST" ] || fail "missing ENGINE_MODULE_MANIFEST.md"

grep -q 'USE_RESEARCH_EXTENSIONS' "$MANIFEST" || fail "manifest missing USE_RESEARCH_EXTENSIONS"
grep -q 'USE_OPEN_WORLD' "$MANIFEST" || fail "manifest missing USE_OPEN_WORLD"
grep -q 'IDTECH3_PROFILE' "$MANIFEST" || fail "manifest missing IDTECH3_PROFILE"
grep -q 'src/extensions/research/radiusfps' "$MANIFEST" || fail "manifest missing radiusfps path"
grep -q 'src/extensions/generative/cl_genetic_gan.c' "$MANIFEST" || fail "manifest missing generative path"

for f in \
  cmake/IdTech3Profile.cmake \
  cmake/IdTech3QcommonExtensions.cmake \
  cmake/client/ClientExtensionSources.cmake \
  cmake/renderers/VulkanExtensionSources.cmake \
  cmake/profiles/core.cmake \
  cmake/profiles/game.cmake \
  cmake/profiles/full.cmake \
  cmake/profiles/research.cmake
do
  [ -f "${ROOT}/${f}" ] || fail "missing ${f}"
done

# Paths referenced in manifest must exist on disk.
while IFS= read -r line; do
  case "$line" in
    *'src/extensions/'*|*'src/world/'*)
      path="$(echo "$line" | sed -n 's/.*`\([^`]*\)`.*/\1/p')"
      [ -z "$path" ] && continue
      # directory entries end with /
      if [[ "$path" == */ ]]; then
        [ -d "${ROOT}/${path%/}" ] || fail "missing directory ${path}"
      elif [[ "$path" == *.* ]]; then
        [ -f "${ROOT}/${path}" ] || fail "missing file ${path}"
      fi
      ;;
  esac
done < "$MANIFEST"

echo "test_engine_module_manifest: passed"
