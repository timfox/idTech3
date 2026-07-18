#!/usr/bin/env bash
# Initialize optional Git submodules (not required to build idtech3).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DO_TILED=0
DO_SVO=0
DO_FREEUSD=0
DO_BACKEND=0
DO_EMULATOR=0
DO_BOX3D=0
DO_JOLT=0
DRY_RUN=0

usage() {
	cat <<'EOF'
Usage: init_optional_submodules.sh [--tiled] [--svo] [--box3d] [--jolt] [--all] [--dry-run] [--help]

Initialize optional Git submodules. Idempotent: safe to run twice.

Options:
  --tiled     tools/tiled (Tiled Map Editor, GPL-2.0) — see docs/TILED.md
  --svo       third_party/src/SparseVoxelOctree (legacy: src/external/src/SparseVoxelOctree)
  --freeusd   third_party/FreeUSD (legacy: src/external/FreeUSD) — see docs/FREEUSD.md
  --backend   third_party/idtech3backend (legacy: src/external/idtech3backend) — see docs/IDTECH3_BACKEND.md
  --emulator  third_party/idtech3-emulator (timfox/idTech3-Emulator QEMU fork) — see docs/IDTECH3_EMULATOR.md
  --box3d     third_party/box3d (default physics substrate) — see docs/PHYSICS.md
  --jolt      third_party/JoltPhysics (optional physics substrate) — see docs/PHYSICS.md
  --all       Initialize every optional submodule listed above
  --dry-run   Print commands without running git submodule
  --help      Show this help

Examples:
  ./scripts/init_optional_submodules.sh --tiled
  ./scripts/init_optional_submodules.sh --box3d
  ./scripts/init_optional_submodules.sh --jolt
  ./scripts/init_optional_submodules.sh --all
  ./scripts/init_optional_submodules.sh --tiled --dry-run

Error: pass at least one of --tiled, --svo, --freeusd, --backend, --emulator, --box3d, --jolt, or --all.
EOF
}

while [ $# -gt 0 ]; do
	case "$1" in
		--tiled) DO_TILED=1 ;;
		--svo) DO_SVO=1 ;;
		--freeusd) DO_FREEUSD=1 ;;
		--backend|--idtech3backend) DO_BACKEND=1 ;;
		--emulator|--idtech3-emulator) DO_EMULATOR=1 ;;
		--box3d|--box3D) DO_BOX3D=1 ;;
		--jolt|--JoltPhysics|--joltphysics) DO_JOLT=1 ;;
		--all) DO_TILED=1; DO_SVO=1; DO_FREEUSD=1; DO_BACKEND=1; DO_EMULATOR=1; DO_BOX3D=1; DO_JOLT=1 ;;
		--dry-run) DRY_RUN=1 ;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Error: unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
	shift
done

if [ "$DO_TILED" -eq 0 ] && [ "$DO_SVO" -eq 0 ] && [ "$DO_FREEUSD" -eq 0 ] \
	&& [ "$DO_BACKEND" -eq 0 ] && [ "$DO_EMULATOR" -eq 0 ] && [ "$DO_BOX3D" -eq 0 ] \
	&& [ "$DO_JOLT" -eq 0 ]; then
	echo "Error: pass at least one of --tiled, --svo, --freeusd, --backend, --emulator, --box3d, --jolt, or --all." >&2
	usage >&2
	exit 2
fi

if [ ! -f "$PROJECT_ROOT/.gitmodules" ]; then
	echo "Error: .gitmodules not found (run from engine repo root clone)." >&2
	exit 1
fi

init_one() {
	local path="$1"
	local label="$2"

	if ! grep -qF "path = $path" "$PROJECT_ROOT/.gitmodules" 2>/dev/null; then
		echo "Error: submodule path not in .gitmodules: $path" >&2
		exit 1
	fi

	if git -C "$PROJECT_ROOT" submodule status "$path" 2>/dev/null | grep -q '^ '; then
		echo "ok: $label already initialized ($path)"
		return 0
	fi

	local cmd=( git -C "$PROJECT_ROOT" submodule update --init "$path" )
	if [ "$DRY_RUN" -eq 1 ]; then
		echo "dry-run: ${cmd[*]}"
		return 0
	fi

	echo "init: $label ($path)"
	"${cmd[@]}"
	echo "done: $path"
}

cd "$PROJECT_ROOT"

if [ "$DO_TILED" -eq 1 ]; then
	init_one "tools/tiled" "Tiled Map Editor"
fi
if [ "$DO_SVO" -eq 1 ]; then
	if grep -qF 'path = third_party/src/SparseVoxelOctree' "$PROJECT_ROOT/.gitmodules"; then
		init_one "third_party/src/SparseVoxelOctree" "SparseVoxelOctree"
	else
		init_one "src/external/src/SparseVoxelOctree" "SparseVoxelOctree"
	fi
fi
if [ "$DO_FREEUSD" -eq 1 ]; then
	if grep -qF 'path = third_party/FreeUSD' "$PROJECT_ROOT/.gitmodules"; then
		init_one "third_party/FreeUSD" "FreeUSD (gopexllc/FreeUSD)"
	else
		init_one "src/external/FreeUSD" "FreeUSD (gopexllc/FreeUSD)"
	fi
fi
if [ "$DO_BACKEND" -eq 1 ]; then
	if grep -qF 'path = third_party/idtech3backend' "$PROJECT_ROOT/.gitmodules"; then
		init_one "third_party/idtech3backend" "idTech3 Backend (timfox/idtech3backend)"
	else
		init_one "src/external/idtech3backend" "idTech3 Backend (timfox/idtech3backend)"
	fi
fi
if [ "$DO_EMULATOR" -eq 1 ]; then
	if grep -qF 'path = third_party/idtech3-emulator' "$PROJECT_ROOT/.gitmodules"; then
		init_one "third_party/idtech3-emulator" "idTech3 Emulator (timfox/idTech3-Emulator)"
	else
		init_one "src/external/idtech3-emulator" "idTech3 Emulator (timfox/idTech3-Emulator)"
	fi
fi
if [ "$DO_BOX3D" -eq 1 ]; then
	init_one "third_party/box3d" "Box3D (timfox/idTech3-box3d)"
fi
if [ "$DO_JOLT" -eq 1 ]; then
	init_one "third_party/JoltPhysics" "Jolt Physics (jrouwe/JoltPhysics)"
fi

echo "optional submodules: finished"
