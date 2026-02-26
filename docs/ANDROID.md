# Android Platform

## Requirements

- Android SDK 35+
- NDK 27+
- CMake 3.22.1+ (via SDK Manager)
- Vulkan 1.1 hardware (required)

## Building

```bash
cd android
./gradlew assembleDebug     # debug APK
./gradlew assembleRelease   # release APK
```

APKs output to `android/app/build/outputs/apk/`.

Or open the `android/` directory in Android Studio.

## Architecture

```
android/
├── build.gradle              Root Gradle project (AGP 8.7.3)
├── settings.gradle
├── gradle.properties
└── app/
    ├── build.gradle          CMake native build config
    └── src/main/
        ├── AndroidManifest.xml   Vulkan 1.1 required
        └── java/com/gopex/idtech3/
            └── GameActivity.java  SDL2 + Vulkan surface
```

## CMake Configuration

The Android build passes these flags to the root CMakeLists.txt:
- `USE_VULKAN=ON` -- Vulkan renderer (primary)
- `USE_RENDERER_DLOPEN=OFF` -- static linking
- `SKIP_SHADER_REGEN=ON` -- use pre-committed shaders
- `BUILD_SERVER=OFF` -- no dedicated server
- `USE_SDL=ON` -- SDL2 for windowing/input
- `USE_OPENAL=OFF` -- Android uses OpenSL ES via SDL
- `USE_CURL=OFF`, `USE_LUA=OFF`, `USE_DUKTAPE=OFF` -- reduced feature set
- `USE_RECAST_NAV=OFF`, `USE_BULLET_PHYSICS=OFF` -- optional

## ABI Targets

- `arm64-v8a` (primary, 64-bit ARM)
- `armeabi-v7a` (legacy 32-bit ARM)

## Game Data

Game data goes in app-specific external storage:
```
/storage/emulated/0/Android/data/com.gopex.idtech3/files/base/
```

## Permissions

- `INTERNET` -- multiplayer networking
- `READ_EXTERNAL_STORAGE` / `WRITE_EXTERNAL_STORAGE` -- game data access
- Vulkan hardware feature required (non-Vulkan devices excluded from Play Store)
