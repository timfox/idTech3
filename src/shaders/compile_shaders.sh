#!/bin/bash

# Shader Compilation Script for idTech3 Vulkan Renderer
# This script compiles GLSL shaders to SPIR-V format

echo "idTech3 Vulkan Shader Compilation Script"
echo "========================================"

cd /home/tim/Desktop/idtech3/src/shaders

# Check if glslangValidator is available
if ! command -v glslangValidator &> /dev/null; then
    echo "ERROR: glslangValidator not found!"
    echo "Install Vulkan SDK or shader tools to get glslangValidator"
    exit 1
fi

echo "Compiling GLSL shaders to SPIR-V..."

# Compile all .glsl files to .spv files
for glsl_file in *.glsl; do
    if [ -f "$glsl_file" ]; then
        base_name="${glsl_file%.glsl}"
        spv_file="${base_name}.spv"

        echo "Compiling $glsl_file -> $spv_file"
        if glslangValidator -V "$glsl_file" -o "$spv_file"; then
            echo "  ✓ Success"
        else
            echo "  ✗ Failed to compile $glsl_file"
        fi
    fi
done

# Copy SPIR-V files to game directories for runtime loading
echo "Copying compiled shaders to game directories..."
mkdir -p ../../../mods/mymod/shaders
cp *.spv ../../../mods/mymod/shaders/ 2>/dev/null

# List compiled shaders
echo ""
echo "Compiled shaders in src/shaders/:"
ls -la *.spv 2>/dev/null || echo "No SPIR-V files found"

echo ""
echo "Shaders copied to mods/mymod/shaders/:"
ls -la ../../../mods/mymod/shaders/*.spv 2>/dev/null || echo "No shaders in mymod directory"

echo ""
echo "Shader compilation completed!"
echo ""
echo "Usage notes:"
echo "- Edit .glsl files in src/shaders/ directory"
echo "- Run this script to compile them to SPIR-V"
echo "- The Vulkan renderer loads .spv files from game directories"
echo ""
echo "Example: To add a new shader 'myshader_vert.glsl':"
echo "  1. Create myshader_vert.glsl in src/shaders/ with #version 450"
echo "  2. Run: ./src/shaders/compile_shaders.sh"
echo "  3. Add to Vulkan code: vk_load_spirv_shader(\"myshader_vert\")"