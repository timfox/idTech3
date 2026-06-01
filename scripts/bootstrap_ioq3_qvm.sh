#!/usr/bin/env bash
# Fetch ioquake3 and build qagame.qvm for renderer devdata (one-time bootstrap).
# Usage: ./scripts/bootstrap_ioq3_qvm.sh
# Then: ./scripts/build_renderer_devdata.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IOQ3_DIR="${IOQ3_DIR:-/tmp/ioq3-check}"
IOQ3_BUILD="$IOQ3_DIR/build"

if [ -f "$IOQ3_BUILD/Release/baseq3/vm/qagame.qvm" ]; then
	echo "qagame.qvm already present: $IOQ3_BUILD/Release/baseq3/vm/qagame.qvm"
	exit 0
fi

if ! command -v git >/dev/null 2>&1; then
	echo "Error: git required" >&2
	exit 2
fi

if [ ! -d "$IOQ3_DIR/.git" ]; then
	echo "Cloning ioquake3 into $IOQ3_DIR (shallow)..."
	git clone --depth 1 https://github.com/ioquake/ioq3.git "$IOQ3_DIR"
fi

echo "Configuring ioquake3 (BUILD_GAME_QVMS=ON)..."
mkdir -p "$IOQ3_BUILD"
cmake -S "$IOQ3_DIR" -B "$IOQ3_BUILD" \
	-DCMAKE_BUILD_TYPE=Release \
	-DBUILD_GAME_QVMS=ON \
	-DBUILD_CLIENT=OFF \
	-DBUILD_SERVER=OFF

echo "Building qagame.qvm..."
cmake --build "$IOQ3_BUILD" --target qagame.qvm -j"$(nproc 2>/dev/null || echo 4)"

if [ ! -f "$IOQ3_BUILD/Release/baseq3/vm/qagame.qvm" ]; then
	echo "Error: build did not produce qagame.qvm" >&2
	exit 1
fi

echo "OK: $IOQ3_BUILD/Release/baseq3/vm/qagame.qvm"
echo "Next: IOQ3_BUILD=$IOQ3_BUILD $PROJECT_ROOT/scripts/build_renderer_devdata.sh"
