#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_engine.sh [game_name] [Debug|Release] [clean] [quiet] [coverage] [vulkan] [opengl] [freetype] [lua] [duktape|no-duktape] [system-duktape]
# Notes:
# - build type defaults to Release
# - vulkan and opengl are mutually exclusive
# - if neither is specified: defaults to OpenGL
# - first unrecognized arg becomes game_name

VULKAN=0
OPENGL=0
SKIP_IDPAK=0
FREETYPE=0
LUA=0
DUKTAPE=1
SYSTEM_DUKTAPE=0

GAME_NAME="idtech3"
BUILD_TYPE="Release"
CLEAN=0
COVERAGE=0
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

RELEASE_DIR="$PROJECT_ROOT/release"

# Argument parsing
for arg in "$@"; do
  norm_bt="$(normalize_build_type "$arg")"
  if [ -n "$norm_bt" ]; then BUILD_TYPE="$norm_bt"; continue; fi

  case "$arg" in
    clean) CLEAN=1 ;;
    coverage|cov) COVERAGE=1 ;;
    quiet|-q|--quiet|q|silent|-s|--silent) QUIET=1 ;;
    vulkan) VULKAN=1 ;;
    skip-idpak-check|skip_idpak_check|skip-pak|skip-paks) SKIP_IDPAK=1 ;;
    opengl) OPENGL=1 ;;
    freetype) FREETYPE=1 ;;
    lua) LUA=1 ;;
    duktape|js) DUKTAPE=1 ;;
    no-duktape|noduktape|nojs) DUKTAPE=0 ;;
    system-duktape|system_duktape) SYSTEM_DUKTAPE=1 ;;
    vendored-duktape|vendored_duktape|no-system-duktape|nosystemduktape) SYSTEM_DUKTAPE=0 ;;
    *) GAME_NAME="$arg" ;;
  esac
done

if [ "$VULKAN" -eq 1 ] && [ "$OPENGL" -eq 1 ]; then
  echo "Error: vulkan and opengl are mutually exclusive"
  exit 1
fi

# Default to OpenGL if neither specified
if [ "$VULKAN" -eq 0 ] && [ "$OPENGL" -eq 0 ]; then
  OPENGL=1
fi

# Renderer-specific build directory (prevents cache collisions)
if [ "$VULKAN" -eq 1 ]; then
  BUILD_DIR="$PROJECT_ROOT/build-vk-${BUILD_TYPE}"
else
  BUILD_DIR="$PROJECT_ROOT/build-gl-${BUILD_TYPE}"
fi

echo "Building id Tech 3 engine (${BUILD_TYPE})..."
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo "Release dir:  $RELEASE_DIR"

if [ "$CLEAN" -eq 1 ]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
  rm -rf "$PROJECT_ROOT/build-coverage" || true
fi

mkdir -p "$BUILD_DIR"

# CPU cores
if command -v nproc &>/dev/null; then
  CORES="$(nproc)"
elif [[ "${OSTYPE:-}" =~ ^darwin ]]; then
  CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  CORES=4
fi

# CMake flags
CMAKE_FLAGS=(
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DUSE_STB_TRUETYPE=ON"
  "-DENABLE_FORTIFY_SOURCE=OFF"
  "-DENABLE_ASAN=OFF"
  "-DBUILD_SERVER=ON"
  "-DUSE_VULKAN=ON"
  "-Wno-dev"
)

if [ "$FREETYPE" -eq 1 ]; then
  CMAKE_FLAGS+=("-DBUILD_FREETYPE=ON")
  echo "CMake: BUILD_FREETYPE=ON"
fi

