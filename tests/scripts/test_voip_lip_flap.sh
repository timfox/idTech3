#!/usr/bin/env bash
# Wiring test: VoIP lip flap APIs and cgame hook.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }

VOIP="${IDTECH3_CLIENT}/platform/cl_voip.c"
VOIPH="${IDTECH3_CLIENT}/platform/cl_voip.h"
CGAME="${IDTECH3_CLIENT}/core/cl_cgame.c"
DOC="${ROOT}/docs/VOIP.md"

[ -f "$VOIP" ] || fail "missing cl_voip.c"
[ -f "$VOIPH" ] || fail "missing cl_voip.h"
[ -f "$CGAME" ] || fail "missing cl_cgame.c"
[ -f "$DOC" ] || fail "missing docs/VOIP.md"

rg -q 'CL_VoIP_ApplyLipFlap' "$VOIPH" || fail "ApplyLipFlap not declared"
rg -q 'CL_VoIP_GetClientPower' "$VOIPH" || fail "GetClientPower not declared"
rg -q 'cl_voipLipFlap' "$VOIP" || fail "cl_voipLipFlap cvar missing"
rg -q 'CL_VoIP_ApplyLipFlap' "$CGAME" || fail "cgame not calling ApplyLipFlap"
rg -q '\+voip' "$VOIP" || fail "+voip command missing"
rg -q 'SetEntityMorphWeight' "$VOIP" || fail "morph weight drive missing"
rg -q 'CL_VoIP_SyncFacs|FACS_AU26' "$VOIP" || fail "VoIP→FACS AU26 sync missing"
rg -q 'cl_voipLipFlapFacs' "$VOIP" || fail "cl_voipLipFlapFacs cvar missing"
rg -q 'g_facial.h' "$VOIP" || fail "VoIP must include g_facial.h for FACS"

echo "test_voip_lip_flap: passed"
