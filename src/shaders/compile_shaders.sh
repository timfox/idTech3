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

# Compile all .glsl and .comp files to .spv files
for glsl_file in *.glsl *.comp; do
    if [ -f "$glsl_file" ]; then
        # Handle both .glsl and .comp extensions
        if [[ "$glsl_file" == *.comp ]]; then
            base_name="${glsl_file%.comp}"
            stage="comp"
        else
            base_name="${glsl_file%.glsl}"
            stage=""
        fi
        spv_file="${base_name}.spv"

        # Determine shader stage from filename (if not already set for .comp files)
        if [ -z "$stage" ]; then
            if [[ "$glsl_file" == *"_vert"* ]] || [[ "$glsl_file" == *".vert"* ]]; then
                stage="vert"
            elif [[ "$glsl_file" == *"_frag"* ]] || [[ "$glsl_file" == *".frag"* ]]; then
                stage="frag"
            elif [[ "$glsl_file" == *"_comp"* ]] || [[ "$glsl_file" == *"comp.glsl" ]]; then
                stage="comp"
            elif [[ "$glsl_file" == *"raytrace"* ]] || [[ "$glsl_file" == *"trace"* ]]; then
                # Ray tracing shaders - check if it's a library or specific stage
                # For now, skip library shaders that don't have a specific stage
                if [[ "$glsl_file" == *"sdf_grid_raytrace"* ]]; then
                    echo "  ⚠ Skipping $glsl_file (library shader, not a specific stage)"
                    continue
                fi
            elif [[ "$glsl_file" == *"_geom"* ]] || [[ "$glsl_file" == *".geom"* ]]; then
                stage="geom"
            elif [[ "$glsl_file" == *"_tesc"* ]] || [[ "$glsl_file" == *".tesc"* ]]; then
                stage="tesc"
            elif [[ "$glsl_file" == *"_tese"* ]] || [[ "$glsl_file" == *".tese"* ]]; then
                stage="tese"
            elif [[ "$glsl_file" == *"_rgen"* ]] || [[ "$glsl_file" == *".rgen"* ]]; then
                stage="rgen"
            elif [[ "$glsl_file" == *"_rchit"* ]] || [[ "$glsl_file" == *".rchit"* ]]; then
                stage="rchit"
            elif [[ "$glsl_file" == *"_rmiss"* ]] || [[ "$glsl_file" == *".rmiss"* ]]; then
                stage="rmiss"
            elif [[ "$glsl_file" == *"_rahit"* ]] || [[ "$glsl_file" == *".rahit"* ]]; then
                stage="rahit"
            elif [[ "$glsl_file" == *"_rint"* ]] || [[ "$glsl_file" == *".rint"* ]]; then
                stage="rint"
            elif [[ "$glsl_file" == *"_rcall"* ]] || [[ "$glsl_file" == *".rcall"* ]]; then
                stage="rcall"
            elif [[ "$glsl_file" == *"_mesh"* ]] || [[ "$glsl_file" == *".mesh"* ]]; then
                stage="mesh"
            elif [[ "$glsl_file" == *"_task"* ]] || [[ "$glsl_file" == *".task"* ]]; then
                stage="task"
            fi
        fi

        if [ -z "$stage" ]; then
            echo "  ⚠ Skipping $glsl_file (could not determine shader stage)"
            continue
        fi

        echo "Compiling $glsl_file -> $spv_file (stage: $stage)"
        if glslangValidator -V -S "$stage" "$glsl_file" -o "$spv_file"; then
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