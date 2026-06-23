#!/usr/bin/env bash
# Classic baseq3 lighting contract: r_classicLighting gates modern overrides.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'r_classicLighting' renderers/vulkan/tr_init.c || fail 'missing r_classicLighting cvar'
grep -q 'R_ClassicLightingActive' renderers/vulkan/tr_light.c || fail 'missing R_ClassicLightingActive'
grep -q 'R_ClassicLightingActive()' renderers/vulkan/tr_shade.c || fail 'tr_shade must gate on classic lighting'
grep -q 'R_ClassicLightingActive()' renderers/vulkan/tr_backend.c || fail 'sun shadow pass must gate on classic lighting'
grep -q 'retail cgame.qvm' runtime/client/core/cl_cgame.c || fail 'client qvm classic lighting log missing'

grep -q '"r_classicLighting", "1"' renderers/vulkan/tr_init.c || fail 'r_classicLighting default must be 1'
grep -q '"r_pbrSunShadow", "0"' renderers/vulkan/tr_init.c || fail 'r_pbrSunShadow default must be 0'
grep -q '"r_forwardPlusOverflowShade", "0"' renderers/vulkan/tr_init.c || fail 'overflow shade default must be 0'

pass "classic lighting contract"
