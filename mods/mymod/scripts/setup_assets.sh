#!/bin/bash

# Enhanced Engine Asset Setup Script
# Downloads minimal OpenArena assets for testing

echo "=== Enhanced idTech3 Engine Asset Setup ==="
echo "Setting up minimal assets for testing..."

# Create base directory
mkdir -p /home/tim/Desktop/idtech3/baseq3
cd /home/tim/Desktop/idtech3/baseq3

echo "Downloading OpenArena assets..."
echo "(This provides free replacement assets for Quake 3)"

# Try to download OpenArena assets
# Note: You'll need to manually download from https://openarena.ws/download.php
# and extract the assets to baseq3/

echo ""
echo "INSTRUCTIONS:"
echo "1. Visit: https://openarena.ws/download.php"
echo "2. Download: OpenArena 0.8.8 assets"
echo "3. Extract to: /home/tim/Desktop/idtech3/baseq3/"
echo "4. Run: ./idtech3.x86_64 +set fs_game mymod"
echo ""
echo "This will give you:"
echo "✓ Working fonts and UI"
echo "✓ Basic textures and models"
echo "✓ Test maps and gameplay"
echo ""
echo "Then test enhanced features:"
echo "lua_exec require('examples/engine_demo'); demo()"
echo ""

# Create basic directory structure
mkdir -p models players textures sound

echo "Base directory structure created."
echo "Add OpenArena assets to enable full testing."
