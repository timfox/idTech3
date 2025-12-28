#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODULE_PATH="src/android-app"
MODE="${1:-debug}"
APK_PATH="$ROOT_DIR/src/android-app/build/outputs/apk/${MODE}/app-${MODE}.apk"

echo "Starting Android signing & smoke flow for /src/android-app..."

if [ -f "$APK_PATH" ]; then
  echo "Release APK already built: $APK_PATH"
else
  # Build according to mode
  if [ "$MODE" = "release" ]; then
    TASK=":src/android-app:assembleRelease"
  else
    TASK=":src/android-app:assembleDebug"
  fi
  if [ -f "$ROOT_DIR/gradlew" ]; then
    (cd "$ROOT_DIR" && ./gradlew "$TASK")
  else
    echo "Gradle wrapper not found; install Gradle or run locally with a wrapper." >&2
    exit 1
  fi
fi
APK_PATH_FINAL="$ROOT_DIR/src/android-app/build/outputs/apk/${MODE}/app-${MODE}.apk"
APK_PATH="$APK_PATH_FINAL"
echo "Installing APK on connected device..."
adb install -r "$APK_PATH" || { echo "Install failed"; exit 1; }

echo "Launching app..."
adb shell am start -n com.idtech3/.MainActivity

echo "Waiting for engineInit/engineRender logs..."
sleep 3
adb logcat -d | grep -E "EngineAndroid|engineInit|engineRender|engineLoadMod" || true

echo "Smoke test complete. Review logcat above for engineInit/engineRender and mod-load entries."

