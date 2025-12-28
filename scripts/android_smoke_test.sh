#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODULE_PATH="src/android-app"
APK_PATH="$ROOT_DIR/src/android-app/build/outputs/apk/debug/app-debug.apk"

echo "Starting Android smoke test for /src android-app..."

# Build debug APK if not present
if [ ! -f "$APK_PATH" ]; then
  echo "Debug APK not found, building..."
  if [ -f "$ROOT_DIR/gradlew" ]; then
    (cd "$ROOT_DIR" && ./gradlew :src/android-app:assembleDebug)
  elif command -v gradle >/dev/null 2>&1; then
    (cd "$ROOT_DIR" && gradle :src/android-app:assembleDebug)
  else
    echo "Gradle wrapper or system Gradle not found. Install Gradle." >&2
    exit 1
  fi
fi

if [ ! -f "$APK_PATH" ]; then
  echo "APK not found after build. Exiting." >&2
  exit 1
fi

echo "Installing APK on connected device..."
adb install -r "$APK_PATH" || { echo "Install failed"; exit 1; }

echo "Launching app..."
adb shell am start -n com.idtech3/.MainActivity

echo "Waiting for engineInit/engineRender logs..."
sleep 2
adb logcat -d | grep -E "EngineAndroid|engineInit|engineRender" || true

echo "Smoke test complete. Review logcat above for engineInit/engineRender entries."
