#!/usr/bin/env bash
set -euo pipefail

# Package game content into pk3 files
# Usage: ./package_content.sh [source_dir] [output.pk3]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SOURCE_DIR="${1:-$PROJECT_ROOT/release/base}"
OUTPUT_PAK="${2:-$PROJECT_ROOT/release/base/demo.pk3}"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory not found: $SOURCE_DIR"
    exit 1
fi

echo "Packaging content from: $SOURCE_DIR"
echo "Output pak file: $OUTPUT_PAK"

# Create output directory if needed
OUTPUT_DIR="$(dirname "$OUTPUT_PAK")"
mkdir -p "$OUTPUT_DIR"

# Remove existing pak file if it exists
if [ -f "$OUTPUT_PAK" ]; then
    rm -f "$OUTPUT_PAK"
fi

# Create pk3 file (zip format)
cd "$SOURCE_DIR"
zip -r "$OUTPUT_PAK" . -x "*.pk3" "*.log" "*.tmp" "*.bak" "*~" "*.swp"

if [ -f "$OUTPUT_PAK" ]; then
    SIZE=$(du -h "$OUTPUT_PAK" | cut -f1)
    echo "Successfully created: $OUTPUT_PAK ($SIZE)"
else
    echo "Error: Failed to create pak file"
    exit 1
fi
