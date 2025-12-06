#!/usr/bin/env bash

# This script supports three build modes:
#   1. debug    - Build debug version of the engine and game modules
#   2. game     - Build game modules only (shared libraries)
#   3. engine   - Build engine only (client/server executables)
#   4. monolith - Build monolithic executable with both engine and game linked together
#
# Usage: ./compile_monolith.sh [mode] [Debug|Release] [clean] [output_name|mod_name]
#
#     [mode]          : Build mode: game, engine, or monolith (default: monolith)
#     [Debug|Release] : CMake build type (default: RelWithDebInfo)
#     [clean]         : Optionally erase the build directory before building
#     [output_name]   : For monolith: Name for the output executable (without extension, optional)
#                       For game: Mod name (default: mymod)

set -e

# Determine project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Parse arguments with smarter detection
MODE=""
BUILD_TYPE=""
CLEAN=""
EXTRA_ARG=""

# Normalize build type strings (case-insensitive)
normalize_build_type() {
    local arg=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$arg" in
        debug|dbg|d)
            echo "Debug"
            ;;
        release|rel|r)
            echo "Release"
            ;;
        relwithdebinfo|relwithdeb|withdebinfo|withdeb|rwd|relwd|rwdi)
            echo "RelWithDebInfo"
            ;;
        *)
            echo ""
            ;;
    esac
}

detect_cores() {
    if command -v nproc &>/dev/null; then
        echo "$(nproc)"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "$(sysctl -n hw.ncpu)"
    else
        echo "4"
    fi
}

# Parse arguments
ARGS=("$@")
i=0

