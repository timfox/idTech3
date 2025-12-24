# Configurable Application Naming Tutorial

This tutorial explains how to use the idTech3 engine's configurable naming system to create branded applications for different games and mods.

## 🎯 Overview

The idTech3 engine supports configurable application naming, allowing you to:

- Remove hardcoded Quake3/Quake3e references
- Brand the engine for specific games or mods
- Create proper package names for app stores
- Maintain separate data directories for different applications

## 📋 Prerequisites

- idTech3 engine source code
- CMake (3.20+)
- Platform-specific build tools (Visual Studio, Xcode, Android NDK, etc.)

## 🚀 Quick Start

### Basic Usage

```bash
# Build with default idTech3 branding
cmake -S . -B build
cmake --build build

# Build with custom mod name
cmake -S . -B build -DMOD_NAME="MyGame"
cmake --build build
```

### Android Builds

```bash
# Use the automated Android build script
./build_android.sh "MyAwesomeGame" Release

# Or configure manually
cmake -S . -B build \
    -DMOD_NAME="MyAwesomeGame" \
    -DUSE_ANDROID=ON \
    -DUSE_VULKAN=ON
cmake --build build
```

## 🔧 Configuration Options

### CMake Variables

| Variable | Description | Default | Example |
|----------|-------------|---------|---------|
| `MOD_NAME` | Game/mod display name | (none) | `"SpaceRunner"` |
| `ENGINE_NAME` | Engine display name | `"idTech3"` | `"MyEngine"` |
| `APP_DISPLAY_NAME` | Application display name | `ENGINE_NAME` | `"Space Runner"` |
| `APP_PACKAGE_NAME` | Android package name | `"com.idtech3.engine"` | `"com.spacestudio.runner"` |
| `CNAME` | Executable/library name | `"idtech3"` | `"spacerunner"` |
| `DNAME` | Dedicated server name | `"idtech3.server"` | `"spacerunner.server"` |

### Build Script Parameters

The `build_android.sh` script accepts:

```bash
./build_android.sh [mod_name] [build_type]
```

- `mod_name`: Display name for your game/mod (default: "idTech3")
- `build_type`: CMake build type - Debug/Release (default: "Release")

## 📱 Platform-Specific Examples

### Desktop (Windows/Linux/macOS)

#### Example 1: Custom Game Branding

```bash
# Configure for a space-themed game
cmake -S . -B build \
    -DMOD_NAME="SpaceRunner" \
    -DAPP_DISPLAY_NAME="Space Runner" \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Result: Executable named "spacerunner.exe" (Windows) or "spacerunner" (Linux/macOS)
```

#### Example 2: Tournament Mod

```bash
# Configure for competitive gameplay
cmake -S . -B build \
    -DMOD_NAME="ArenaMasters" \
    -DAPP_DISPLAY_NAME="Arena Masters Tournament" \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

### Android Mobile

#### Using Build Script (Recommended)

```bash
# Simple build
./build_android.sh "RacingChamp" Release

# This automatically:
# - Sets display name to "RacingChamp"
# - Creates package name "com.racingchamp.game"
# - Updates Android manifest
# - Renames Java activity class
# - Configures build settings
```

#### Manual CMake Configuration

```bash
# Advanced configuration
cmake -S . -B build \
    -DMOD_NAME="RacingChamp" \
    -DAPP_DISPLAY_NAME="Racing Championship" \
    -DAPP_PACKAGE_NAME="com.racingstudio.championship" \
    -DUSE_ANDROID=ON \
    -DUSE_VULKAN=ON \
    -DCMAKE_BUILD_TYPE=Release

# Build APK
cmake --build build --config Release
```

#### iOS/tvOS

```bash
# Configure for iOS
cmake -S . -B build \
    -DMOD_NAME="PuzzleQuest" \
    -DAPP_DISPLAY_NAME="Puzzle Quest Adventures" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DUSE_METAL=ON

# Build Xcode project
cmake --build build --config Release
```

## 📁 File Structure Changes

When you build with a custom mod name, the following files are automatically updated:

### Android Files
```
android/
├── AndroidManifest.xml          # Package name updated
├── IdTech3Activity.java         # Renamed to [ModName]Activity.java
└── ...
```

### Build Outputs
```
build/
├── idtech3                      # Default executable
├── spacerunner                  # Custom named executable
├── spacerunner.server           # Custom named server
└── ...
```

### Data Directories (Runtime)
```
# Android
/sdcard/Android/data/com.spacerunner.game/files/
/sdcard/Android/data/com.spacerunner.game/cache/

# Desktop (varies by platform)
~/spacerunner/                    # User data directory
spacerunner/                      # Working directory
```

## 🛠️ Advanced Configuration

### Custom Package Names

```cmake
# Advanced Android configuration
set(MOD_NAME "MyGame" CACHE STRING "Game name")
set(APP_PACKAGE_NAME "com.mystudio.mygame" CACHE STRING "Android package")
set(APP_DISPLAY_NAME "My Awesome Game" CACHE STRING "Display name")

# Generate version codes
set(APP_VERSION_CODE 100 CACHE STRING "Android version code")
set(APP_VERSION_NAME "1.0.0" CACHE STRING "Android version name")
```

### Multi-Platform Branding

```bash
#!/bin/bash
# build_all_platforms.sh

