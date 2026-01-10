# Android Build Status

## Current Status: ⚠️ Requires Setup

The Android build system is configured but requires the following to build an APK:

### Prerequisites Missing:
1. **Gradle**: Not installed (can install via `sudo apt install gradle` or `sudo snap install gradle`)
2. **Android SDK**: ANDROID_HOME not set
3. **Gradle Wrapper JAR**: Missing (needs to be downloaded or Gradle installed)

### What's Configured:
- ✅ Android project structure (`platform/android/`)
- ✅ Gradle build files (`build.gradle`, `settings.gradle`)
- ✅ AndroidManifest.xml
- ✅ Native code structure (CMakeLists.txt, native-lib.cpp)
- ✅ Java source files (MainActivity.java, EngineSurfaceRenderer.java)
- ✅ Gradle wrapper script (`gradlew`) - created
- ✅ Gradle wrapper properties file

### To Build APK:

**Option 1: Install Gradle system-wide**
```bash
sudo apt install gradle
# or
sudo snap install gradle
```

**Option 2: Use Android Studio**
- Open `platform/android/` in Android Studio
- Build -> Build Bundle(s) / APK(s) -> Build APK(s)

**Option 3: Download Gradle wrapper JAR manually**
```bash
cd platform/android
curl -L -o gradle/wrapper/gradle-wrapper.jar \
  https://raw.githubusercontent.com/gradle/gradle/v8.0.0/gradle/wrapper/gradle-wrapper.jar
```

**Option 4: Set up Android SDK**
```bash
export ANDROID_HOME=$HOME/Android/Sdk
export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools
```

### Build Command (once prerequisites are met):
```bash
cd platform/android
./gradlew assembleDebug    # For debug APK
./gradlew assembleRelease  # For release APK (requires signing)
```

### APK Output Location:
- Debug: `platform/android/app/build/outputs/apk/debug/app-debug.apk`
- Release: `platform/android/app/build/outputs/apk/release/app-release.apk`
