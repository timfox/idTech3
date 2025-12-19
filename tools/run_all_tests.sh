#!/bin/bash

# Script to run all unit tests in the idtech3 project
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Running all idtech3 unit tests..."
echo "Project root: $PROJECT_ROOT"
echo

cd "$PROJECT_ROOT"

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Build directory not found. Please build the project first with:"
    echo "  ./tools/compile_engine.sh"
    exit 1
fi

cd build

echo "=== Running Math Tests ==="
if [ -f "test_qmath" ]; then
    ./test_qmath
    echo
fi

echo "=== Running Network Tests ==="
if [ -f "test_network_enet" ]; then
    ./test_network_enet
    echo
fi

echo "=== Running Geometry Tests ==="
if [ -f "test_geometry" ]; then
    ./test_geometry
    echo
fi

echo "=== Running Security Tests ==="
if [ -f "test_security" ]; then
    ./test_security
    echo
fi

echo "=== Running GUI Tests ==="
if [ -f "test_gui_basic" ]; then
    ./test_gui_basic
    echo
fi

echo "=== Running Property-Based Tests ==="
if [ -f "test_property_based" ]; then
    ./test_property_based
    echo
fi

echo "=== Running Performance Benchmarks ==="
if [ -f "benchmark_performance" ]; then
    ./benchmark_performance
    echo
fi

echo "All tests completed successfully! ✅"
