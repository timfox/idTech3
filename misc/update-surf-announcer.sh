#!/usr/bin/env bash
# Downloads the latest Surf announcer pk3 and removes stale checksum-named
# variants left by auto-download.
#
# Place announcer sounds in zzz-surf-announcer.pk3 (sound/ paths as usual).
# Drop the pk3 into the surf game directory (fs_game), e.g. release/surf/.
#
# Override the download URL with SURF_ANNOUNCER_URL, or set SURF_ANNOUNCER_REPO
# to a GitHub "owner/repo" whose latest release attaches zzz-surf-announcer.pk3.
# Placeholder default (replace when a public release exists):
#   https://github.com/OWNER/surf/releases/latest/download/zzz-surf-announcer.pk3

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Prefer a staged Surf game dir next to the engine; fall back to misc/surf.
DEFAULT_SURF_DIR="$REPO_ROOT/release/surf"
if [ ! -d "$DEFAULT_SURF_DIR" ]; then
  DEFAULT_SURF_DIR="$SCRIPT_DIR/surf"
fi
SURF_DIR="${SURF_GAME_DIR:-$DEFAULT_SURF_DIR}"

ANNOUNCER_PK3="zzz-surf-announcer.pk3"
PLACEHOLDER_REPO="${SURF_ANNOUNCER_REPO:-OWNER/surf}"
DEFAULT_URL="https://github.com/${PLACEHOLDER_REPO}/releases/latest/download/${ANNOUNCER_PK3}"
URL="${SURF_ANNOUNCER_URL:-$DEFAULT_URL}"

mkdir -p "$SURF_DIR"

# Remove checksummed variants (e.g. zzz-surf-announcer.0a1b2c3d.pk3) and the
# current copy, then download fresh.
rm -f "$SURF_DIR"/zzz-surf-announcer.*.pk3 "$SURF_DIR/$ANNOUNCER_PK3"

echo "Downloading Surf announcer from:"
echo "  $URL"
echo "  -> $SURF_DIR/$ANNOUNCER_PK3"

if curl -fL -o "$SURF_DIR/$ANNOUNCER_PK3" "$URL"; then
  echo "  OK"
else
  echo "  FAILED (set SURF_ANNOUNCER_URL to a real download URL)" >&2
  rm -f "$SURF_DIR/$ANNOUNCER_PK3"
  exit 1
fi

echo "Done."
