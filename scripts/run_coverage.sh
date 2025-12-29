#!/usr/bin/env bash
#===========================================================================
# Code Coverage Reporting Script
#===========================================================================
# Generates code coverage reports using gcovr or lcov
#
# Usage:
#   ./tools/run_coverage.sh [gcovr|lcov|both]
#
# Prerequisites:
#   - Build with: cmake -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON
#   - gcovr or lcov installed
#===========================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
REPORT_TYPE="${1:-gcovr}"

cd "$PROJECT_ROOT"

# Check if coverage is enabled in build
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist"
    echo "Please build first with: cmake -B $BUILD_DIR -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON"
    exit 1
fi

# Check for coverage files
if ! find "$BUILD_DIR" -name "*.gcda" -o -name "*.gcno" | head -1 | grep -q .; then
    echo "Warning: No coverage files (.gcda/.gcno) found"
    echo "Ensure build was configured with: -DENABLE_COVERAGE=ON"
    echo "And that tests have been run"
fi

cd "$BUILD_DIR"

# Run tests if they haven't been run
if [ ! -f "Testing/TAG" ]; then
    echo "Running tests to generate coverage data..."
    ctest --output-on-failure || {
        echo "Warning: Some tests failed, but continuing with coverage report"
    }
fi

case "$REPORT_TYPE" in
    gcovr)
        if ! command -v gcovr >/dev/null 2>&1; then
            echo "Error: gcovr not found"
            echo "Install with: sudo apt install gcovr  or  pip install gcovr"
            exit 1
        fi
        
        echo "Generating coverage report with gcovr..."
        cmake --build . --target coverage
        echo ""
        echo "Coverage reports generated:"
        echo "  - HTML: $BUILD_DIR/coverage.html"
        echo "  - XML:  $BUILD_DIR/coverage.xml"
        echo "  - TXT:  $BUILD_DIR/coverage.txt"
        ;;
    
    lcov)
        if ! command -v lcov >/dev/null 2>&1 || ! command -v genhtml >/dev/null 2>&1; then
            echo "Error: lcov/genhtml not found"
            echo "Install with: sudo apt install lcov"
            exit 1
        fi
        
        echo "Generating coverage report with lcov..."
        cmake --build . --target coverage_lcov
        echo ""
        echo "Coverage report generated:"
        echo "  - HTML: $BUILD_DIR/coverage_lcov/index.html"
        ;;
    
    both)
        echo "Generating coverage reports with both tools..."
        "$0" gcovr
        echo ""
        "$0" lcov
        ;;
    
    *)
        echo "Usage: $0 [gcovr|lcov|both]"
        echo ""
        echo "Options:"
        echo "  gcovr  - Generate reports using gcovr (default)"
        echo "  lcov   - Generate reports using lcov"
        echo "  both   - Generate reports using both tools"
        exit 1
        ;;
esac

echo ""
echo "To view HTML report:"
if [ "$REPORT_TYPE" = "lcov" ] || [ "$REPORT_TYPE" = "both" ]; then
    echo "  xdg-open $BUILD_DIR/coverage_lcov/index.html  # Linux"
    echo "  open $BUILD_DIR/coverage_lcov/index.html      # macOS"
fi
if [ "$REPORT_TYPE" = "gcovr" ] || [ "$REPORT_TYPE" = "both" ]; then
    echo "  xdg-open $BUILD_DIR/coverage.html              # Linux"
    echo "  open $BUILD_DIR/coverage.html                  # macOS"
fi
