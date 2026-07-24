#!/usr/bin/env bash
#
# Pack a signed/notarized Surf.app into a drag-to-Applications DMG.
#
# Usage:
#   ./misc/macos-make-dmg.sh <path-to-Surf.app> <output-dmg-path>
#
# Optional assets under misc/macos/ (script works without them):
#   dmg-readme.rtf      — included as "Read Me Before Playing.rtf" when present
#   dmg-background.png  — Finder window background when present
#   volume-icon.icns    — DMG / volume icon when present
#
# No Apple credentials required.

set -euo pipefail

if [ $# -ne 2 ]; then
  echo "Usage: $0 <path-to-Surf.app> <output-dmg-path>" >&2
  exit 1
fi

APP="$1"
OUT_DMG="$2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
README_SOURCE="${SCRIPT_DIR}/macos/dmg-readme.rtf"
VOLUME_ICON="${SCRIPT_DIR}/macos/volume-icon.icns"
BACKGROUND="${SCRIPT_DIR}/macos/dmg-background.png"
VOLUME_NAME="Surf"
APP_BASENAME="$(basename "$APP")"

if [ ! -d "$APP" ]; then
  echo "error: app bundle not found: $APP" >&2
  exit 1
fi

STAGE_DIR="$(mktemp -d)"
RW_DMG="$(mktemp -u).dmg"
SET_ICON_SWIFT="$(mktemp).swift"
trap 'rm -rf "$STAGE_DIR"; rm -f "$RW_DMG" "$SET_ICON_SWIFT"' EXIT

# ditto, not cp -R: bundle xattrs (incl. signature metadata) survive ditto.
ditto "$APP" "$STAGE_DIR/$APP_BASENAME"
ln -s /Applications "$STAGE_DIR/Applications"

HAS_README=0
if [ -f "$README_SOURCE" ]; then
  cp "$README_SOURCE" "$STAGE_DIR/Read Me Before Playing.rtf"
  HAS_README=1
fi

HAS_BACKGROUND=0
if [ -f "$BACKGROUND" ]; then
  mkdir -p "$STAGE_DIR/.background"
  cp "$BACKGROUND" "$STAGE_DIR/.background/background.png"
  HAS_BACKGROUND=1
fi

HAS_VOLUME_ICON=0
if [ -f "$VOLUME_ICON" ]; then
  HAS_VOLUME_ICON=1
fi

echo ">>> Creating writable DMG"
hdiutil create \
  -srcfolder "$STAGE_DIR" \
  -volname "$VOLUME_NAME" \
  -fs HFS+ \
  -format UDRW \
  -size 300m \
  -ov \
  "$RW_DMG"

echo ">>> Mounting writable DMG"
MOUNT_INFO="$(hdiutil attach -readwrite -noverify -noautoopen "$RW_DMG")"
DEVICE="$(echo "$MOUNT_INFO" | grep -E '^/dev/' | head -1 | awk '{print $1}')"
MOUNT_PATH="/Volumes/$VOLUME_NAME"

if [ ! -d "$MOUNT_PATH" ]; then
  echo "error: DMG mounted but $MOUNT_PATH does not exist" >&2
  [ -n "$DEVICE" ] && hdiutil detach "$DEVICE" >/dev/null 2>&1 || true
  exit 1
fi

echo ">>> Laying out Finder window"
osascript <<APPLESCRIPT
tell application "Finder"
    tell disk "$VOLUME_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {200, 100, 900, 585}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 160
        if $HAS_BACKGROUND is 1 then
            try
                set background picture of viewOptions to file ".background:background.png"
            end try
        end if
        set position of item "$APP_BASENAME" of container window to {200, 0}
        set position of item "Applications" of container window to {500, 0}
        if $HAS_README is 1 then
            try
                set position of item "Read Me Before Playing.rtf" of container window to {350, 210}
            end try
        end if
        try
            set position of item ".background" of container window to {1500, 100}
        end try
        try
            set position of item ".VolumeIcon.icns" of container window to {1500, 300}
        end try
        close
        open
        update without registering applications
        delay 2
    end tell
end tell
APPLESCRIPT

echo ">>> Removing macOS housekeeping dotfiles"
rm -rf "$MOUNT_PATH/.fseventsd" "$MOUNT_PATH/.Trashes" "$MOUNT_PATH/.Spotlight-V100"

if [ "$HAS_VOLUME_ICON" -eq 1 ]; then
  cat >"$SET_ICON_SWIFT" <<'SWIFT'
import AppKit
let args = CommandLine.arguments
guard args.count == 3, let icon = NSImage(contentsOfFile: args[1]) else {
    FileHandle.standardError.write("set-icon: failed to load icon\n".data(using: .utf8)!)
    exit(1)
}
exit(NSWorkspace.shared.setIcon(icon, forFile: args[2], options: []) ? 0 : 1)
SWIFT

  echo ">>> Setting volume icon"
  cp "$VOLUME_ICON" "$MOUNT_PATH/.VolumeIcon.icns"
  if command -v SetFile >/dev/null 2>&1; then
    SetFile -a C "$MOUNT_PATH"
  else
    xattr -wx com.apple.FinderInfo \
      "0000000000000000040000000000000000000000000000000000000000000000" \
      "$MOUNT_PATH"
  fi
  swift "$SET_ICON_SWIFT" "$VOLUME_ICON" "$MOUNT_PATH" || \
    echo "warning: NSWorkspace.setIcon failed on volume root"
fi

echo ">>> Unmounting writable DMG"
sync
hdiutil detach "$DEVICE"

echo ">>> Compressing to final DMG"
hdiutil convert "$RW_DMG" \
  -format UDZO \
  -imagekey zlib-level=9 \
  -ov \
  -o "$OUT_DMG"

if [ "$HAS_VOLUME_ICON" -eq 1 ]; then
  echo ">>> Embedding icon into .dmg file"
  swift "$SET_ICON_SWIFT" "$VOLUME_ICON" "$OUT_DMG" || \
    echo "warning: NSWorkspace.setIcon failed on .dmg file"
fi

echo ">>> DMG created: $OUT_DMG"
