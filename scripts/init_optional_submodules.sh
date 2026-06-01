#!/usr/bin/env bash
# Initialize optional Git submodules (not required to build idtech3).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DO_TILED=0
DO_SVO=0
DO_SYMFORCE=0
DRY_RUN=0

usage() {
	cat <<'EOF'
Usage: init_optional_submodules.sh [--tiled] [--svo] [--symforce] [--all] [--dry-run] [--help]

Initialize optional Git submodules. Idempotent: safe to run twice.

Options:
  --tiled     tools/tiled (Tiled Map Editor, GPL-2.0) — see docs/TILED.md
  --svo       src/external/src/SparseVoxelOctree
  --symforce  external/symforce (SymForce v0.10.1 + Caspar) — see docs/CASPAR.md
  --all       Initialize every optional submodule listed above
  --dry-run   Print commands without running git submodule
  --help      Show this help

Examples:
  ./scripts/init_optional_submodules.sh --tiled
  ./scripts/init_optional_submodules.sh --all
  ./scripts/init_optional_submodules.sh --tiled --dry-run

Error: pass at least one of --tiled, --svo, --symforce, or --all.
EOF
}

while [ $# -gt 0 ]; do
	case "$1" in
		--tiled) DO_TILED=1 ;;
		--svo) DO_SVO=1 ;;
		--symforce) DO_SYMFORCE=1 ;;
		--all) DO_TILED=1; DO_SVO=1; DO_SYMFORCE=1 ;;
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

if [ "$DO_TILED" -eq 0 ] && [ "$DO_SVO" -eq 0 ] && [ "$DO_SYMFORCE" -eq 0 ]; then
	echo "Error: pass at least one of --tiled, --svo, --symforce, or --all." >&2
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
	init_one "src/external/src/SparseVoxelOctree" "SparseVoxelOctree"
fi
if [ "$DO_SYMFORCE" -eq 1 ]; then
	init_one "external/symforce" "SymForce (Caspar)"
	# Shallow submodule clones may not include tag v0.10.1; pin when Caspar tree is missing.
	if [ ! -f "$PROJECT_ROOT/external/symforce/symforce/experimental/caspar/README.md" ]; then
		echo "checkout: external/symforce -> v0.10.1"
		if [ "$DRY_RUN" -eq 1 ]; then
			echo "dry-run: git -C external/symforce fetch --depth 1 origin tag v0.10.1 && git checkout FETCH_HEAD"
		else
			git -C "$PROJECT_ROOT/external/symforce" fetch --depth 1 origin tag v0.10.1
			git -C "$PROJECT_ROOT/external/symforce" checkout FETCH_HEAD
		fi
	fi
fi

echo "optional submodules: finished"
