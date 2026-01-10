#!/usr/bin/env bash
set -euo pipefail

# Usage: ./compile_ios.sh [Debug|Release] [clean] [quiet] [--simulator] [--device] [--xcode]
# Notes:
# - build type defaults to Release
# - --simulator: build for iOS Simulator (x86_64/arm64)
# - --device: build for physical iOS device (arm64)
# - --xcode: generate Xcode project instead of building
# - clean: clean build directory before building
# - quiet: suppress build output
#
# IMPORTANT: iOS builds require macOS with Xcode installed.
# This script will detect the platform and provide instructions if run on non-macOS.

BUILD_TYPE="Release"
CLEAN=0
QUIET=0
TARGET="device"  # device or simulator
GENERATE_XCODE=0

normalize_build_type() {
  local arg
  arg="$(echo "$1" | tr '[:upper:]' '[:lower:]')"
  case "$arg" in
    debug|dbg|d) echo "Debug" ;;
    release|rel|r) echo "Release" ;;
    *) echo "" ;;
  esac
}

# Find project root robustly
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt near script location."
  exit 1
fi

BUILD_DIR="$PROJECT_ROOT/build/ios-${BUILD_TYPE}"
RELEASE_DIR="$PROJECT_ROOT/release"

# Argument parsing
for arg in "$@"; do
  norm_bt="$(normalize_build_type "$arg")"
  if [ -n "$norm_bt" ]; then BUILD_TYPE="$norm_bt"; continue; fi

  case "$arg" in
    clean) CLEAN=1 ;;
    quiet|-q|--quiet|q|silent|-s|--silent) QUIET=1 ;;
    --simulator|simulator|sim) TARGET="simulator" ;;
    --device|device) TARGET="device" ;;
    --xcode|xcode) GENERATE_XCODE=1 ;;
    --help|-h|help) 
      echo "Usage: $0 [Debug|Release] [clean] [quiet] [--simulator] [--device] [--xcode]"
      echo ""
      echo "Options:"
      echo "  Debug|Release    Build type (default: Release)"
      echo "  clean            Clean build directory before building"
      echo "  quiet            Suppress build output"
      echo "  --simulator      Build for iOS Simulator (x86_64/arm64)"
      echo "  --device         Build for physical iOS device (arm64, default)"
      echo "  --xcode          Generate Xcode project instead of building"
      echo ""
      echo "Examples:"
      echo "  $0                      # Build for device (Release)"
      echo "  $0 Debug --simulator    # Build for simulator (Debug)"
      echo "  $0 --xcode              # Generate Xcode project"
      echo ""
      echo "IMPORTANT: iOS builds require macOS with Xcode installed."
      exit 0
      ;;
    *) 
      echo "Warning: Unknown argument '$arg' (ignored)"
      echo "Use --help for usage information"
      ;;
  esac
done

# Update build directory with final build type
BUILD_DIR="$PROJECT_ROOT/build/ios-${BUILD_TYPE}"

# Check if we're on macOS
if [[ "${OSTYPE:-}" != "darwin"* ]] && [[ "$(uname -s)" != "Darwin" ]]; then
  echo "❌ ERROR: iOS builds require macOS with Xcode."
  echo ""
  echo "Current platform: $(uname -s)"
  echo ""
  echo "To build for iOS, you need:"
  echo "  1. macOS (10.13+ recommended)"
  echo "  2. Xcode 9.0+ (with iOS SDK)"
  echo "  3. iOS 11.0+ target (13.0+ recommended)"
  echo ""
  echo "Build instructions for macOS:"
  echo "  cd $PROJECT_ROOT"
  echo "  mkdir -p build/ios"
  echo "  cd build/ios"
  echo "  cmake ../.. \\"
  echo "    -DCMAKE_SYSTEM_NAME=iOS \\"
  echo "    -DCMAKE_OSX_SYSROOT=iphoneos \\"
  echo "    -DCMAKE_OSX_ARCHITECTURES=arm64 \\"
  echo "    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \\"
  echo "    -DUSE_METAL=ON"
  echo "  cmake --build ."
  echo ""
  echo "Or generate Xcode project:"
  echo "  cmake ../.. -G Xcode -DUSE_METAL=ON"
  echo "  open idtech3.xcodeproj"
  echo ""
  exit 1
