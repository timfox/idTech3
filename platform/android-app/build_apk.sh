#!/bin/bash
# Build Android APK with Vulkan support
# Usage: ./build_apk.sh [build_type]
#   build_type: Debug or Release (default: Release)

set -e

BUILD_TYPE="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "Building Android APK with Vulkan support (${BUILD_TYPE})"
echo "Project root: ${PROJECT_ROOT}"

# Check for Java 17+ (required for AGP 8.x)
JAVA_VERSION=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2 | cut -d'.' -f1)
if [ "$JAVA_VERSION" -lt 17 ]; then
    echo "ERROR: Java 17+ is required for Android Gradle Plugin 8.x"
    echo "Current Java version: $(java -version 2>&1 | head -n 1)"
    echo ""
    echo "Please run: ./setup_java.sh"
    echo "Or install Java 17+: sudo apt install openjdk-17-jdk"
    exit 1
fi

# Check for required environment variables
if [ -z "$ANDROID_NDK" ] && [ -z "$ANDROID_HOME" ] && [ -z "$ANDROID_NDK_HOME" ]; then
    echo "ERROR: ANDROID_NDK, ANDROID_HOME, or ANDROID_NDK_HOME must be set"
    exit 1
fi

# Set ANDROID_NDK if not set
if [ -z "$ANDROID_NDK" ]; then
    if [ -n "$ANDROID_NDK_HOME" ]; then
        export ANDROID_NDK="$ANDROID_NDK_HOME"
    elif [ -n "$ANDROID_HOME" ]; then
        # Try to find the latest NDK version
        NDK_PATH="$ANDROID_HOME/ndk"
        if [ -d "$NDK_PATH" ]; then
            LATEST_NDK=$(ls -1 "$NDK_PATH" | sort -V | tail -1)
            export ANDROID_NDK="$NDK_PATH/$LATEST_NDK"
        fi
    fi
fi

if [ -z "$ANDROID_NDK" ]; then
    echo "ERROR: Could not determine ANDROID_NDK path"
    exit 1
fi

echo "Using Android NDK: $ANDROID_NDK"

# Check for Gradle
if ! command -v ./gradlew &> /dev/null && ! command -v gradle &> /dev/null; then
    echo "ERROR: Gradle not found. Please install Gradle or use the Gradle wrapper."
    exit 1
fi

# Change to android-app directory
cd "${SCRIPT_DIR}"

# Clean previous build
echo "Cleaning previous build..."
if [ -f "gradlew" ]; then
    ./gradlew clean
else
    gradle clean
fi

# Build APK
echo "Building APK..."
BUILD_TYPE_LOWER=$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')
if [ -f "gradlew" ]; then
    ./gradlew "assemble${BUILD_TYPE}" -Pandroid.injected.build.api=24
else
    gradle "assemble${BUILD_TYPE}" -Pandroid.injected.build.api=24
fi

# Find the output APK
APK_PATH=""
if [ -d "build/outputs/apk/${BUILD_TYPE,,}" ]; then
    APK_PATH="build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk"
elif [ -d "build/outputs/apk" ]; then
    APK_PATH=$(find build/outputs/apk -name "*.apk" | head -1)
fi

if [ -n "$APK_PATH" ] && [ -f "$APK_PATH" ]; then
    echo ""
    echo "=========================================="
    echo "APK built successfully!"
    echo "Location: ${SCRIPT_DIR}/${APK_PATH}"
    echo "=========================================="
    echo ""
    echo "To install on device:"
    echo "  adb install ${APK_PATH}"
    echo ""
    echo "To run:"
    echo "  adb shell am start -n com.idtech3/.MainActivity"
else
    echo "WARNING: APK not found in expected location"
    echo "Searching for APK files..."
    find build -name "*.apk" 2>/dev/null || echo "No APK files found"
    exit 1
fi
