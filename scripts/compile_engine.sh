#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_engine.sh [game_name] [Debug|Release] [clean] [quiet] [coverage] [vulkan] [opengl] [freetype]
# Notes:
# - build type defaults to Release
# - vulkan and opengl are mutually exclusive
# - if neither is specified: defaults to OpenGL
# - first unrecognized arg becomes game_name

VULKAN=0
OPENGL=0
SKIP_IDPAK=0
FREETYPE=0
ASAN=0

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
    asan|sanitize|san) ASAN=1 ;;
    skip-idpak-check|skip_idpak_check|skip-pak|skip-paks) SKIP_IDPAK=1 ;;
    opengl) OPENGL=1 ;;
    freetype) FREETYPE=1 ;;
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

generate_build_graph() {
  local graph_file="${BUILD_DIR}/build_graph.txt"

  echo "Generating CMake build graph (targets list)..."
  if cmake --build "$BUILD_DIR" --target help >"$graph_file" 2>/dev/null; then
    echo "Saved build graph to $graph_file"
  else
    echo "Warning: failed to generate build graph (target help)"
  fi
}

write_build_metadata() {
  local metadata_file="${BUILD_DIR}/build_metadata.json"
  local graph_file_name="build_graph.txt"
  local python_cmd
  if command -v python3 >/dev/null 2>&1; then
    python_cmd=python3
  elif command -v python >/dev/null 2>&1; then
    python_cmd=python
  else
    echo "Warning: python interpreter not found; skipping build metadata"
    return
  fi

  local git_sha="unknown"
  if command -v git >/dev/null 2>&1; then
    git_sha="$(git -C "$PROJECT_ROOT" rev-parse HEAD 2>/dev/null || echo "unknown")"
  fi
  local timestamp
  timestamp="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  local renderer_name="opengl"
  if [ "$VULKAN" -eq 1 ]; then
    renderer_name="vulkan"
  fi
  local cmake_payload
  cmake_payload="$(printf '%s\n' "${CMAKE_FLAGS[@]}")"

  echo "Writing build metadata to $metadata_file"
  CMAKE_FLAG_PAYLOAD="$cmake_payload" \
    PROJECT_NAME="${GAME_NAME}" \
    BUILD_TYPE_VALUE="$BUILD_TYPE" \
    RENDERER_NAME="$renderer_name" \
    GIT_SHA_VALUE="$git_sha" \
    TIMESTAMP_VALUE="$timestamp" \
    GRAPH_FILE_NAME="$graph_file_name" \
    $python_cmd <<'PY' >"$metadata_file"
import json, os

flags = [
    line for line in os.environ.get("CMAKE_FLAG_PAYLOAD", "").splitlines() if line
]
data = {
    "project": os.environ.get("PROJECT_NAME", "idtech3"),
    "build_type": os.environ["BUILD_TYPE_VALUE"],
    "renderer": os.environ["RENDERER_NAME"],
    "git_sha": os.environ["GIT_SHA_VALUE"],
    "timestamp": os.environ["TIMESTAMP_VALUE"],
    "graph_file": os.environ["GRAPH_FILE_NAME"],
    "cmake_flags": flags,
}
print(json.dumps(data, indent=2))
PY
}

copy_metadata_to_release() {
  for entry in build_metadata.json build_graph.txt; do
    if [ -f "$BUILD_DIR/$entry" ]; then
      cp -f "$BUILD_DIR/$entry" "$RELEASE_DIR/$entry"
      echo "Copied $entry -> $RELEASE_DIR/"
    fi
  done

  local shader_meta_src="$PROJECT_ROOT/src/renderers/vulkan/shaders/spirv/shader_permutations.json"
  if [ -f "$shader_meta_src" ]; then
    mkdir -p "$RELEASE_DIR/shaders/spirv"
    cp -f "$shader_meta_src" "$RELEASE_DIR/shaders/spirv/"
    echo "Copied shader metadata -> $RELEASE_DIR/shaders/spirv/"
  fi
}

if [ "$FREETYPE" -eq 1 ]; then
  CMAKE_FLAGS+=("-DBUILD_FREETYPE=ON")
  echo "CMake: BUILD_FREETYPE=ON"
fi

if [ "$ASAN" -eq 1 ]; then
  CMAKE_FLAGS+=("-DENABLE_ASAN=ON")
  echo "CMake: ENABLE_ASAN=ON"
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

if [ "$VULKAN" -eq 1 ]; then
  echo "Regenerating Vulkan shaders..."
  python3 "$PROJECT_ROOT/scripts/compile_vulkan_shaders.py"
fi

generate_build_graph
write_build_metadata

echo "Building with ${CORES} parallel jobs..."
if [ "$QUIET" -eq 1 ]; then
  cmake --build "$BUILD_DIR" -- -j"${CORES}" -s
else
  # Explicit Vulkan Release command for convenience
  if [ "$VULKAN" -eq 1 ] && [ "$BUILD_TYPE" = "Release" ] && [ "$BUILD_DIR" = "$PROJECT_ROOT/build-vk-Release" ]; then
    cmake --build "$PROJECT_ROOT/build-vk-Release" -- -j"${CORES}"
  else
    cmake --build "$BUILD_DIR" -- -j"${CORES}"
  fi
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
for sofile in "$BUILD_DIR"/idtech3_*_*.so "$BUILD_DIR"/release/idtech3_*_*.so; do
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

copy_metadata_to_release

echo "✓ Engine artifacts ready in $RELEASE_DIR"
