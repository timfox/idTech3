# Android Build Guide with Vulkan Support

## Quick Start

### Prerequisites
1. **Android NDK** (r27.0.12077973 or newer)
   - Set `ANDROID_NDK`, `ANDROID_HOME`, or `ANDROID_NDK_HOME` environment variable
2. **JDK 17+** (REQUIRED for AGP 8.x - JDK 8 will not work)
   - `export JAVA_HOME=/path/to/jdk-17`
3. **Gradle** (8.4+ - included with wrapper, no manual installation needed)

### Build APK

```bash
cd platform/android-app
./build_apk.sh Release
```

Or manually:
```bash
cd platform/android-app
./gradlew assembleRelease
```

The APK will be in `build/outputs/apk/release/app-release.apk`

### Install and Run

```bash
# Install
adb install build/outputs/apk/release/app-release.apk

# Run
adb shell am start -n com.idtech3/.MainActivity

# View logs
adb logcat | grep -i "idtech3\|vulkan"
```

## Build Configuration

### CMake Configuration
The build uses the main `CMakeLists.txt` with Android-specific settings:
- **Minimum SDK**: 24 (Android 7.0) - required for Vulkan
- **Target SDK**: 35 (Android 15)
- **ABIs**: arm64-v8a, armeabi-v7a, x86_64
- **Renderer**: Vulkan (default on Android)
- **STL**: c++_static
- **JDK**: 17+ (REQUIRED - AGP 8.x does not support JDK 8)

#### Modern Tooling Requirements
This build now uses **Android Gradle Plugin 8.6.1** and **Gradle 8.10.2**:
- **AGP 8.x** requires **JDK 17+**
- **Constitution Compliant**: Uses modern compiler standards
- **Security**: Latest patches and features
- **Performance**: Improved build times and optimizations

### Vulkan Support
- Vulkan headers and library are provided by Android NDK
- Automatically linked via `-lvulkan`
- Requires Android 7.0+ (API 24+)
- Device must support Vulkan 1.0+

## Project Structure

```
platform/android-app/
├── build.gradle              # Gradle build configuration
├── settings.gradle           # Gradle settings
├── build_apk.sh              # Build script
├── src/
│   ├── main/
│   │   ├── AndroidManifest.xml  # App manifest with Vulkan permissions
│   │   ├── java/                # Java/Kotlin code
│   │   ├── cpp/                 # Native code (JNI bridge)
│   │   └── assets/              # Game assets
└── gradle/
    └── wrapper/              # Gradle wrapper
```

## Native Libraries

The build produces:
- `libidtech3.so` - Main engine library
- `libidtech3_vulkan.so` - Vulkan renderer
- `libnative-lib.so` - JNI bridge

All libraries are automatically packaged into the APK by Gradle.

## Troubleshooting

### Vulkan Not Found
- Check device support: `adb shell dumpsys | grep vulkan`
- Verify minSdkVersion is 24+
- Check logcat for Vulkan initialization errors

### Build Failures
- Ensure `ANDROID_NDK` or `ANDROID_HOME` is set
- Check JDK version: `java -version` (should be 11+)
- Verify Gradle version compatibility

### APK Not Generated
- Check build output: `./gradlew assembleRelease --info`
- Verify CMake configuration succeeded
- Check for missing dependencies

## Advanced

### Custom NDK Version
```bash
export ANDROID_NDK=/path/to/android-ndk-r25c
cd platform/android-app
./gradlew assembleRelease
```

### Debug Build
```bash
./gradlew assembleDebug
```

### Sign APK
Create `keystore.properties` and set environment variables:
```bash
export RELEASE_STORE_FILE=/path/to/keystore.jks
export RELEASE_STORE_PASSWORD=password
export RELEASE_KEY_ALIAS=alias
export RELEASE_KEY_PASSWORD=password
./gradlew assembleRelease
```
