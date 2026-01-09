#!/bin/bash
# Manual Vulkan launcher
# Uses the main launcher script for proper environment setup

set -euo pipefail

# Simply call the main launcher with Vulkan
exec "$(dirname "$0")/run_engine.sh" --vulkan "$@"