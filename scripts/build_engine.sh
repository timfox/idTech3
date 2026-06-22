#!/usr/bin/env bash
# Convenience wrapper: same as scripts/compile_engine.sh vulkan (Release).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
exec "$ROOT/compile_engine.sh" vulkan "$@"
