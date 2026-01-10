# Android Build Status

## Summary

**Status**: ⚠️ **Cannot build APK - Java version incompatible**

The Android project is configured but cannot build an APK because:

### Current Issue:
- **Java Version**: System has Java 8 (OpenJDK 1.8.0_472)
- **Required**: Java 11+ (Android Gradle Plugin 7.4+ and 8.0+ require Java 11)
- **Android SDK**: Not configured (ANDROID_HOME not set)

### What's Working:
✅ Android project structure (`platform/android/`)
✅ Gradle wrapper script (`gradlew`) - created and functional
✅ Gradle wrapper JAR - downloaded successfully
✅ Gradle 8.0 - downloads and runs correctly
✅ Build configuration files (build.gradle, settings.gradle)
✅ Java source files and native code structure

### What's Missing:
❌ Java 11 or higher
❌ Android SDK (ANDROID_HOME environment variable)
❌ Android build tools

### To Build APK:

**Step 1: Install Java 11+**
```bash
sudo apt install openjdk-11-jdk
# or
sudo apt install openjdk-17-jdk
```

**Step 2: Set JAVA_HOME**
```bash
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
# or for Java 17:
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

**Step 3: Install Android SDK**
```bash
# Option A: Install via Android Studio
# Download from https://developer.android.com/studio
# Install and set:
export ANDROID_HOME=$HOME/Android/Sdk
export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools

# Option B: Install command-line tools only
mkdir -p $HOME/Android/Sdk
cd $HOME/Android/Sdk
wget https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
unzip commandlinetools-linux-9477386_latest.zip
mkdir -p cmdline-tools/latest
mv cmdline-tools/* cmdline-tools/latest/ 2>/dev/null || true
export ANDROID_HOME=$HOME/Android/Sdk
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools

# Accept licenses and install SDK
yes | sdkmanager --licenses
sdkmanager "platform-tools" "platforms;android-33" "build-tools;33.0.0"
```

**Step 4: Build APK**
```bash
cd platform/android
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64  # or java-17
export ANDROID_HOME=$HOME/Android/Sdk
./gradlew assembleDebug    # Debug APK
./gradlew assembleRelease  # Release APK (requires signing)
```

**Step 5: Find APK**
```bash
# Debug APK location:
platform/android/app/build/outputs/apk/debug/app-debug.apk

# Release APK location:
platform/android/app/build/outputs/apk/release/app-release.apk
```

### Alternative: Use Android Studio
1. Install Android Studio
2. Open `platform/android/` directory
3. Let Android Studio sync Gradle and download dependencies
4. Build -> Build Bundle(s) / APK(s) -> Build APK(s)

### Project Structure:
```
platform/android/
├── app/
│   ├── build.gradle
│   └── src/
│       ├── main/
│       │   ├── AndroidManifest.xml
│       │   ├── java/com/idtech3/
│       │   │   ├── MainActivity.java
│       │   │   └── EngineSurfaceRenderer.java
│       │   └── cpp/
│       │       ├── CMakeLists.txt
│       │       └── native-lib.cpp
├── build.gradle
├── settings.gradle
├── gradlew (executable)
└── gradle/wrapper/
    ├── gradle-wrapper.jar
    └── gradle-wrapper.properties
```

### Current Configuration:
- **Gradle Version**: 8.0
- **Android Gradle Plugin**: 7.4.2 (compatible with Java 11+)
- **Compile SDK**: 33
- **Min SDK**: 21
- **Target SDK**: 33
- **ABIs**: arm64-v8a, armeabi-v7a, x86_64

### Next Steps:
1. Install Java 11 or 17
2. Install Android SDK
3. Run `./gradlew assembleDebug` to build APK
