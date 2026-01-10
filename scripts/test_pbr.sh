#!/bin/bash

# PBR (Physically Based Rendering) Feature Test Script for idTech3
# Tests material system, advanced materials, and PBR rendering features

set -euo pipefail

echo "idTech3 PBR Feature Test"
echo "========================"
echo

# Set paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="$PROJECT_ROOT/release/idtech3.x86_64"

# Check if engine exists
if [ ! -x "$ENGINE" ]; then
    echo "Error: Engine not found at $ENGINE"
    echo "Please build the engine first with: ./scripts/compile_engine.sh vulkan"
    exit 1
fi

echo "Testing PBR/Material System Features..."
echo

# Test 1: Basic material system enable
echo "Test 1: Material System Enable"
echo "Command: +set r_materialSystem 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_materialSystem 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(material|MATERIAL|PBR|pbr|RTX|rtx)" | head -5

echo
echo "Test 2: PBR with HDR and Tonemapping"
echo "Command: +set r_materialSystem 1 +set r_hdr 1 +set r_tonemapMode 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_materialSystem 1 \
    +set r_hdr 1 \
    +set r_tonemapMode 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(HDR|hdr|tonemap|TONEMAP|material|MATERIAL)" | head -5

echo
echo "Test 3: Advanced Post-Processing with PBR"
echo "Command: +set r_materialSystem 1 +set r_bloom 1 +set r_postQuality 3 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_materialSystem 1 \
    +set r_bloom 1 \
    +set r_postQuality 3 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(bloom|BLOOM|post|POST|material|MATERIAL)" | head -5

echo
echo "Test 4: Combined RTX + PBR Features"
echo "Command: +set r_rtx_enable 1 +set r_materialSystem 1 +set r_hdr 1 +set r_bloom 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_materialSystem 1 \
    +set r_hdr 1 \
    +set r_bloom 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|rtx|PBR|pbr|HDR|hdr|bloom|BLOOM)" | head -5

echo
echo "PBR Feature Test Complete!"
echo
echo "Available PBR/Material CVars:"
echo "  r_materialSystem - Enable advanced material system (0/1)"
echo "  r_hdr - Enable HDR rendering (0/1)"
echo "  r_tonemapMode - Tonemapping mode (0=off, 1=ACES, 2=Reinhard, 3=Uncharted)"
echo "  r_tonemapExposure - Tonemapping exposure adjustment"
echo "  r_bloom - Enable bloom post-processing (0/1)"
echo "  r_postQuality - Post-processing quality (0-4)"
echo "  r_gamma - Gamma correction value"
echo
echo "Material Features:"
echo "- Physically Based Rendering (PBR) pipeline"
echo "- Advanced material properties (metallic, roughness, etc.)"
echo "- HDR rendering with proper tonemapping"
echo "- Bloom and advanced post-processing effects"
echo "- Material clearcoat, anisotropy, and subsurface scattering"
echo
echo "Performance Notes:"
echo "- PBR requires Vulkan renderer (r_vulkan 1)"
echo "- HDR rendering increases VRAM usage"
echo "- Post-processing quality affects performance"
echo "- RTX + PBR provides the most advanced visual quality"