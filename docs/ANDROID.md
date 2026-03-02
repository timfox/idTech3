# Android Platform

## Requirements

- **Java 17** (JDK 17) — AGP 8.7.3 requires Java 11+; Java 17 is recommended. If you see "Java 8 JVM" errors, set `JAVA_HOME` or add `org.gradle.java.home=/path/to/jdk17` to `android/gradle.properties`. In Android Studio: File → Settings → Build → Gradle → Gradle JDK.
- Android SDK 35+
- NDK 27.0.12077973 (or compatible)
- CMake 3.22.1+ (via SDK Manager or system)
- Vulkan 1.1 hardware (recommended; non-Vulkan devices may not run)

## Setup

1. **Install Android SDK and NDK** (one of):
   - Android Studio: SDK Manager → install Android SDK + NDK 27
   - Command line: `sdkmanager "ndk;27.0.12077973" "cmake;3.22.1"`

2. **Set SDK location** (one of):
   - `export ANDROID_HOME=/path/to/Android/Sdk`
   - Or create `android/local.properties` with `sdk.dir=/path/to/Android/Sdk`

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
├── gradlew, gradlew.bat      Gradle wrapper
├── gradle/wrapper/           Wrapper JAR and properties
├── settings.gradle
├── gradle.properties
└── app/
    ├── build.gradle          CMake native build config
    └── src/main/
        ├── AndroidManifest.xml   Vulkan optional
        └── java/com/gopex/idtech3/
            └── GameActivity.java  NativeActivity + Vulkan surface
```

## CMake Configuration

The Android build passes these flags to the root CMakeLists.txt:
- `USE_VULKAN=ON` -- Vulkan renderer (primary)
- `USE_RENDERER_DLOPEN=OFF` -- static linking
- `SKIP_SHADER_REGEN=ON` -- use pre-committed shaders
- `BUILD_SERVER=OFF` -- no dedicated server
- `USE_SDL=OFF` -- NativeActivity + direct Vulkan/input
- `USE_OPENAL=OFF` -- AAudio + OpenSL ES (native Android audio)
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
- `ACCESS_NETWORK_STATE` -- network status
- `VIBRATE` -- haptic feedback
- `RECORD_AUDIO` -- reserved for future VoIP

Game data is stored in app-specific external storage (no storage permission needed).
