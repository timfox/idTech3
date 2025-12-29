#!/usr/bin/env bash
set -euo pipefail

# Validate asset formats (BSP, TGA, WAV, etc.)
# Usage: ./validate_assets.sh [directory]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DIR="${1:-$PROJECT_ROOT/release/base}"

if [ ! -d "$DIR" ]; then
    echo "Error: Directory not found: $DIR"
    exit 1
fi

echo "Validating assets in: $DIR"
echo ""

ERRORS=0

# Check for BSP files
echo "Checking BSP files..."
BSP_COUNT=$(find "$DIR" -name "*.bsp" -type f 2>/dev/null | wc -l)
if [ "$BSP_COUNT" -gt 0 ]; then
    echo "  Found $BSP_COUNT BSP file(s)"
    # Basic validation: check file size (BSP files should be > 1KB)
    find "$DIR" -name "*.bsp" -type f -size -1k 2>/dev/null | while read -r file; do
        echo "  WARNING: BSP file appears too small: $file"
        ERRORS=$((ERRORS + 1))
    done
else
    echo "  No BSP files found (this is OK for content-only setups)"
fi

# Check for TGA files
echo "Checking TGA files..."
TGA_COUNT=$(find "$DIR" -name "*.tga" -type f 2>/dev/null | wc -l)
if [ "$TGA_COUNT" -gt 0 ]; then
    echo "  Found $TGA_COUNT TGA file(s)"
else
    echo "  No TGA files found"
fi

# Check for WAV files
echo "Checking WAV files..."
WAV_COUNT=$(find "$DIR" -name "*.wav" -type f 2>/dev/null | wc -l)
if [ "$WAV_COUNT" -gt 0 ]; then
    echo "  Found $WAV_COUNT WAV file(s)"
else
    echo "  No WAV files found"
fi

# Check for shader files
echo "Checking shader files..."
SHADER_COUNT=$(find "$DIR" -name "*.shader" -type f 2>/dev/null | wc -l)
if [ "$SHADER_COUNT" -gt 0 ]; then
    echo "  Found $SHADER_COUNT shader file(s)"
else
    echo "  No shader files found"
fi

# Check for pk3 files
echo "Checking PK3 files..."
PK3_COUNT=$(find "$DIR" -name "*.pk3" -type f 2>/dev/null | wc -l)
if [ "$PK3_COUNT" -gt 0 ]; then
    echo "  Found $PK3_COUNT PK3 file(s)"
    # Validate pk3 files are valid zip files
    find "$DIR" -name "*.pk3" -type f 2>/dev/null | while read -r file; do
        if ! unzip -t "$file" >/dev/null 2>&1; then
            echo "  ERROR: Invalid PK3 file (not a valid zip): $file"
            ERRORS=$((ERRORS + 1))
        fi
    done
else
    echo "  No PK3 files found"
fi

echo ""
if [ "$ERRORS" -eq 0 ]; then
    echo "Asset validation: OK"
    exit 0
else
    echo "Asset validation: Found $ERRORS error(s)"
    exit 1
fi
