#!/usr/bin/env bash
# Foundation Consolidation — standalone CPU unit tests (no engine link).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CC="${CC:-gcc}"
CFLAGS=( -std=c23 -O0 -Wall -Wextra -pedantic )
TMP="${TMPDIR:-/tmp}/idtech3_foundation_unit_$$"
mkdir -p "$TMP"

failures=0

run_test() {
	local src="$1"
	local name
	name="$(basename "$src" .c)"
	local bin="$TMP/$name"

	if ! "$CC" "${CFLAGS[@]}" "$src" -o "$bin" -lm 2>"$TMP/${name}.build.log"; then
		echo "FAIL: compile $name"
		cat "$TMP/${name}.build.log" >&2
		failures=$((failures + 1))
		return
	fi
	if ! "$bin"; then
		echo "FAIL: run $name"
		failures=$((failures + 1))
		return
	fi
	echo "PASS: $name"
}

shaders=(
	"$ROOT/shaders/glsl/pbr_brdf_core.glsl"
	"$ROOT/shaders/glsl/gbuffer_octahedral.glsl"
	"$ROOT/shaders/glsl/hiz_downsample.comp"
)

for f in "${shaders[@]}"; do
	if [[ ! -f "$f" ]]; then
		echo "FAIL: missing canonical shader $f"
		failures=$((failures + 1))
	fi
done

for link in \
	"$ROOT/renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl" \
	"$ROOT/renderers/vulkan/shaders/glsl/gbuffer_octahedral.glsl" \
	"$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp"; do
	if [[ ! -e "$link" ]]; then
		echo "FAIL: missing Vulkan shader path $link"
		failures=$((failures + 1))
	fi
done

grep -q 'hiz_downsample.comp' "$ROOT/scripts/compile_shaders.sh" || {
	echo "FAIL: compile_shaders.sh missing hiz_downsample entry"
	failures=$((failures + 1))
}

tests=(
	test_gpu_scene_generation
	test_indirect_cmd_bounds
	test_material_routing
	test_brdf_reference
	test_roughness_filter
	test_probe_selection
	test_reflection_fallback
	test_gbuffer_octahedral
)

# Cluster indexing lives in cmake target unit_cluster_math (links vk_cluster_math.cpp).
if [[ -x "$ROOT/build-vk-Release/unit_cluster_math" ]]; then
	if "$ROOT/build-vk-Release/unit_cluster_math"; then
		echo "PASS: unit_cluster_math (cmake)"
	else
		echo "FAIL: unit_cluster_math"
		failures=$((failures + 1))
	fi
else
	echo "SKIP: unit_cluster_math binary not present (build unit_cluster_math)"
fi

for t in "${tests[@]}"; do
	run_test "$ROOT/tests/unit/${t}.c"
done

rm -rf "$TMP"

if [[ $failures -ne 0 ]]; then
	echo "$failures foundation unit check(s) failed"
	exit 1
fi

echo "All foundation unit tests passed."
