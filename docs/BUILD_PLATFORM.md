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
  - Setup Gradle/NDK project skeleton
  - configure externalNativeBuild with CMake
  - ./gradlew assembleRelease

## Quick tips
- Keep mods in a single `mods/` directory at repo root
- Ensure JAVA_HOME for Android builds points to the correct JDK
- If you switch backends, clean and reconfigure with CMake

