#!/usr/bin/env bash
# Minimal smoke test runner for CI to ensure the engine starts with common mods.
# Env overrides:
#   ENGINE_BIN     - path to engine binary (default: build/idtech3.x86_64)
#   MOD_LIST       - space-separated fs_game list (default: "mymod blacksun")
#   RENDERER_LIST  - space-separated renderers (opengl|vulkan), default: "opengl vulkan"
#   SMOKE_TIMEOUT  - seconds to wait before quit (default: 10)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="${ENGINE_BIN:-$ROOT/build/idtech3.x86_64}"
MOD_LIST="${MOD_LIST:-mymod blacksun}"
RENDERER_LIST="${RENDERER_LIST:-opengl vulkan}"
SMOKE_TIMEOUT="${SMOKE_TIMEOUT:-10}"

if [[ ! -x "${ENGINE_BIN}" ]]; then
	echo "[ci_smoke] Engine binary not found or not executable: ${ENGINE_BIN}" >&2
	exit 1
fi

echo "[ci_smoke] Engine: ${ENGINE_BIN}"
for RENDERER in ${RENDERER_LIST}; do
	for MOD in ${MOD_LIST}; do
		echo "[ci_smoke] Starting smoke for renderer=${RENDERER} fs_game=${MOD}"
		LOG_FILE="/tmp/ci_smoke_${RENDERER}_${MOD}.log"
		set +e
		"${ENGINE_BIN}" \
			+set r_renderer "${RENDERER}" \
			+set fs_game "${MOD}" \
			+set sv_pure 0 \
			+set com_speeds 1 \
			+set ttycon 1 \
			+wait "${SMOKE_TIMEOUT}" \
			+quit >"${LOG_FILE}" 2>&1
		STATUS=$?
		set -e

		if [[ ${STATUS} -ne 0 ]]; then
			echo "[ci_smoke] FAILED renderer=${RENDERER} fs_game=${MOD} (exit ${STATUS}), see ${LOG_FILE}" >&2
			exit ${STATUS}
		fi
		echo "[ci_smoke] PASS renderer=${RENDERER} fs_game=${MOD}"
	done
done

echo "[ci_smoke] All smoke tests passed."

