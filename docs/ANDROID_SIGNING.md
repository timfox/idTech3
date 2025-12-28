Android signing for the /src/android-app module

- Prerequisites
  - Java Development Kit (JDK) 11 or newer
  - Android Studio/SDK and Android Gradle Plugin configured
  - Keystore management: keep keystore outside the repo in a secure location

- Generate a release keystore (example)
  - Command:
    ```
    keytool -genkeypair -v -keystore release.keystore -alias idtech3 -keyalg RSA -keysize 2048 -validity 10000
    ```
  - Store the resulting release.keystore securely, e.g., in a private keystore directory.

- Configure Gradle signing in /src/android-app/build.gradle
  - Add a signingConfigs.release block and reference it in the release buildType.
  - Example (do not commit real credentials):
    ```
    signingConfigs {
      release {
        storeFile file("$rootDir/keystore/release.keystore")
        storePassword "changeit"
        keyAlias "idtech3"
        keyPassword "changeit"
      }
    }

    buildTypes {
      release {
        signingConfig signingConfigs.release
        minifyEnabled false
        proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
      }
    }
    ```

- Verification
  - Build: `./gradlew :src/android-app:assembleRelease`
  - Sign: if you supply a keystore, the release APK will be signed automatically
  - Install: use adb to push the APK and run on a device

### APK signing verification (CI-friendly)
- After signing in CI, verify the produced APK with `apksigner` to ensure the signature is valid.
- Local verification:
  - Install the APK and run the app, verify EngineAndroid logs
- Quick local command:
  - `bash scripts/verify_apksigner.sh src/android-app/build/outputs/apk/release/app-release.apk`
- Notes
  - Do not commit the keystore to the repository
  - If you want to sign via CI, provide the keystore securely via environment or secret store

## Environment-based signing workflow (recommended for CI)
- Purpose: avoid storing credentials in the repository; use environment variables or CI secrets.
- Prerequisites: same as above (JDK 11+, Android Studio/SDK/NDK)
- Sign config (Gradle)
  - Replace the signing block in `src/android-app/build.gradle` with:
```gradle
signingConfigs {
  release {
    def keystorePath = System.getenv("RELEASE_STORE_FILE")
    if (keystorePath != null) {
      storeFile file(keystorePath)
      def storePassword = System.getenv("RELEASE_STORE_PASSWORD")
      def keyAlias = System.getenv("RELEASE_KEY_ALIAS")
      def keyPassword = System.getenv("RELEASE_KEY_PASSWORD")
      if (storePassword != null) storePassword storePassword
      if (keyAlias != null) keyAlias keyAlias
      if (keyPassword != null) keyPassword keyPassword
    } else {
      throw new GradleException("Release keystore not configured. Set RELEASE_STORE_FILE.")
    }
  }
}
```
- Build locally with a release key
-CI integration: set RELEASE_STORE_FILE, RELEASE_STORE_PASSWORD, RELEASE_KEY_ALIAS, RELEASE_KEY_PASSWORD as secrets, then run the release assemble:
```
./gradlew :src/android-app:assembleRelease
```
- Validation: install and verify EngineAndroid logs as in the quick-start above.

## CI workflow (example)
- This repo supports environment-based signing for /src Android app to enable CI/CD signing without storing credentials in the repo.
- Use the following GitHub Actions snippet to sign and build a release APK, then upload as an artifact.
- Prerequisites: secrets RELEASE_STORE_FILE, RELEASE_STORE_PASSWORD, RELEASE_KEY_ALIAS, RELEASE_KEY_PASSWORD configured in the CI environment.
- Snippet:
```yaml
name: Android Sign & Build (Release)
on:
  push:
    branches: [ main ]

jobs:
  build-release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v3
        with:
          distribution: 'temurin'
          java-version: '11'
      - name: Build Release
        env:
          RELEASE_STORE_FILE: ${{ secrets.RELEASE_STORE_FILE }}
          RELEASE_STORE_PASSWORD: ${{ secrets.RELEASE_STORE_PASSWORD }}
          RELEASE_KEY_ALIAS: ${{ secrets.RELEASE_KEY_ALIAS }}
          RELEASE_KEY_PASSWORD: ${{ secrets.RELEASE_KEY_PASSWORD }}
        run: |
          ./gradlew :src/android-app:assembleRelease
      - name: Upload APK
        if: ${{ success() }}
        uses: actions/upload-artifact@v3
        with:
          name: app-release-apk
          path: src/android-app/build/outputs/apk/release/app-release.apk
```
- Verification: download the APK from artifacts, install on a test device/emulator, and verify EngineAndroid logs with logcat.