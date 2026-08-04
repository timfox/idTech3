#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'OIT_FILTER_STRAIGHT_SOURCE\|OIT_FILTER_PREMULTIPLIED_SOURCE\|transparentFilterMode\|filterMode' "$ROOT/renderers/vulkan/vk_oit_alpha.h"
grep -q 'r_alphaFilterDebug' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'filtering\|Filtering\|mip' "$ROOT/docs/TRANSPARENT_TEXTURE_AUTHORING.md"
for shader in oit_accum.frag oit_accum_mboit.frag oit_moments.frag; do
	path="$ROOT/renderers/vulkan/shaders/glsl/$shader"
	grep -Eq 'texture\( *tex0,' "$path" || { echo "FAIL: $shader must use implicit-LOD source filtering" >&2; exit 1; }
	if grep -Eq 'textureLod\( *tex0,.*0\.0' "$path"; then
		echo "FAIL: $shader must not force source textures to mip 0" >&2
		exit 1
	fi
done
grep -q 'texelFetch' "$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" || { echo "FAIL: OIT resolve must retain exact texel reads" >&2; exit 1; }
echo "OK: alpha filtering docs/API"
