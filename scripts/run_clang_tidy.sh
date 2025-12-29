#!/bin/bash

# Clang-tidy runner script for id Tech 3
# Usage: ./tools/run_clang_tidy.sh [options] [files...]

set -e

# Check if clang-tidy is installed
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found. Install with: sudo apt install clang-tidy"
    exit 1
fi

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default options
CLANG_TIDY_OPTIONS=""
OUTPUT_FILE="clang_tidy.log"
QUIET_MODE=false
FIX_MODE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX_MODE=true
            CLANG_TIDY_OPTIONS="$CLANG_TIDY_OPTIONS --fix"
            shift
            ;;
        --quiet)
            QUIET_MODE=true
            CLANG_TIDY_OPTIONS="$CLANG_TIDY_OPTIONS --quiet"
            shift
            ;;
        --output=*)
            OUTPUT_FILE="${1#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [options] [files...]"
            echo ""
            echo "Options:"
            echo "  --fix          Automatically fix issues where possible"
            echo "  --quiet        Suppress progress messages"
            echo "  --output=FILE  Write output to FILE (default: clang_tidy.log)"
            echo "  --help         Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 src/common/*.c          # Analyze specific files"
            echo "  $0 --fix src/client/*.c     # Fix issues in client code"
            echo "  $0 --quiet                   # Analyze all source files quietly"
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
        *)
            # File argument
            break
            ;;
    esac
done

# If no files specified, find all C/C++ source files
if [ $# -eq 0 ]; then
    echo "Finding all C/C++ source files..."
    FILES=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/radiant" \
        -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \
        | grep -v "_autogen" \
        | grep -v "moc_" \
        | head -50)  # Limit to first 50 files for performance
else
    FILES="$@"
fi

# Check if .clang-tidy config exists
if [ -f "$PROJECT_ROOT/.clang-tidy" ]; then
    echo "Using clang-tidy configuration from .clang-tidy"
else
    echo "Warning: .clang-tidy configuration file not found"
    echo "Using default clang-tidy checks"
fi

echo "Running clang-tidy on $(echo "$FILES" | wc -w) files..."
echo "Output will be written to: $OUTPUT_FILE"
echo ""

# Run clang-tidy
if [ "$QUIET_MODE" = true ]; then
    clang-tidy $CLANG_TIDY_OPTIONS "$FILES" > "$OUTPUT_FILE" 2>&1
else
    clang-tidy $CLANG_TIDY_OPTIONS "$FILES" 2>&1 | tee "$OUTPUT_FILE"
fi

# Check for errors
ERROR_COUNT=$(grep -c "error:" "$OUTPUT_FILE" 2>/dev/null || true)
WARNING_COUNT=$(grep -c "warning:" "$OUTPUT_FILE" 2>/dev/null || true)

echo ""
echo "Analysis complete. Results written to: $OUTPUT_FILE"
echo "Errors: $ERROR_COUNT"
echo "Warnings: $WARNING_COUNT"

if [ "$ERROR_COUNT" -gt 0 ]; then
    echo ""
    echo "Note: Some errors may be false positives or require manual review."
    echo "Check the clang-tidy configuration in .clang-tidy for customization."
fi

exit 0