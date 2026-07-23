#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
f="$ROOT/renderers/vulkan/vk_renderer_p1_live.h"
c="$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
failures=0
fail(){ echo "FAIL: $*"; failures=$((failures+1)); }
for s in P1_LIVE_IDLE P1_LIVE_PREFLIGHT P1_LIVE_WARMUP P1_LIVE_WAIT_FOR_READBACK \
	P1_LIVE_VALIDATE_FRAME_IDENTITY P1_LIVE_EVALUATE P1_LIVE_COMPLETE P1_LIVE_FAILED; do
	grep -q "$s" "$f" || fail "missing state $s"
done
grep -q 'iq_certify_status' "$c" || fail 'iq_certify_status'
grep -q 'iq_certify_abort' "$c" || fail 'iq_certify_abort'
grep -q 'iq_certify_retry' "$c" || fail 'iq_certify_retry'
grep -q 'renderer_p1_certify' "$c" || fail 'renderer_p1_certify'
grep -q 'Do not evaluate before' "$ROOT/docs/RENDERER_IQ_LIVE_CERTIFICATION.md" || \
	grep -q 'Do not evaluate before the readback fence' "$ROOT/docs/RENDERER_IQ_LIVE_CERTIFICATION.md" || fail 'doc fence rule'
[[ $failures -eq 0 ]] || exit 1
echo "All p1_live_state_machine checks passed."
