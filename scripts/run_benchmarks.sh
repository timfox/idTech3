#!/usr/bin/env bash
# Simple timedemo harness for CI/local perf checks.
# Env overrides:
#   ENGINE_BIN - path to engine binary (default: build/idtech3.x86_64)
#   MOD        - fs_game to run (default: mymod)
#   DEMO       - demo name (default: demo1)
#   OUTDIR     - output directory for logs (default: benchmarks/)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="${ENGINE_BIN:-$ROOT/build/idtech3.x86_64}"
MOD="${MOD:-mymod}"
DEMO="${DEMO:-demo1}"
OUTDIR="${OUTDIR:-$ROOT/benchmarks}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_BASENAME="timedemo_${MOD}_${DEMO}_${TIMESTAMP}.log"
LOG_PATH="${OUTDIR}/${LOG_BASENAME}"

mkdir -p "${OUTDIR}"

if [[ ! -x "${ENGINE_BIN}" ]]; then
	echo "[run_benchmarks] Engine binary not found or not executable: ${ENGINE_BIN}" >&2
	exit 1
fi

echo "[run_benchmarks] Engine: ${ENGINE_BIN}"
echo "[run_benchmarks] Mod: ${MOD}"
echo "[run_benchmarks] Demo: ${DEMO}"
echo "[run_benchmarks] Log: ${LOG_PATH}"

# Run timedemo; exit code will be non-zero if demo/assets are missing.
set +e
"${ENGINE_BIN}" \
	+set fs_game "${MOD}" \
	+set com_speeds 1 \
	+set com_timedemoLog 1 \
	+timedemo 1 \
	+demo "${DEMO}" \
	+quit 2>&1 | tee "${LOG_PATH}"
STATUS=$?
set -e

if [[ ${STATUS} -ne 0 ]]; then
	echo "[run_benchmarks] Timedemo exited with status ${STATUS} (assets or demo may be missing)" >&2
	exit ${STATUS}
fi

echo "[run_benchmarks] Done. See ${LOG_PATH}"

