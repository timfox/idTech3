#!/usr/bin/env bash
#
# Sign and notarize a macOS .app bundle for Surf.
#
# Usage:
#   ./misc/macos-codesign.sh <path-to-Surf.app>
#
# Environment variables (all optional at commit time — script soft-fails
# with a clear message if any are missing at run time):
#   MACOS_SIGNING_IDENTITY   "Developer ID Application: Your Name (TEAMID)"
#   AC_API_KEY_PATH          Path to an App Store Connect API key (.p8)
#   AC_API_KEY_ID            10-char Key ID from App Store Connect
#   AC_API_ISSUER_ID         Issuer UUID from App Store Connect
#
# Assumes the signing identity is importable from the default keychain
# search list. In CI, set that up first via `security create-keychain` /
# `security import`.
#
# Exit codes:
#   0  signed + notarized + stapled
#   2  soft-fail: credentials or tools missing (safe to skip in CI without secrets)
#   1  hard failure during signing/notarization

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <path-to-Surf.app>" >&2
    exit 1
fi

APP="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENTITLEMENTS="${SCRIPT_DIR}/xcode/surf/surf.entitlements"

# Prefer Surf.app layout; fall back to idtech3 binary names if present.
CLIENT_BIN="idtech3"
DEDICATED_BIN="idtech3_server"
if [ -f "$APP/Contents/MacOS/surf" ]; then
    CLIENT_BIN="surf"
fi
if [ -f "$APP/Contents/MacOS/surf.ded" ]; then
    DEDICATED_BIN="surf.ded"
elif [ -f "$APP/Contents/MacOS/idtech3.ded" ]; then
    DEDICATED_BIN="idtech3.ded"
fi

soft_fail() {
    echo "macos-codesign: $1" >&2
    echo "macos-codesign: soft-fail (exit 2) — Apple credentials are optional; skipping sign/notarize." >&2
    exit 2
}

if [ ! -d "$APP" ]; then
    echo "error: app bundle not found at $APP" >&2
    exit 1
fi
if [ ! -f "$ENTITLEMENTS" ]; then
    echo "error: entitlements file not found at $ENTITLEMENTS" >&2
    exit 1
fi

if [ -z "${MACOS_SIGNING_IDENTITY:-}" ]; then
    soft_fail "MACOS_SIGNING_IDENTITY is not set"
fi
if [ -z "${AC_API_KEY_PATH:-}" ] || [ ! -f "${AC_API_KEY_PATH}" ]; then
    soft_fail "AC_API_KEY_PATH is missing or not a readable .p8 file"
fi
if [ -z "${AC_API_KEY_ID:-}" ]; then
    soft_fail "AC_API_KEY_ID is not set"
fi
if [ -z "${AC_API_ISSUER_ID:-}" ]; then
    soft_fail "AC_API_ISSUER_ID is not set"
fi

if ! command -v codesign >/dev/null 2>&1 || ! command -v xcrun >/dev/null 2>&1; then
    soft_fail "codesign/xcrun not available (macOS + Xcode CLT required)"
fi

echo ">>> Signing $APP"
echo "    identity: $MACOS_SIGNING_IDENTITY"

# Inside-out signing. Order matters: nested code must be signed before the
# outer bundle, so the bundle's signature covers the (signed) children.

# Bundled dylibs: no entitlements — they don't need JIT/library-validation
# relaxations, and applying app entitlements to libraries can cause
# notarization to reject the bundle.
find "$APP/Contents/MacOS" -name '*.dylib' -print0 2>/dev/null | while IFS= read -r -d '' lib; do
    codesign --force --options runtime --timestamp \
        --sign "$MACOS_SIGNING_IDENTITY" "$lib"
done

# Dedicated server: needs the same JIT entitlement as the client because
# it also runs the game VM.
if [ -f "$APP/Contents/MacOS/$DEDICATED_BIN" ]; then
    codesign --force --options runtime --timestamp \
        --entitlements "$ENTITLEMENTS" \
        --sign "$MACOS_SIGNING_IDENTITY" \
        "$APP/Contents/MacOS/$DEDICATED_BIN"
fi

# Main client executable.
if [ -f "$APP/Contents/MacOS/$CLIENT_BIN" ]; then
    codesign --force --options runtime --timestamp \
        --entitlements "$ENTITLEMENTS" \
        --sign "$MACOS_SIGNING_IDENTITY" \
        "$APP/Contents/MacOS/$CLIENT_BIN"
else
    echo "error: client binary not found under $APP/Contents/MacOS/ (looked for $CLIENT_BIN)" >&2
    exit 1
fi

# Outer bundle entitlements are what the kernel applies at launch.
codesign --force --options runtime --timestamp \
    --entitlements "$ENTITLEMENTS" \
    --sign "$MACOS_SIGNING_IDENTITY" \
    "$APP"

codesign --verify --deep --strict --verbose=2 "$APP"

echo ">>> Submitting to Apple notary service"

NOTARIZE_DIR="$(mktemp -d)"
NOTARIZE_ZIP="$NOTARIZE_DIR/notarize.zip"
trap 'rm -rf "$NOTARIZE_DIR"' EXIT

# notarytool wants a flat archive of the .app (or a .dmg / .pkg).
ditto -c -k --sequesterRsrc --keepParent "$APP" "$NOTARIZE_ZIP"

xcrun notarytool submit "$NOTARIZE_ZIP" \
    --key "$AC_API_KEY_PATH" \
    --key-id "$AC_API_KEY_ID" \
    --issuer "$AC_API_ISSUER_ID" \
    --wait

xcrun stapler staple "$APP"
xcrun stapler validate "$APP"

echo ">>> Signed, notarized, stapled: $APP"
