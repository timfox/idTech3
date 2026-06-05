#!/bin/bash

# Compile Game Script for id Tech 3 mods, with robust CMake cache handling
set -euo pipefail

# Usage: ./compile_game.sh [mod_name] [Debug|Release] [clean]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

MOD_NAME=""
BUILD_TYPE="Release"
CLEAN=0
PK3_ENABLED=1

normalize_build_type() {
    local arg=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$arg" in
        debug|dbg|d) echo "Debug" ;;
        release|rel|r) echo "Release" ;;
        *) echo "" ;;
    esac
}

# Argument parsing: first non-flag is mod name
for arg in "$@"; do
    # Handle help flag
    if [ "$arg" = "--help" ] || [ "$arg" = "-h" ]; then
        echo "Usage: $0 [mod_name] [Debug|Release] [clean]"
        echo ""
        echo "Options:"
        echo "  mod_name    Name of the mod to compile (default: mymod)"
        echo "  Debug       Build in Debug mode"
        echo "  Release     Build in Release mode (default)"
        echo "  clean       Clean build directory before building"
        echo ""
        echo "Examples:"
        echo "  $0 openarena              # Build openarena mod in Release mode"
        echo "  $0 mymod Debug            # Build mymod in Debug mode"
        echo "  $0 openarena Release clean # Clean build of openarena"
        exit 0
    fi
    norm_bt="$(normalize_build_type "$arg")"
    if [ -n "$norm_bt" ]; then
        BUILD_TYPE="$norm_bt"
        continue
    fi
    if [ "$arg" = "clean" ]; then
        CLEAN=1
        continue
    fi
    if [ -z "$MOD_NAME" ]; then
        MOD_NAME="$arg"
    fi
done
MOD_NAME=${MOD_NAME:-mymod}

# Users often confuse renderer/backend names with mod names.
_mod_lc="$(echo "$MOD_NAME" | tr '[:upper:]' '[:lower:]')"
if [[ "$_mod_lc" == "vulkan" || "$_mod_lc" == "vk" || "$_mod_lc" == "opengl" || "$_mod_lc" == "gl" || "$_mod_lc" == "gles" ]]; then
	echo "Error: \"$MOD_NAME\" is a renderer/backend, not a game mod under mods/<name>/gamesrc."
	echo ""
	echo "Build the engine (client + renderers):"
	echo "  $SCRIPT_DIR/compile_engine.sh vulkan"
	echo ""
	echo "Package the example demo mod (.pk3 only, no native game DLL):"
	echo "  $PROJECT_ROOT/examples/demo_game/build_demo_pack.sh"
	echo ""
	echo "Compile a real mod from source (expects mods/$MOD_NAME/gamesrc/):"
	echo "  $0 <mod_name>   # see mods/*/gamesrc"
	exit 1
fi
unset _mod_lc

MOD_ROOT="$PROJECT_ROOT/mods/$MOD_NAME"
MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
MOD_BUILD_DIR="$MOD_SOURCE_DIR/build"
MOD_VM_DIR="$MOD_ROOT/vm"
RELEASE_MOD_DIR="$PROJECT_ROOT/release/$MOD_NAME"
RELEASE_VM_DIR="$RELEASE_MOD_DIR/vm"
RELEASE_PK3="$RELEASE_MOD_DIR/$MOD_NAME.pk3"
RUNTIME_VM_DIR="$HOME/.$MOD_NAME/vm"

