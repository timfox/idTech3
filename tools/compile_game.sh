#!/bin/bash

# Compile Game Script for id Tech 3 mods
set -e

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

MOD_ROOT="$PROJECT_ROOT/$MOD_NAME"
MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
MOD_BUILD_DIR="$MOD_SOURCE_DIR/build"
MOD_VM_DIR="$MOD_ROOT/vm"
RELEASE_MOD_DIR="$PROJECT_ROOT/release/$MOD_NAME"
RELEASE_VM_DIR="$RELEASE_MOD_DIR/vm"
RELEASE_PK3="$RELEASE_MOD_DIR/$MOD_NAME.pk3"
RELEASE_SO="$PROJECT_ROOT/release/$MOD_NAME.so"

if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: ${MOD_SOURCE_DIR} not found."
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
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build with parallel jobs
if command -v nproc &>/dev/null; then
    CORES=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi
cmake --build . -- -j${CORES}

# VM files should be in mod/vm/ directory
echo "Checking for compiled VM files in $MOD_VM_DIR"
mkdir -p "$MOD_VM_DIR"

shopt -s nullglob
ARTIFACTS=("$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll)
shopt -u nullglob

if [ ${#ARTIFACTS[@]} -eq 0 ]; then
    echo "Warning: No shared libraries found in $MOD_VM_DIR/"
else
    mkdir -p "$RELEASE_VM_DIR"
    echo "Copying files to mod's release directory: $RELEASE_VM_DIR"
    for lib in "${ARTIFACTS[@]}"; do
        libname=$(basename "$lib")
        cp -v "$lib" "$RELEASE_VM_DIR/$libname"
    done
    echo "Libraries copied to $RELEASE_VM_DIR/"
fi

if [ ${#ARTIFACTS[@]} -gt 0 ]; then
    primary_so="${ARTIFACTS[0]}"
    for lib in "${ARTIFACTS[@]}"; do
        if [[ "$(basename "$lib")" == game*.so ]]; then
            primary_so="$lib"
            break
        fi
    done
    echo "Copying primary module to $RELEASE_SO"
    mkdir -p "$(dirname "$RELEASE_SO")"
    cp -v "$primary_so" "$RELEASE_SO"
fi

package_pk3() {
    INCLUDES=()
    add_if_exists() {
        local rel="$1"
        if [ -e "$PROJECT_ROOT/$rel" ]; then
            INCLUDES+=("$rel")
        fi
    }

    add_if_exists "$MOD_NAME/default.cfg"
    add_if_exists "$MOD_NAME/autoexec.cfg"
    add_if_exists "$MOD_NAME/config"
    add_if_exists "$MOD_NAME/fonts"
    add_if_exists "$MOD_NAME/scripts"
    add_if_exists "$MOD_NAME/shaders"
    add_if_exists "$MOD_NAME/ui"
    add_if_exists "$MOD_NAME/vm"
    add_if_exists "$MOD_NAME/maps"
    add_if_exists "$MOD_NAME/levelshots"
    add_if_exists "$MOD_NAME/gfx"
    add_if_exists "$MOD_NAME/sound"

    if [ ${#INCLUDES[@]} -eq 0 ]; then
        echo "Nothing to package into pk3 for $MOD_NAME."
        return
    fi

    mkdir -p "$RELEASE_MOD_DIR"
    cd "$PROJECT_ROOT"

    echo "Packaging ${MOD_NAME}.pk3 ..."
    zip -r "$RELEASE_PK3" "${INCLUDES[@]}" \
        -x "$MOD_NAME/gamesrc/*" "$MOD_NAME/gamesrc/**" \
           "$MOD_NAME/build/*" "$MOD_NAME/build/**" \
           "$MOD_NAME/out/*" "$MOD_NAME/out/**" \
           "$MOD_NAME/vm/*.a" "$MOD_NAME/vm/*.pdb" "$MOD_NAME/vm/*.dll" \
           "$MOD_NAME/**/.DS_Store" "$MOD_NAME/**/.git*" "$MOD_NAME/**/CMakeFiles/**" \
        >/dev/null
    echo "✓ Wrote $RELEASE_PK3"
}

if [ $PK3_ENABLED -eq 1 ]; then
    package_pk3
fi

echo ""
if [ -d "$RELEASE_VM_DIR" ]; then
    SO_COUNT=$(ls -1 "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null | wc -l)
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
echo "  Primary:  $RELEASE_SO"
echo "  Package:  $RELEASE_PK3"
echo "  Source: $MOD_SOURCE_DIR/"
