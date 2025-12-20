#!/bin/bash

# Enhanced idTech3 Engine Distribution Package Creator
# Creates a complete distribution package for end users

# Configuration
PACKAGE_NAME="Enhanced_idTech3_Engine"
PACKAGE_VERSION="2024.1"
OUTPUT_DIR="./distribution"
SOURCE_DIR="."

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() {
    echo -e "${GREEN}✓${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_step() {
    echo -e "${YELLOW}▶${NC} $1"
}

# Create package structure
create_package_structure() {
    print_step "Creating package structure..."

    rm -rf "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"

    # Main directories
    mkdir -p "$OUTPUT_DIR/bin"
    mkdir -p "$OUTPUT_DIR/lib"
    mkdir -p "$OUTPUT_DIR/share"
    mkdir -p "$OUTPUT_DIR/docs"
    mkdir -p "$OUTPUT_DIR/config"
    mkdir -p "$OUTPUT_DIR/mymod"
    mkdir -p "$OUTPUT_DIR/baseq3"
    mkdir -p "$OUTPUT_DIR/tools"

    print_status "Package structure created"
}

# Copy engine components
copy_engine_components() {
    print_step "Copying engine components..."

    # Copy source code (for reference)
    cp -r src "$OUTPUT_DIR/src"

    # Copy build system
    cp CMakeLists.txt "$OUTPUT_DIR/"
    cp -r cmake "$OUTPUT_DIR/" 2>/dev/null || true

    # Copy tools
    cp -r tools/* "$OUTPUT_DIR/tools/" 2>/dev/null || true

    print_status "Engine components copied"
}

# Copy enhanced mod
copy_enhanced_mod() {
    print_step "Copying enhanced mod..."

    if [ -d "mods/mymod" ]; then
        cp -r mods/mymod/* "$OUTPUT_DIR/mymod/"
        print_status "Enhanced mod copied"
    else
        print_info "Enhanced mod not found - creating basic structure"
        mkdir -p "$OUTPUT_DIR/mymod"
    fi
}

# Create asset placeholders
create_asset_placeholders() {
    print_step "Creating asset placeholders..."

    # Create README for asset requirements
    cat > "$OUTPUT_DIR/ASSETS_README.md" << 'EOF'
# Required Game Assets

## Overview
The Enhanced idTech3 Engine requires base game assets to function properly. These assets provide the core game content including textures, models, sounds, and UI elements.

## Recommended Assets: OpenArena

**OpenArena** provides free, high-quality replacement assets that are fully compatible with the enhanced engine.

### Download Instructions:
1. Visit: https://openarena.ws/download.php
2. Download: "OpenArena 0.8.8 assets" (approximately 400MB)
3. Extract the contents to: `baseq3/` directory in your installation

### What You'll Get:
- ✅ High-quality textures and materials
- ✅ Complete weapon and player models
- ✅ Professional sound effects and music
- ✅ Balanced gameplay content
- ✅ Modern UI elements

## Alternative: Original Quake 3 Arena

If you own the original Quake 3 Arena, you can use those assets:
1. Locate your Quake 3 Arena installation
2. Copy `pak0.pk3` through `pak8.pk3` files
3. Copy `baseq3/` directory contents
4. Place in the engine's `baseq3/` directory

## File Structure After Installation

```
Enhanced_idTech3_Engine/
├── baseq3/
│   ├── pak0.pk3          # Game data (required)
│   ├── pak1.pk3          # Game data (required)
│   ├── pak2.pk3          # Game data (required)
│   ├── pak3.pk3          # Game data (required)
│   ├── pak4.pk3          # Game data (required)
│   ├── pak5.pk3          # Game data (required)
│   ├── pak6.pk3          # Game data (required)
│   ├── pak7.pk3          # Game data (required)
│   ├── pak8.pk3          # Game data (required)
│   ├── font1_prop.tga    # Bitmap fonts
│   └── font2_prop.tga    # Bitmap fonts
├── mymod/                # Enhanced mod content
└── [engine files]
```

## Testing Asset Installation

After installing assets, test with:
```bash
./launch_game.sh
```

In the console (`~` key), you should see:
- Main menu loads properly
- Text renders correctly
- 3D models display
- Sounds play

## Troubleshooting

### Still Black Screen?
- Verify `pak0.pk3` exists in `baseq3/`
- Check console for "Unable to read font file" errors
- Try: `set r_fontQuality 0` in console

### Missing Textures?
- Ensure all `pak*.pk3` files are present
- Check file permissions
- Verify extraction completed successfully

### Performance Issues?
- Use `exec config/performance.cfg`
- Check system requirements
- Monitor with `set com_speeds 1`

## Getting Help

- Check the main `README.md` for detailed instructions
- Visit the documentation in `docs/`
- Use the built-in help: `lua_exec require("hardening_test"); run_all()`

## Asset Sources

- **OpenArena**: https://openarena.ws/ (Free, Recommended)
- **Quake 3 Arena**: Original game purchase
- **Community Maps**: Various online sources for additional content

---

**Note**: The engine will run without assets but will display placeholder content and missing textures. For the full enhanced experience, install complete game assets.
EOF

    # Create basic directory structure info
    cat > "$OUTPUT_DIR/baseq3/README.md" << 'EOF'
# Base Game Assets Directory

This directory should contain the base game assets for Quake 3 Arena or OpenArena.

## Required Files:
- pak0.pk3 through pak8.pk3 (game data archives)
- font1_prop.tga and font2_prop.tga (bitmap fonts)

## Installation:
1. Download OpenArena assets from https://openarena.ws/download.php
2. Extract all files directly into this directory
3. The engine will automatically detect and use the assets

## Alternative:
Copy the baseq3 directory from your Quake 3 Arena installation.
EOF

    print_status "Asset placeholders and documentation created"
}

# Create documentation package
create_documentation_package() {
    print_step "Creating documentation package..."

    # Copy existing documentation
    cp README.md "$OUTPUT_DIR/docs/" 2>/dev/null || true
    cp LICENSE.md "$OUTPUT_DIR/docs/" 2>/dev/null || true
    cp mods/mymod/README_TESTING.md "$OUTPUT_DIR/docs/TESTING_GUIDE.md" 2>/dev/null || true
    cp mods/mymod/HARDENING_README.md "$OUTPUT_DIR/docs/" 2>/dev/null || true
    cp mods/mymod/BANNER_ASSETS_README.md "$OUTPUT_DIR/docs/" 2>/dev/null || true

    # Create comprehensive user manual
    cat > "$OUTPUT_DIR/docs/USER_MANUAL.md" << 'EOF'
# Enhanced idTech3 Engine - User Manual

## Welcome to Modern Gaming

You've installed the **Enhanced idTech3 Engine** - a completely modernized version of the classic Quake 3 Arena engine. This manual will help you get the most out of your enhanced gaming experience.

## 🚀 Quick Start Guide

### First Launch
1. Run: `./launch_game.sh`
2. If you see a black screen, you need game assets (see ASSETS_README.md)
3. Press `~` to open the console
4. Type: `lua_exec require("examples/cinematic"); cinematic.showcase_all()`

### Basic Controls
- **WASD**: Movement
- **Mouse**: Look around
- **Left Click**: Fire weapon
- **Space**: Jump
- **~**: Open console
- **Tab**: Scoreboard (multiplayer)
- **F1-F2**: Vote yes/no (multiplayer)

## 🎮 Enhanced Features Overview

### Visual Enhancements
- **PBR Rendering**: Realistic materials and lighting
- **SSAO**: Better depth perception and realism
- **Bloom Effects**: Cinematic lighting and glow
- **Anti-Aliasing**: Smooth edges and reduced jaggies
- **TrueType Fonts**: Modern, scalable text rendering

### Gameplay Improvements
- **Advanced Weapons**: Plasma Rifle, Smart Rockets, Nano Blades
- **Dynamic Power-ups**: Speed, Shield, Quad Damage with visual effects
- **Environmental Hazards**: Toxic zones, radiation, lava pits
- **Weather Systems**: Rain, fog, storms affecting gameplay

### Stability & Performance
- **Crash Recovery**: Automatic restart on errors
- **Memory Safety**: Protection against corruption
- **Performance Monitoring**: Real-time FPS and stats
- **Error Logging**: Detailed diagnostics

## ⚙️ Configuration Guide

### Configuration Files
- `config/default.cfg`: Balanced settings (recommended)
- `config/performance.cfg`: Maximum FPS
- `config/compatibility.cfg`: Older hardware support

### Video Settings
```bash
// High quality (default)
set r_mode "6"           // 800x600
set r_fullscreen "1"     // Fullscreen
set r_colorbits "32"     // 32-bit color

// Performance mode
exec config/performance.cfg

// Compatibility mode
exec config/compatibility.cfg
```

### Audio Settings
```bash
set s_volume "0.8"       // Master volume
set s_musicvolume "0.4"  // Music volume
set s_khz "44"          // High quality audio
```

## 🎯 Advanced Features

### Lua Scripting
Execute custom scripts in the console:
```bash
// Showcase all features
lua_exec require("examples/cinematic"); cinematic.showcase_all()

// Test weapon systems
lua_exec require("examples/weapons"); weapons.test_all()

// Test power-ups
lua_exec require("examples/powerups"); powerups.test_all()

// Run hardening tests
lua_exec require("hardening_test"); run_all()
```

### Performance Monitoring
```bash
// Enable monitoring
set perf_monitor_enable 1
set com_speeds 1
set r_speeds 1

// View current stats
set cg_drawFPS 1
set cg_lagometer 1
```

### Multiplayer Commands
```bash
// Connect to server
/connect server.ip:27960

// Start local server
lua_exec require("examples/multiplayer_enhancements"); server.generate_report()

// Client commands
lua_exec require("examples/client_multiplayer"); client.enhanced_browser()
```

## 🛠️ Troubleshooting

### Common Issues

#### Black Screen
**Cause**: Missing game assets
**Solution**:
1. Download OpenArena assets
2. Extract to `baseq3/` directory
3. Restart game

#### Poor Performance
**Cause**: High graphics settings
**Solution**:
```
exec config/performance.cfg
vid_restart
```

#### Crashes on Startup
**Cause**: Driver or hardware compatibility
**Solution**:
```
exec config/compatibility.cfg
vid_restart
```

#### No Sound
**Cause**: Audio driver issues
**Solution**:
```
set s_initsound 0
vid_restart
set s_initsound 1
s_restart
```

### Console Commands

#### System Information
```bash
version         // Engine version
sysinfo         // System information
meminfo         // Memory usage
rendererinfo    // Graphics card info
```

#### Debugging
```bash
developer 1     // Enable developer mode
logfile 1       // Enable logging
condump         // Dump console to file
```

#### Network Testing
```bash
net_restart     // Restart network
ping            // Test connection
```

## 🎮 Multiplayer Guide

### Joining Servers
```bash
// Use enhanced server browser
lua_exec require("examples/client_multiplayer"); client.enhanced_browser()

// Direct connect
/connect server.address:27960
```

### Server Administration
```bash
// Start dedicated server
./launch_server.sh

// Server commands (in server console)
lua_exec require("examples/multiplayer_enhancements"); server.generate_report()
status          // Show connected players
kick player     // Remove player
map q3dm1       // Change map
```

### Server Configuration
```bash
// Load server config
exec mymod/config/server_dedicated.cfg

// Customize settings
set sv_hostname "My Enhanced Server"
set sv_maxclients 16
set timelimit 15
```

## 🔧 Advanced Configuration

### Custom Settings
Create your own config file:
```bash
// Create custom.cfg
set r_mode "8"          // 1024x768
set sensitivity "7"     // Higher mouse sensitivity
set cl_maxpackets "125" // Higher packet rate

// Load it
exec custom.cfg
```

### Key Bindings
```bash
bind w "+forward"
bind s "+back"
bind a "+moveleft"
bind d "+moveright"
bind mouse1 "+attack"
bind mouse2 "+zoom"     // New zoom feature
bind f "+use"           // Use items
```

### Advanced Graphics
```bash
// Enable all enhancements
set r_pbr "1"
set r_ssao "1"
set r_bloom "1"
set r_temporal_aa "1"

// Texture quality
set r_picmip "0"        // Maximum quality
set r_texturebits "32"  // 32-bit textures
```

## 📊 Performance Optimization

### For High-End Systems
```bash
set r_mode "9"          // 1152x864 or higher
set r_ext_multisample 8 // 8x MSAA
set r_texture_anisotropy "16"
set com_maxfps "125"    // Unlock FPS
```

### For Low-End Systems
```bash
set r_mode "4"          // 640x480
set r_picmip "3"        // Lowest texture quality
set r_texturebits "16"  // 16-bit textures
set r_ext_multisample 0 // No MSAA
```

### Battery Life (Laptops)
```bash
set com_maxfps "60"     // Limit FPS
set r_finish "0"        // Disable vsync
set r_swapInterval "0"  // No sync
```

## 🎯 Feature Showcase

### Complete Feature Demo
```bash
lua_exec require("examples/cinematic"); cinematic.showcase_all()
```
This demonstrates:
- PBR material rendering
- Advanced weapon effects
- Dynamic power-ups
- Environmental systems
- Lua scripting capabilities

### Individual Feature Tests
```bash
// Test all systems
lua_exec require("hardening_test"); run_all()

// Weapon systems
lua_exec require("examples/weapons"); weapons.test_all()

// Power-up mechanics
lua_exec require("examples/powerups"); powerups.test_all()

// Environmental effects
lua_exec require("examples/environmental_effects"); environmental.test_all()
```

## 🔒 Security & Stability

### Built-in Protections
- **Memory Corruption Detection**: Prevents buffer overflows
- **Input Sanitization**: Blocks malicious input
- **Rate Limiting**: Prevents spam and DoS attacks
- **Thread Safety**: Race condition prevention

### Monitoring Your System
```bash
// Enable stability monitoring
set stability_enable 1
set perf_monitor_enable 1

// View system health
lua_exec print(Stability_GetStats().total_allocations)
lua_exec print(MemorySafety_GetStats().current_memory)
```

## 📚 Additional Resources

- **Complete Documentation**: `docs/` directory
- **Configuration Examples**: `config/` directory
- **Scripting Examples**: `mymod/scripts/examples/`
- **Hardening Guide**: `docs/HARDENING_README.md`

## 🆘 Getting Help

### In-Game Help
```bash
help            // Basic commands
cvarlist        // All configuration options
cmdlist         // Available commands
lua_help        // Lua scripting help
```

### External Resources
- **Official Documentation**: This manual
- **Console Commands**: Type `help` in-game
- **Community Support**: Check for updates and community resources

---

## 🎉 Enjoy Enhanced Gaming!

The Enhanced idTech3 Engine provides a modern gaming experience with professional stability, cutting-edge graphics, and advanced features. Explore, experiment, and enjoy your enhanced Quake 3 Arena experience!

**Happy Gaming!** 🚀🎮
EOF

    print_status "Comprehensive documentation created"
}

# Create binary placeholders and build instructions
create_build_package() {
    print_step "Creating build package..."

    # Create build instructions
    cat > "$OUTPUT_DIR/BUILD_INSTRUCTIONS.md" << 'EOF'
# Building the Enhanced idTech3 Engine

## Overview
This package includes the complete source code for the Enhanced idTech3 Engine. Follow these instructions to build the engine for your platform.

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libvulkan-dev libfreetype-dev liblua5.3-dev
```

### Windows
- Visual Studio 2019 or later
- Vulkan SDK
- CMake 3.16 or later

### macOS
```bash
brew install cmake sdl2 vulkan-headers freetype lua
```

## Quick Build

### Linux/macOS
```bash
cd Enhanced_idTech3_Engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Windows
```bash
cd Enhanced_idTech3_Engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 16 2019"
cmake --build . --config Release
```

## Build Options

### CMake Configuration
```bash
# Standard build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Custom installation directory
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/enhanced-idtech3

# Enable all features
cmake .. -DUSE_VULKAN=ON -DUSE_LUA=ON -DUSE_FREETYPE=ON

# Minimal build
cmake .. -DUSE_VULKAN=OFF -DUSE_LUA=OFF -DUSE_SYSTEM_CONSOLE=OFF
```

### Build Targets
```bash
# Build everything
make -j$(nproc)

# Build specific targets
make idtech3.x86_64        # Main game executable
make idtech3ded.x86_64     # Dedicated server
make install               # Install to system
```

## Testing the Build

### Run Basic Tests
```bash
# Test executable
./idtech3.x86_64 +set developer 1 +version +quit

# Test with mod
./idtech3.x86_64 +set fs_game mymod +exec config/default.cfg +quit
```

### Run Enhanced Tests
```bash
# Full feature test (requires assets)
./idtech3.x86_64 +set fs_game mymod +lua_exec 'require("hardening_test"); run_all(); quit'
```

## Troubleshooting Build Issues

### Common Problems

#### CMake Not Found
```bash
# Ubuntu/Debian
sudo apt install cmake

# CentOS/RHEL
sudo yum install cmake3
```

#### SDL2 Not Found
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel
```

#### Vulkan Not Found
```bash
# Ubuntu/Debian
sudo apt install libvulkan-dev vulkan-tools

# Download Vulkan SDK for Windows/macOS
```

#### Lua Not Found
```bash
# Ubuntu/Debian
sudo apt install liblua5.3-dev

# Or build without Lua
cmake .. -DUSE_LUA=OFF
```

### Compiler Errors

#### "C++ compiler not found"
```bash
# Install build tools
sudo apt install build-essential  # Ubuntu/Debian
sudo yum groupinstall "Development Tools"  # CentOS/RHEL
```

#### "Header not found"
```bash
# Check include paths
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON

# Install missing development packages
sudo apt install libfreetype-dev liblua5.3-dev
```

#### Linker Errors
```bash
# Check library paths
ldd ./idtech3.x86_64  # Linux

# Install missing runtime libraries
sudo apt install libvulkan1 mesa-vulkan-drivers
```

## Advanced Build Options

### Custom Compiler
```bash
# Use Clang instead of GCC
CC=clang CXX=clang++ cmake ..

# Use specific GCC version
CC=gcc-9 CXX=g++-9 cmake ..
```

### Cross-Compilation
```bash
# 32-bit build on 64-bit system
cmake .. -DCMAKE_C_FLAGS="-m32" -DCMAKE_CXX_FLAGS="-m32"

# ARM build
cmake .. -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm
```

### Optimization Flags
```bash
# Maximum optimization
cmake .. -DCMAKE_C_FLAGS="-O3 -march=native -flto" -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"

# Debug build with optimizations
cmake .. -DCMAKE_C_FLAGS="-O2 -g" -DCMAKE_CXX_FLAGS="-O2 -g"
```

## Platform-Specific Notes

### Linux
- Uses system SDL2 and Vulkan
- Installs to `/usr/local` by default
- Desktop integration available

### Windows
- Includes SDL2.dll in distribution
- Uses Windows Vulkan loader
- Creates Start Menu shortcuts

### macOS
- Uses system Vulkan (MoltenVK)
- Creates .app bundle
- Gatekeeper notarization required

## Distribution

### Creating Release Packages
```bash
# Linux
cpack -G "DEB"
cpack -G "RPM"

# Windows
cpack -G "NSIS"

# macOS
cpack -G "DragNDrop"
```

### Custom Installers
Use the provided `package_installer.sh` script for custom installations.

## Getting Help

- Check `docs/` for detailed documentation
- Review `CMakeLists.txt` for build configuration
- Check GitHub issues for common problems
- Use `cmake --help` for available options

---

**The Enhanced idTech3 Engine is designed for easy building and deployment across all major platforms.**
EOF

    # Create build verification script
    cat > "$OUTPUT_DIR/verify_build.sh" << 'EOF'
#!/bin/bash

# Enhanced idTech3 Engine Build Verification Script

echo "Enhanced idTech3 Engine - Build Verification"
echo "==========================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS="${GREEN}PASS${NC}"
FAIL="${RED}FAIL${NC}"
WARN="${YELLOW}WARN${NC}"

# Check if we're in the right directory
if [ ! -f "idtech3.x86_64" ] && [ ! -f "idtech3.exe" ]; then
    echo -e "${FAIL}: No executable found. Run build first."
    exit 1
fi

echo "Checking build artifacts..."

# Check main executable
if [ -f "idtech3.x86_64" ]; then
    echo -e "${PASS}: Main executable (idtech3.x86_64) found"

    # Check if executable
    if [ -x "idtech3.x86_64" ]; then
        echo -e "${PASS}: Executable has correct permissions"
    else
        echo -e "${FAIL}: Executable missing execute permissions"
    fi

    # Check file size (should be > 1MB)
    size=$(stat -c%s "idtech3.x86_64" 2>/dev/null || stat -f%z "idtech3.x86_64" 2>/dev/null)
    if [ "$size" -gt 1000000 ]; then
        echo -e "${PASS}: Executable size looks reasonable ($size bytes)"
    else
        echo -e "${FAIL}: Executable size too small ($size bytes)"
    fi
elif [ -f "idtech3.exe" ]; then
    echo -e "${PASS}: Main executable (idtech3.exe) found"
fi

# Check dedicated server
if [ -f "idtech3ded.x86_64" ]; then
    echo -e "${PASS}: Dedicated server (idtech3ded.x86_64) found"
elif [ -f "idtech3ded.exe" ]; then
    echo -e "${PASS}: Dedicated server (idtech3ded.exe) found"
else
    echo -e "${WARN}: Dedicated server not found (optional)"
fi

# Check mod directory
if [ -d "mymod" ]; then
    echo -e "${PASS}: Enhanced mod directory found"

    # Check key mod files
    if [ -f "mymod/gamesrc/ui/ui_menu.c" ]; then
        echo -e "${PASS}: UI enhancements found"
    else
        echo -e "${FAIL}: UI enhancements missing"
    fi

    if [ -d "mymod/scripts" ]; then
        echo -e "${PASS}: Lua scripts found"
    else
        echo -e "${FAIL}: Lua scripts missing"
    fi
else
    echo -e "${FAIL}: Enhanced mod directory missing"
fi

# Check configuration files
if [ -f "config/default.cfg" ]; then
    echo -e "${PASS}: Default configuration found"
else
    echo -e "${FAIL}: Default configuration missing"
fi

echo ""
echo "Running basic functionality tests..."

# Test version command (if executable exists)
if [ -f "idtech3.x86_64" ] && [ -x "idtech3.x86_64" ]; then
    echo "Testing version command..."
    timeout 5 ./idtech3.x86_64 +version +quit 2>/dev/null | grep -q "idTech3"
    if [ $? -eq 0 ]; then
        echo -e "${PASS}: Version command works"
    else
        echo -e "${FAIL}: Version command failed"
    fi
fi

echo ""
echo "Build verification complete!"
echo "If all tests passed, your build is ready for use."
echo "Run './launch_game.sh' to start the game."
EOF
    chmod +x "$OUTPUT_DIR/verify_build.sh"

    print_status "Build package and instructions created"
}

# Create final package archive
create_package_archive() {
    print_step "Creating distribution archive..."

    cd "$OUTPUT_DIR/.."
    tar -czf "${PACKAGE_NAME}_${PACKAGE_VERSION}.tar.gz" "$(basename "$OUTPUT_DIR")"

    print_status "Distribution package created: ${PACKAGE_NAME}_${PACKAGE_VERSION}.tar.gz"
    print_info "Package size: $(du -sh "${PACKAGE_NAME}_${PACKAGE_VERSION}.tar.gz" | cut -f1)"
}

# Main packaging function
main() {
    echo "Enhanced idTech3 Engine Distribution Package Creator"
    echo "=================================================="
    echo ""
    echo "Creating complete distribution package..."
    echo ""

    create_package_structure
    copy_engine_components
    copy_enhanced_mod
    create_asset_placeholders
    create_documentation_package
    create_build_package
    create_package_archive

    echo ""
    echo "Distribution package creation complete!"
    echo ""
    echo "Package contents:"
    echo "  📦 ${PACKAGE_NAME}_${PACKAGE_VERSION}.tar.gz"
    echo ""
    echo "Installation instructions:"
    echo "  1. Extract: tar -xzf ${PACKAGE_NAME}_${PACKAGE_VERSION}.tar.gz"
    echo "  2. Run: cd ${PACKAGE_NAME} && ./package_installer.sh"
    echo "  3. Get assets: Follow ASSETS_README.md"
    echo "  4. Launch: ./launch_game.sh"
    echo ""
    echo "Happy gaming! 🚀🎮"
}

# Run main packaging
main "$@"
EOF

<xai:function_call name="run_terminal_cmd">
<parameter name="command">chmod +x /home/tim/Desktop/idtech3/create_distribution_package.sh
