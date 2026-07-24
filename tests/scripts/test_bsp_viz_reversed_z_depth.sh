#!/usr/bin/env bash
# Alias gate kept for older CI references; delegates to full regression.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
exec bash "${repo_root}/tests/scripts/test_bsp_viz_regression.sh"
