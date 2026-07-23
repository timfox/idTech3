#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$ROOT/tests/scripts/test_renderer_iq_p1.sh"
bash "$ROOT/tests/scripts/test_renderer_iq_profile.sh"
bash "$ROOT/tests/scripts/test_bloom_source_contract.sh"
bash "$ROOT/tests/scripts/test_bloom_firefly_control.sh"
bash "$ROOT/tests/scripts/test_temporal_history_registry.sh"
grep -q 'RENDERER_P1' "$ROOT/docs/RENDERER_P1_CERTIFICATION.md"
echo "test_renderer_p1_certification.sh OK"
