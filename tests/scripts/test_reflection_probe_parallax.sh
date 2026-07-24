#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -qi 'parallax' "$ROOT/renderers/vulkan/vk_environment_probes.c"
echo "PASS: parallax probe design noted"