fi

# Check for Xcode
if ! command -v xcodebuild &>/dev/null; then
  echo "❌ ERROR: Xcode not found. Please install Xcode from the App Store."
  echo ""
  echo "After installing Xcode, run:"
  echo "  xcode-select --install"
  exit 1
fi

# Check for CMake
if ! command -v cmake &>/dev/null; then
  echo "❌ ERROR: CMake not found. Please install CMake:"
  echo "  brew install cmake"
  exit 1
fi

echo "Building id Tech 3 for iOS (${BUILD_TYPE})..."
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo "Release dir:  $RELEASE_DIR"
echo "Target:       $TARGET"
echo ""

if [ "$CLEAN" -eq 1 ]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# CPU cores
if command -v sysctl &>/dev/null; then
  CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  CORES=4
fi

# Determine iOS SDK and architecture
if [ "$TARGET" = "simulator" ]; then
  IOS_SYSROOT="iphonesimulator"
  # Support both x86_64 (Intel Mac) and arm64 (Apple Silicon) simulators
  if [[ "$(uname -m)" == "arm64" ]]; then
    IOS_ARCH="arm64"
  else
    IOS_ARCH="x86_64"
  fi
else
  IOS_SYSROOT="iphoneos"
  IOS_ARCH="arm64"
fi

# CMake flags for iOS build
CMAKE_FLAGS=(
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DCMAKE_SYSTEM_NAME=iOS"
  "-DCMAKE_OSX_SYSROOT=${IOS_SYSROOT}"
  "-DCMAKE_OSX_ARCHITECTURES=${IOS_ARCH}"
  "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0"
  "-DUSE_METAL=ON"
  "-DBUILD_SERVER=OFF"
  "-Wno-dev"
)

# If generating Xcode project, use Xcode generator
if [ "$GENERATE_XCODE" -eq 1 ]; then
  echo "Generating Xcode project..."
  cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Xcode "${CMAKE_FLAGS[@]}"
  echo ""
  echo "✅ Xcode project generated at: $BUILD_DIR"
  echo ""
  echo "To build:"
  echo "  cd $BUILD_DIR"
  echo "  open idtech3.xcodeproj"
  echo "  # Or build from command line:"
  echo "  xcodebuild -project idtech3.xcodeproj -scheme idtech3 -configuration ${BUILD_TYPE}"
  exit 0
fi

echo "Running CMake configuration..."
echo "  iOS SDK: ${IOS_SYSROOT}"
echo "  Architecture: ${IOS_ARCH}"
echo "  Deployment Target: iOS 13.0"
echo ""

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"

echo "Building with ${CORES} parallel jobs..."
if [ "$QUIET" -eq 1 ]; then
  cmake --build "$BUILD_DIR" -- -j"${CORES}" -s
else
  cmake --build "$BUILD_DIR" -- -j"${CORES}"
fi

echo ""
echo "Build completed. iOS app bundle is in $BUILD_DIR"

# Find the .app bundle
APP_BUNDLE=$(find "$BUILD_DIR" -name "*.app" -type d | head -1)

if [ -n "$APP_BUNDLE" ]; then
  echo ""
  echo "✅ iOS app bundle: $APP_BUNDLE"
  echo ""
  echo "To install on device:"
  echo "  1. Open Xcode"
  echo "  2. Window > Devices and Simulators"
  echo "  3. Select your device"
  echo "  4. Drag and drop the .app bundle"
  echo ""
  echo "Or use xcodebuild to create .ipa:"
  echo "  xcodebuild -exportArchive -archivePath <path> -exportPath <path> -exportOptionsPlist <plist>"
else
  echo ""
  echo "⚠️  Warning: .app bundle not found. Build may have failed or target name differs."
  echo "   Check build output above for errors."
fi

echo ""
echo "For more information, see: docs/ios-macos-platform.md"
