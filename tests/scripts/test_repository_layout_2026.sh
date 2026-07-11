#!/usr/bin/env bash
# Wiring test: 2026 layout — Phase 5c physical roots; Phase 5e dropped src/* shims.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

resolve() {
	readlink -f "$1" 2>/dev/null || realpath "$1"
}

check_physical() {
	local path="$1"
	[ -d "${ROOT}/${path}" ] || fail "missing physical dir ${path}"
	[ ! -L "${ROOT}/${path}" ] || fail "${path} must be a directory, not a symlink"
}

# Layout bridges remain after Phase 5e (relative includes + MSVC).
check_bridge() {
	local shim="$1" expected_suffix="$2"
	[ -L "${ROOT}/${shim}" ] || fail "${shim} must be a layout bridge symlink"
	local target
	target="$(resolve "${ROOT}/${shim}")"
	case "$target" in
		*"${expected_suffix}") ;;
		*) fail "${shim} -> ${target} (expected path ending in ${expected_suffix})" ;;
	esac
}

LAYOUT="${ROOT}/cmake/IdTech3Layout.cmake"
FWD="${ROOT}/scripts/layout_forwarding_symlinks.sh"
DROP="${ROOT}/scripts/migrate_phase_5e_drop_shims.sh"
[ -f "$LAYOUT" ] || fail "missing IdTech3Layout.cmake"
[ -x "$FWD" ] || fail "missing layout_forwarding_symlinks.sh"
[ -x "$DROP" ] || fail "missing migrate_phase_5e_drop_shims.sh"
"$FWD"
rg -q 'IDTECH3_DIR_ENGINE_CORE' "$LAYOUT" || fail "layout cmake missing engine core var"
rg -q 'IDTECH3_DIR_RUNTIME_CLIENT' "$LAYOUT" || fail "layout cmake missing runtime client var"
rg -q 'IDTECH3_DIR_EXTENSIONS' "$LAYOUT" || fail "layout cmake missing extensions var"
rg -q 'NOT IS_SYMLINK' "$LAYOUT" || fail "IdTech3Layout must prefer physical layout dirs (Phase 5c)"

check_physical engine/core
check_physical engine/platform
check_physical runtime/client
check_physical runtime/server
check_physical runtime/game
check_physical modules/world
check_physical modules/navigation
check_physical modules/physics
check_physical modules/audio
check_physical modules/botlib
check_physical runtime/cgame
check_physical runtime/ui
check_physical engine/asm
check_physical extensions
check_physical renderers
check_physical third_party

# Phase 5e: src/* forwarding shims must be gone.
for d in qcommon client server game platform world navigation physics audio botlib \
	cgame ui asm extensions renderers external; do
	[ ! -e "${ROOT}/src/${d}" ] || fail "src/${d} shim must be removed (Phase 5e)"
done
shim_count=$(find "${ROOT}/src" -maxdepth 1 -type l 2>/dev/null | wc -l)
[ "$shim_count" -eq 0 ] || fail "src/ must have no forwarding shims after Phase 5e (found ${shim_count})"
[ -f "${ROOT}/src/README.md" ] || fail "src/README.md missing after Phase 5e"

# Cross-domain layout bridges (kept)
check_bridge runtime/qcommon engine/core
check_bridge runtime/world modules/world
check_bridge modules/qcommon engine/core
check_bridge modules/client runtime/client
check_bridge engine/client runtime/client
check_bridge engine/platform/client runtime/client
check_bridge world modules/world

[ -L "${ROOT}/samples" ] || fail "samples alias symlink missing"
samples_target="$(resolve "${ROOT}/samples")"
case "$samples_target" in
	*examples) ;;
	*) fail "samples -> ${samples_target} (expected examples)" ;;
esac

[ -f "${ROOT}/engine/README.md" ] || fail "engine/README.md missing"
[ -f "${ROOT}/runtime/README.md" ] || fail "runtime/README.md missing"
[ -f "${ROOT}/modules/README.md" ] || fail "modules/README.md missing"

echo "test_repository_layout_2026: passed"
