#!/usr/bin/env bash
# Prepare examples/demo_skeleton for first run: dirs, env template, copy demo pk3 if built.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-$SCRIPT_DIR}"

mkdir -p "$TARGET/base" "$TARGET/idtech3_demo"

if [[ ! -f "$TARGET/base/README.txt" && -f "$SCRIPT_DIR/base/README.txt" ]]; then
	cp "$SCRIPT_DIR/base/README.txt" "$TARGET/base/README.txt"
fi
if [[ ! -f "$TARGET/idtech3_demo/README.txt" && -f "$SCRIPT_DIR/idtech3_demo/README.txt" ]]; then
	cp "$SCRIPT_DIR/idtech3_demo/README.txt" "$TARGET/idtech3_demo/README.txt"
fi

if [[ ! -f "$TARGET/local.env" ]]; then
	cp "$SCRIPT_DIR/demo_skeleton.env.example" "$TARGET/local.env"
	echo "Created $TARGET/local.env (optional — defaults work if you use this folder as-is)"
fi

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_PK3=""
for d in "$REPO_ROOT/build-vk-Release" "$REPO_ROOT/build-gl-Release"; do
	if [[ -f "$d/idtech3_demo.pk3" ]]; then
		BUILD_PK3="$d/idtech3_demo.pk3"
		break
	fi
done
if [[ -n "$BUILD_PK3" && ! -f "$TARGET/idtech3_demo/idtech3_demo.pk3" ]]; then
	cp "$BUILD_PK3" "$TARGET/idtech3_demo/idtech3_demo.pk3"
	echo "Copied idtech3_demo.pk3 from $(dirname "$BUILD_PK3")"
elif [[ ! -f "$TARGET/idtech3_demo/idtech3_demo.pk3" ]]; then
	echo "Tip: run ./examples/demo_game/build_demo_pack.sh then re-run this script to copy idtech3_demo.pk3"
fi

ABS="$(cd "$TARGET" && pwd)"
ENV_LINE="IDTECH3_DEMO_ROOT=$ABS"
TMP_ENV="$(mktemp)"
{
	grep -v '^IDTECH3_DEMO_ROOT=' "$TARGET/local.env" 2>/dev/null || true
	printf '# Playfield root (updated by setup_demo_layout.sh)\n%s\n' "$ENV_LINE"
} > "$TMP_ENV" && mv "$TMP_ENV" "$TARGET/local.env"

echo ""
echo "Playfield ready: $ABS"
echo "  1. (Optional) Add game .pk3 files → $ABS/base/ for maps/menus; bare window works with empty base + rebuilt idtech3_demo.pk3"
echo "  2. Launch (repo root): ./scripts/run_demo.sh"
echo "     or:                ./examples/demo_skeleton/run_demo.sh"
echo ""
