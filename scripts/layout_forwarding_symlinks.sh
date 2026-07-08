#!/usr/bin/env bash
# Cross-domain forwarding symlinks for Phase 5c relative #include paths.
# Complements src/* shims (migrate_phase_5c.sh). Safe to re-run.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

link() {
	local path="$1" target="$2"
	if [ -e "$path" ] || [ -L "$path" ]; then
		rm -rf "$path"
	fi
	ln -sfn "$target" "$path"
}

# MSVC-era copies under engine/platform/; canonical trees live elsewhere after 5c.
replace_stale_platform_dir() {
	local name="$1" rel_target="$2"
	local path="engine/platform/${name}"
	if [ -d "$path" ] && [ ! -L "$path" ]; then
		rm -rf "$path"
	fi
	if [ ! -e "$path" ]; then
		mkdir -p engine/platform
		link "$path" "$rel_target"
	fi
}

echo "[layout_fwd] runtime/ cross-links..."
link runtime/qcommon ../engine/core
link runtime/world ../modules/world
link runtime/physics ../modules/physics
link runtime/navigation ../modules/navigation
link runtime/audio ../modules/audio
link runtime/renderers ../renderers
link runtime/extensions ../extensions
link runtime/external ../third_party
link runtime/platform ../engine/platform
link runtime/vuda extensions/research/vuda

echo "[layout_fwd] modules/ cross-links..."
link modules/qcommon ../engine/core
link modules/client ../runtime/client
link modules/game ../runtime/game
link modules/external ../third_party
link modules/renderers ../renderers

echo "[layout_fwd] repo-root links (deep renderer includes)..."
link world modules/world
link external third_party
link vuda extensions/research/vuda
link qcommon engine/core

echo "[layout_fwd] engine/platform stale duplicates -> canonical..."
replace_stale_platform_dir botlib ../../modules/botlib
replace_stale_platform_dir cgame ../../runtime/cgame
replace_stale_platform_dir asm ../asm

# MSVC vcxproj ClCompile paths use ../../foo from win32/msvc2017 -> engine/platform/foo
echo "[layout_fwd] MSVC bridge (engine/platform/*)..."
mkdir -p engine/platform
link engine/platform/client ../../runtime/client
link engine/platform/server ../../runtime/server
link engine/platform/game ../../runtime/game
link engine/platform/ui ../../runtime/ui
link engine/platform/qcommon ../core
link engine/platform/audio ../../modules/audio
link engine/platform/renderers ../../renderers
link engine/platform/external ../../third_party

# ../../../foo from msvc2017 -> engine/foo (AdditionalIncludeDirectories)
echo "[layout_fwd] MSVC bridge (engine/*)..."
link engine/qcommon core
# platform/sdl, platform/unix, platform/win32 use ../../client (engine/client), not platform/client.
link engine/client ../runtime/client
link engine/server ../runtime/server
link engine/botlib ../modules/botlib
link engine/physics ../modules/physics
link engine/navigation ../modules/navigation
link engine/world ../modules/world
link engine/external ../third_party
link engine/renderers ../renderers

echo "[layout_fwd] done"
