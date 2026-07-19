#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_engine.sh [game_name] [Debug|Release] [clean] ... [python|py] [csharp] ...
# Notes:
# - build type defaults to Release
# - Vulkan is the only renderer backend
# - demo: BUILD_SAMPLES_DEMO_GAME=ON, builds demo_game_pk3 (idtech3_demo.pk3), copies to release/demo_game/ when present
# - aarch64: cross-compile for Linux aarch64 (requires gcc-aarch64-linux-gnu); may fail without ARM sysroot
# - mac-app wraps the legacy bundle script (requires release|debug target, optional architecture)
# - mac-ub2 compiles universal-2 binaries (release only) and can optionally notarize
# - first unrecognized arg becomes game_name

VULKAN=0
SKIP_IDPAK=0
FREETYPE=0
LUA=0
DUKTAPE=1
SYSTEM_DUKTAPE=0
CSHARP=0
FREEUSD=1
PHYSICS_BACKEND="box3d"
VULKAN_RTX=0
VUDA=0
EMULATOR=0
MIMIR=0
PYTHON=0
CROSS_AARCH64=0
CODECS_FOR_CROSS=0
BUILD_DEMO_PK3=0

GAME_NAME="idtech3"
BUILD_TYPE="Release"
CLEAN=0
COVERAGE=0
ASAN=0
LTO=0
QUIET=0
SKIP_SHADERS=0
RUN_MSVC_SYNC=0
MSVC_SYNC_ONLY=0
MAC_APP=0
MAC_APP_TARGET=""
MAC_APP_ARCH=""
MAC_UB2=0
MAC_UB2_NOTARIZE=0
EXTRA_OUT_DIR=""
MAC_HAS_LIPO="$(command -v lipo || true)"
MAC_HAS_CP="$(command -v cp || true)"

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

# Phase 5c: cross-domain include symlinks (idempotent; safe before cmake).
if [[ -x "${PROJECT_ROOT}/scripts/layout_forwarding_symlinks.sh" ]] \
	&& [[ -f "${PROJECT_ROOT}/engine/core/common.c" ]]; then
	"${PROJECT_ROOT}/scripts/layout_forwarding_symlinks.sh"
fi

IDTECH3_PROFILE="game"

