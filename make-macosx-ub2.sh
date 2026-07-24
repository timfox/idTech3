#!/usr/bin/env bash
#
# Build a Universal 2 (x86_64 + aarch64) Surf.app.
#
# Usage:
#   ./make-macosx-ub2.sh            # build + bundle only
#   ./make-macosx-ub2.sh notarize   # also sign/notarize (soft-fails if no creds)
#
# Notarization uses misc/macos-codesign.sh and these env vars (optional —
# missing credentials soft-fail with exit 2 from the codesign script):
#   MACOS_SIGNING_IDENTITY
#   AC_API_KEY_PATH
#   AC_API_KEY_ID
#   AC_API_ISSUER_ID
#
# Default build uses Makefile PLATFORM=darwin when present; otherwise you
# must pre-build per-arch binaries into build/release-darwin-{x86_64,aarch64}/.

set -euo pipefail

cd "$(dirname "$0")"

if [ ! -f Makefile ] && [ ! -f CMakeLists.txt ]; then
	echo "This script must be run from the idtech3 / Surf engine build directory" >&2
	exit 1
fi

DO_NOTARIZE=0
if [ "${1:-}" = "notarize" ]; then
	DO_NOTARIZE=1
	echo "Notarize requested — will call misc/macos-codesign.sh after the bundle is built."
	echo "(Missing Apple credentials soft-fail; the .app is still produced.)"
else
	echo "Run with a 'notarize' argument to also sign and notarize the bundle."
fi

echo "Building x86_64 + aarch64 universal2 client/server..."
echo

NCPU=4
if command -v sysctl >/dev/null 2>&1; then
	NCPU="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
elif command -v nproc >/dev/null 2>&1; then
	NCPU="$(nproc)"
fi

if [ -f Makefile ]; then
	(PLATFORM=darwin ARCH=x86_64 make -j"$NCPU") || exit 1
	echo; echo
	(PLATFORM=darwin ARCH=aarch64 make -j"$NCPU") || exit 1
	echo
else
	echo "No Makefile — expecting prebuilt binaries in build/release-darwin-{x86_64,aarch64}/"
fi

if [ -d build/release-darwin-universal2 ]; then
	rm -rf build/release-darwin-universal2
fi

./make-macosx-app.sh release || exit 1

APP="build/release-darwin-universal2/Surf.app"
if [ ! -d "$APP" ]; then
	echo "error: expected $APP after make-macosx-app.sh" >&2
	exit 1
fi

if [ "$DO_NOTARIZE" -eq 1 ]; then
	# Soft-fail: macos-codesign.sh exits 2 when credentials are absent.
	set +e
	./misc/macos-codesign.sh "$APP"
	rc=$?
	set -e
	if [ "$rc" -eq 0 ]; then
		echo "Notarization complete."
	elif [ "$rc" -eq 2 ]; then
		echo "Skipped notarization (credentials/tools missing). Bundle left at $APP"
	else
		echo "Notarization failed with exit $rc" >&2
		exit "$rc"
	fi
fi

echo "Done: $APP"
