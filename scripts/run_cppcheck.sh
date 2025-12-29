#!/bin/bash

# Cppcheck runner script for id Tech 3
# Usage: ./tools/run_cppcheck.sh [options] [files...]

set -e

# Check if cppcheck is installed
if ! command -v cppcheck &> /dev/null; then
    echo "Error: cppcheck not found. Install with: sudo apt install cppcheck"
    exit 1
fi

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default options
CPPCHECK_OPTIONS="--enable=all --suppress=missingIncludeSystem --inline-suppr --xml"
OUTPUT_FILE="cppcheck_results.xml"
QUIET_MODE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --quiet)
            QUIET_MODE=true
            CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS --quiet"
            shift
            ;;
        --output=*)
            OUTPUT_FILE="${1#*=}"
            shift
            ;;
        --html)
            OUTPUT_FILE="cppcheck_results.html"
            CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS --xml"
            shift
            ;;
        --text)
            OUTPUT_FILE="cppcheck.log"
            CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS"
            shift
            ;;
        --help)
            echo "Usage: $0 [options] [files...]"
            echo ""
            echo "Options:"
            echo "  --quiet        Suppress progress messages"
            echo "  --output=FILE  Write output to FILE"
            echo "  --html         Generate HTML report"
            echo "  --text         Generate plain text report"
            echo "  --help         Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 src/common/*.c              # Analyze specific files"
            echo "  $0 --html                       # Generate HTML report"
            echo "  $0 --quiet --text               # Quiet analysis with text output"
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

# Add include paths
CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS -Isrc/ -Isrc/common/ -Isrc/client/ -Isrc/server/ -Isrc/renderercommon/ -Isrc/renderervk/ -Isrc/renderer/ -Ilibs/"

# Add platform and build defines
CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS -D__linux__ -DUSE_VULKAN -DUSE_OPENGL -DUSE_SDL -DQ3_VM"

# If no files specified, find all C/C++ source files
if [ $# -eq 0 ]; then
    echo "Finding all C/C++ source files..."
    FILES=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/radiant" \
        -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \
        | grep -v "_autogen" \
        | grep -v "moc_" \
        | head -100)  # Limit for performance
else
    FILES="$@"
fi

# Check if cppcheck.cfg config exists
if [ -f "$PROJECT_ROOT/cppcheck.cfg" ]; then
    echo "Using cppcheck configuration from cppcheck.cfg"
    CPPCHECK_OPTIONS="$CPPCHECK_OPTIONS --config-exclude=$PROJECT_ROOT/cppcheck.cfg"
else
    echo "Warning: cppcheck.cfg configuration file not found"
    echo "Using default cppcheck configuration"
fi

echo "Running cppcheck on $(echo "$FILES" | wc -w) files..."
echo "Output will be written to: $OUTPUT_FILE"
echo ""

# Run cppcheck
if [ "$QUIET_MODE" = true ]; then
    cppcheck $CPPCHECK_OPTIONS "$FILES" > "$OUTPUT_FILE" 2>&1
else
    cppcheck $CPPCHECK_OPTIONS "$FILES" 2>&1 | tee "$OUTPUT_FILE"
fi

# Check for errors and warnings
if [[ "$OUTPUT_FILE" == *.xml ]]; then
    # XML output
    ERROR_COUNT=$(grep -c "<error " "$OUTPUT_FILE" 2>/dev/null || true)
    WARNING_COUNT=$(grep -c "<error.*severity=\"warning\"" "$OUTPUT_FILE" 2>/dev/null || true)
elif [[ "$OUTPUT_FILE" == *.html ]]; then
    # HTML output would need different parsing
    echo "HTML report generated: $OUTPUT_FILE"
    exit 0
else
    # Text output
    ERROR_COUNT=$(grep -c "error:" "$OUTPUT_FILE" 2>/dev/null || true)
    WARNING_COUNT=$(grep -c "warning:" "$OUTPUT_FILE" 2>/dev/null || true)
fi

echo ""
echo "Analysis complete. Results written to: $OUTPUT_FILE"
if [ -n "$ERROR_COUNT" ] && [ -n "$WARNING_COUNT" ]; then
    echo "Errors: $ERROR_COUNT"
    echo "Warnings: $WARNING_COUNT"
fi

if [[ "$OUTPUT_FILE" == *.xml ]] && [ "$ERROR_COUNT" -gt 0 ]; then
    echo ""
    echo "To convert XML to HTML: cppcheck-htmlreport --file=$OUTPUT_FILE --report-dir=cppcheck_html"
fi

exit 0