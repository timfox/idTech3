# Android Build Guide with Vulkan Support

## Overview
This guide explains how to build id Tech 3 for Android with Vulkan renderer support and generate an APK.

## Prerequisites

### Required Software
1. **Android Studio** (recommended) or **Android NDK** (standalone)
   - Minimum NDK version: r21 or newer
   - Recommended: NDK r25 or newer
   - Download from: https://developer.android.com/ndk/downloads

2. **Java Development Kit (JDK)**
   - JDK 11 or newer (JDK 17 recommended)
   - Required for Gradle builds

3. **Gradle**
   - Included with Android Studio
   - Or install standalone from: https://gradle.org/

4. **Android SDK**
   - Included with Android Studio
   - Or install via command line tools

### Environment Variables
Set one of the following:
```bash
export ANDROID_NDK=/path/to/android-ndk-r25c
# OR
export ANDROID_HOME=/path/to/android-sdk
# OR
export ANDROID_NDK_HOME=/path/to/android-ndk-r25c
```

## Build Methods

### Method 1: Using Gradle (Recommended for APK)

This method uses the Android Gradle plugin to build a complete APK.

```bash
cd platform/android-app
./build_apk.sh Release
```

Or manually:
```bash
cd platform/android-app
./gradlew assembleRelease
```

The APK will be in `platform/android-app/build/outputs/apk/release/`

### Method 2: Using CMake Directly

This method builds the native libraries, which can then be packaged manually.

```bash
# Configure build
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID=ON \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DUSE_VULKAN=ON \
  -DUSE_RENDERER_DLOPEN=ON \
  -DRENDERER_DEFAULT=vulkan \
  -DUSE_ANDROID=ON

# Build
cmake --build build-android
```

This produces:
- `libidtech3.so` - Main engine library
- `libidtech3_vulkan.so` - Vulkan renderer library
- Other native libraries as needed

## Vulkan Requirements

### Minimum Android Version
- **Android 7.0 (API 24)** - Minimum for Vulkan 1.0 support
- **Android 8.0 (API 26)** - Recommended for better Vulkan support
- The build system sets `minSdkVersion 24` by default

### Device Requirements
- Device must support Vulkan 1.0 or higher
- GPU drivers must include Vulkan support
- Most modern Android devices (2017+) support Vulkan

### Checking Vulkan Support
```bash
# On device
adb shell dumpsys | grep -i vulkan
# Or check via app
adb shell pm list features | grep vulkan
```

## Build Configuration

### CMake Options for Android

| Option | Default | Description |
|--------|---------|-------------|
| `ANDROID` | Auto-detected | Enable Android build |
| `USE_VULKAN` | ON | Enable Vulkan renderer |
| `USE_RENDERER_DLOPEN` | ON | Build renderers as separate libraries |
| `RENDERER_DEFAULT` | vulkan | Default renderer (forced to vulkan on Android) |
| `ANDROID_PLATFORM` | android-24 | Minimum API level |
| `ANDROID_ABI` | arm64-v8a | Target architecture |

### Gradle Configuration

The `build.gradle` file is configured to:
- Use API 24 as minimum SDK (Vulkan requirement)
- Build for arm64-v8a, armeabi-v7a, and x86_64
- Link Vulkan library from Android NDK
- Package all native libraries in the APK

## APK Structure

The generated APK contains:
```
app-release.apk
├── lib/
│   ├── arm64-v8a/
│   │   ├── libidtech3.so
│   │   ├── libidtech3_vulkan.so
│   │   └── libnative-lib.so
│   ├── armeabi-v7a/
│   │   └── (same structure)
│   └── x86_64/
│       └── (same structure)
├── assets/
│   └── mods/
└── AndroidManifest.xml
```

## Installation and Testing

### Install APK
```bash
adb install platform/android-app/build/outputs/apk/release/app-release.apk
```

### Run Application
```bash
adb shell am start -n com.idtech3/.MainActivity
```

### Check Logs
```bash
adb logcat | grep -i "idtech3\|vulkan\|engine"
```

### Verify Vulkan
```bash
adb logcat | grep -i "vulkan.*initialized\|vulkan.*found"
```

## Troubleshooting

### Vulkan Not Found
**Symptom:** App crashes or falls back to OpenGL

**Solutions:**
1. Check device Vulkan support: `adb shell dumpsys | grep vulkan`
2. Verify minSdkVersion is 24 or higher
3. Check that Vulkan library is linked: `nm -D libidtech3_vulkan.so | grep vulkan`
4. Review logcat for Vulkan initialization errors

### Build Failures

**CMake can't find Android NDK:**
```bash
export ANDROID_NDK=/path/to/ndk
# Or
export ANDROID_HOME=/path/to/sdk
```

**Gradle build fails:**
- Ensure JDK 11+ is installed and `JAVA_HOME` is set
- Check that `ANDROID_NDK` or `ANDROID_HOME` is set
- Verify Gradle version compatibility (8.1.4+)

**Native library not found:**
- Ensure `USE_RENDERER_DLOPEN=ON` (required for Android)
- Check that all renderer libraries are built
- Verify library names match what the app expects

### Performance Issues

**Low frame rate:**
- Ensure device supports Vulkan (not all devices do)
- Check GPU driver version (update if needed)
- Reduce render quality settings in-game
- Monitor with `adb shell dumpsys gfxinfo`

**Memory issues:**
- Reduce texture quality
- Lower render resolution
- Check memory usage: `adb shell dumpsys meminfo com.idtech3`

## Advanced Configuration

### Custom NDK Version
```bash
export ANDROID_NDK=/path/to/android-ndk-r25c
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  ...
```

### Multiple ABIs
Edit `build.gradle`:
```gradle
ndk {
    abiFilters "arm64-v8a", "armeabi-v7a", "x86_64"
}
```

### Debug Build
```bash
cd platform/android-app
./gradlew assembleDebug
```

### Signing APK
Create `keystore.properties`:
```properties
storeFile=/path/to/keystore.jks
storePassword=your_password
keyAlias=your_alias
keyPassword=your_key_password
```

Then build:
```bash
./gradlew assembleRelease -Pkeystore.properties=keystore.properties
```

## CI/CD Integration

### GitHub Actions Example
```yaml
- name: Build Android APK
  run: |
    export ANDROID_NDK=${{ secrets.ANDROID_NDK_PATH }}
    cd platform/android-app
    ./gradlew assembleRelease
```

### Signing in CI
```yaml
- name: Sign APK
  run: |
    jarsigner -verbose -sigalg SHA256withRSA -digestalg SHA-256 \
      -keystore keystore.jks \
      -storepass ${{ secrets.KEYSTORE_PASSWORD }} \
      app-release-unsigned.apk \
      your_key_alias
```

## References

- [Android NDK Documentation](https://developer.android.com/ndk)
- [Vulkan on Android](https://developer.android.com/ndk/guides/graphics/getting-started)
- [Gradle Plugin Documentation](https://developer.android.com/studio/build)
- [CMake Android Toolchain](https://developer.android.com/ndk/guides/cmake)
