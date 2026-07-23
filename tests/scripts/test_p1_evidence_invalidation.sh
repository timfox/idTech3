#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'vk_renderer_p1_evidence_invalidate_dep' "$ROOT/renderers/vulkan/vk_renderer_p1_evidence.c"
grep -q 'iq_evidence_invalidate' "$ROOT/renderers/vulkan/vk_renderer_p1_evidence.c"
grep -q 'threshold' "$ROOT/docs/RENDERER_P1_EVIDENCE.md"
echo "All p1_evidence_invalidation checks passed."