# Argument parsing
while [[ $# -gt 0 ]]; do
  case "$1" in
    core|game|full|research)
      IDTECH3_PROFILE="$1"
      shift
      ;;
    clean)
      CLEAN=1
      shift
      ;;
    coverage|cov)
      COVERAGE=1
      shift
      ;;
    asan)
      ASAN=1
      BUILD_TYPE="Debug"
      shift
      ;;
    lto)
      LTO=1
      shift
      ;;
    quiet|-q|--quiet|q|silent|-s|--silent)
      QUIET=1
      shift
      ;;
    skipshaders|skip-shaders|skip_shaders)
      SKIP_SHADERS=1
      shift
      ;;
    msvc|msvc-sync|sync-msvc)
      RUN_MSVC_SYNC=1
      shift
      ;;
    msvc-only|sync-msvc-only)
      RUN_MSVC_SYNC=1
      MSVC_SYNC_ONLY=1
      shift
      ;;
    demo|demo-game|demo-pk3|with-demo)
      BUILD_DEMO_PK3=1
      shift
      ;;
    vulkan)
      VULKAN=1
      shift
      ;;
    skip-idpak-check|skip_idpak_check|skip-pak|skip-paks)
      SKIP_IDPAK=1
      shift
      ;;
    opengl)
      echo "Error: OpenGL has been removed; use the Vulkan renderer." >&2
      exit 1
      shift
      ;;
    aarch64|cross-aarch64)
      CROSS_AARCH64=1
      shift
      ;;
    codecs)
      CODECS_FOR_CROSS=1
      shift
      ;;
    all-linux|both)
      shift
      echo "Building native (x86_64) first..."
      "$0" "$@" vulkan || exit 1
      echo ""
      echo "Building aarch64 (cross-compile)..."
      "$0" "$@" vulkan aarch64 || echo "Warning: aarch64 cross-build failed (install gcc-aarch64-linux-gnu; may need ARM sysroot)"
      exit 0
      ;;
    freetype)
      FREETYPE=1
      shift
      ;;
    lua)
      LUA=1
      shift
      ;;
    duktape|js)
      DUKTAPE=1
      shift
      ;;
    rtx|vulkan-rtx|vulkan_rtx)
      VULKAN_RTX=1
      shift
      ;;
    vuda|cuda-vulkan)
      VUDA=1
      shift
      ;;
    emulator|qemu)
      EMULATOR=1
      shift
      ;;
    mimir|mimir-cuda)
      MIMIR=1
      shift
      ;;
    nofreeusd|no-freeusd|nofree-usd)
      FREEUSD=0
      shift
      ;;
    box3d|physics-box3d)
      PHYSICS_BACKEND="box3d"
      shift
      ;;
    bullet|physics-bullet)
      PHYSICS_BACKEND="bullet"
      shift
      ;;
    jolt|physics-jolt)
      PHYSICS_BACKEND="jolt"
      shift
      ;;
    no-physics|nophysics|physics-none)
      PHYSICS_BACKEND="none"
      shift
      ;;
    no-duktape|noduktape|nojs)
      DUKTAPE=0
      shift
      ;;
    system-duktape|system_duktape)
      SYSTEM_DUKTAPE=1
      shift
      ;;
    vendored-duktape|vendored_duktape|no-system-duktape|nosystemduktape)
      SYSTEM_DUKTAPE=0
      shift
      ;;
    csharp|cs|mono)
      CSHARP=1
      shift
      ;;
    no-csharp|nocs|nomono)
      CSHARP=0
      shift
      ;;
    python|py)
      PYTHON=1
      shift
      ;;
    no-python|nopy)
      PYTHON=0
      shift
      ;;
    --out|--output|--dir)
      if [[ $# -lt 2 ]]; then
        echo "Error: $1 requires a directory argument." >&2
        exit 1
      fi
      EXTRA_OUT_DIR="$2"
      shift
      shift
      ;;
    mac-app)
      MAC_APP=1
      shift
      if [[ $# -eq 0 ]]; then
        echo "mac-app requires a target (release|debug)." >&2
        exit 1
      fi
      target_norm="$(normalize_build_type "$1")"
      if [[ -z "$target_norm" ]]; then
        echo "mac-app target must be release or debug." >&2
        exit 1
      fi
      MAC_APP_TARGET="$(echo "$target_norm" | tr '[:upper:]' '[:lower:]')"
      shift
      if [[ $# -gt 0 ]] && [[ "$1" =~ ^(x86|x86_64|ppc|aarch64)$ ]]; then
        MAC_APP_ARCH="$1"
        shift
      fi
      ;;
    mac-ub2)
      MAC_UB2=1
      shift
      if [[ $# -gt 0 ]] && [[ "$1" == "notarize" ]]; then
        MAC_UB2_NOTARIZE=1
        shift
      fi
      ;;
    *)
      norm_bt="$(normalize_build_type "$1")"
      if [[ -n "$norm_bt" ]]; then
        BUILD_TYPE="$norm_bt"
        shift
        continue
      fi
      GAME_NAME="$1"
      shift
      ;;
  esac
done

# Default to Vulkan if neither specified
if [ "$VULKAN" -eq 0 ]; then
  VULKAN=1
fi

if [ "$RUN_MSVC_SYNC" -eq 1 ]; then
  echo "Syncing MSVC vcxproj from CMake manifest..."
  "${PROJECT_ROOT}/scripts/msvc/sync_all_vcxproj.sh"
  if [ "$MSVC_SYNC_ONLY" -eq 1 ]; then
    echo "MSVC sync complete (msvc-only)."
    exit 0
  fi
fi

BUILD_DIR="$PROJECT_ROOT/build-vk-${BUILD_TYPE}"
if [ "$CROSS_AARCH64" -eq 1 ]; then
  BUILD_DIR="${BUILD_DIR}-aarch64"
  if ! command -v aarch64-linux-gnu-gcc &>/dev/null; then
    echo "Error: aarch64 cross-compiler not found. Install with:" >&2
    echo "  sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu" >&2
    echo "  (Cross-compilation may still fail without ARM sysroot for SDL3/OpenAL.)" >&2
    exit 1
  fi
  TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/toolchains/linux-aarch64.cmake"
  if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "Error: toolchain file not found: $TOOLCHAIN_FILE" >&2
    exit 1
  fi
  echo "Cross-compiling for Linux aarch64 (toolchain: $TOOLCHAIN_FILE)"
fi

echo "Building id Tech 3 engine (${BUILD_TYPE})..."
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo "Release dir:  $RELEASE_DIR"
echo "Profile:      $IDTECH3_PROFILE"
[[ -n "$EXTRA_OUT_DIR" ]] && echo "Extra out:    $EXTRA_OUT_DIR"

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

if [[ "$SKIP_SHADERS" -eq 1 ]]; then
  echo "Skipping Vulkan shader compilation/apply (skipshaders requested)."
else
  SHADER_SCRIPT="$PROJECT_ROOT/scripts/compile_shaders.sh"
  if [[ -x "$SHADER_SCRIPT" ]]; then
    echo "Compiling and applying Vulkan shaders..."
    "$SHADER_SCRIPT" --apply
  else
    echo "Warning: shader compile script not found or not executable: $SHADER_SCRIPT" >&2
  fi
fi

# CMake flags
CMAKE_FLAGS=(
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DUSE_STB_TRUETYPE=ON"
  "-DUSE_FFMPEG=ON"
  "-DUSE_DAV1D=ON"
  "-DUSE_VPX=ON"
  "-DUSE_THEORA=ON"
  "-DENABLE_FORTIFY_SOURCE=ON"
  "-DENABLE_ASAN=$([ "$ASAN" -eq 1 ] && echo ON || echo OFF)"
  "-DENABLE_LTO=$([ "$LTO" -eq 1 ] && echo ON || echo OFF)"
  "-DUSE_VULKAN=ON"
  "-DSKIP_SHADER_REGEN=ON"
  "-Wno-dev"
)
if [ "$CROSS_AARCH64" -eq 1 ]; then
  CMAKE_FLAGS+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
  # Cross-compilation: disable codecs by default (requires ARM sysroot with libavcodec-dev:arm64 etc)
  if [ "$CODECS_FOR_CROSS" -eq 0 ]; then
    CMAKE_FLAGS+=("-DUSE_FFMPEG=OFF" "-DUSE_DAV1D=OFF" "-DUSE_VPX=OFF" "-DUSE_THEORA=OFF")
    echo "CMake: cross-compile aarch64 (FFmpeg/AV1/VPX/Theora disabled; add 'codecs' to try)"
  fi
fi

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

if [ "$CSHARP" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_CSHARP=ON")
  echo "CMake: USE_CSHARP=ON"
else
  CMAKE_FLAGS+=("-DUSE_CSHARP=OFF")
  echo "CMake: USE_CSHARP=OFF"
fi

if [ "$PYTHON" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_PYTHON=ON")
  echo "CMake: USE_PYTHON=ON (embedded CPython; py_reload, Infernux batch bridge)"
else
  CMAKE_FLAGS+=("-DUSE_PYTHON=OFF")
  echo "CMake: USE_PYTHON=OFF"
fi

if [ "$FREEUSD" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_FREEUSD=ON")
  echo "CMake: USE_FREEUSD=ON (USDA mesh import + usd_* console tools)"
else
  CMAKE_FLAGS+=("-DUSE_FREEUSD=OFF")
  echo "CMake: USE_FREEUSD=OFF"
fi

if [ "$VULKAN_RTX" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_VULKAN_RTX=ON")
  echo "CMake: USE_VULKAN_RTX=ON (chocolate RT tier: Hybrid1/Raygun + r_rtx demo; exec demo_hybrid1.cfg)"
else
  CMAKE_FLAGS+=("-DUSE_VULKAN_RTX=OFF")
fi

if [ "$VUDA" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_VUDA=ON")
  echo "CMake: USE_VUDA=ON (CUDA-Vulkan interop; set r_vuda 1 + cl_vuda 1 + vid_restart)"
else
  CMAKE_FLAGS+=("-DUSE_VUDA=OFF")
fi

if [ "$EMULATOR" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_IDTECH3_EMULATOR=ON")
  echo "CMake: USE_IDTECH3_EMULATOR=ON (QEMU guest bridge; cl_emulator 1 + r_emulatorScreen 1)"
  if [ ! -f "$PROJECT_ROOT/third_party/idtech3-emulator/README.rst" ]; then
    echo "Initializing idTech3-Emulator submodule..."
    git -C "$PROJECT_ROOT" submodule update --init third_party/idtech3-emulator || true
  fi
else
  CMAKE_FLAGS+=("-DUSE_IDTECH3_EMULATOR=OFF")
fi

if [ "$MIMIR" -eq 1 ]; then
  CMAKE_FLAGS+=("-DUSE_MIMIR_CUDA=ON")
  echo "CMake: USE_MIMIR_CUDA=ON (Mímir CUDA interop; r_mimir 1 + r_mimir_cuda 1 + vid_restart)"
else
  CMAKE_FLAGS+=("-DUSE_MIMIR_CUDA=OFF")
fi

CMAKE_FLAGS+=("-DRENDERER_DEFAULT=vulkan")
CMAKE_FLAGS+=("-DIDTECH3_PROFILE=${IDTECH3_PROFILE}")
CMAKE_FLAGS+=("-DIDTECH3_PHYSICS_BACKEND=${PHYSICS_BACKEND}")
echo "CMake: IDTECH3_PROFILE=${IDTECH3_PROFILE}"
echo "CMake: IDTECH3_PHYSICS_BACKEND=${PHYSICS_BACKEND}"

if [ "$SKIP_IDPAK" -eq 1 ]; then
  CMAKE_FLAGS+=("-DSKIP_IDPAK_CHECK=ON")
  echo "CMake: SKIP_IDPAK_CHECK=ON"
fi

if command -v ccache &>/dev/null; then
  CMAKE_FLAGS+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache" "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
  echo "CMake: ccache enabled (faster incremental builds)"
fi

if [ "$BUILD_DEMO_PK3" -eq 1 ]; then
  CMAKE_FLAGS+=("-DBUILD_SAMPLES_DEMO_GAME=ON")
  echo "CMake: BUILD_SAMPLES_DEMO_GAME=ON (idtech3_demo.pk3 via demo_game_pk3)"
fi

if [ "$LTO" -eq 1 ]; then
  echo "CMake: ENABLE_LTO=ON (IPO/LTO for Release/RelWithDebInfo on GCC/Clang; expect longer links)"
fi

if [ "$FREEUSD" -eq 1 ] && [ -f "$PROJECT_ROOT/.gitmodules" ]; then
  if [ ! -f "$PROJECT_ROOT/third_party/FreeUSD/CMakeLists.txt" ] && [ ! -f "$PROJECT_ROOT/src/external/FreeUSD/CMakeLists.txt" ]; then
    echo "Initializing FreeUSD submodule (third_party/FreeUSD)..."
    if ! git -C "$PROJECT_ROOT" submodule update --init third_party/FreeUSD; then
      echo "Warning: FreeUSD submodule init failed; CMake may use FetchContent fallback." >&2
    fi
  fi
fi

if [ "$PHYSICS_BACKEND" = "box3d" ] && [ -f "$PROJECT_ROOT/.gitmodules" ]; then
  if [ ! -f "$PROJECT_ROOT/third_party/box3d/CMakeLists.txt" ]; then
    echo "Initializing Box3D submodule (third_party/box3d)..."
    if ! git -C "$PROJECT_ROOT" submodule update --init third_party/box3d; then
      echo "Warning: Box3D submodule init failed; physics backend may fall back to none." >&2
    fi
  fi
fi

if [ "$PHYSICS_BACKEND" = "jolt" ] && [ -f "$PROJECT_ROOT/.gitmodules" ]; then
  if [ ! -f "$PROJECT_ROOT/third_party/JoltPhysics/Build/CMakeLists.txt" ]; then
    echo "Initializing JoltPhysics submodule (third_party/JoltPhysics)..."
    if ! git -C "$PROJECT_ROOT" submodule update --init third_party/JoltPhysics; then
      echo "Warning: JoltPhysics submodule init failed; physics backend may fall back to none." >&2
    fi
  fi
fi

echo "Running CMake configuration..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"

echo "Building with ${CORES} parallel jobs..."
if [ "$QUIET" -eq 1 ]; then
  cmake --build "$BUILD_DIR" -- -j"${CORES}" -s
else
  cmake --build "$BUILD_DIR" -- -j"${CORES}"
fi

if [ "$BUILD_DEMO_PK3" -eq 1 ]; then
  echo ""
  echo "Building example demo pack (demo_game_pk3)..."
  if [ "$QUIET" -eq 1 ]; then
    cmake --build "$BUILD_DIR" --target demo_game_pk3 -- -j"${CORES}" -s
  else
    cmake --build "$BUILD_DIR" --target demo_game_pk3 -- -j"${CORES}"
  fi
fi

echo ""
echo "Build completed. Binaries are in $BUILD_DIR"

copy_to_release() {
  local dest="$1"
  mkdir -p "$dest"
  mkdir -p "$dest/base"

  # Game data (minimal): ensure base/default.cfg, steamdeck.cfg, and fonts exist in release.
  if [ -d "$PROJECT_ROOT/base" ] && [ -f "$PROJECT_ROOT/base/default.cfg" ]; then
    cp -f "$PROJECT_ROOT/base/default.cfg" "$dest/base/default.cfg"
  fi
  if [ -d "$PROJECT_ROOT/base/fonts" ]; then
    mkdir -p "$dest/base/fonts"
    cp -f "$PROJECT_ROOT/base/fonts/"*.ttf "$dest/base/fonts/" 2>/dev/null || true
  fi
  # SDF console: explicit uiSdfText must be in scanned shader text (not only inside a mod pk3).
  if [ -d "$PROJECT_ROOT/base/scripts" ]; then
    mkdir -p "$dest/base/scripts"
    shopt -s nullglob
    for f in "$PROJECT_ROOT/base/scripts/"*.shader "$PROJECT_ROOT/base/scripts/"*.shaderx; do
      [ -f "$f" ] || continue
      cp -f "$f" "$dest/base/scripts/"
    done
    shopt -u nullglob
  fi
  if [ -f "$PROJECT_ROOT/config/steamdeck.cfg" ]; then
    cp -f "$PROJECT_ROOT/config/steamdeck.cfg" "$dest/base/steamdeck.cfg"
  fi
  for cfg in classic_baseq3.cfg modern_vulkan.cfg modern_vulkan_stable.cfg modern_vulkan_quality.cfg modern_vulkan_rt.cfg modern_vulkan_experimental.cfg gfx_safe.cfg modern_clustered.cfg deferred_vulkan.cfg vulkan_overlay_deferred.cfg vulkan_overlay_unified_clustered.cfg vulkan_overlay_unified_clustered_safe.cfg vulkan_overlay_oit_clustered.cfg vulkan_overlay_visibility_2027.cfg vulkan_overlay_rtx.cfg vulkan_overlay_hybrid1.cfg vulkan_overlay_aa_sharp.cfg vulkan_overlay_temporal_recon.cfg vulkan_overlay_temporal_perf.cfg modern_native.cfg modern_lighting.cfg console_ttf.cfg console_sdf.cfg; do
    if [ -f "$PROJECT_ROOT/config/$cfg" ]; then
      cp -f "$PROJECT_ROOT/config/$cfg" "$dest/base/$cfg"
      if [ -d "$dest/baseq3" ]; then
        cp -f "$PROJECT_ROOT/config/$cfg" "$dest/baseq3/$cfg"
      fi
    fi
  done

  # Client
  if [ -f "$BUILD_DIR/idtech3.x86_64" ]; then
    cp -f "$BUILD_DIR/idtech3.x86_64" "$dest/${GAME_NAME}.x86_64"
    echo "Copied client -> $dest/${GAME_NAME}.x86_64"
  fi
  if [ -f "$BUILD_DIR/idtech3.aarch64" ]; then
    cp -f "$BUILD_DIR/idtech3.aarch64" "$dest/${GAME_NAME}.aarch64"
    echo "Copied client -> $dest/${GAME_NAME}.aarch64"
  fi
  if [ -f "$BUILD_DIR/idtech3" ]; then
    cp -f "$BUILD_DIR/idtech3" "$dest/${GAME_NAME}"
    echo "Copied client -> $dest/${GAME_NAME}"
  fi

  # Server
  if [ -f "$BUILD_DIR/idtech3_server.x86_64" ]; then
    cp -f "$BUILD_DIR/idtech3_server.x86_64" "$dest/${GAME_NAME}_server.x86_64"
    echo "Copied server -> $dest/${GAME_NAME}_server.x86_64"
  fi
  if [ -f "$BUILD_DIR/idtech3_server.aarch64" ]; then
    cp -f "$BUILD_DIR/idtech3_server.aarch64" "$dest/${GAME_NAME}_server.aarch64"
    echo "Copied server -> $dest/${GAME_NAME}_server.aarch64"
  fi
  if [ -f "$BUILD_DIR/idtech3_server" ]; then
    cp -f "$BUILD_DIR/idtech3_server" "$dest/${GAME_NAME}_server"
    echo "Copied server -> $dest/${GAME_NAME}_server"
  fi

  # Renderers (Vulkan + optional roadmap scaffolds)
  shopt -s nullglob
  for sofile in "$BUILD_DIR"/idtech3_vulkan*.so "$BUILD_DIR"/idtech3_metal*.so "$BUILD_DIR"/idtech3_dxr*.so; do
    base="$(basename "$sofile")"
    cp -f "$sofile" "$dest/$base"
    echo "Copied renderer -> $dest/$base"
  done
  shopt -u nullglob

  # Bundled FetchContent SDL3 (libSDL3.so*) next to binaries for release/ runs
  shopt -s nullglob
  for sdlso in "$BUILD_DIR"/_deps/idtech3_sdl3-build/libSDL3.so*; do
    cp -a "$sdlso" "$dest/"
    echo "Copied SDL3 -> $dest/$(basename "$sdlso")"
  done
  shopt -u nullglob

  # FLUX CLI helper
  if [ -f "$BUILD_DIR/flux_cli" ]; then
    cp -f "$BUILD_DIR/flux_cli" "$dest/flux_cli"
    echo "Copied flux_cli -> $dest/flux_cli"
  elif [ -f "$BUILD_DIR/src/external/src/cflux2/flux_cli" ]; then
    cp -f "$BUILD_DIR/src/external/src/cflux2/flux_cli" "$dest/flux_cli"
    echo "Copied flux_cli -> $dest/flux_cli"
  fi
  if [ -f "$BUILD_DIR/flux_cli.x86_64" ]; then
    cp -f "$BUILD_DIR/flux_cli.x86_64" "$dest/flux_cli.x86_64"
    echo "Copied flux_cli -> $dest/flux_cli.x86_64"
  fi

  # ImGui shared
  if [ -f "$BUILD_DIR/libimgui_shared.so" ]; then
    cp -f "$BUILD_DIR/libimgui_shared.so" "$dest/"
    echo "Copied libimgui_shared.so to $dest/"
  fi

  # Vulkan launcher (sets LD_LIBRARY_PATH for custom SDL on RPi)
  if [ -f "$PROJECT_ROOT/scripts/run_vulkan.sh" ]; then
    cp -f "$PROJECT_ROOT/scripts/run_vulkan.sh" "$dest/run_vulkan.sh"
    chmod +x "$dest/run_vulkan.sh"
    echo "Copied run_vulkan.sh -> $dest/run_vulkan.sh"
  fi

  if [ -f "$PROJECT_ROOT/scripts/trellis_image_to_glb.py" ]; then
    cp -f "$PROJECT_ROOT/scripts/trellis_image_to_glb.py" "$dest/trellis_image_to_glb.py"
    chmod +x "$dest/trellis_image_to_glb.py"
    echo "Copied trellis_image_to_glb.py -> $dest/trellis_image_to_glb.py"
  fi

  if [ -f "$PROJECT_ROOT/scripts/genetic_gan_decode.py" ]; then
    cp -f "$PROJECT_ROOT/scripts/genetic_gan_decode.py" "$dest/genetic_gan_decode.py"
    chmod +x "$dest/genetic_gan_decode.py"
    echo "Copied genetic_gan_decode.py -> $dest/genetic_gan_decode.py"
  fi
}

echo ""
echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
copy_to_release "$RELEASE_DIR"

demo_pk3_copy() {
  local dest_root="$1"
  if [ "$BUILD_DEMO_PK3" -ne 1 ]; then
    return 0
  fi
  if [ ! -f "$BUILD_DIR/idtech3_demo.pk3" ]; then
    echo "Note: idtech3_demo.pk3 not found under $BUILD_DIR (demo_game_pk3 may have been skipped by CMake)." >&2
    return 0
  fi
  mkdir -p "$dest_root/demo_game"
  cp -f "$BUILD_DIR/idtech3_demo.pk3" "$dest_root/demo_game/idtech3_demo.pk3"
  echo "Copied idtech3_demo.pk3 -> $dest_root/demo_game/"
}

demo_pk3_copy "$RELEASE_DIR"

if [[ -n "$EXTRA_OUT_DIR" ]]; then
  echo ""
  echo "Copying engine binaries to extra output dir: $EXTRA_OUT_DIR"
  copy_to_release "$EXTRA_OUT_DIR"
  demo_pk3_copy "$EXTRA_OUT_DIR"
  echo "✓ Extra artifacts ready in $EXTRA_OUT_DIR"
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

macos_action() {
  local dest="$1"
  shift
  local sources=("$@")
  if [[ ${#sources[@]} -eq 0 ]]; then
    echo "macos_action: no source binaries provided for $dest" >&2
    return 1
  fi

  if [[ -x "$MAC_HAS_LIPO" ]]; then
    "$MAC_HAS_LIPO" -create -o "$dest" "${sources[@]}"
  elif [[ -x "$MAC_HAS_CP" ]]; then
    "$MAC_HAS_CP" "${sources[0]}" "$dest"
  else
    echo "macos_action: lipo and cp are unavailable; cannot create $dest" >&2
    return 1
  fi
}

macos_app_bundle() {
  local target="${1:-release}"
  local arch_override="${2:-}"
  local target_lower
  target_lower="$(echo "$target" | tr '[:upper:]' '[:lower:]')"
  if [[ "$target_lower" != "release" && "$target_lower" != "debug" ]]; then
    echo "mac-app: unsupported target '$target'; use release or debug." >&2
    return 1
  fi

  local objroot="$PROJECT_ROOT/build"
  local product="quake3e"
  local dedicated="quake3e.ded"
  local wrapper="${product}.app"
  local contents="${wrapper}/Contents"
  local executable_folder="${contents}/MacOS"
  local resources="${contents}/Resources"
  local base_dir="baseq3"
  local icnsdir="code/unix"
  local icns="quake3_flat.icns"
  local pkginfo="APPLQ3E"
  local q3e_version="1.32e"
  local deployment="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
  local search_archs="x86 x86_64 ppc aarch64"
  if [[ -n "$arch_override" ]]; then
    search_archs="$arch_override"
  fi

  local valid_archs=()
  local client_archs=()
  local server_archs=()

  pushd "$PROJECT_ROOT" > /dev/null
  for arch in $search_archs; do
    local build_dir="${objroot}/${target_lower}-darwin-${arch}"
    if [[ ! -d "$build_dir" ]]; then
      continue
    fi
    local client_bin="${build_dir}/${product}.${arch}"
    if [[ -f "$client_bin" ]]; then
      client_archs+=("$client_bin")
      valid_archs+=("$arch")
    fi
    local server_bin="${build_dir}/${dedicated}.${arch}"
    if [[ -f "$server_bin" ]]; then
      server_archs+=("$server_bin")
    fi
  done

  if [[ ${#client_archs[@]} -eq 0 ]]; then
    echo "mac-app: no client binaries found for target ${target_lower} (${search_archs})." >&2
    popd > /dev/null
    return 1
  fi

  local built_products_dir
  if [[ -n "$arch_override" ]]; then
    built_products_dir="${objroot}/${target_lower}-darwin-${arch_override}"
  else
    built_products_dir="${objroot}/${target_lower}-darwin-universal2"
  fi
  mkdir -p "$built_products_dir"
  mkdir -p "${built_products_dir}/${executable_folder}/${base_dir}"
  mkdir -p "${built_products_dir}/${resources}"

  local libs=()
  shopt -s nullglob
  libs=(code/libsdl/macosx/*.dylib)
  shopt -u nullglob
  if [[ ${#libs[@]} -gt 0 ]]; then
    cp "${libs[@]}" "${built_products_dir}/${executable_folder}"
  fi

  if [[ -f "${icnsdir}/${icns}" ]]; then
    cp "${icnsdir}/${icns}" "${built_products_dir}/${resources}/${icns}"
  fi

  printf "%s" "${pkginfo}" > "${built_products_dir}/${contents}/PkgInfo"

  cat <<EOF > "${built_products_dir}/${contents}/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${product}</string>
    <key>CFBundleIconFile</key>
    <string>${icns%.icns}</string>
    <key>CFBundleIdentifier</key>
    <string>org.quake3e.${product}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${product}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${q3e_version}</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>${q3e_version}</string>
    <key>CGDisableCoalescedUpdates</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>${deployment}</string>
    <key>NSHumanReadableCopyright</key>
    <string>QUAKE III ARENA Copyright © 1999-2000 id Software, Inc. All rights reserved.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSHighResolutionCapable</key>
    <false/>
    <key>NSRequiresAquaSystemAppearance</key>
    <false/>
</dict>
</plist>
EOF

  echo "Creating bundle '${built_products_dir}/${wrapper}'"
  echo "with architectures:"
  for arch in "${valid_archs[@]}"; do
    echo " ${arch}"
  done
  echo ""

  macos_action "${built_products_dir}/${executable_folder}/${product}" "${client_archs[@]}"
  if [[ ${#server_archs[@]} -gt 0 ]]; then
    macos_action "${built_products_dir}/${executable_folder}/${dedicated}" "${server_archs[@]}"
  fi

  popd > /dev/null
}

macos_build_universal() {
  local target="${1:-release}"
  local target_lower
  target_lower="$(echo "$target" | tr '[:upper:]' '[:lower:]')"
  local jobs="${CORES:-4}"
  local archs=("x86_64" "aarch64")

  pushd "$PROJECT_ROOT" > /dev/null
  for arch in "${archs[@]}"; do
    echo "Building ${arch} client/dedicated server (${target_lower})"
    (PLATFORM=darwin ARCH="$arch" make -j"$jobs") || {
      echo "mac-ub2: failed to build ${arch}" >&2
      popd > /dev/null
      return 1
    }
    echo
  done
  popd > /dev/null

  macos_app_bundle "$target_lower"
  if [[ "$MAC_UB2_NOTARIZE" -eq 1 ]]; then
    macos_notarize_app "$target_lower"
  fi
}

macos_notarize_app() {
  local target="${1:-release}"
  local target_lower
  target_lower="$(echo "$target" | tr '[:upper:]' '[:lower:]')"
  local values_file="${SCRIPT_DIR}/make-macosx-values.local"
  if [[ ! -f "$values_file" ]]; then
    echo "Notarization values file missing at $values_file; skipping notarization." >&2
    return 1
  fi

  # shellcheck disable=SC1090
  source "$values_file"

  if [[ -z "${SIGNING_IDENTITY:-}" || -z "${ASC_USERNAME:-}" || -z "${ASC_PASSWORD:-}" || -z "${ASC_PROVIDER:-}" ]]; then
    echo "Notarization credentials are incomplete; please define SIGNING_IDENTITY, ASC_USERNAME, ASC_PASSWORD, and ASC_PROVIDER." >&2
    return 1
  fi

  if ! command -v codesign >/dev/null 2>&1 || ! command -v xcrun >/dev/null 2>&1; then
    echo "codesign and xcrun are required for notarization; skipping." >&2
    return 1
  fi

  local release_location="$PROJECT_ROOT/build/${target_lower}-darwin-universal2"
  local release_build="quake3e.app"
  local pre_notarized_zip="quake3e_prenotarized.zip"
  local post_notarized_zip="quake3e_notarized.zip"
  local bundle_id="org.quake3e.quake3e"
  local entitlements="$PROJECT_ROOT/misc/xcode/quake3e/quake3e.entitlements"

  if [[ ! -d "$release_location" ]]; then
    echo "Notarization target missing: $release_location" >&2
    return 1
  fi
  if [[ ! -f "$entitlements" ]]; then
    echo "Entitlements file missing: $entitlements" >&2
    return 1
  fi

  codesign --force --options runtime --deep --entitlements "$entitlements" --sign "$SIGNING_IDENTITY" "$release_location/$release_build"

  if ! command -v ditto >/dev/null 2>&1; then
    echo "ditto is required to package the app for notarization; skipping." >&2
    return 1
  fi

  pushd "$release_location" > /dev/null

  rm -f "$pre_notarized_zip" "$post_notarized_zip"
  ditto -c -k --sequesterRsrc --keepParent "$release_build" "$pre_notarized_zip"

  local notarize_app_log
  local notarize_info_log
  notarize_app_log="$(mktemp -t notarize-app)"
  notarize_info_log="$(mktemp -t notarize-info)"

  if ! xcrun altool --notarize-app --primary-bundle-id "$bundle_id" --asc-provider "$ASC_PROVIDER" --username "$ASC_USERNAME" --password "$ASC_PASSWORD" -f "$pre_notarized_zip" > "$notarize_app_log" 2>&1; then
    cat "$notarize_app_log" 1>&2
    rm -f "$notarize_app_log" "$notarize_info_log"
    popd > /dev/null
    return 1
  fi

  local request_uuid
  request_uuid="$(awk -F ' = ' '/RequestUUID/ {print $2}' "$notarize_app_log")"

  while sleep 60 && date; do
    if xcrun altool --notarization-info "$request_uuid" --asc-provider "$ASC_PROVIDER" --username "$ASC_USERNAME" --password "$ASC_PASSWORD" > "$notarize_info_log" 2>&1; then
      cat "$notarize_info_log"
      if ! grep -q "Status: in progress" "$notarize_info_log"; then
        xcrun stapler staple "$release_build"
        break
      fi
    else
      cat "$notarize_info_log" 1>&2
      rm -f "$notarize_app_log" "$notarize_info_log"
      popd > /dev/null
      return 1
    fi
  done

  rm -f "$notarize_app_log" "$notarize_info_log"
  ditto -c -k --sequesterRsrc --keepParent "$release_build" "$post_notarized_zip"
  popd > /dev/null
  echo "Notarization artifacts ready at ${release_location}/${post_notarized_zip}"
}

MAC_APP_TARGET="${MAC_APP_TARGET:-release}"
if [[ "$MAC_UB2" -eq 1 ]]; then
  macos_build_universal "$MAC_APP_TARGET"
elif [[ "$MAC_APP" -eq 1 ]]; then
  macos_app_bundle "$MAC_APP_TARGET" "$MAC_APP_ARCH"
fi

echo "✓ Engine artifacts ready in $RELEASE_DIR"
