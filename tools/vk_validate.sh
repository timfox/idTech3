#!/usr/bin/env bash
#
# Helper: launch the engine with Vulkan validation layers enabled.
# - Enables core validation + best-practices + sync validation.
# - Writes validation output to stdout and optionally to a log file.
#
# Usage:
#   ./tools/vk_validate.sh [engine_binary] [args...]
# Example:
#   ./tools/vk_validate.sh ./release/idtech3.x86_64.so +set r_renderer vulkan
#
# Notes:
# - Requires the Vulkan SDK runtime layers to be installed
#   (e.g., mesa-vulkan-drivers and vulkan-validationlayers).
# - Does not modify config; it only sets process-local env vars.

set -euo pipefail

ENGINE_BIN="${1:-./release/idtech3.x86_64.so}"
shift || true

# Recommended layer set for dev: validation + best practices + sync
export VK_LAYER_PATH="${VK_LAYER_PATH:-}"
export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
export VK_KHRONOS_VALIDATION_ENABLES="VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT"

# Optional: direct messages to a log file instead of stderr
if [[ -n "${VK_VALIDATE_LOG:-}" ]]; then
  export VK_LOADER_DEBUG_FILE="$VK_VALIDATE_LOG"
fi

exec "${ENGINE_BIN}" "$@"

