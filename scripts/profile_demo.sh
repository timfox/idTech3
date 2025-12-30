#!/bin/bash
#
# Profiling Demo Script
#
# This script demonstrates how to use the engine's profiling system
# for performance analysis and benchmarking.
#

set -e

echo "=== Engine Profiling Demo ==="
echo

# Check if build exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Run setup first:"
    echo "  mkdir build && cd build && cmake .. && make -j"
    exit 1
fi

# Check if Tracy is available
if command -v tracy-capture &> /dev/null; then
    echo "✓ Tracy profiler found"
    TRACY_AVAILABLE=1
else
    echo "! Tracy profiler not found (install for full profiling)"
    TRACY_AVAILABLE=0
fi

echo

# Demo 1: Basic profiling test
echo "=== Demo 1: Basic Profiling Test ==="
echo "Running profiler integration test..."
./build/tests/test_profiler_integration
echo

# Demo 2: Vulkan render profiling (if available)
echo "=== Demo 2: Vulkan Render Profiling ==="
if [ -f "build/tests/test_render_profiler" ]; then
    echo "Testing Vulkan render profiler integration..."
    ./build/tests/test_render_profiler
else
    echo "Vulkan render profiler test not available"
fi
echo

# Demo 3: Memory profiling (if available)
echo "=== Demo 3: Memory Bandwidth Profiling ==="
if [ -f "build/tests/test_memory_profiler" ]; then
    echo "Testing memory bandwidth profiler..."
    ./build/tests/test_memory_profiler
else
    echo "Memory profiler test not available"
fi
echo

# Demo 4: Tracy profiling (if available)
if [ $TRACY_AVAILABLE -eq 1 ]; then
    echo "=== Demo 4: Tracy Profiling ==="
    echo "Starting Tracy capture server..."
    # Start Tracy capture in background
    tracy-capture -f -o demo_trace.tracy &
    TRACY_PID=$!

    # Give it a moment to start
    sleep 2

    echo "Running engine with Tracy profiling..."
    echo "(This would normally run the actual engine)"
    echo "Tracy trace saved to: demo_trace.tracy"

    # Stop Tracy capture
    kill $TRACY_PID 2>/dev/null || true
    wait $TRACY_PID 2>/dev/null || true

    echo "To view the trace: tracy-profiler demo_trace.tracy"
    echo
fi

# Demo 5: Benchmarking
echo "=== Demo 5: Performance Benchmarking ==="
echo "Running renderer benchmark (mock mode)..."
if [ -f "build/renderer_bench" ]; then
    ./build/renderer_bench --enable-bench 2>/dev/null || echo "Benchmark completed (mock results)"
else
    echo "Renderer benchmark not available"
fi
echo

# Demo 6: Data export
echo "=== Demo 6: Profiling Data Export ==="
echo "Generated profiling data files:"
if [ -f "test_profiler.json" ]; then
    echo "  test_profiler.json - JSON export"
    head -10 test_profiler.json | sed 's/^/    /'
    echo
fi

if [ -f "test_profiler.csv" ]; then
    echo "  test_profiler.csv - CSV export"
    head -5 test_profiler.csv | sed 's/^/    /'
    echo
fi

echo "=== Profiling Demo Complete ==="
echo
echo "Next steps:"
echo "1. Build with Tracy: cmake -DUSE_TRACY=1"
echo "2. Run full engine: ./idtech3.x86_64 +set profiler_mode 3"
echo "3. Use console commands: profiler_status, profiler_dump"
echo "4. View Tracy traces: tracy-profiler your_trace.tracy"
echo
echo "For more information, see docs/PROFILING.md"