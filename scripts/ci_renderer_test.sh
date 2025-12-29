#!/bin/bash
#
# CI Renderer Test Script
# Tests renderer loading and basic functionality for CI/CD pipelines
#

set -e

echo "=== CI Renderer Test ==="
echo "Testing renderer libraries and basic functionality..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test directory
TEST_DIR="$(dirname "$0")/../build"
RENDERER_DIR="$TEST_DIR"

# Check if we're in the right directory
if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}ERROR: Build directory not found at $TEST_DIR${NC}"
    echo "Please run this script from the project root or ensure build/ exists"
    exit 1
fi

cd "$TEST_DIR"

echo "Testing renderer library loading..."

# Test renderer libraries
RENDERERS=("vulkan" "opengl2" "opengl")
FOUND_RENDERERS=0

for renderer in "${RENDERERS[@]}"; do
    LIB_PATH="./idtech3_${renderer}_x86_64.so"
    if [ -f "$LIB_PATH" ]; then
        echo -e "${GREEN}✓ Found $renderer renderer: $LIB_PATH${NC}"

        # Test basic symbol loading
        if nm -D "$LIB_PATH" | grep -q "GetRefAPI"; then
            echo -e "${GREEN}  ✓ GetRefAPI symbol found${NC}"
        else
            echo -e "${RED}  ✗ GetRefAPI symbol missing${NC}"
            exit 1
        fi

        FOUND_RENDERERS=$((FOUND_RENDERERS + 1))
    else
        echo -e "${YELLOW}⚠ $renderer renderer not found: $LIB_PATH${NC}"
    fi
done

if [ $FOUND_RENDERERS -eq 0 ]; then
    echo -e "${RED}ERROR: No renderer libraries found!${NC}"
    exit 1
fi

echo -e "${GREEN}Found $FOUND_RENDERERS renderer(s)${NC}"

# Test renderer init test
echo "Testing renderer initialization..."
if [ -f "./tests/test_renderer_init" ]; then
    echo "Running renderer init test..."
    if ./tests/test_renderer_init; then
        echo -e "${GREEN}✓ Renderer init test passed${NC}"
    else
        echo -e "${RED}✗ Renderer init test failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠ Renderer init test not found, skipping...${NC}"
fi

# Test integration test
echo "Testing integration..."
if [ -f "./tests/test_integration" ]; then
    echo "Running integration test..."
    if ./tests/test_integration; then
        echo -e "${GREEN}✓ Integration test passed${NC}"
    else
        echo -e "${RED}✗ Integration test failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠ Integration test not found, skipping...${NC}"
fi

# Test CMake configuration
echo "Testing CMake configuration..."
if grep -q "Tests enabled:" CMakeCache.txt; then
    echo -e "${GREEN}✓ CMake tests configured${NC}"
else
    echo -e "${YELLOW}⚠ CMake test configuration unclear${NC}"
fi

echo -e "${GREEN}=== CI Renderer Test Completed Successfully ===${NC}"
echo "Renderer libraries: $FOUND_RENDERERS found"
echo "Tests: All available tests passed"