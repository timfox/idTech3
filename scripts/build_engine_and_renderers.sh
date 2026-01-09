#!/usr/bin/env bash
set -euo pipefail

./scripts/compile_engine.sh OpenGL Release
./scripts/compile_engine.sh Vulkan Release