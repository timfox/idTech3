#!/usr/bin/env bash
set -euo pipefail

# Create demo content for testing the engine
# This creates a minimal demo pak with basic shaders and a test map

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEMO_DIR="$PROJECT_ROOT/release/demo"
BASE_DIR="$PROJECT_ROOT/release/base"

echo "Creating demo content..."

# Create directories
mkdir -p "$DEMO_DIR"
mkdir -p "$BASE_DIR"

# Create minimal default.cfg
if [ ! -f "$BASE_DIR/default.cfg" ]; then
    cat > "$BASE_DIR/default.cfg" << 'EOF'
// Default configuration for idTech3 engine
seta r_mode "-1"
seta r_customwidth "1920"
seta r_customheight "1080"
seta r_fullscreen "0"
seta com_maxfps "125"
seta cl_maxpackets "30"
seta cl_packetdup "1"
seta s_khz "44"
seta s_musicvolume "0.5"
seta s_volume "0.5"
EOF
    echo "Created default.cfg"
fi

# Create minimal shader file
mkdir -p "$BASE_DIR/scripts"
if [ ! -f "$BASE_DIR/scripts/common.shader" ]; then
    cat > "$BASE_DIR/scripts/common.shader" << 'EOF'
// Common shaders for demo content

textures/common/caulk
{
    {
        map $whiteimage
        rgbGen const ( 0.5 0.5 0.5 )
    }
}

textures/common/nodraw
{
    {
        map $whiteimage
        rgbGen const ( 0 0 0 )
    }
}

textures/common/trigger
{
    {
        map $whiteimage
        rgbGen const ( 1 1 0 )
        alphaGen const 0.5
    }
}
EOF
    echo "Created common.shader"
fi

# Create a simple test map info file
mkdir -p "$BASE_DIR/maps"
if [ ! -f "$BASE_DIR/maps/test.bsp" ]; then
    echo "Note: test.bsp map file not created (requires BSP compiler)"
    echo "  Place your compiled .bsp map files in: $BASE_DIR/maps/"
    echo "  Or package them in a .pk3 file"
fi

# Create a simple pak file structure (empty for now, user can add content)
echo ""
echo "Demo content structure created in: $DEMO_DIR"
echo "Base content structure created in: $BASE_DIR"
echo ""
echo "To create a working demo:"
echo "  1. Place .bsp map files in $BASE_DIR/maps/"
echo "  2. Place textures in $BASE_DIR/textures/"
echo "  3. Place shaders in $BASE_DIR/scripts/"
echo "  4. Package everything into a .pk3 file using:"
echo "     zip -r $BASE_DIR/demo.pk3 $BASE_DIR/*"
echo ""
echo "Or use the package_content.sh script to create pak files."
