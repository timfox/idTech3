#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_editor.sh [Debug|Release] [clean] [quiet] [--gtk] [--qt-only] [--tools-only]
# Notes:
# - build type defaults to Release
# - --gtk: build legacy GtkRadiant editor + q3map2/q3data tools
# - --qt-only: build only Qt editor (default if no flags)
# - --tools-only: build only q3map2/q3data tools (no editor)
# - clean: clean build directory before building
# - quiet: suppress build output

BUILD_GTK=0
BUILD_QT=1
BUILD_TOOLS=0
BUILD_TYPE="Release"
CLEAN=0
QUIET=0

normalize_build_type() {
  local arg
  arg="$(echo "$1" | tr '[:upper:]' '[:lower:]')"
  case "$arg" in
    debug|dbg|d) echo "Debug" ;;
    release|rel|r) echo "Release" ;;
    *) echo "" ;;
  esac
}

# Find project root robustly (works whether script is in repo root or /scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt near script location."
  exit 1
fi

BUILD_DIR="$PROJECT_ROOT/build/radiant-${BUILD_TYPE}"
RELEASE_DIR="$PROJECT_ROOT/release"

# Argument parsing
for arg in "$@"; do
  norm_bt="$(normalize_build_type "$arg")"
  if [ -n "$norm_bt" ]; then BUILD_TYPE="$norm_bt"; continue; fi

  case "$arg" in
    clean) CLEAN=1 ;;
    quiet|-q|--quiet|q|silent|-s|--silent) QUIET=1 ;;
    --gtk|gtk) BUILD_GTK=1; BUILD_QT=1; BUILD_TOOLS=1 ;;
    --qt-only|qt-only) BUILD_GTK=0; BUILD_QT=1; BUILD_TOOLS=0 ;;
    --tools-only|tools-only) BUILD_GTK=0; BUILD_QT=0; BUILD_TOOLS=1 ;;
    --help|-h|help) 
      echo "Usage: $0 [Debug|Release] [clean] [quiet] [--gtk] [--qt-only] [--tools-only]"
      echo ""
      echo "Options:"
      echo "  Debug|Release    Build type (default: Release)"
      echo "  clean            Clean build directory before building"
      echo "  quiet            Suppress build output"
      echo "  --gtk            Build legacy GtkRadiant + Qt editor + tools (q3map2, q3data)"
      echo "  --qt-only        Build only Qt editor (default)"
      echo "  --tools-only     Build only q3map2/q3data tools (no editor)"
      echo ""
      echo "Examples:"
      echo "  $0                    # Build Qt editor (Release)"
      echo "  $0 Debug              # Build Qt editor (Debug)"
      echo "  $0 --gtk              # Build everything (Qt + GTK + tools)"
      echo "  $0 --tools-only clean # Clean and build only tools"
      exit 0
      ;;
    *) 
      echo "Warning: Unknown argument '$arg' (ignored)"
      echo "Use --help for usage information"
      ;;
  esac
done

# Update build directory with final build type
BUILD_DIR="$PROJECT_ROOT/build/radiant-${BUILD_TYPE}"

echo "Building Radiant Editor (${BUILD_TYPE})..."
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo "Release dir:  $RELEASE_DIR"
echo "Build options:"
echo "  - Qt Editor:     $([[ $BUILD_QT -eq 1 ]] && echo 'ON' || echo 'OFF')"
echo "  - GTK Editor:    $([[ $BUILD_GTK -eq 1 ]] && echo 'ON' || echo 'OFF')"
echo "  - Tools (q3map2): $([[ $BUILD_TOOLS -eq 1 ]] && echo 'ON' || echo 'OFF')"
echo ""

if [ "$CLEAN" -eq 1 ]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# Ensure radiant directory exists (CMake expects it at root level)
# Create symlink if radiant is in tools/radiant
if [ ! -d "$PROJECT_ROOT/radiant" ] && [ -d "$PROJECT_ROOT/tools/radiant" ]; then
  echo "Creating symlink: radiant -> tools/radiant (required by CMake)"
  ln -sf tools/radiant "$PROJECT_ROOT/radiant"
fi

# CPU cores
if command -v nproc &>/dev/null; then
  CORES="$(nproc)"
