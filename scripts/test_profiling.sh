#!/bin/bash

# Performance Profiling Test Script for idTech3
# Tests advanced profiling tools, benchmarks, and performance monitoring

set -euo pipefail

echo "idTech3 Performance Profiling Test"
echo "==================================="
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

echo "Testing Performance Profiling Features..."
echo

# Test 1: Basic Vulkan profiling
echo "Test 1: Vulkan Performance Profiling"
echo "Command: +set r_vk_profiling 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 15 "$ENGINE" \
    +set r_vk_profiling 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(profile|PROFILE|perf|PERF|fps|FPS|frame|FRAME)" | head -10

echo
echo "Test 2: Advanced Performance Monitoring"
echo "Command: +set r_vk_debug_overlay 1 +set com_speeds 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 15 "$ENGINE" \
    +set r_vk_debug_overlay 1 \
    +set com_speeds 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(overlay|OVERLAY|speeds|SPEEDS|fps|FPS|debug|DEBUG)" | head -10

echo
echo "Test 3: Frame Telemetry and Benchmarking"
echo "Command: +set r_frameTelemetry 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 15 "$ENGINE" \
    +set r_frameTelemetry 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(telemetry|TELEMETRY|frame|FRAME|benchmark|BENCHMARK)" | head -10

echo
echo "Test 4: Memory and Performance Statistics"
echo "Command: +set developer 1 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 15 "$ENGINE" \
    +set developer 1 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(developer|DEVELOPER|mem|MEM|alloc|ALLOC|perf|PERF)" | head -15

echo
echo "Test 5: GPU Performance Analysis"
echo "Command: +set r_vk_hotReload 0 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1 +set com_introplayed 1 +map q3dm9"
timeout 15 "$ENGINE" \
    +set r_vk_hotReload 0 \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    +map q3dm9 \
    2>&1 | grep -E "(GPU|gpu|hotReload|reload|shader|SHADER)" | head -10

echo
echo "Performance Profiling Test Complete!"
echo
echo "Available Profiling CVars:"
echo "  r_vk_profiling - Enable Vulkan performance profiling (0/1)"
echo "  r_vk_debug_overlay - Show Vulkan debug overlay (0/1)"
echo "  r_frameTelemetry - Enable frame time telemetry (0/1)"
echo "  com_speeds - Show engine performance counters (0/1)"
echo "  developer - Enable developer mode with extra logging (0/1)"
echo "  r_vk_hotReload - Enable shader hot reloading (0/1)"
echo
echo "Performance Monitoring Features:"
echo "- Real-time FPS and frame time display"
echo "- Vulkan API call profiling"
echo "- Memory allocation tracking"
echo "- GPU performance counters"
echo "- Shader compilation statistics"
echo "- Frame pacing analysis"
echo "- Debug overlay with performance metrics"
echo
echo "Benchmarking Tools:"
echo "- Built-in benchmark suite (run_benchmarks.sh)"
echo "- Performance regression testing"
echo "- Memory usage analysis"
echo "- GPU utilization monitoring"
echo "- Frame time analysis and histograms"
echo
echo "Usage Tips:"
echo "- Use 'developer 1' for detailed performance logging"
echo "- Enable 'com_speeds 1' for real-time performance counters"
echo "- Vulkan debug overlay shows GPU memory and pipeline stats"
echo "- Frame telemetry helps identify performance bottlenecks"
echo "- Hot reload allows shader iteration without restarting"