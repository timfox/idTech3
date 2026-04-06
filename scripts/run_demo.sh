#!/usr/bin/env bash
# One-command launcher: run idtech3 with the idtech3_demo mod using examples/demo_skeleton layout.
# Prerequisite: game .pk3 files in examples/demo_skeleton/base/ and idtech3_demo.pk3 in examples/demo_skeleton/idtech3_demo/
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT/examples/demo_skeleton/run_demo_client.sh" "$@"
