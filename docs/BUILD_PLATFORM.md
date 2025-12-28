# Cross-Platform Build/Run Guide

- Target platforms: Windows, Linux, macOS, Android
- Build system: CMake (with optional Vulkan/OpenGL backends)
- Backends:
  - Vulkan (default on most platforms where available)
  - OpenGL (fallback or explicit choice)
- Android:
  - Use Android Gradle + NDK to build native libs and package as APK
- Run workflow:
  - Linux/macOS: use `scripts/run_engine.sh` or `release/idtech3.x86_64`
  - Windows: use `scripts/run_engine.bat` or packaged exe in `release/`
- MoltenVK:
  - macOS: enable MoltenVK path to run Vulkan on top of Metal

## Prerequisites
- Compiler toolchains for target platform (MSVC on Windows, GCC/Clang on Linux/macOS)
- Vulkan SDK or MoltenVK for Vulkan path when needed
- SDL2 and other engine dependencies (per platform)
- Android Studio/NDK for Android builds

## Basic build steps (typical)
- Linux/macOS:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  - cmake --build build
- Windows:
  - cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  - cmake --build build --config Release
- Android:
 - Android (src path):
   - Overview: Mirror Wolf Android launcher structure under `/src/android-app` to support Android builds for the fork.
   - Structure:
     - `src/android-app/AndroidManifest.xml`
     - `src/android-app/build.gradle`
     - `src/android-app/src/main/java/com/idtech3/MainActivity.java`
     - `src/android-app/src/main/java/com/idtech3/EngineSurfaceRenderer.java`
     - `src/android-app/src/main/cpp/native-lib.cpp`
     - `src/android-app/src/main/cpp/CMakeLists.txt`
     - `src/android-app/assets/mods/` (test mod included)
   - Prerequisites: JDK 11+, Android Studio/SDK/NDK installed
   - Build commands:
     - Debug: `./gradlew :src/android-app:assembleDebug`
     - Release: `./gradlew :src/android-app:assembleRelease`
   - Signing: see `src/android-app/ANDROID_SIGNING.md` for guidance
   - Testing:
     - Install: `adb install -r src/android-app/build/outputs/apk/debug/app-debug.apk` (or release path)
     - Launch: `adb shell am start -n com.idtech3/.MainActivity`
     - Verify: `adb logcat | grep EngineAndroid` for engineInit/engineRender and mod-load logs
   - Mod loading:
     - Assets path: `src/android-app/src/main/assets/mods/testmod.txt`
     - Optional: load mod bytes via `engineLoadModFromBytes` (JNI bridge)
   - ABIs:
     - arm64-v8a, armeabi-v7a, x86_64 enabled via `src/android-app/build.gradle`
   - Documentation: see `src/android-app/ANDROID_SIGNING.md` and `scripts/android_smoke_test.sh`

## Quick tips
- Keep mods in a single `mods/` directory at repo root
- Ensure JAVA_HOME for Android builds points to the correct JDK
- Android (src path) quick-start
Overview: Mirror Wolf Android launcher structure under `/src/android-app` to support Android builds for the fork.
- Prerequisites: JDK 11+, Android Studio/SDK/NDK installed
- Build commands:
  - Debug: `./gradlew :src/android-app:assembleDebug`
  - Release: `./gradlew :src/android-app:assembleRelease`
- Signing: See `src/android-app/ANDROID_SIGNING.md`
- Testing:
  - Install: `adb install -r src/android-app/build/outputs/apk/debug/app-debug.apk`
  - Launch: `adb shell am start -n com.idtech3/.MainActivity`
  - Verify: `adb logcat | grep EngineAndroid`
- ABIs: arm64-v8a, armeabi-v7a, x86_64

