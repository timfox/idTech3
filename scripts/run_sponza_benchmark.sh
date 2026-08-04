#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${IDTECH3_BIN:-$ROOT/build-vk-Release/idtech3}"
BASE="${GAME_BASE:-$ROOT/release}"
OUT="${SPONZA_BENCHMARK_OUT:-$ROOT/sponza_benchmark_results}"

[[ -x "$ENGINE" ]] || { echo "missing client: $ENGINE" >&2; exit 1; }
[[ -d "$BASE/demo_game" ]] || { echo "missing demo game tree: $BASE/demo_game" >&2; exit 1; }

mkdir -p "$OUT"

run_case() {
	local name="$1"
	local mode="$2"
	local deferred="$3"
	local oit="$4"
	local home="$OUT/home_$name"
	local log="$OUT/${name}.log"

	mkdir -p "$home"
	echo "[sponza] ${name}: renderMode=${mode} deferred=${deferred} oit=${oit}"
	timeout "${SPONZA_BENCHMARK_TIMEOUT:-120}s" xvfb-run -a "$ENGINE" \
		+set fs_basepath "$BASE" \
		+set fs_game demo_game \
		+set r_renderer vulkan \
		+set r_renderMode "$mode" \
		+set r_deferredLighting "$deferred" \
		+set r_oit "$oit" \
		+set com_freeusd 1 \
		+set fs_homepath "$home" \
		+exec sponza_benchmark_capture.cfg >"$log" 2>&1

	grep -q 'District_Sponza full load' "$log" || {
		echo "Sponza scene did not load for ${name}; see ${log}" >&2
		return 1
	}
	grep -q 'CAPTURE end fails=0' "$log" || {
		echo "frame contract failed for ${name}; see ${log}" >&2
		return 1
	}
	find "$home" -type f -name 'sponza_benchmark_capture.jpg' -exec cp {} "$OUT/${name}.jpg" \; -quit
	[[ -s "$OUT/${name}.jpg" ]] || {
		echo "capture missing for ${name}; see ${log}" >&2
		return 1
	}
}

run_case forwardplus 2 0 0
run_case deferred 1 1 0
run_case wboit 3 0 1
echo "Sponza benchmark passed: $OUT"
