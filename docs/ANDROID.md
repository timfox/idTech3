# Android Platform

## Requirements

- **Java 17** (JDK 17) — AGP 8.7.3 requires Java 11+; Java 17 is recommended. Set **`JAVA_HOME`** to your JDK 17 install (do not rely on a machine-specific `org.gradle.java.home` in `gradle.properties`). In Android Studio: **File → Settings → Build → Gradle → Gradle JDK**.
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

### CI (GitHub Actions)

The **`android`** job in `.github/workflows/build.yml` cross-compiles with CMake using the **same feature toggles** as `android/app/build.gradle` (Lua, Duktape, curl, video flags, FLUX, Recast, Bullet, FreeType, DTLS). First configure in CI may **fetch** FreeType and the Lua tarball like a local Gradle build.

**Local tip:** pass `-DANDROID_DEPS_CACHE=/path/to/dir` to CMake. For **Gradle**, set env **`ANDROID_DEPS_CACHE`** or use **`-PandroidDepsCache=/path`** so `externalNativeBuild` forwards the same flag (reuses Lua / OpenSSL / FreeType downloads).

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

The Android Gradle project passes these flags to the root `CMakeLists.txt` (see `android/app/build.gradle`):
- `USE_VULKAN=ON` -- Vulkan renderer (primary)
- `USE_RENDERER_DLOPEN=OFF` -- static linking
- `SKIP_SHADER_REGEN=ON` -- use pre-committed shaders
- `BUILD_SERVER=OFF` -- no dedicated server (same engine code paths as desktop minus ded binary)
- `USE_SDL=OFF` -- NativeActivity + direct Vulkan/input
- `USE_OPENAL=OFF` -- AAudio + OpenSL ES (native Android audio)
- `USE_LUA=ON`, `USE_DUKTAPE=ON` -- scripting (Lua is built as a static library from upstream source on first configure; requires network once)
- `USE_CURL=ON` -- HTTP client when the NDK provides **libcurl** under the sysroot (if missing, `cl_curl` is omitted with a CMake warning)
- `USE_FFMPEG=ON`, `USE_DAV1D=ON`, `USE_VPX=ON`, `USE_THEORA=ON` -- same toggles as desktop; **pkg-config is usually absent** on Android, so these often disable at configure time unless you add prebuilt codec libraries and CMake hints
- `USE_FLUX=ON` -- FLUX static library (generic backend on Android); **flux_cli** is not built on Android
- `USE_RECAST_NAV=ON`, `USE_BULLET_PHYSICS=ON` -- same as desktop (Bullet full backend still needs a discoverable **libbullet** for the ABI)
- `BUILD_FREETYPE=ON` -- FreeType: uses **FetchContent** from the official FreeType GitHub repo if `find_package(Freetype)` fails (first configure needs network)
- `USE_DTLS=ON` -- uses **find_package(OpenSSL)** when available; otherwise, with **`OPENSSL_ANDROID_AUTOBUILD=ON`** (default), CMake builds **static OpenSSL 3.0.x** via `ExternalProject` (needs **Perl** + **GNU make** on the host, plus `ANDROID_NDK`). Tarballs cache under `ANDROID_DEPS_CACHE` when set. Toggle off to require a prefab install and `OPENSSL_ROOT_DIR`.

**Note:** The APK still does not ship **game** `.pk3` data; use external storage or `apkassets/` as documented below.

## ABI Targets

- `arm64-v8a` (primary, 64-bit ARM)
- `armeabi-v7a` (legacy 32-bit ARM)

## Surface lifecycle (rotation / background)

When the `ANativeWindow` is destroyed (rotation, multi-window, going to background), Vulkan must destroy `VkSurfaceKHR` before the callback returns. The engine synchronizes the NativeActivity thread with the game thread: presentation targets are torn down, the surface is destroyed, and after a new window is created the surface and swapchain are recreated. A startup log line is emitted when the surface is restored (`log_verbosity` ≥ 1).

## Game Data

Game data goes in app-specific external storage:
```
/storage/emulated/0/Android/data/com.gopex.idtech3/files/base/
```

### Bundled APK assets (optional)

To ship read-only content inside the APK, add an `apkassets/` tree under `android/app/src/main/assets/`. Example:

```
android/app/src/main/assets/apkassets/base/pak0.pk3
```

At startup, missing files are copied into `fs_basepath` (the same directory `GameActivity` sets as the data path). Files that already exist on disk are left unchanged. See `android/app/src/main/assets/apkassets/README.txt`.

## Input and console

- **Touch HUD** (Java overlay): **left stick** = `+forward` / `+back` / `+moveleft` / `+moveright`; **right stick** = look (relative mouse deltas). **Buttons** (right column): Menu (`K_ESCAPE`), weapon next (`]` key event), sprint (`+speed`), jump (`+moveup`), fire (`+attack`). Touches **outside** sticks and buttons are passed to the engine (e.g. UI mouse with `K_MOUSE1`). JNI is registered at startup (`GameActivity.nativeRegisterTouchOverlayJni`); native side pumps the queue each frame (`Android_TouchOverlay_PumpEvents`).
- **Raw touch** (full-screen): drag still sends **relative** mouse motion; primary tap uses `K_MOUSE1`. Sensitivity: **`com_androidTouchSens`** (default `1.0`, archived).
- **Logcat**: `Com_Printf` output is mirrored to Android logcat (tag `idTech3`) with Q3 color codes stripped, in addition to `Sys_Print`.
- **Focus**: **`gw_active`** is cleared on activity pause and set on resume so unfocused behavior (e.g. `com_maxfpsUnfocused`) matches the app lifecycle.

## Permissions

- `INTERNET` -- multiplayer networking
- `ACCESS_NETWORK_STATE` -- network status
- `VIBRATE` -- haptic feedback
- `RECORD_AUDIO` -- reserved for future VoIP

Game data is stored in app-specific external storage (no storage permission needed).
