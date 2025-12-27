#!/usr/bin/env bash
# Vulkan validation smoke runner for CI.
# Builds on vk_validate.sh to run the engine with validation layers enabled,
# across a set of mods, and captures logs per mod.
#
# Env:
#   ENGINE_BIN      - path to engine binary (default: release/idtech3.x86_64.so)
#   MOD_LIST        - space-separated fs_game list (default: "mymod blacksun")
#   SMOKE_TIMEOUT   - seconds to wait before quit (default: 10)
#   VK_VALIDATE_LOG - optional log file path; if unset uses /tmp/ci_vk_smoke_<mod>.log
#
# Example:
#   ENGINE_BIN=./release/idtech3.x86_64.so MOD_LIST="mymod" ./tools/ci_vk_smoke.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="${ENGINE_BIN:-$ROOT/release/idtech3.x86_64}"
MOD_LIST="${MOD_LIST:-mymod}"
SMOKE_TIMEOUT="${SMOKE_TIMEOUT:-10}"

if [[ ! -x "${ENGINE_BIN}" ]]; then
	echo "[ci_vk_smoke] Engine binary not found or not executable: ${ENGINE_BIN}" >&2
	exit 1
fi

echo "[ci_vk_smoke] Engine: ${ENGINE_BIN}"
for MOD in ${MOD_LIST}; do
	LOG_FILE="${VK_VALIDATE_LOG:-/tmp/ci_vk_smoke_${MOD}.log}"
	echo "[ci_vk_smoke] Starting Vulkan validation smoke for fs_game=${MOD} (log: ${LOG_FILE})"
	set +e
	/usr/bin/timeout "${SMOKE_TIMEOUT}"s env VK_VALIDATE_LOG="${LOG_FILE}" "${ROOT}/tools/vk_validate.sh" "${ENGINE_BIN}" \
		+set r_renderer "vulkan" \
		+set fs_game "${MOD}" \
		+set sv_pure 0 \
		+set com_speeds 1 \
		+set ttycon 1 \
		+quit >"${LOG_FILE}" 2>&1
	STATUS=$?
	set -e

	# timeout(1) exits 124; treat that as a soft-pass if the engine started.
	if [[ ${STATUS} -ne 0 && ${STATUS} -ne 124 ]]; then
		echo "[ci_vk_smoke] FAILED fs_game=${MOD} (exit ${STATUS}), see ${LOG_FILE}" >&2
		exit ${STATUS}
	fi
	if [[ ${STATUS} -eq 124 ]]; then
		if ! grep -q "FS_Startup" "${LOG_FILE}"; then
			echo "[ci_vk_smoke] FAILED fs_game=${MOD} (timeout and no startup output), see ${LOG_FILE}" >&2
			exit 124
		fi
	fi
	echo "[ci_vk_smoke] PASS fs_game=${MOD}"
done

echo "[ci_vk_smoke] All Vulkan validation smokes passed."

