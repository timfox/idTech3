#!/bin/bash
# Asset Cooking Pipeline Script
# Automates the cooking of game assets for optimal runtime performance

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
BUILD_DIR="${PROJECT_ROOT}/build"
TOOLS_DIR="${BUILD_DIR}/tools"
ASSET_COOKER="${TOOLS_DIR}/asset_cooker"
MOD_DIR="${PROJECT_ROOT}/mymod"
COOKED_DIR="${MOD_DIR}/cooked"

# Default options
QUALITY="${ASSET_COOK_QUALITY:-high}"
PLATFORM="${ASSET_COOK_PLATFORM:-desktop}"
GENERATE_MIPS="${ASSET_COOK_MIPS:-true}"

echo "=== Asset Cooking Pipeline ==="
echo "Quality: $QUALITY"
echo "Platform: $PLATFORM"
echo "Generate Mips: $GENERATE_MIPS"
echo "Mod directory: $MOD_DIR"
echo "Output directory: $COOKED_DIR"
echo

# Check if asset cooker exists
if [[ ! -x "$ASSET_COOKER" ]]; then
    echo "ERROR: Asset cooker not found at $ASSET_COOKER"
    echo "Please build the project first with asset cooking tools enabled"
    exit 1
fi

# Check if mod directory exists
if [[ ! -d "$MOD_DIR" ]]; then
    echo "WARNING: Mod directory $MOD_DIR does not exist"
    echo "Creating sample mod structure..."

    mkdir -p "$MOD_DIR/textures"
    mkdir -p "$MOD_DIR/models"
    mkdir -p "$MOD_DIR/sound"

    # Create a simple sample texture (placeholder)
    cat > "$MOD_DIR/textures/sample.png.placeholder" << 'EOF'
This is a placeholder for a PNG texture file.
In a real scenario, this would be an actual PNG/JPG/TGA image file.
The asset cooker would compress it to KTX2 or BasisU format.
EOF

    echo "Sample mod structure created"
fi

# Create cooked assets directory
mkdir -p "$COOKED_DIR"

# Build common arguments
COOK_ARGS="--quality $QUALITY --platform $PLATFORM"
if [[ "$GENERATE_MIPS" == "true" ]]; then
    COOK_ARGS="$COOK_ARGS --generate-mips"
fi

# Cook textures
echo "=== Cooking Textures ==="
TEXTURE_DIR="$MOD_DIR/textures"
if [[ -d "$TEXTURE_DIR" ]]; then
    find "$TEXTURE_DIR" -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.tga" -o -iname "*.dds" \) | while read -r texture_file; do
        # Skip if already cooked
        relative_path="${texture_file#$MOD_DIR/}"
        cooked_file="$COOKED_DIR/${relative_path%.*}"

        # Determine output format based on platform and quality
        if [[ "$PLATFORM" == "web" ]] || [[ "$QUALITY" == "potato" ]] || [[ "$QUALITY" == "low" ]]; then
            output_format="basisu"
            cooked_file="$cooked_file.basis"
        else
            output_format="ktx2"
            cooked_file="$cooked_file.ktx2"
        fi

        if [[ ! -f "$cooked_file" ]] || [[ "$texture_file" -nt "$cooked_file" ]]; then
            echo "Cooking: $texture_file -> $cooked_file"
            mkdir -p "$(dirname "$cooked_file")"

            if "$ASSET_COOKER" $COOK_ARGS --format "$output_format" "$texture_file" "$cooked_file"; then
                echo "✓ Successfully cooked $texture_file"
            else
                echo "✗ Failed to cook $texture_file"
                # Continue with other files
            fi
        else
            echo "Skipping (up to date): $texture_file"
        fi
    done
else
    echo "No textures directory found, skipping texture cooking"
fi

# Cook models (placeholder for future implementation)
echo "=== Cooking Models ==="
MODEL_DIR="$MOD_DIR/models"
if [[ -d "$MODEL_DIR" ]]; then
    echo "Model cooking not yet implemented"
    # Future: Cook .obj, .fbx, .dae files to optimized formats
fi

# Cook sounds (placeholder for future implementation)
echo "=== Cooking Sounds ==="
SOUND_DIR="$MOD_DIR/sound"
if [[ -d "$SOUND_DIR" ]]; then
    echo "Sound cooking not yet implemented"
    # Future: Convert audio formats, apply compression, etc.
fi

# Generate asset manifest
echo "=== Generating Asset Manifest ==="
MANIFEST_FILE="$COOKED_DIR/manifest.json"
cat > "$MANIFEST_FILE" << EOF
{
    "version": "1.0",
    "cooking_pipeline": {
        "quality": "$QUALITY",
        "platform": "$PLATFORM",
        "generate_mips": $GENERATE_MIPS,
        "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
        "tool_version": "idtech3-asset-cooker-v1.0"
    },
    "assets": []
}
EOF

# Add cooked assets to manifest
find "$COOKED_DIR" -type f \( -iname "*.ktx2" -o -iname "*.basis" \) | while read -r asset_file; do
    relative_path="${asset_file#$COOKED_DIR/}"
    original_file="${MOD_DIR}/${relative_path%.*}.png"  # Assuming PNG source

    # Get file sizes for compression ratio
    if [[ -f "$asset_file" ]] && [[ -f "$original_file" ]]; then
        cooked_size=$(stat -c%s "$asset_file" 2>/dev/null || stat -f%z "$asset_file" 2>/dev/null || echo "0")
        original_size=$(stat -c%s "$original_file" 2>/dev/null || stat -f%z "$original_file" 2>/dev/null || echo "0")

        if [[ "$original_size" -gt 0 ]]; then
            compression_ratio=$(( cooked_size * 100 / original_size ))
        else
            compression_ratio=0
        fi

        # Add to manifest
        jq --arg path "$relative_path" \
           --arg original "$original_file" \
           --arg cooked "$asset_file" \
           --arg ratio "$compression_ratio" \
           '.assets += [{"path": $path, "original": $original, "cooked": $cooked, "compression_ratio_percent": $ratio}]' \
           "$MANIFEST_FILE" > "${MANIFEST_FILE}.tmp" && mv "${MANIFEST_FILE}.tmp" "$MANIFEST_FILE"
    fi
done

echo "=== Asset Cooking Complete ==="
echo "Cooked assets saved to: $COOKED_DIR"
echo "Asset manifest: $MANIFEST_FILE"

# Print summary
total_assets=$(find "$COOKED_DIR" -type f \( -iname "*.ktx2" -o -iname "*.basis" \) | wc -l)
echo "Total cooked assets: $total_assets"

if [[ $total_assets -gt 0 ]]; then
    echo "Asset cooking completed successfully!"
    exit 0
else
    echo "WARNING: No assets were cooked. Check that your mod directory contains supported asset files."
    exit 1
fi