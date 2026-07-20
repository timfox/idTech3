#!/usr/bin/env bash
# Optional GPU lifecycle stress for Spine 1.1 cert.
# SKIP (77) without client/display unless IDTECH3_RUNTIME_REQUIRED=1.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=../tests/scripts/idtech3_client_runtime_smoke.sh
source "$ROOT/tests/scripts/idtech3_client_runtime_smoke.sh"

VID_RESTARTS="${SPINE11_VID_RESTARTS:-5}"
RESIZES="${SPINE11_RESIZES:-6}"
FOCUS="${SPINE11_FOCUS:-4}"
MAPS="${SPINE11_MAPS:-0}"
TIMEOUT_SEC="${IDTECH3_CLIENT_TIMEOUT:-180}"

echo "=== Spine 1.1 lifecycle stress (optional GPU) ==="
echo "counts: vid_restart=$VID_RESTARTS resize=$RESIZES focus=$FOCUS map=$MAPS timeout=${TIMEOUT_SEC}s"

export IDTECH3_CLIENT_TIMEOUT="$TIMEOUT_SEC"
idtech3_client_run_optional "spine_1_1_lifecycle" \
	+set r_fullscreen 0 \
	+set developer 1 \
	+exec vulkan_overlay_spine_1_1_cert.cfg \
	+vid_restart \
	+wait +wait +wait +wait \
	+spine_1_1_stress "$VID_RESTARTS" "$RESIZES" "$FOCUS" "$MAPS" \
	+wait +wait +wait +wait +wait \
	+quit
rc=$?
if [[ $rc -eq 77 ]]; then
	echo "SKIP: Spine 1.1 GPU lifecycle (no client/display). Static: scripts/spine_1_1_cert_check.sh"
	exit 77
fi
if [[ $rc -ne 0 ]]; then
	echo "FAIL: Spine 1.1 lifecycle client exited $rc" >&2
	exit 1
fi
echo "PASS: Spine 1.1 lifecycle client completed (inspect log for CERT PASS/FAIL)"
exit 0
