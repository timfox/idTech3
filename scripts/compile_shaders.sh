#!/bin/bash

# Shader Compilation Script for idTech3 Vulkan Renderer
# This script compiles GLSL shaders to SPIR-V format
# Shaders are located in src/shaders/

echo "idTech3 Vulkan Shader Compilation Script"
echo "========================================"
echo "Shaders are located in src/shaders/"
echo ""

# Run the actual compilation script from src/shaders/
exec ./src/shaders/compile_shaders.sh