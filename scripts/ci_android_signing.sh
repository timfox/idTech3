#!/usr/bin/env bash
set -euo pipefail

echo "Android signing CI check (minimal) started..."

MODULE_DIR="src/android-app"
BUILD_GRADLE="$MODULE_DIR/build.gradle"

if [ ! -d "$MODULE_DIR" ]; then
  echo "WARN: Android app module not found at $MODULE_DIR; skipping signing check."
  exit 0
fi

if [ ! -f "$BUILD_GRADLE" ]; then
  echo "ERROR: Expected Gradle build file not found: $BUILD_GRADLE"
  exit 1
fi

HAS_SIGNING_CONFIG=$(grep -E -c "signingConfigs|storeFile|storePassword|keyAlias|keyPassword" "$BUILD_GRADLE" || true)
if [ "$HAS_SIGNING_CONFIG" -eq 0 ]; then
  echo "ERROR: No signingConfigs/release configuration found in $BUILD_GRADLE"
  exit 1
fi

RELEASE_STORE_FILE="${RELEASE_STORE_FILE:-}"
RELEASE_STORE_PASSWORD="${RELEASE_STORE_PASSWORD:-}"
RELEASE_KEY_ALIAS="${RELEASE_KEY_ALIAS:-}"
RELEASE_KEY_PASSWORD="${RELEASE_KEY_PASSWORD:-}"

# If CI signing environment variables are provided, validate their presence
if [ -n "$RELEASE_STORE_FILE" ] || [ -n "$RELEASE_STORE_PASSWORD" ] || [ -n "$RELEASE_KEY_ALIAS" ] || [ -n "$RELEASE_KEY_PASSWORD" ]; then
  MISSING=0
  if [ -z "$RELEASE_STORE_FILE" ] || [ ! -f "$RELEASE_STORE_FILE" ]; then
    echo "ERROR: RELEASE_STORE_FILE not set or file not found: $RELEASE_STORE_FILE"
    MISSING=1
  fi
  if [ -z "$RELEASE_STORE_PASSWORD" ]; then
    echo "ERROR: RELEASE_STORE_PASSWORD not set"
    MISSING=1
  fi
  if [ -z "$RELEASE_KEY_ALIAS" ]; then
    echo "ERROR: RELEASE_KEY_ALIAS not set"
    MISSING=1
  fi
  if [ -z "$RELEASE_KEY_PASSWORD" ]; then
    echo "ERROR: RELEASE_KEY_PASSWORD not set"
    MISSING=1
  fi
  if [ "$MISSING" -ne 0 ]; then
    exit 1
  fi
  echo "CI signing config appears present. Keystore path: ${RELEASE_STORE_FILE}"
fi

echo "Android signing CI check passed (minimal)."
exit 0

