#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
H="$ROOT/renderers/vulkan/extensions/neural/vk_neural_deferred.h"
C="$ROOT/renderers/vulkan/extensions/neural/vk_neural_deferred.c"
DOC="$ROOT/docs/NEURAL_DEFERRED_SHADER.md"
CFG="$ROOT/config/vulkan_overlay_neural_deferred.cfg"
SHADER="$ROOT/renderers/vulkan/shaders/glsl/neural_deferred/neural_energy_guard.comp"

grep -q 'vkNeuralDeferredContract_t' "$H"
grep -q 'channelCount' "$H"
grep -q 'r_neuralDeferredDarkGate' "$C"
grep -q 'compare_only' "$C"
grep -q 'does not own primary visibility' "$DOC"
grep -q 'r_neuralDeferred 1' "$CFG"
grep -q 'smoothstep' "$SHADER"
grep -q 'darkThreshold' "$SHADER"
grep -q 'neural_energy_guard.comp' "$ROOT/scripts/compile_shaders.sh"
echo "test_neural_deferred.sh: ok"

