#!/usr/bin/env bash
# Create base/, idtech3_demo/, copy local.env example; optionally symlink pk3 from build dir.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-$SCRIPT_DIR}"

mkdir -p "$TARGET/base" "$TARGET/idtech3_demo"

if [[ ! -f "$TARGET/local.env" ]]; then
	cp "$SCRIPT_DIR/demo_skeleton.env.example" "$TARGET/local.env"
	echo "Wrote $TARGET/local.env — set IDTECH3_DEMO_ROOT=$(cd "$TARGET" && pwd)"
fi

BUILD_PK3=""
for d in "$SCRIPT_DIR/../../build-vk-Release" "$SCRIPT_DIR/../../build-gl-Release"; do
	if [[ -f "$d/idtech3_demo.pk3" ]]; then
		BUILD_PK3="$d/idtech3_demo.pk3"
		break
	fi
done
if [[ -n "$BUILD_PK3" && ! -f "$TARGET/idtech3_demo/idtech3_demo.pk3" ]]; then
	cp "$BUILD_PK3" "$TARGET/idtech3_demo/idtech3_demo.pk3"
	echo "Copied idtech3_demo.pk3 from $(dirname "$BUILD_PK3")"
elif [[ ! -f "$TARGET/idtech3_demo/idtech3_demo.pk3" ]]; then
	echo "No idtech3_demo.pk3 yet — run: ./examples/demo_game/build_demo_pack.sh"
fi

echo "Layout ready under $TARGET"
echo "Next: install game .pk3 files into $TARGET/base/ then run ./run_demo_client.sh (with local.env in examples/demo_skeleton or export IDTECH3_DEMO_ROOT)"
