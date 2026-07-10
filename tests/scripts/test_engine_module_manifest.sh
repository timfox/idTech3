#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
MANIFEST="${ROOT}/docs/ENGINE_MODULE_MANIFEST.md"

fail() { echo "FAIL: $*" >&2; exit 1; }

[ -f "$MANIFEST" ] || fail "missing ENGINE_MODULE_MANIFEST.md"

grep -q 'USE_RESEARCH_EXTENSIONS' "$MANIFEST" || fail "manifest missing USE_RESEARCH_EXTENSIONS"
grep -q 'USE_OPEN_WORLD' "$MANIFEST" || fail "manifest missing USE_OPEN_WORLD"
grep -q 'IDTECH3_PROFILE' "$MANIFEST" || fail "manifest missing IDTECH3_PROFILE"
grep -qE 'src/extensions/research/radiusfps|extensions/research/radiusfps' "$MANIFEST" \
	|| fail "manifest missing radiusfps path"
grep -qE 'src/extensions/generative/cl_genetic_gan\.c|extensions/generative/cl_genetic_gan\.c' "$MANIFEST" \
	|| fail "manifest missing generative path"

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

# Resolve manifest path (canonical first, src/* shim fallback).
idtech3_manifest_resolve() {
	local path="$1"
	local canon shim base

	case "$path" in
		src/extensions/*)
			base="${path#src/extensions/}"
			canon="extensions/${base}"
			shim="$path"
			;;
		extensions/*)
			canon="$path"
			shim="src/${path}"
			;;
		src/world/*)
			base="${path#src/world/}"
			canon="modules/world/${base}"
			shim="$path"
			;;
		modules/world/*)
			canon="$path"
			shim="src/world/${path#modules/world/}"
			;;
		*)
			echo "${ROOT}/${path}"
			return
			;;
	esac

	if [[ "$path" == */ ]]; then
		canon="${canon%/}"
		shim="${shim%/}"
		if [ -d "${ROOT}/${canon}" ]; then
			echo "${ROOT}/${canon}"
		elif [ -d "${ROOT}/${shim}" ]; then
			echo "${ROOT}/${shim}"
		else
			echo "${ROOT}/${canon}"
		fi
	else
		idtech3_file "$canon" "$shim"
	fi
}

# Paths referenced in manifest must exist on disk.
while IFS= read -r line; do
  case "$line" in
    *'src/extensions/'*|*'extensions/'*|*'src/world/'*|*'modules/world/'*)
      path="$(echo "$line" | sed -n 's/.*`\([^`]*\)`.*/\1/p')"
      [ -z "$path" ] && continue
      resolved="$(idtech3_manifest_resolve "$path")"
      # directory entries end with /
      if [[ "$path" == */ ]]; then
        [ -d "$resolved" ] || fail "missing directory ${path}"
      elif [[ "$path" == *.* ]]; then
        [ -f "$resolved" ] || fail "missing file ${path}"
      fi
      ;;
  esac
done < "$MANIFEST"

echo "test_engine_module_manifest: passed"
