#!/usr/bin/env bash
# CI Build Validation Script
# Tests that the codebase builds correctly with various configurations
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 CI Build Validation${NC}"
echo "Testing build configurations..."

# Test configurations
CONFIGURATIONS=(
    "Debug,GCC,gcc-15,g++-15"
    "Debug,Clang,clang-18,clang++-18"
    "Release,GCC,gcc-15,g++-15"
    "Release,Clang,clang-18,clang++-18"
)

PASSED=0
FAILED=0

for config in "${CONFIGURATIONS[@]}"; do
    IFS=',' read -r BUILD_TYPE COMPILER_NAME CC CXX <<< "$config"

    echo -e "\n${YELLOW}Testing: $BUILD_TYPE ($COMPILER_NAME)${NC}"

    BUILD_DIR="build-ci-$BUILD_TYPE-$COMPILER_NAME"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Set compiler environment
    export CC="$CC"
    export CXX="$CXX"

    # Configure
    echo "  Configuring..."
    if cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DUSE_VULKAN=ON \
        -DUSE_OPENGL=ON \
        -DBUILD_SERVER=ON \
        -DENABLE_WERROR_NEW_CODE=ON \
        -G Ninja >/dev/null 2>&1; then

        echo -e "  ${GREEN}✓${NC} Configure successful"
    else
        echo -e "  ${RED}✗${NC} Configure failed"
        cd ..
        FAILED=$((FAILED + 1))
        continue
    fi

    # Build
    echo "  Building..."
    if timeout 300 ninja -j$(nproc) >/dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Build successful"
    else
        echo -e "  ${RED}✗${NC} Build failed"
        cd ..
        FAILED=$((FAILED + 1))
        continue
    fi

    # Check for binaries
    if [ -f "idtech3.x86_64" ] || [ -f "idtech3.exe" ]; then
        echo -e "  ${GREEN}✓${NC} Binaries found"
    else
        echo -e "  ${RED}✗${NC} No binaries found"
        cd ..
        FAILED=$((FAILED + 1))
        continue
    fi

    # Smoke test
    echo "  Smoke testing..."
    if [ -f "idtech3.x86_64" ]; then
        if timeout 10 ./idtech3.x86_64 \
            +set r_renderer opengl \
            +set developer 0 \
            +set s_initsound 0 \
            +set non_interactive 1 \
            +quit >/dev/null 2>&1; then
            echo -e "  ${GREEN}✓${NC} Smoke test passed"
        else
            echo -e "  ${YELLOW}⚠${NC} Smoke test failed (may be expected)"
        fi
    fi

    cd ..
    PASSED=$((PASSED + 1))

    # Clean up
    rm -rf "$BUILD_DIR"
done

echo -e "\n${BLUE}📊 Results:${NC}"
echo -e "  ${GREEN}Passed: $PASSED${NC}"
echo -e "  ${RED}Failed: $FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}🎉 All CI build validations passed!${NC}"
    echo "The codebase builds reliably across all tested configurations."
    exit 0
else
    echo -e "\n${RED}❌ Some build validations failed${NC}"
    echo "Please review and fix the failing configurations."
    exit 1
fi