#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'classic_alpha_translate_status' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'AGEN_PORTAL\|AGEN_WAVEFORM' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'CLASSIC_COMPATIBILITY' "$ROOT/docs/CLASSIC_SHADER_ALPHA_TRANSLATION.md"
echo "OK: classic alpha translation"
