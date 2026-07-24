#!/usr/bin/env bash
#
# Build a Surf.app bundle from per-arch darwin binaries.
#
# Usage:
#   ./make-macosx-app.sh <release|debug> [arch]
#
# Looks for binaries under build/<target>-darwin-<arch>/ named:
#   idtech3[.arch] / idtech3_server[.arch]
#   or surf[.arch] / surf.ded[.arch]
#
# Bundle identity: com.surf.engine (Surf.app).
# Optional assets: set SURF_ASSETS_DIR to a directory containing
# zzz-surf-announcer.pk3 (copied into Contents/Resources/surf/).
#
# Ad-hoc codesigns when codesign is available (required on Apple Silicon).
# Developer ID + notarization: use misc/macos-codesign.sh separately.

set -euo pipefail

if [ $# -eq 0 ] || [ $# -gt 2 ]; then
	echo "Usage:   $0 target <arch>"
	echo "Example: $0 release x86_64"
	echo "Valid targets: release | debug"
	echo "Optional architectures: x86 | x86_64 | ppc | aarch64"
	exit 1
fi

TARGET_NAME=""
case "$1" in
	release|debug) TARGET_NAME="$1" ;;
	*)
		echo "Invalid target: $1 (expected release or debug)" >&2
		exit 1
		;;
esac

CURRENT_ARCH="${2:-}"
case "$CURRENT_ARCH" in
	""|x86|x86_64|ppc|aarch64) ;;
	*)
		echo "Invalid architecture: $CURRENT_ARCH" >&2
		exit 1
		;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Prefer repo root (script may live in misc/ or repo root).
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ] || [ -f "$SCRIPT_DIR/Makefile" ]; then
	REPO_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ] || [ -f "$SCRIPT_DIR/../Makefile" ]; then
	REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
	REPO_ROOT="$SCRIPT_DIR"
fi
cd "$REPO_ROOT"

SEARCH_ARCHS="x86 x86_64 ppc aarch64"
HAS_LIPO="$(command -v lipo || true)"
HAS_CP="$(command -v cp || true)"

if [ -z "$HAS_LIPO" ] && [ -z "$CURRENT_ARCH" ]; then
	CURRENT_ARCH="$(uname -m)"
	[ "$CURRENT_ARCH" = "i386" ] && CURRENT_ARCH="x86"
	echo "$0 cannot make a universal binary, falling back to architecture ${CURRENT_ARCH}"
fi
if [ -n "$CURRENT_ARCH" ]; then
	SEARCH_ARCHS="$CURRENT_ARCH"
fi

SURF_VERSION="${SURF_VERSION:-1.0}"
PRODUCT_NAME="${SURF_PRODUCT_NAME:-idtech3}"
DEDICATED_NAME="${SURF_DEDICATED_NAME:-idtech3_server}"
BUNDLE_NAME="Surf"
WRAPPER_NAME="${BUNDLE_NAME}.app"
PKGINFO="APPLSURF"
ICNSDIR="${SURF_ICNS_DIR:-misc/macos}"
ICNS="${SURF_ICNS:-Surf.icns}"
# Fall back to common engine icon names if Surf.icns is absent.
OBJROOT="${SURF_OBJROOT:-build}"
CONTENTS_FOLDER_PATH="${WRAPPER_NAME}/Contents"
UNLOCALIZED_RESOURCES_FOLDER_PATH="${CONTENTS_FOLDER_PATH}/Resources"
EXECUTABLE_FOLDER_PATH="${CONTENTS_FOLDER_PATH}/MacOS"

CLIENT_ARCHS=""
SERVER_ARCHS=""
VALID_ARCHS=""

for ARCH in $SEARCH_ARCHS; do
	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${ARCH}"
	[ -d "$BUILT_PRODUCTS_DIR" ] || continue

	CLIENT=""
	for candidate in \
		"${PRODUCT_NAME}.${ARCH}" "${PRODUCT_NAME}" \
		"surf.${ARCH}" "surf" \
		"idtech3.${ARCH}" "idtech3"
	do
		if [ -e "${BUILT_PRODUCTS_DIR}/${candidate}" ]; then
			CLIENT="${BUILT_PRODUCTS_DIR}/${candidate}"
			PRODUCT_NAME="$(basename "$CLIENT" | sed "s/\\.${ARCH}\$//")"
			break
		fi
	done
	[ -n "$CLIENT" ] || continue

	CLIENT_ARCHS="${CLIENT} ${CLIENT_ARCHS}"
	VALID_ARCHS="${ARCH} ${VALID_ARCHS}"

	for candidate in \
		"${DEDICATED_NAME}.${ARCH}" "${DEDICATED_NAME}" \
		"surf.ded.${ARCH}" "surf.ded" \
		"idtech3_server.${ARCH}" "idtech3_server" \
		"idtech3.ded.${ARCH}" "idtech3.ded"
	do
		if [ -e "${BUILT_PRODUCTS_DIR}/${candidate}" ]; then
			SERVER_ARCHS="${BUILT_PRODUCTS_DIR}/${candidate} ${SERVER_ARCHS}"
			DEDICATED_NAME="$(basename "${BUILT_PRODUCTS_DIR}/${candidate}" | sed "s/\\.${ARCH}\$//")"
			break
		fi
	done
done

if [ -z "$CLIENT_ARCHS" ]; then
	echo "$0: no client binaries found for target '${TARGET_NAME}' under ${OBJROOT}/" >&2
	echo "  expected e.g. ${OBJROOT}/${TARGET_NAME}-darwin-aarch64/${PRODUCT_NAME}" >&2
	exit 1
