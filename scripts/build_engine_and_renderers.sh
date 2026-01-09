#!/usr/bin/env bash
set -euo pipefail

./scripts/compile_engine.sh opengl Release
./scripts/compile_engine.sh vulkan Release