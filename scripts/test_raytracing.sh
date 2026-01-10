#!/bin/bash

# Ray Tracing Feature Test Script for idTech3
# Tests various ray tracing capabilities and performance

set -euo pipefail

echo "idTech3 Ray Tracing Feature Test"
echo "================================="
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

echo "Testing Ray Tracing Features..."
echo

# Test 1: Basic ray tracing enable
echo "Test 1: Basic Ray Tracing Enable"
echo "Command: +set r_rtx_enable 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|ray|RT_|rt_)" | head -5

echo
echo "Test 2: Ray Tracing with Reflections"
echo "Command: +set r_rtx_enable 1 +set r_rtx_reflections 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_rtx_reflections 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|ray|RT_|rt_|reflection)" | head -5

echo
echo "Test 3: Ray Tracing with Global Illumination"
echo "Command: +set r_rtx_enable 1 +set r_rtx_gi 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_rtx_gi 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|ray|RT_|rt_|GI|illumination)" | head -5

echo
echo "Test 4: Ray Tracing with Shadows"
echo "Command: +set r_rtx_enable 1 +set r_rtx_shadows 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_rtx_shadows 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|ray|RT_|rt_|shadow)" | head -5

echo
echo "Test 5: Ray Tracing Quality Settings"
echo "Command: +set r_rtx_enable 1 +set r_rtx_quality 2 +set r_rt_samples 4 +set r_rt_maxDepth 3 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 10 "$ENGINE" \
    +set r_rtx_enable 1 \
    +set r_rtx_quality 2 \
    +set r_rt_samples 4 \
    +set r_rt_maxDepth 3 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(RTX|ray|RT_|rt_|quality|samples|depth)" | head -5

echo
echo "Ray Tracing Feature Test Complete!"
echo
echo "Available Ray Tracing CVars:"
echo "  r_rtx_enable - Enable/disable ray tracing (0/1)"
echo "  r_rtx_shadows - Enable ray-traced shadows (0/1)"
echo "  r_rtx_reflections - Enable ray-traced reflections (0/1)"
echo "  r_rtx_gi - Enable ray-traced global illumination (0/1)"
echo "  r_rtx_quality - Ray tracing quality (0-3)"
echo "  r_rt_samples - Ray samples per pixel (1-16)"
echo "  r_rt_maxDepth - Maximum ray bounce depth (1-8)"
echo "  r_rt_debugMagenta - Debug visualization (0/1)"
echo
echo "Performance Notes:"
echo "- Ray tracing requires Vulkan renderer (r_vulkan 1)"
echo "- Higher quality settings significantly impact performance"
echo "- RTX-capable GPU required for hardware acceleration"
echo "- Fallback to software ray tracing on non-RTX GPUs"