fi

if [ -z "${2:-}" ]; then
	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-universal2"
	mkdir -p "$BUILT_PRODUCTS_DIR"
else
	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${CURRENT_ARCH}"
fi

BUNDLEBINDIR="${BUILT_PRODUCTS_DIR}/${EXECUTABLE_FOLDER_PATH}"
mkdir -p "$BUNDLEBINDIR"
mkdir -p "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"

echo "Creating bundle '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}'"
echo "with architectures: ${VALID_ARCHS}"
echo "  client:    ${PRODUCT_NAME}"
echo "  dedicated: ${DEDICATED_NAME}"

# Optional bundled libs (best-effort; layouts differ across forks).
for libdir in code/libsdl/macosx code/libopenal/macosx code/libvulkan/macosx \
              third_party/sdl/macosx external/sdl/macosx; do
	if [ -d "$libdir" ]; then
		shopt -s nullglob
		libs=("$libdir"/*.dylib)
		shopt -u nullglob
		if [ ${#libs[@]} -gt 0 ]; then
			cp "${libs[@]}" "$BUNDLEBINDIR/"
		fi
	fi
done

# Icon (optional).
ICON_COPIED=0
for icns_candidate in \
	"${ICNSDIR}/${ICNS}" \
	"misc/macos/Surf.icns" \
	"misc/macos/icon.icns" \
	"code/unix/quake3_flat.icns"
do
	if [ -f "$icns_candidate" ]; then
		cp "$icns_candidate" "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}/$(basename "$icns_candidate")"
		ICNS="$(basename "$icns_candidate")"
		ICON_COPIED=1
		break
	fi
done

echo -n "$PKGINFO" > "${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/PkgInfo"

DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ICON_KEY="${ICNS%.icns}"
[ "$ICON_COPIED" -eq 0 ] && ICON_KEY=""

cat > "${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${PRODUCT_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>${ICON_KEY}</string>
    <key>CFBundleIdentifier</key>
    <string>com.surf.engine</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${BUNDLE_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${SURF_VERSION}</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>${SURF_VERSION}</string>
    <key>CGDisableCoalescedUpdates</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>${DEPLOYMENT_TARGET}</string>
    <key>NSHumanReadableCopyright</key>
    <string>Surf — based on Quake III Arena / id Tech 3. Copyright © id Software, Inc. and contributors.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSHighResolutionCapable</key>
    <false/>
    <key>NSRequiresAquaSystemAppearance</key>
    <false/>
</dict>
</plist>
EOF

action() {
	local dest="$1"
	shift
	if [ -n "$HAS_LIPO" ]; then
		# shellcheck disable=SC2086
		"$HAS_LIPO" -create -o "$dest" "$@"
	elif [ -n "$HAS_CP" ]; then
		"$HAS_CP" "$1" "$dest"
	else
		echo "$0 cannot create an application bundle (need lipo or cp)." >&2
		exit 1
	fi
}

# shellcheck disable=SC2086
action "${BUNDLEBINDIR}/${PRODUCT_NAME}" ${CLIENT_ARCHS}
if [ -n "$SERVER_ARCHS" ]; then
	# shellcheck disable=SC2086
	action "${BUNDLEBINDIR}/${DEDICATED_NAME}" ${SERVER_ARCHS}
fi

# Per-arch renderer dylibs (keep distinct; never lipo).
for ARCH in $VALID_ARCHS; do
	SRC_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${ARCH}"
	shopt -s nullglob
	for dll in "${SRC_DIR}"/${PRODUCT_NAME}_*_${ARCH}.dylib \
	           "${SRC_DIR}"/idtech3_*_${ARCH}.dylib \
	           "${SRC_DIR}"/idtech3_vulkan.so \
	           "${SRC_DIR}"/*.dylib; do
		[ -f "$dll" ] || continue
		cp "$dll" "${BUNDLEBINDIR}/"
		if command -v install_name_tool >/dev/null 2>&1; then
			install_name_tool -id "@rpath/$(basename "$dll")" \
				"${BUNDLEBINDIR}/$(basename "$dll")" 2>/dev/null || true
		fi
	done
	shopt -u nullglob
done

# Optionally bundle Surf announcer (and other paks) under Resources/surf/.
# Paks must NOT live under Contents/MacOS/ — Sequoia+ codesign treats files
# there as nested code. Resources/ files are hashed as resources.
if [ -n "${SURF_ASSETS_DIR:-}" ]; then
	if [ ! -d "$SURF_ASSETS_DIR" ]; then
		echo "**** ERROR: SURF_ASSETS_DIR set to '${SURF_ASSETS_DIR}' but directory does not exist" >&2
		exit 1
	fi
	mkdir -p "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}/surf"
	for pak in zzz-surf-announcer.pk3; do
		if [ -f "${SURF_ASSETS_DIR}/${pak}" ]; then
			cp "${SURF_ASSETS_DIR}/${pak}" \
				"${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}/surf/${pak}"
			echo "Bundled Resources/surf/${pak}"
		else
			echo "**** WARNING: ${pak} not found in SURF_ASSETS_DIR"
		fi
	done
fi

# Ad-hoc sign (Hardened Runtime belongs in macos-codesign.sh with Developer ID).
if command -v codesign >/dev/null 2>&1; then
	BUNDLE="${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}"
	find "${BUNDLE}/Contents/MacOS" -type f \( -name '*.dylib' -o -perm +111 \) \
		-exec codesign --force --sign - {} \; 2>/dev/null || true
	codesign --force --sign - "${BUNDLE}" || true
fi

echo "Created ${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}"
