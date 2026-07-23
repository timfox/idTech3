#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
for f in r_oitFaultTreatStraightAsPremul r_oitFaultTreatPremulAsStraight r_oitFaultDoublePremultiply \
	r_oitFaultSkipAlphaMultiply r_oitFaultZeroAlphaColoredRgb r_oitFaultInvalidAlpha; do
	grep -q "$f" "$ROOT/renderers/vulkan/vk_oit_alpha.c" || { echo "FAIL missing $f"; exit 1; }
done
echo "OK: alpha fault injection cvars"
