#!/bin/bash -xe
# Build script for ModelDoc mod
# Generates a .pk3 file for distribution

# Configuration
MOD_NAME="modeldoc"
MOD_VERSION="1.0.0"
BUILD_DIR="build_modeldoc"
OUTPUT_DIR="release_modeldoc"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== ModelDoc Build Script ===${NC}"
echo -e "${YELLOW}Version: ${MOD_VERSION}${NC}"
echo ""

# Create build directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${OUTPUT_DIR}"

# Copy mod files to build directory
echo -e "${YELLOW}Copying mod files...${NC}"
cp -r "mod/*" "${BUILD_DIR}/"

# Create pk3 structure
echo -e "${YELLOW}Creating pk3 structure...${NC}"
cd "${BUILD_DIR}"

# Create pk3 file
echo -e "${YELLOW}Building pk3 file...${NC}"

# Create a simple zip-based pk3
zip -r "${OUTPUT_DIR}/${MOD_NAME}.pk3" . -x "*.git*" -x "*.pyc" -x "__pycache__/*"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo -e "${GREEN}Output: ${OUTPUT_DIR}/${MOD_NAME}.pk3${NC}"
echo ""
echo "To use ModelDoc:"
echo "  ./idtech3 +set fs_game modeldoc"
echo ""
echo "Or copy the pk3 to your base mod directory:"
echo "  cp ${OUTPUT_DIR}/${MOD_NAME}.pk3 path/to/baseq3/mods/"
echo ""

# Cleanup
cd ".."
rm -rf "${BUILD_DIR}"

echo -e "${GREEN}Build successful!${NC}"