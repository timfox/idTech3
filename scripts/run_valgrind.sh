#!/bin/bash

# Valgrind memory checker script for id Tech 3
# Usage: ./tools/run_valgrind.sh [executable] [args...]

set -e

# Check if valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo "Error: valgrind not found. Install with: sudo apt install valgrind"
    exit 1
fi

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <executable> [args...]"
    echo "Example: $0 ./build/ioquake3.x86_64 +set dedicated 1 +map q3dm1"
    exit 1
fi

EXECUTABLE="$1"
shift
ARGS="$@"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Use suppression file if it exists
SUPPRESSION_FILE="$PROJECT_ROOT/valgrind.supp"
SUPPRESSION_ARGS=""
if [ -f "$SUPPRESSION_FILE" ]; then
    SUPPRESSION_ARGS="--suppressions=$SUPPRESSION_FILE"
fi

# Valgrind options for memory checking
VALGRIND_ARGS=(
    "--tool=memcheck"
    "--leak-check=full"
    "--leak-resolution=high"
    "--show-leak-kinds=definite,possible"
    "--track-origins=yes"
    "--verbose"
    "--log-file=valgrind.log"
    "--num-callers=20"
    "--track-fds=yes"
    $SUPPRESSION_ARGS
)

echo "Starting $EXECUTABLE with Valgrind memory checking..."
echo "Log will be written to: valgrind.log"
echo "Suppression file: $SUPPRESSION_FILE"
echo ""

# Run with valgrind
valgrind "${VALGRIND_ARGS[@]}" "$EXECUTABLE" $ARGS

echo ""
echo "Valgrind analysis complete. Check valgrind.log for results."