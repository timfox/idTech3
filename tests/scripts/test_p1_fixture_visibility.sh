#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
h="$ROOT/renderers/vulkan/vk_renderer_p1_live.h"
c="$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
for x in IQ_FIXTURE_NOT_ARMED IQ_FIXTURE_NOT_SUBMITTED IQ_FIXTURE_REGION_EMPTY IQ_FIXTURE_TARGET_UNCHANGED; do
	grep -q "$x" "$h"
done
grep -q 'P1_Live_ProveFixture\|IQ_FIXTURE_REGION_EMPTY' "$c"
echo "All p1_fixture_visibility checks passed."
