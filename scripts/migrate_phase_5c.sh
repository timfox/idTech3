#!/usr/bin/env bash
# Phase 5c: physical move off src/ with one-release src/* forwarding shims.
# Idempotent guard: refuses to run if engine/core is already a directory with sources.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ -d engine/core ] && [ ! -L engine/core ] && [ -f engine/core/common.c ]; then
	echo "migrate_phase_5c: engine/core already populated — skip physical move"
	"${ROOT}/scripts/layout_forwarding_symlinks.sh"
	exit 0
fi

echo "[5c] removing top-level layout symlinks (untracked aliases)..."
rm -f engine/core engine/platform
rm -f runtime/client runtime/server runtime/game
rm -f modules/world modules/navigation modules/physics modules/audio
rm -f extensions renderers

mkdir -p engine runtime modules

echo "[5c] git mv domains to 2026 layout roots..."
git mv src/qcommon engine/core
git mv src/platform engine/platform
git mv src/client runtime/client
git mv src/server runtime/server
git mv src/game runtime/game
git mv src/world modules/world
git mv src/navigation modules/navigation
git mv src/physics modules/physics
git mv src/audio modules/audio
git mv src/extensions extensions
git mv src/renderers renderers

echo "[5c] third_party physical tree..."
git rm -f third_party
git mv src/external third_party

echo "[5c] remaining src islands -> layout roots..."
if [ -d src/botlib ] && [ ! -L src/botlib ]; then
	git mv src/botlib modules/botlib
fi
if [ -d src/cgame ] && [ ! -L src/cgame ]; then
	rm -f runtime/cgame
	git mv src/cgame runtime/cgame
fi
if [ -d src/ui ] && [ ! -L src/ui ]; then
	rm -f runtime/ui
	git mv src/ui runtime/ui
fi
if [ -d src/asm ] && [ ! -L src/asm ]; then
	git mv src/asm engine/asm
fi

echo "[5c] clean stale symlinks inside engine/platform..."
for link in audio cgame client external game qcommon renderers server ui; do
	[ -L "engine/platform/${link}" ] && rm -f "engine/platform/${link}"
done

echo "[5c] src/* forwarding shims (one release)..."
ln -sfn ../engine/core src/qcommon
ln -sfn ../engine/platform src/platform
ln -sfn ../runtime/client src/client
ln -sfn ../runtime/server src/server
ln -sfn ../runtime/game src/game
ln -sfn ../modules/world src/world
ln -sfn ../modules/navigation src/navigation
ln -sfn ../modules/physics src/physics
ln -sfn ../modules/audio src/audio
ln -sfn ../modules/botlib src/botlib
ln -sfn ../runtime/cgame src/cgame
ln -sfn ../runtime/ui src/ui
ln -sfn ../engine/asm src/asm
ln -sfn ../extensions src/extensions
ln -sfn ../renderers src/renderers
ln -sfn ../third_party src/external

git add src/ 2>/dev/null || true

"${ROOT}/scripts/layout_forwarding_symlinks.sh"

echo "[5c] physical move complete"