# Check first argument - could be mode or build type
if [ $i -lt ${#ARGS[@]} ]; then
    FIRST_ARG="${ARGS[$i]}"
    NORMALIZED_BUILD_TYPE=$(normalize_build_type "$FIRST_ARG")
    
    # If first arg is a build type, use default mode (monolith)
    if [ -n "$NORMALIZED_BUILD_TYPE" ]; then
        MODE="monolith"
        BUILD_TYPE="$NORMALIZED_BUILD_TYPE"
        i=$((i+1))
    # Otherwise, treat as mode
    elif [[ "$FIRST_ARG" == "game" || "$FIRST_ARG" == "engine" || "$FIRST_ARG" == "monolith" ]]; then
        MODE="$FIRST_ARG"
        i=$((i+1))
    else
        # Default to monolith if unrecognized
        MODE="monolith"
    fi
fi

# Default mode if not set
MODE=${MODE:-monolith}

# Get build type (if not already set from first arg)
if [ -z "$BUILD_TYPE" ] && [ $i -lt ${#ARGS[@]} ]; then
    BUILD_TYPE_ARG="${ARGS[$i]}"
    NORMALIZED_BUILD_TYPE=$(normalize_build_type "$BUILD_TYPE_ARG")
    if [ -n "$NORMALIZED_BUILD_TYPE" ]; then
        BUILD_TYPE="$NORMALIZED_BUILD_TYPE"
        i=$((i+1))
    fi
fi

# Default build type (with debug symbols)
BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}

# Check for "clean" flag
if [ $i -lt ${#ARGS[@]} ]; then
    if [[ "${ARGS[$i]}" == "clean" ]]; then
        CLEAN="clean"
        i=$((i+1))
    fi
fi

# Get extra argument (mod name or output name)
if [ $i -lt ${#ARGS[@]} ]; then
    EXTRA_ARG="${ARGS[$i]}"
fi

# Validate mode
if [[ "$MODE" != "game" && "$MODE" != "engine" && "$MODE" != "monolith" ]]; then
    echo "Error: Invalid mode '$MODE'. Must be one of: game, engine, monolith"
    echo ""
    echo "Usage: $0 [mode] [Debug|Release] [clean] [output_name|mod_name]"
    echo ""
    echo "  mode:        game, engine, or monolith (default: monolith)"
    echo "  Debug|Release: Build type (case-insensitive, default: Release)"
    echo "  clean:       Optional flag to clean build directory"
    echo "  output_name: For monolith: executable name (optional)"
    echo "               For game: mod name (default: mymod)"
    echo ""
    echo "Examples:"
    echo "  $0 monolith Release clean mymod"
    echo "  $0 release clean mymod          # release = Release build type"
    echo "  $0 game Debug mymod"
    exit 1
fi

echo "== Build Script =="
echo "Mode: $MODE"
echo "Build type: $BUILD_TYPE"
CORES=$(detect_cores)

# Handle each mode
case "$MODE" in
    game)
        MOD_NAME=${EXTRA_ARG:-mymod}
        MOD_ROOT="$PROJECT_ROOT/$MOD_NAME"
        MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
        MOD_BUILD_DIR="$MOD_SOURCE_DIR/build"
        MOD_VM_DIR="$MOD_ROOT/vm"
        RELEASE_MOD_DIR="$PROJECT_ROOT/release/$MOD_NAME"
        RELEASE_VM_DIR="$RELEASE_MOD_DIR/vm"

        if [ ! -d "$MOD_SOURCE_DIR" ]; then
            echo "Error: ${MOD_SOURCE_DIR} not found."
            echo "Usage: $0 game [Debug|Release] [clean] [mod_name]"
            exit 1
        fi

        echo "Building game modules for mod: $MOD_NAME"
        echo "Module sources: $MOD_SOURCE_DIR"

        cd "$MOD_SOURCE_DIR"

        if [ "$CLEAN" == "clean" ]; then
            echo "Cleaning build directory..."
            rm -rf "$MOD_BUILD_DIR"
        fi

        mkdir -p "$MOD_BUILD_DIR"
        cd "$MOD_BUILD_DIR"
        cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

        # Determine number of CPU cores for parallel build
        if command -v nproc &>/dev/null; then
            CORES=$(nproc)
        fi

        echo "Building with ${CORES} parallel jobs..."
        cmake --build . -- -j${CORES}

        # VM files should be in mod/vm/ directory
        echo "Checking for compiled VM files in $MOD_VM_DIR"
        mkdir -p "$MOD_VM_DIR"

        # Move compiled files from mod/vm to release/mod/vm
        shopt -s nullglob
        ARTIFACTS=("$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll)
        shopt -u nullglob

        if [ ${#ARTIFACTS[@]} -eq 0 ]; then
            echo "Warning: No shared libraries found in $MOD_VM_DIR/"
        else
            mkdir -p "$RELEASE_VM_DIR"
            echo "Copying files to release directory: $RELEASE_VM_DIR"
            for lib in "${ARTIFACTS[@]}"; do
                libname=$(basename "$lib")
                cp -v "$lib" "$RELEASE_VM_DIR/$libname"
            done
            echo "Libraries copied to $RELEASE_VM_DIR/"
        fi

        echo ""
        echo "Game build completed!"
        echo "  Libraries: $RELEASE_VM_DIR/*.so"
        ;;

    engine)
        GAME_NAME=${EXTRA_ARG:-idtech3}
        BUILD_DIR="$PROJECT_ROOT/build"
        RELEASE_DIR="$PROJECT_ROOT/release"

        echo "Building id Tech 3 engine (${BUILD_TYPE}) as ${GAME_NAME}..."

        if [ "$CLEAN" == "clean" ]; then
            echo "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
        fi

        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"

        echo "Running CMake configuration..."
        CMAKE_ARGS=(
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
            -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O2 -g"
            -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g"
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        )
        cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT"

        echo "Building with ${CORES} parallel jobs..."
        cmake --build . -- -j${CORES}

        echo ""
        echo "Engine build completed. Binaries are in the build directory ($BUILD_DIR)."
        echo "  - Client:   $BUILD_DIR/idtech3.x86_64"
        echo "  - Server:   $BUILD_DIR/idtech3.server.x86_64"
        echo "  - Renderers: $BUILD_DIR/idtech3_*_*.so"

        # Copy to release directory
        echo ""
        echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
        mkdir -p "$RELEASE_DIR"

        # Copy/rename main client executable (ship with .so suffix)
        if [ -f "idtech3.x86_64" ]; then
            cp -f "idtech3.x86_64" "$RELEASE_DIR/${GAME_NAME}.x86_64.so"
            echo "Copied idtech3.x86_64 to $RELEASE_DIR/${GAME_NAME}.x86_64.so"
        fi

        # Copy/rename dedicated server executable if present
        if [ -f "idtech3.server.x86_64" ]; then
            cp -f "idtech3.server.x86_64" "$RELEASE_DIR/${GAME_NAME}.server.x86_64"
            cp -f "idtech3.server.x86_64" "$RELEASE_DIR/${GAME_NAME}.ded.x86_64"
            echo "Copied idtech3.server.x86_64 to $RELEASE_DIR/${GAME_NAME}.server.x86_64 (alias *.ded.x86_64)"
        fi

        # Copy renderer .so files (canonical names only)
        shopt -s nullglob
        for sofile in idtech3_*_*.so; do
            base=$(basename "$sofile")
            cp -f "$sofile" "$RELEASE_DIR/$base"
            echo "Copied $sofile to $RELEASE_DIR/$base"
        done
        shopt -u nullglob

        # Copy shared ImGui runtime if present
        if [ -f "libimgui_shared.so" ]; then
            cp -f "libimgui_shared.so" "$RELEASE_DIR/"
            echo "Copied libimgui_shared.so to $RELEASE_DIR/"
        fi

        echo "Engine binaries updated in $RELEASE_DIR"
        ;;

    monolith)
        MOD_NAME=${EXTRA_ARG:-mymod}
        OUTPUT_NAME=${EXTRA_ARG:-${MOD_NAME}}

        echo "Building monolithic executable (engine + game modules)..."
        echo "Mod name: $MOD_NAME"
        echo "Output name: $OUTPUT_NAME"
        echo ""

        MOD_ROOT="$PROJECT_ROOT/$MOD_NAME"
        MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"

        if [ ! -d "$MOD_SOURCE_DIR" ]; then
            echo "Error: ${MOD_SOURCE_DIR} not found."
            echo "Cannot build monolithic executable without game modules."
            exit 1
        fi

        BUILD_DIR="$PROJECT_ROOT/build_monolith"
        RELEASE_DIR="$PROJECT_ROOT/release"

        if [ "$CLEAN" == "clean" ]; then
            echo "Cleaning monolithic build directory..."
            rm -rf "$BUILD_DIR"
        fi

        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"

        echo "Preparing CMake configuration..."
        CMAKE_ARGS=(
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE
            -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O2 -g"
            -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g"
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            -DCOMBINED_MONOLITH=ON
            -DMONOLITH_EXECUTABLE_NAME=$OUTPUT_NAME
            -DMONOLITH_MOD_NAME=$MOD_NAME
            -DMONOLITH_MOD_DIR=$MOD_ROOT
        )

        echo "Running CMake configuration..."
        cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT"

        echo "Building monolithic executable with ${CORES} parallel jobs..."
        cmake --build . -- -j${CORES}

        # Determine output executable name with proper extension
        EXEC_BASE_NAME="$OUTPUT_NAME"
        if [ "$(uname)" == "Darwin" ]; then
            EXEC_NAME="$EXEC_BASE_NAME"
            OUTPUT_BIN="./$EXEC_NAME.app/Contents/MacOS/$EXEC_NAME"
        elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
            EXEC_NAME="$EXEC_BASE_NAME.exe"
            OUTPUT_BIN="$EXEC_NAME"
        else
            # Linux monolith keeps .so suffix per legacy behavior
            EXEC_NAME="$EXEC_BASE_NAME.so"
            OUTPUT_BIN="$EXEC_NAME"
        fi

        # Check for output executable (try various locations and names)
        MONOLITH_EXEC=""
        if [ -f "$OUTPUT_BIN" ]; then
            MONOLITH_EXEC="$OUTPUT_BIN"
        elif [ -f "$EXEC_NAME" ]; then
            MONOLITH_EXEC="$EXEC_NAME"
        elif [ -f "$EXEC_BASE_NAME" ]; then
            MONOLITH_EXEC="$EXEC_BASE_NAME"
        elif [ -f "idtech3.x86_64" ]; then
            MONOLITH_EXEC="idtech3.x86_64"
        elif [ -f "idtech3" ]; then
            MONOLITH_EXEC="idtech3"
        fi

        # Copy to release directory
        if [ ! -d "$RELEASE_DIR" ]; then
            mkdir -p "$RELEASE_DIR"
        fi
        
        if [ -n "$MONOLITH_EXEC" ] && [ -f "$MONOLITH_EXEC" ]; then
            cp -f "$MONOLITH_EXEC" "$RELEASE_DIR/$EXEC_NAME"
            
            echo ""
            echo "=== Step 2: Creating pk3 package ==="
            
            # Create mod directory in release
            RELEASE_MOD_DIR="$RELEASE_DIR/$MOD_NAME"
            mkdir -p "$RELEASE_MOD_DIR"
            
            # Create pk3 file directly from source mod folder (excluding gamesrc since it's compiled into executable)
            PAK_FILE="$RELEASE_MOD_DIR/pak0.pk3"
            if [ -d "$MOD_ROOT" ]; then
                echo "Creating pk3 package: $PAK_FILE"
                echo "Excluding gamesrc (already compiled into executable)..."
                
                cd "$MOD_ROOT"
                
                # Create pk3 file, excluding gamesrc and build artifacts
                zip -r "$PAK_FILE" . \
                    -x "gamesrc/*" \
                    -x "build/*" \
                    -x "build_monolith/*" \
                    -x "vm/*" \
                    -x "*.o" \
                    -x "*.a" \
                    -x "*.so" \
                    -x "*.md" \
                    -x "*.txt" \
                    -x "CHANGES" \
                    -x "COPYING" \
                    -x "CREDITS" \
                    -x "LINUXNOTES" \
                    -x "SDL.README.txt" \
                    -x "OGGVORBIS.README.txt" \
                    -x "NATIVE_COMPILATION.md" \
                    -x "PBR_GUIDE.md" \
                    -x "README" \
                    -x "README.md" \
                    -x "*.pspimage" \
                    -q || {
                    echo "Warning: Failed to create pak0.pk3"
                }
                
                if [ -f "$PAK_FILE" ]; then
                    FILE_SIZE=$(du -h "$PAK_FILE" | cut -f1)
                    FILE_COUNT=$(unzip -l "$PAK_FILE" 2>/dev/null | tail -1 | awk '{print $2}' || echo "unknown")
                    echo "✓ Created pak0.pk3 (Size: $FILE_SIZE, Files: $FILE_COUNT)"
                else
                    echo "Warning: pak0.pk3 was not created"
                fi
            else
                echo "Warning: Mod source folder not found, skipping pk3 creation"
            fi
            
            echo ""
            echo "=== Monolithic build completed! ==="
            echo "Monolithic executable: $RELEASE_DIR/$EXEC_NAME"
            echo "Mod directory: $RELEASE_MOD_DIR"
            if [ -f "$PAK_FILE" ]; then
                echo "pk3 package: $PAK_FILE"
            fi
            echo ""
            echo "The executable will automatically use mod: $MOD_NAME"
        else
            echo ""
            echo "Error: Monolithic executable not found."
            echo "Expected locations: $OUTPUT_BIN, $EXEC_NAME, $EXEC_BASE_NAME, idtech3.x86_64, or idtech3"
            exit 1
        fi
        ;;
esac


