#!/usr/bin/env bash
set -euo pipefail

APK_PATH="$1"
if [ -z "$APK_PATH" ]; then
  echo "Usage: verify_apksigner.sh <apk_path>"
  exit 2
fi

if ! command -v apksigner >/dev/null 2>&1; then
  echo "apksigner not found in PATH. Ensure Android SDK build-tools are installed." >&2
  exit 3
fi

echo "Verifying APK signature: $APK_PATH"
apksigner verify "$APK_PATH"
echo "Signature verification completed."