if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: ${MOD_SOURCE_DIR} not found."
    echo ""
    echo "Available mods with gamesrc:"
    for moddir in "$PROJECT_ROOT/mods"/*/; do
        if [ -d "${moddir}gamesrc" ]; then
            echo "  - $(basename "$moddir")"
        fi
    done
    echo ""
    echo "Usage: $0 [mod_name] [Debug|Release] [clean]"
    exit 1
fi

echo "Building game modules..."
echo "Project root: $PROJECT_ROOT"
echo "Mod name: $MOD_NAME"
echo "Build type: $BUILD_TYPE"
echo "Module sources: $MOD_SOURCE_DIR"
echo "Release destination: $RELEASE_MOD_DIR"

cd "$MOD_SOURCE_DIR"

# Check for CMake cache source mismatch or cross-directory confusion and fix it
CMAKE_CACHE="$MOD_BUILD_DIR/CMakeCache.txt"
CMAKE_CACHE_DIR_OK=1
if [ -f "$CMAKE_CACHE" ]; then
    # Extract CMAKE_HOME_DIRECTORY and look for old/bad values
    CACHE_SRC_DIR=$(grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' "$CMAKE_CACHE" | head -n1 | cut -d= -f2)
    if [ -n "$CACHE_SRC_DIR" ] && [ "$CACHE_SRC_DIR" != "$MOD_SOURCE_DIR" ]; then
        echo ""
        echo "Warning:"
        echo "CMake cache source mismatch detected."
        echo "  CMakeCache.txt: $CMAKE_CACHE"
        echo "  Current source: $MOD_SOURCE_DIR"
        echo "  Cached source:  $CACHE_SRC_DIR"
        echo "Removing bad CMake cache. (This happens when moving or renaming mod/source directories.)"
        rm -rf "$MOD_BUILD_DIR"
    fi
fi

if [ $CLEAN -eq 1 ] && [ -d "$MOD_BUILD_DIR" ]; then
    echo "Cleaning old build directory..."
    rm -rf "$MOD_BUILD_DIR"
fi

# Remove old VM files from the mod's own release directory (optional)
if [ $CLEAN -eq 1 ] && [ -d "$RELEASE_VM_DIR" ]; then
    echo "Removing old VM files from $RELEASE_VM_DIR ..."
    rm -f "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null || true
fi

# Create build directory and configure CMake
mkdir -p "$MOD_BUILD_DIR"
cd "$MOD_BUILD_DIR"
if ! cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..; then
    echo "Error: CMake configuration failed for $MOD_NAME"
    exit 1
fi

# Build with parallel jobs
if command -v nproc &>/dev/null; then
    CORES=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi
if ! cmake --build . -- -j${CORES}; then
    echo "Error: Build failed for $MOD_NAME"
    exit 1
fi

# VM files should be in mod/vm/ directory
echo "Checking for compiled VM files in $MOD_VM_DIR"
mkdir -p "$MOD_VM_DIR"

# Map upstream artifact names to engine-expected names
map_vm_name() {
    local base="$1"
    case "$base" in
        gamex86_64.so)  echo "game.x86_64.so" ;;
        cgamex86_64.so) echo "cgame.x86_64.so" ;;
        uix86_64.so)    echo "ui.x86_64.so" ;;
        *)              echo "$base" ;;
    esac
}

shopt -s nullglob
# Prefer vm/ outputs, but also pick up direct outputs in the mod root (CMake BS_OUTPUT_DIR=..)
ARTIFACTS=("$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll "$MOD_ROOT"/*x86_64.so "$MOD_ROOT"/*x86_64.dll)
shopt -u nullglob

if [ ${#ARTIFACTS[@]} -eq 0 ]; then
    echo "Warning: No shared libraries found in $MOD_VM_DIR/"
else
    mkdir -p "$RELEASE_VM_DIR"
    mkdir -p "$RUNTIME_VM_DIR"
    # Clear stale qvms and misnamed so files so the engine loads the fresh natives
    rm -f "$RELEASE_VM_DIR"/*.qvm "$RUNTIME_VM_DIR"/*.qvm \
          "$RELEASE_VM_DIR"/gamex86_64.so "$RELEASE_VM_DIR"/cgamex86_64.so "$RELEASE_VM_DIR"/uix86_64.so \
          "$RUNTIME_VM_DIR"/gamex86_64.so "$RUNTIME_VM_DIR"/cgamex86_64.so "$RUNTIME_VM_DIR"/uix86_64.so 2>/dev/null || true
    echo "Copying files to mod's release directory: $RELEASE_VM_DIR (and runtime: $RUNTIME_VM_DIR)"
    
    # Track which files we've already copied to avoid duplicates
    declare -A copied_files
    
    for lib in "${ARTIFACTS[@]}"; do
        if [ ! -f "$lib" ]; then
            continue
        fi
        libname=$(basename "$lib")
        mapped=$(map_vm_name "$libname")
        
        # Skip if we've already copied this mapped name
        # Use [[ -v ]] to check if key exists (works with set -u)
        if [[ -v copied_files[$mapped] ]]; then
            continue
        fi
        
        # Only copy if the target doesn't exist or source is newer
        if [ ! -f "$RELEASE_VM_DIR/$mapped" ] || [ "$lib" -nt "$RELEASE_VM_DIR/$mapped" ]; then
            cp -v "$lib" "$RELEASE_VM_DIR/$mapped"
            cp -v "$lib" "$RUNTIME_VM_DIR/$mapped"
            copied_files[$mapped]=1
        fi
    done
    echo "Libraries copied to $RELEASE_VM_DIR/"
fi

package_pk3() {
    INCLUDES=()
    add_if_exists() {
        local rel="$1"
        if [ -e "$MOD_ROOT/$rel" ]; then
            INCLUDES+=("$rel")
        fi
    }

    add_if_exists "default.cfg"
    add_if_exists "autoexec.cfg"
    add_if_exists "config"
    add_if_exists "fonts"
    add_if_exists "scripts"
    add_if_exists "shaders"
    add_if_exists "textures"
    add_if_exists "models"
    add_if_exists "players"
    add_if_exists "ui"
    add_if_exists "vm"
    add_if_exists "maps"
    add_if_exists "levelshots"
    add_if_exists "gfx"
    add_if_exists "sound"

    if [ ${#INCLUDES[@]} -eq 0 ]; then
        echo "Nothing to package into pk3 for $MOD_NAME."
        return
    fi

    mkdir -p "$RELEASE_MOD_DIR"
    cd "$MOD_ROOT"

    echo "Packaging ${MOD_NAME}.pk3 ..."

    # Verbose output: list contents and details before packaging
    echo "Included in pk3:"
    for inc in "${INCLUDES[@]}"; do
        if [ -d "$MOD_ROOT/$inc" ]; then
            # List files recursively for this directory
            echo " [DIR] $inc"
            find "$inc" -type f | while IFS= read -r file; do
                # Print with indent and size
                if [ -f "$file" ]; then
                    size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
                    printf "   - %s (%s bytes)\n" "$file" "$size"
                fi
            done
        elif [ -f "$MOD_ROOT/$inc" ]; then
            size=$(stat -c%s "$MOD_ROOT/$inc" 2>/dev/null || stat -f%z "$MOD_ROOT/$inc")
            echo " [FILE] $inc ($size bytes)"
        else
            echo " [??] $inc (not found?)"
        fi
    done

    # zip may return non-zero exit codes for warnings, so check if file was created
    zip -r "$RELEASE_PK3" "${INCLUDES[@]}" \
        -x "gamesrc/*" "gamesrc/**" \
           "build/*" "build/**" \
           "out/*" "out/**" \
           "vm/*.a" "vm/*.pdb" "vm/*.dll" \
           "**/.DS_Store" "**/.git*" "**/CMakeFiles/**" \
        >/dev/null 2>&1 || true  # Ignore zip exit code, check file existence instead
    
    if [ -f "$RELEASE_PK3" ]; then
        echo "✓ Wrote $RELEASE_PK3"
    else
        echo "Warning: PK3 package creation may have failed, but build succeeded"
    fi
}

if [ $PK3_ENABLED -eq 1 ]; then
    package_pk3
fi

echo ""
if [ -d "$RELEASE_VM_DIR" ]; then
    # Use find instead of ls to avoid exit code issues with globs
    SO_COUNT=$(find "$RELEASE_VM_DIR" -maxdepth 1 \( -name "*.so" -o -name "*.dll" \) 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries available in $RELEASE_VM_DIR/:"
        ls -lh "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null || true
        echo "✓ Game modules ready in $RELEASE_VM_DIR"
    else
        echo "Warning: No shared libraries found in $RELEASE_VM_DIR/"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: $RELEASE_VM_DIR/*.so"
if [ -f "$RELEASE_VM_DIR/game.x86_64.so" ]; then
    echo "  Game module: $RELEASE_VM_DIR/game.x86_64.so"
fi
if [ -f "$RELEASE_PK3" ]; then
    echo "  Package: $RELEASE_PK3"
fi
echo "  Source: $MOD_SOURCE_DIR/"
