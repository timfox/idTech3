#!/bin/bash

# idTech3 Android Build Script with Mod Name Configuration
# Usage: ./build_android.sh [mod_name] [build_type]
#   mod_name: Name of the mod/game (default: idTech3)
#   build_type: Debug/Release (default: Release)

set -e

# Default values
MOD_NAME="${1:-idTech3}"
BUILD_TYPE="${2:-Release}"

echo "Building idTech3 Android with mod: $MOD_NAME ($BUILD_TYPE)"

# Convert mod name to proper format
MOD_NAME_LOWER=$(echo "$MOD_NAME" | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9]//g')
MOD_NAME_UPPER=$(echo "$MOD_NAME" | tr '[:lower:]' '[:upper:]')
MOD_PACKAGE_NAME="com.${MOD_NAME_LOWER}.game"

# Update CMake cache if it exists
if [ -f "build/CMakeCache.txt" ]; then
    echo "Updating existing CMake configuration..."
    sed -i "s/ENGINE_NAME:STRING=.*/ENGINE_NAME:STRING=$MOD_NAME/" build/CMakeCache.txt
    sed -i "s/APP_DISPLAY_NAME:STRING=.*/APP_DISPLAY_NAME:STRING=$MOD_NAME/" build/CMakeCache.txt
    sed -i "s/APP_PACKAGE_NAME:STRING=.*/APP_PACKAGE_NAME:STRING=$MOD_PACKAGE_NAME/" build/CMakeCache.txt
fi

# Configure build with mod-specific settings
echo "Configuring build for $MOD_NAME..."
cmake -S . -B build \
    -DMOD_NAME="$MOD_NAME" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DUSE_ANDROID=ON \
    -DUSE_VULKAN=ON

# Build the project
echo "Building $MOD_NAME for Android..."
cmake --build build --config "$BUILD_TYPE"

# Update Android manifest with mod-specific information
if [ -d "android" ]; then
    echo "Updating Android manifest for $MOD_NAME..."

    # Update package name
    sed -i "s/package=\"[^\"]*\"/package=\"$MOD_PACKAGE_NAME\"/" android/AndroidManifest.xml

    # Update activity class name if needed
    sed -i "s/android:name=\"\.[^\"]*\"/android:name=\".${MOD_NAME}Activity\"/" android/AndroidManifest.xml

    # Update display name
    sed -i "s/android:label=\"@string\/app_name\"/android:label=\"$MOD_NAME\"/" android/AndroidManifest.xml

    # Rename Java activity class
    if [ -f "android/IdTech3Activity.java" ]; then
        mv "android/IdTech3Activity.java" "android/${MOD_NAME}Activity.java"

        # Update class name in Java file
        sed -i "s/package com\.idtech3\.engine;/package $MOD_PACKAGE_NAME;/" "android/${MOD_NAME}Activity.java"
        sed -i "s/public class IdTech3Activity/public class ${MOD_NAME}Activity/" "android/${MOD_NAME}Activity.java"
    fi
fi

echo "Build completed for $MOD_NAME!"
echo "APK package: $MOD_PACKAGE_NAME"
echo "Display name: $MOD_NAME"
echo ""
echo "To install on device:"
echo "  adb install build/idtech3_vulkan_arm64-v8a.apk"
echo ""
echo "To run:"
echo "  adb shell am start -n $MOD_PACKAGE_NAME/.${MOD_NAME}Activity"
