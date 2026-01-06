#!/bin/bash

# Linux shader compilation script for idTech3 Vulkan renderer
# Compiles GLSL shaders to SPIR-V using glslangValidator

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLSL_DIR="$SCRIPT_DIR/glsl"
SPIRV_DIR="$SCRIPT_DIR/spirv"
TOOLS_DIR="$SCRIPT_DIR/tools"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if glslangValidator is available
if ! command -v glslangValidator &> /dev/null; then
    echo -e "${RED}Error: glslangValidator not found. Please install Vulkan SDK.${NC}"
    exit 1
fi

# Check if bin2hex tool exists, build it if not
if [ ! -f "$TOOLS_DIR/bin2hex" ]; then
    echo "Building bin2hex tool..."
    gcc -o "$TOOLS_DIR/bin2hex" "$TOOLS_DIR/bin2hex.c"
fi

# Check if bindshader tool exists, build it if not
if [ ! -f "$TOOLS_DIR/bindshader" ]; then
    echo "Building bindshader tool..."
    g++ -o "$TOOLS_DIR/bindshader" "$TOOLS_DIR/bindshader.c"
fi

# Create SPIRV directory if it doesn't exist
mkdir -p "$SPIRV_DIR"

echo "Compiling shaders from $GLSL_DIR to $SPIRV_DIR..."

# Function to compile shader
compile_shader() {
    local input_file="$1"
    local output_file="$2"
    local stage="$3"
    local defines="$4"

    echo "Compiling $input_file -> $output_file"

    # Use Vulkan 1.2 target environment for ray tracing shaders
    local target_env=""
    if [[ "$stage" == "rgen" || "$stage" == "rmiss" || "$stage" == "rchit" || "$stage" == "rahit" || "$stage" == "rint" ]]; then
        target_env="--target-env vulkan1.2"
    fi

    if ! glslangValidator -S "$stage" -V $target_env $defines -o "$output_file" "$input_file" 2>/dev/null; then
        echo -e "${RED}Failed to compile $input_file${NC}"
        glslangValidator -S "$stage" -V $defines -o "$output_file" "$input_file"
        return 1
    fi

    return 0
}

# Function to convert SPIR-V to C header
convert_to_header() {
    local spirv_file="$1"
    local var_name="$2"
    local output_file="$3"

    echo "Converting $spirv_file to C header..."

    "$TOOLS_DIR/bin2hex" "$spirv_file" "$output_file" "$var_name"
}

# Compile individual shaders (vert, frag, geom, comp)
echo "Compiling individual shaders..."

# Vertex shaders
for shader in "$GLSL_DIR"/*.vert; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .vert)
        spirv_file="$SPIRV_DIR/${basename}_vert.spv"
        if compile_shader "$shader" "$spirv_file" "vert" ""; then
            convert_to_header "$spirv_file" "${basename}_vert_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Fragment shaders
for shader in "$GLSL_DIR"/*.frag; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .frag)
        spirv_file="$SPIRV_DIR/${basename}_frag.spv"
        if compile_shader "$shader" "$spirv_file" "frag" ""; then
            convert_to_header "$spirv_file" "${basename}_frag_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Geometry shaders
for shader in "$GLSL_DIR"/*.geom; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .geom)
        spirv_file="$SPIRV_DIR/${basename}_geom.spv"
        if compile_shader "$shader" "$spirv_file" "geom" ""; then
            convert_to_header "$spirv_file" "${basename}_geom_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Compute shaders
for shader in "$GLSL_DIR"/*.comp; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .comp)
        spirv_file="$SPIRV_DIR/${basename}_comp.spv"
        if compile_shader "$shader" "$spirv_file" "comp" ""; then
            convert_to_header "$spirv_file" "${basename}_comp_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Mesh shaders
for shader in "$GLSL_DIR"/*.mesh; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .mesh)
        spirv_file="$SPIRV_DIR/${basename}.mesh.spv"
        if compile_shader "$shader" "$spirv_file" "mesh" ""; then
            convert_to_header "$spirv_file" "${basename}_mesh_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Task shaders
for shader in "$GLSL_DIR"/*.task; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .task)
        spirv_file="$SPIRV_DIR/${basename}.task.spv"
        if compile_shader "$shader" "$spirv_file" "task" ""; then
            convert_to_header "$spirv_file" "${basename}_task_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Ray tracing shaders
for shader in "$GLSL_DIR"/*.rgen; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .rgen)
        spirv_file="$SPIRV_DIR/${basename}.rgen.spv"
        if compile_shader "$shader" "$spirv_file" "rgen" "-I$GLSL_DIR"; then
            convert_to_header "$spirv_file" "${basename}_rgen_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

for shader in "$GLSL_DIR"/*.rmiss; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .rmiss)
        spirv_file="$SPIRV_DIR/${basename}.rmiss.spv"
        if compile_shader "$shader" "$spirv_file" "rmiss" "-I$GLSL_DIR"; then
            convert_to_header "$spirv_file" "${basename}_rmiss_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

for shader in "$GLSL_DIR"/*.rchit; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .rchit)
        spirv_file="$SPIRV_DIR/${basename}.rchit.spv"
        if compile_shader "$shader" "$spirv_file" "rchit" "-I$GLSL_DIR"; then
            convert_to_header "$spirv_file" "${basename}_rchit_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

for shader in "$GLSL_DIR"/*.rahit; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .rahit)
        spirv_file="$SPIRV_DIR/${basename}.rahit.spv"
        if compile_shader "$shader" "$spirv_file" "rahit" "-I$GLSL_DIR"; then
            convert_to_header "$spirv_file" "${basename}_rahit_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

for shader in "$GLSL_DIR"/*.rint; do
    if [ -f "$shader" ]; then
        basename=$(basename "$shader" .rint)
        spirv_file="$SPIRV_DIR/${basename}.rint.spv"
        if compile_shader "$shader" "$spirv_file" "rint" "-I$GLSL_DIR"; then
            convert_to_header "$spirv_file" "${basename}_rint_spv" "$SPIRV_DIR/shader_data.c"
        fi
    fi
done

# Compile template-based shaders
echo "Compiling template-based shaders..."

# Template shader variations (simplified version)
# This would need to be expanded to match the Windows script

# Generate shader binding file
echo "Generating shader binding file..."

# Create binding file header
cat > "$SPIRV_DIR/shader_binding.c" << 'EOF'
// This file is autogenerated during shader compilation
__attribute__((used)) static void vk_set_shader_name(VkShaderModule shader, const char *name) {
    SET_OBJECT_NAME(shader, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
}

void vk_bind_generated_shaders(void) {
EOF

# Add shader bindings (this is a simplified version - would need full implementation)
# For now, just add a basic structure

echo "    // TODO: Add shader bindings here" >> "$SPIRV_DIR/shader_binding.c"
echo "}" >> "$SPIRV_DIR/shader_binding.c"

echo -e "${GREEN}Shader compilation completed!${NC}"
echo "SPIR-V files generated in $SPIRV_DIR/"
echo "Shader data: $SPIRV_DIR/shader_data.c"
echo "Shader bindings: $SPIRV_DIR/shader_binding.c"
