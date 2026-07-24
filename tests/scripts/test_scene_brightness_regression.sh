#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
"$ROOT/tests/scripts/test_tonemap_middle_gray.sh"
"$ROOT/tests/scripts/test_exposure_ev_sign.sh"
echo "test_scene_brightness_regression: ok"