if [ "$LUA" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_LUA=ON")
  echo "CMake: USE_LUA=ON"
fi

if [ "$DUKTAPE" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_DUKTAPE=ON")
  echo "CMake: USE_DUKTAPE=ON"
  if [ "$SYSTEM_DUKTAPE" -eq 1 ]; then
    CMAKE_FLAGS+=("-DUSE_SYSTEM_DUKTAPE=ON")
    echo "CMake: USE_SYSTEM_DUKTAPE=ON"
  else
    CMAKE_FLAGS+=("-DUSE_SYSTEM_DUKTAPE=OFF")
    echo "CMake: USE_SYSTEM_DUKTAPE=OFF"
  fi
else
  CMAKE_FLAGS+=("-DUSE_DUKTAPE=OFF")
  echo "CMake: USE_DUKTAPE=OFF"
fi

if [ "$VULKAN" -eq 1 ]; then
  CMAKE_FLAGS+=("-DRENDERER_DEFAULT=vulkan")
else
  CMAKE_FLAGS+=("-DRENDERER_DEFAULT=opengl")
fi

if [ "$SKIP_IDPAK" -eq 1 ]; then
  CMAKE_FLAGS+=("-DSKIP_IDPAK_CHECK=ON")
  echo "CMake: SKIP_IDPAK_CHECK=ON"
fi

echo "Running CMake configuration..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"

echo "Building with ${CORES} parallel jobs..."
if [ "$QUIET" -eq 1 ]; then
  cmake --build "$BUILD_DIR" -- -j"${CORES}" -s
else
  cmake --build "$BUILD_DIR" -- -j"${CORES}"
fi

echo ""
echo "Build completed. Binaries are in $BUILD_DIR"

echo ""
echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
mkdir -p "$RELEASE_DIR"

# Game data (minimal): ensure base/default.cfg exists in release.
# The engine reads base/default.cfg very early (before +set fs_game is applied).
if [ -d "$PROJECT_ROOT/base" ]; then
  mkdir -p "$RELEASE_DIR/base"
  # Copy only small text assets; keep this conservative.
  if [ -f "$PROJECT_ROOT/base/default.cfg" ]; then
    cp -f "$PROJECT_ROOT/base/default.cfg" "$RELEASE_DIR/base/default.cfg"
  fi
fi

# Client
if [ -f "$BUILD_DIR/idtech3.x86_64" ]; then
  cp -f "$BUILD_DIR/idtech3.x86_64" "$RELEASE_DIR/${GAME_NAME}.x86_64"
  echo "Copied client -> $RELEASE_DIR/${GAME_NAME}.x86_64"
fi


if [ -f "$BUILD_DIR/idtech3" ]; then
  cp -f "$BUILD_DIR/idtech3" "$RELEASE_DIR/${GAME_NAME}"
  echo "Copied client -> $RELEASE_DIR/${GAME_NAME}"
fi

# Server
if [ -f "$BUILD_DIR/idtech3_server.x86_64" ]; then
  cp -f "$BUILD_DIR/idtech3_server.x86_64" "$RELEASE_DIR/${GAME_NAME}_server.x86_64"
  echo "Copied server -> $RELEASE_DIR/${GAME_NAME}_server.x86_64"
fi


if [ -f "$BUILD_DIR/idtech3_server" ]; then
  cp -f "$BUILD_DIR/idtech3_server" "$RELEASE_DIR/${GAME_NAME}_server"
  echo "Copied server -> $RELEASE_DIR/${GAME_NAME}_server"
fi


# Renderers
shopt -s nullglob
for sofile in "$BUILD_DIR"/idtech3_*.so; do
  base="$(basename "$sofile")"
  cp -f "$sofile" "$RELEASE_DIR/$base"
  echo "Copied renderer -> $RELEASE_DIR/$base"
done
shopt -u nullglob

# FLUX CLI helper (external generation)
if [ -f "$BUILD_DIR/flux_cli" ]; then
  cp -f "$BUILD_DIR/flux_cli" "$RELEASE_DIR/flux_cli"
  echo "Copied flux_cli -> $RELEASE_DIR/flux_cli"
elif [ -f "$BUILD_DIR/src/external/src/cflux2/flux_cli" ]; then
  cp -f "$BUILD_DIR/src/external/src/cflux2/flux_cli" "$RELEASE_DIR/flux_cli"
  echo "Copied flux_cli -> $RELEASE_DIR/flux_cli"
fi
if [ -f "$BUILD_DIR/flux_cli.x86_64" ]; then
  cp -f "$BUILD_DIR/flux_cli.x86_64" "$RELEASE_DIR/flux_cli.x86_64"
  echo "Copied flux_cli -> $RELEASE_DIR/flux_cli.x86_64"
fi

# ImGui shared
if [ -f "$BUILD_DIR/libimgui_shared.so" ]; then
  cp -f "$BUILD_DIR/libimgui_shared.so" "$RELEASE_DIR/"
  echo "Copied libimgui_shared.so to $RELEASE_DIR/"
fi

# Coverage
if [ "$COVERAGE" -eq 1 ]; then
  echo ""
  echo "Running coverage configuration/build (Debug, ENABLE_COVERAGE=ON)..."
  if ! command -v gcovr >/dev/null 2>&1; then
    echo "gcovr not found in PATH; skipping coverage report generation."
  else
    cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build-coverage" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
    cmake --build "$PROJECT_ROOT/build-coverage" --target coverage
    echo "Coverage artifacts (HTML/XML) should be in build-coverage/"
  fi
fi

echo "✓ Engine artifacts ready in $RELEASE_DIR"
