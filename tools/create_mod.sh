#!/usr/bin/env bash
set -euo pipefail

# Create mod structure template
# Usage: ./create_mod.sh [mod_name]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <mod_name>"
    echo "Example: $0 mymod"
    exit 1
fi

MOD_NAME="$1"
MOD_DIR="$PROJECT_ROOT/mods/$MOD_NAME"

if [ -d "$MOD_DIR" ]; then
    echo "Error: Mod directory already exists: $MOD_DIR"
    exit 1
fi

echo "Creating mod structure: $MOD_NAME"
echo "Directory: $MOD_DIR"

# Create mod directory structure
mkdir -p "$MOD_DIR/scripts"
mkdir -p "$MOD_DIR/maps"
mkdir -p "$MOD_DIR/textures"
mkdir -p "$MOD_DIR/sounds"
mkdir -p "$MOD_DIR/models"

# Create mod info file
cat > "$MOD_DIR/modinfo.txt" << EOF
Mod Name: $MOD_NAME
Description: Custom mod for idTech3
Version: 1.0
Author: Your Name
EOF

# Create basic shader file
cat > "$MOD_DIR/scripts/$MOD_NAME.shader" << 'EOF'
// Shaders for this mod

textures/mymod/test
{
    {
        map $whiteimage
        rgbGen const ( 1 1 1 )
    }
}
EOF

# Create README
cat > "$MOD_DIR/README.md" << EOF
# $MOD_NAME Mod

This is a custom mod for idTech3.

## Structure

- \`scripts/\` - Shader files (.shader)
- \`maps/\` - BSP map files (.bsp)
- \`textures/\` - Texture files (.tga, .jpg)
- \`sounds/\` - Sound files (.wav)
- \`models/\` - Model files (.md3, .md2)

## Usage

1. Place this mod in the \`mods/\` directory
2. Launch the engine with: \`+set fs_game $MOD_NAME\`
3. Or use the launcher: \`./idtech3_launcher +set fs_game $MOD_NAME\`

## Packaging

To create a .pk3 file from this mod:

\`\`\`bash
cd $MOD_DIR
zip -r ../${MOD_NAME}.pk3 .
\`\`\`

Then place the .pk3 file in the \`base/\` directory.
EOF

echo ""
echo "Mod structure created successfully!"
echo ""
echo "Next steps:"
echo "  1. Add your content files to the appropriate directories"
echo "  2. Test the mod: ./idtech3_launcher +set fs_game $MOD_NAME"
echo "  3. Package as .pk3: cd $MOD_DIR && zip -r ../${MOD_NAME}.pk3 ."
echo ""
