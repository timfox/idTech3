#!/bin/bash

# Demo Content Creation Tool
# Creates minimal demo content for testing the idTech3 engine

set -e

echo "Creating minimal demo content for idTech3..."

# Create demo directory structure
DEMO_DIR="demo_content"
mkdir -p "$DEMO_DIR"

# Create a simple map file
cat > "$DEMO_DIR/demo.map" << 'EOMAP'
// Minimal demo map for testing
{
"classname" "worldspawn"
"message" "idTech3 Demo Map"
"music" "music/demo"

// Simple room
{
"classname" "func_static"
"model" "*1"
{
brushDef
{
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
( 0 0 0 0 ) ( ( 0.015625 0 0 ) ( 0 0.015625 0 ) ) "textures/demo/floor" 0 0 0
}
}
}

// Player start
{
"classname" "info_player_start"
"origin" "0 0 32"
"angle" "0"
}
}
EOMAP

# Create basic shader files
mkdir -p "$DEMO_DIR/scripts"

cat > "$DEMO_DIR/scripts/demo.shader" << 'EOSHD'
textures/demo/floor
{
    qer_editorimage textures/demo/floor.tga
    {
        map $lightmap
        rgbGen identity
    }
    {
        map textures/demo/floor.tga
        blendFunc filter
        rgbGen identity
    }
}

textures/demo/wall
{
    qer_editorimage textures/demo/wall.tga
    {
        map $lightmap
        rgbGen identity
    }
    {
        map textures/demo/wall.tga
        blendFunc filter
        rgbGen identity
    }
}
EOSHD

# Create simple textures (basic colored squares)
mkdir -p "$DEMO_DIR/textures/demo"

# Create floor texture (checkerboard pattern)
convert -size 64x64 xc: -fill '#404040' -draw 'rectangle 0,0 31,31' -fill '#606060' -draw 'rectangle 32,0 63,31' -fill '#606060' -draw 'rectangle 0,32 31,63' -fill '#404040' -draw 'rectangle 32,32 63,63' "$DEMO_DIR/textures/demo/floor.tga" 2>/dev/null || {
    echo "Warning: ImageMagick not available, creating placeholder texture files"
    echo "Please install ImageMagick and re-run this script to generate proper textures"
    # Create placeholder files
    echo "placeholder" > "$DEMO_DIR/textures/demo/floor.tga"
    echo "placeholder" > "$DEMO_DIR/textures/demo/wall.tga"
}

# Create wall texture (solid color)
convert -size 64x64 xc:'#808080' "$DEMO_DIR/textures/demo/wall.tga" 2>/dev/null || {
    echo "placeholder" > "$DEMO_DIR/textures/demo/wall.tga"
}

# Create basic game scripts
cat > "$DEMO_DIR/demo.cfg" << 'EOCFG'
// Demo configuration
seta r_mode "-1"
seta r_customwidth "1024"
seta r_customheight "768"
seta r_fullscreen "0"
seta com_maxfps "60"

// Demo settings
seta cg_drawFPS "1"
seta developer "1"
EOCFG

mkdir -p "$DEMO_DIR/maps"
# Create a basic BSP file (placeholder - would need q3map2 to compile)
echo "This is a placeholder BSP file. Use q3map2 to compile demo.map into demo.bsp" > "$DEMO_DIR/maps/demo.bsp"

# Create README
cat > "$DEMO_DIR/README.txt" << 'EOREADME'
idTech3 Demo Content
====================

This is minimal demo content for testing the idTech3 engine.

Files:
- demo.map: Source map file
- maps/demo.bsp: Compiled map (placeholder - needs q3map2)
- scripts/demo.shader: Material definitions
- textures/demo/: Basic textures
- demo.cfg: Demo configuration

To compile the map:
1. Install GtkRadiant or q3map2
2. Run: q3map2 -fs_basepath . -game demo_content demo.map

To run the demo:
1. Place demo_content directory in your idtech3 base path
2. Launch engine with: +set fs_game demo_content +map demo

Note: This is very basic content for testing engine functionality.
For full game content, obtain official Quake 3 assets.
EOREADME

echo "Demo content created in: $DEMO_DIR"
echo "To package as .pk3: zip -r demo.pk3 $DEMO_DIR/*"
echo "Then place demo.pk3 in your base/ directory"
