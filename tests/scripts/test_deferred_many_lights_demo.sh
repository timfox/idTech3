#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

fail() { echo "FAIL: $*" >&2; exit 1; }

CFG="$ROOT/config/demo_deferred_many_lights.cfg"
SCENE="$ROOT/renderers/vulkan/tr_scene.c"
INIT="$ROOT/renderers/vulkan/tr_init.c"
LOCAL="$ROOT/renderers/vulkan/tr_local.h"
DOC="$ROOT/docs/DEFERRED_MANY_LIGHTS_DEMO.md"
STAGE="$ROOT/scripts/compile_engine.sh"

[[ -f "$CFG" ]] || fail "demo config missing"
[[ -f "$DOC" ]] || fail "demo docs missing"

grep -q 'seta r_renderMode 3' "$CFG" || fail "demo must use unified deferred clustered mode"
grep -q 'seta r_deferredLighting 1' "$CFG" || fail "deferred lighting must be enabled"
grep -q 'seta r_deferredLightDemo 1' "$CFG" || fail "demo light injector must be enabled"
grep -q 'seta r_deferredLightDemoCount 64' "$CFG" || fail "demo should exercise 64 lights"
grep -q 'seta r_forwardPlusDebug' "$CFG" || fail "demo needs visible occupancy proof"

grep -q 'R_AddDeferredLightDemoLights' "$SCENE" || fail "scene injector missing"
grep -q 'R_DeferredLightDemoStatus_f' "$SCENE" || fail "status command implementation missing"
grep -q 'deferred_many_lights_status' "$INIT" || fail "status command registration missing"
grep -q 'r_deferredLightDemoCount' "$LOCAL" || fail "cvar declarations missing"
grep -q 'demo_deferred_many_lights.cfg' "$STAGE" || fail "compile_engine staging list missing demo cfg"
grep -q 'cluster_status' "$DOC" || fail "docs must include runtime proof command"

echo "PASS: deferred many-lights demo"