MOD_NAME="EpicQuest"
BUILD_TYPE="Release"

echo "Building $MOD_NAME for all platforms..."

# Android
./build_android.sh "$MOD_NAME" "$BUILD_TYPE"

# Windows
cmake -S . -B build_windows \
    -DMOD_NAME="$MOD_NAME" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -G "Visual Studio 17 2022"
cmake --build build_windows --config "$BUILD_TYPE"

# Linux
cmake -S . -B build_linux \
    -DMOD_NAME="$MOD_NAME" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build_linux

# macOS
cmake -S . -B build_macos \
    -DMOD_NAME="$MOD_NAME" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build_macos
```

### CI/CD Integration

```yaml
# .github/workflows/build.yml
name: Build Game
on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        mod: ["BaseGame", "Tournament", "CustomMod"]
        platform: ["windows", "linux", "android"]

    steps:
    - uses: actions/checkout@v3

    - name: Configure CMake
      run: |
        cmake -S . -B build \
          -DMOD_NAME="${{ matrix.mod }}" \
          -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --config Release

    - name: Package
      run: |
        # Create platform-specific packages
        # with proper naming
```

## 🔍 Troubleshooting

### Common Issues

#### 1. "MOD_NAME variable not recognized"

**Problem**: CMake doesn't recognize the MOD_NAME variable.

**Solution**: Make sure you're using CMake 3.20+ and the variable is set before the project() command or use -DMOD_NAME="Value" on the command line.

#### 2. Android package name conflicts

**Problem**: Package name already exists on device.

**Solution**: Use a unique package name:

```bash
./build_android.sh "MyGame" Release
# Creates: com.mygame.game

# Or specify custom package:
cmake -DAPP_PACKAGE_NAME="com.mystudio.mygame" ...
```

#### 3. Java class not found

**Problem**: Android build fails with missing Java class.

**Solution**: The build script automatically renames Java classes. If building manually, ensure the AndroidManifest.xml references the correct class name.

#### 4. Data directory conflicts

**Problem**: Multiple games using same data directory.

**Solution**: Each mod gets its own package name and data directory automatically. No manual intervention needed.

### Debug Information

```bash
# Check current configuration
cmake -S . -B build -LA | grep -E "(MOD_NAME|APP_|ENGINE_NAME)"

# Verify Android package
grep -n "package=" android/AndroidManifest.xml

# Check executable names
ls -la build/ | grep -E "(idtech3|${MOD_NAME_LOWER})"
```

### Reset to Defaults

```bash
# Clear CMake cache
rm -rf build/CMakeCache.txt build/CMakeFiles/

# Rebuild with defaults
cmake -S . -B build
```

## 📋 Best Practices

### Naming Conventions

1. **Use PascalCase for display names**: `"SpaceRunner"` not `"space runner"`
2. **Use lowercase for package names**: `"com.studio.game"` not `"com.Studio.Game"`
3. **Keep names descriptive**: `"ZombieSurvival"` not `"ZS"`
4. **Avoid special characters**: Use only letters, numbers, and underscores

### Package Name Guidelines

- Start with reverse domain: `com.company.game`
- Use unique identifiers: `com.studio.gamename`
- Keep consistent across platforms
- Follow app store guidelines

### Build Organization

```bash
# Recommended directory structure
project/
├── build_default/     # Default idTech3 build
├── build_my_game/     # Custom game build
├── build_android/     # Android-specific build
└── build_server/     # Dedicated server build
```

### Version Management

```cmake
# Version configuration
set(GAME_VERSION_MAJOR 1)
set(GAME_VERSION_MINOR 0)
set(GAME_VERSION_PATCH 0)

set(APP_VERSION_NAME "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}.${GAME_VERSION_PATCH}")
set(APP_VERSION_CODE ${GAME_VERSION_MAJOR}${GAME_VERSION_MINOR}${GAME_VERSION_PATCH})
```

## 🎮 Example Game Configurations

### First-Person Shooter

```bash
./build_android.sh "BattleZone" Release
# Package: com.battlezone.game
# Display: BattleZone
```

### Racing Game

```bash
./build_android.sh "SpeedRacer" Release
# Package: com.speedracer.game
# Display: SpeedRacer
```

### Puzzle Game

```bash
./build_android.sh "MindBender" Release
# Package: com.mindbender.game
# Display: MindBender
```

### Strategy Game

```bash
./build_android.sh "EmpireBuilder" Release
# Package: com.empirebuilder.game
# Display: EmpireBuilder
```

## 📚 Related Documentation

- [BUILD.md](BUILD.md) - General build instructions
- [Android Build Guide](android/README.md) - Android-specific setup
- [CMake Configuration](cmake/README.md) - Advanced CMake usage
- [Modding Guide](modding/README.md) - Creating custom content

## 🤝 Contributing

When adding new configuration options:

1. Update this document with new examples
2. Test on all supported platforms
3. Verify Android package naming works correctly
4. Update CI/CD pipelines if needed

## 📞 Support

For issues with configurable naming:

1. Check the troubleshooting section above
2. Verify CMake version (3.20+ required)
3. Test with default configuration first
4. Check platform-specific documentation

---

**The configurable naming system makes idTech3 a true multi-purpose engine capable of powering any game or mod with proper branding and professional presentation!** 🎮✨