elif [[ "${OSTYPE:-}" =~ ^darwin ]]; then
  CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  CORES=4
fi

# CMake flags for editor build
CMAKE_FLAGS=(
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DBUILD_RADIANT=ON"
  "-DRADIANT_BUILD_QT=$([[ $BUILD_QT -eq 1 ]] && echo ON || echo OFF)"
  "-DRADIANT_BUILD_EDITOR=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF)"
  "-DRADIANT_BUILD_PLUGINS=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF)"
  "-DRADIANT_BUILD_CONTRIB=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF)"
  "-DRADIANT_USE_ENGINE_RENDERER_VK=OFF"
  "-Wno-dev"
)

# Determine build targets
BUILD_TARGETS=()
if [ "$BUILD_QT" -eq 1 ]; then
  BUILD_TARGETS+=("radiant_qt")
fi
if [ "$BUILD_GTK" -eq 1 ]; then
  BUILD_TARGETS+=("radiant")
fi
if [ "$BUILD_TOOLS" -eq 1 ]; then
  BUILD_TARGETS+=("q3map2" "q3data")
fi

if [ ${#BUILD_TARGETS[@]} -eq 0 ]; then
  echo "Error: No build targets selected. Use --gtk, --qt-only, or --tools-only"
  exit 1
fi

echo "Running CMake configuration..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"

echo "Building with ${CORES} parallel jobs..."
echo "Targets: ${BUILD_TARGETS[*]}"
if [ "$QUIET" -eq 1 ]; then
  cmake --build "$BUILD_DIR" --target "${BUILD_TARGETS[@]}" -- -j"${CORES}" -s
else
  cmake --build "$BUILD_DIR" --target "${BUILD_TARGETS[@]}" -- -j"${CORES}"
fi

echo ""
echo "Build completed. Binaries are in $BUILD_DIR"

echo ""
echo "Copying editor binaries to $RELEASE_DIR..."
mkdir -p "$RELEASE_DIR"

# Copy Qt editor
if [ "$BUILD_QT" -eq 1 ]; then
  if [ -f "$BUILD_DIR/radiant_qt" ]; then
    cp -f "$BUILD_DIR/radiant_qt" "$RELEASE_DIR/"
    echo "Copied Qt editor -> $RELEASE_DIR/radiant_qt"
  elif [ -f "$BUILD_DIR/radiant/radiant_qt" ]; then
    cp -f "$BUILD_DIR/radiant/radiant_qt" "$RELEASE_DIR/"
    echo "Copied Qt editor -> $RELEASE_DIR/radiant_qt"
  fi
fi

# Copy GTK editor
if [ "$BUILD_GTK" -eq 1 ]; then
  if [ -f "$BUILD_DIR/radiant" ]; then
    cp -f "$BUILD_DIR/radiant" "$RELEASE_DIR/"
    echo "Copied GTK editor -> $RELEASE_DIR/radiant"
  elif [ -f "$BUILD_DIR/radiant/radiant" ]; then
    cp -f "$BUILD_DIR/radiant/radiant" "$RELEASE_DIR/"
    echo "Copied GTK editor -> $RELEASE_DIR/radiant"
  fi
fi

# Copy tools
if [ "$BUILD_TOOLS" -eq 1 ]; then
  if [ -f "$BUILD_DIR/q3map2" ]; then
    cp -f "$BUILD_DIR/q3map2" "$RELEASE_DIR/"
    echo "Copied q3map2 -> $RELEASE_DIR/q3map2"
  elif [ -f "$BUILD_DIR/radiant/q3map2" ]; then
    cp -f "$BUILD_DIR/radiant/q3map2" "$RELEASE_DIR/"
    echo "Copied q3map2 -> $RELEASE_DIR/q3map2"
  fi
  
  if [ -f "$BUILD_DIR/q3data" ]; then
    cp -f "$BUILD_DIR/q3data" "$RELEASE_DIR/"
    echo "Copied q3data -> $RELEASE_DIR/q3data"
  elif [ -f "$BUILD_DIR/radiant/q3data" ]; then
    cp -f "$BUILD_DIR/radiant/q3data" "$RELEASE_DIR/"
    echo "Copied q3data -> $RELEASE_DIR/q3data"
  fi
fi

echo "✓ Editor artifacts ready in $RELEASE_DIR"
