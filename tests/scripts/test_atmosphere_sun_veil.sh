#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
SHADER="$ROOT/renderers/vulkan/shaders/glsl/atmosphere.frag"

grep -Fq 'vec3 origin = vec3(0.0, 0.0, PLANET_RADIUS + observerHeight);' "$SHADER"
grep -Fq 'vec3 color = atm.sunColor.rgb * scale * sunIntensity *' "$SHADER"

if grep -Fq 'vec3 origin = vec3(0.0, PLANET_RADIUS + observerHeight, 0.0);' "$SHADER"; then
	echo "FAIL: atmosphere observer is still Y-up" >&2
	exit 1
fi

echo "PASS: Z-up atmosphere and chromatic sun scattering"
