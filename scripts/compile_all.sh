#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_all.sh [modname] [Debug|Release] [clean] [vulkan|opengl]
# Notes:
# - modname defaults to "mymod" if not specified
# - build type defaults to Release
# - clean flag cleans build directories before building
# - renderer defaults to OpenGL if not specified

MODNAME=""
BUILD_TYPE="Release"
CLEAN=0
RENDERER=""

# Show usage if --help is requested
if [[ "$*" == *"--help"* ]] || [[ "$*" == *"-h"* ]]; then
    echo "Usage: $0 [modname] [Debug|Release] [clean] [vulkan|opengl]"
    echo ""
    echo "Options:"
    echo "  modname       Mod name (defaults to 'mymod' if not specified)"
    echo "  Debug|Release Build type (defaults to Release)"
    echo "  clean         Clean build directories before building"
    echo "  vulkan|opengl Renderer backend (defaults to OpenGL)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build mymod in Release mode with OpenGL"
    echo "  $0 mymod              # Build mymod in Release mode"
    echo "  $0 mymod Debug clean  # Build mymod in Debug mode, clean first"
    echo "  $0 mymod Release vulkan # Build mymod in Release mode with Vulkan"
    exit 0
fi

normalize_build_type() {
    local arg=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$arg" in
        debug|dbg|d) echo "Debug" ;;
        release|rel|r) echo "Release" ;;
        *) echo "" ;;
    esac
}

# Argument parsing
for arg in "$@"; do
    norm_bt="$(normalize_build_type "$arg")"
    if [ -n "$norm_bt" ]; then
        BUILD_TYPE="$norm_bt"
        continue
    fi
    
    case "$arg" in
        clean) CLEAN=1 ;;
        vulkan) RENDERER="vulkan" ;;
        opengl) RENDERER="opengl" ;;
        *) 
            # First unrecognized arg is mod name
            if [ -z "$MODNAME" ]; then
                MODNAME="$arg"
            fi
            ;;
    esac
done

# Default mod name if not specified
if [ -z "$MODNAME" ]; then
    MODNAME="mymod"
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Compiling everything (engine, game, editor) with MOD: $MODNAME"
echo "Build type: $BUILD_TYPE"
if [ "$CLEAN" -eq 1 ]; then
    echo "Clean build: Yes"
fi
if [ -n "$RENDERER" ]; then
    echo "Renderer: $RENDERER"
fi
echo "Root directory: $ROOT_DIR"
echo

# Build engine compile command
ENGINE_CMD=("$ROOT_DIR/scripts/compile_engine.sh")
if [ -n "$MODNAME" ] && [ "$MODNAME" != "idtech3" ]; then
    ENGINE_CMD+=("$MODNAME")
fi
ENGINE_CMD+=("$BUILD_TYPE")
if [ "$CLEAN" -eq 1 ]; then
    ENGINE_CMD+=("clean")
fi
if [ -n "$RENDERER" ]; then
    ENGINE_CMD+=("$RENDERER")
fi

# Compile engine
echo "=== Compiling Engine ==="
"${ENGINE_CMD[@]}"

# Build game compile command
GAME_CMD=("$ROOT_DIR/scripts/compile_game.sh" "$MODNAME")
GAME_CMD+=("$BUILD_TYPE")
if [ "$CLEAN" -eq 1 ]; then
    GAME_CMD+=("clean")
fi

# Compile game VM for the specified mod
echo ""
echo "=== Compiling Game VM ==="
"${GAME_CMD[@]}"

# Build editor compile command
EDITOR_CMD=("$ROOT_DIR/scripts/compile_editor.sh")
EDITOR_CMD+=("$BUILD_TYPE")
if [ "$CLEAN" -eq 1 ]; then
    EDITOR_CMD+=("clean")
fi

# Compile Radiant (editor)
echo ""
echo "=== Compiling Editor ==="
"${EDITOR_CMD[@]}"

# Ensure loose autoexec.cfg is available (engine may not exec from PK3)
if [ -f "$ROOT_DIR/mods/$MODNAME/autoexec.cfg" ]; then
  mkdir -p "$ROOT_DIR/release/$MODNAME"
  cp "$ROOT_DIR/mods/$MODNAME/autoexec.cfg" "$ROOT_DIR/release/$MODNAME/autoexec.cfg"
  echo "Copied autoexec.cfg to release/$MODNAME/"
fi

# Optionally run the game (comment out if running isn't needed)
# "$ROOT_DIR/release/idtech3.x86_64" \
#   +set fs_basepath "$ROOT_DIR/release" \
#   +set fs_homepath "$ROOT_DIR/release" \
#   +set fs_game "$MODNAME"

# Coverage build/report (Debug + ENABLE_COVERAGE=ON) if gcovr is available
if command -v gcovr >/dev/null 2>&1; then
  echo ""
  echo "[compile_all] Running coverage build..."
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build-coverage" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
  cmake --build "$ROOT_DIR/build-coverage" --target coverage
  echo "[compile_all] Coverage artifacts should be in build-coverage/"
else
  echo ""
  echo "[compile_all] gcovr not found; skipping coverage build."
fi

echo
echo "All components (engine, mod, editor) compiled successfully for MOD: $MODNAME!"
