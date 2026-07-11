#!/usr/bin/env bash
# Phase 5e: remove src/* one-release forwarding shims.
# Keeps layout bridges (runtime/qcommon, engine/platform/*, etc.) — relative
# #include paths and MSVC vcxproj still need those until a later include rewrite.
#
# Usage:
#   ./scripts/migrate_phase_5e_drop_shims.sh           # dry-run
#   ./scripts/migrate_phase_5e_drop_shims.sh --apply   # remove tracked src/* shims
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

APPLY=0
if [[ "${1:-}" == "--apply" ]]; then
	APPLY=1
elif [[ "${1:-}" == "--dry-run" ]] || [[ -z "${1:-}" ]]; then
	APPLY=0
else
	echo "usage: $0 [--dry-run|--apply]" >&2
	exit 2
fi

SHIMS=(
	src/asm
	src/audio
	src/botlib
	src/cgame
	src/client
	src/extensions
	src/external
	src/game
	src/navigation
	src/physics
	src/platform
	src/qcommon
	src/renderers
	src/server
	src/ui
	src/world
)

echo "[5e] Phase 5e src/* shim drop ($([[ "$APPLY" -eq 1 ]] && echo apply || echo dry-run))"

missing=0
for s in "${SHIMS[@]}"; do
	if [ -L "$s" ]; then
		target="$(readlink "$s")"
		echo "  remove symlink $s -> $target"
	elif [ -e "$s" ]; then
		echo "  WARN: $s exists but is not a symlink — leave untouched" >&2
		missing=$((missing + 1))
	else
		echo "  already absent: $s"
	fi
done

if [[ "$APPLY" -eq 0 ]]; then
	echo "[5e] dry-run only (pass --apply to remove)"
	exit 0
fi

# Prefer git rm for tracked symlinks so the index stays clean.
tracked=()
for s in "${SHIMS[@]}"; do
	if git ls-files --error-unmatch "$s" >/dev/null 2>&1; then
		tracked+=("$s")
	elif [ -L "$s" ] || [ -e "$s" ]; then
		rm -f "$s"
	fi
done
if [[ "${#tracked[@]}" -gt 0 ]]; then
	git rm -f "${tracked[@]}"
fi

mkdir -p src
cat > src/README.md <<'EOF'
# `src/` (removed forwarding shims)

Phase 5e dropped the one-release `src/*` symlinks that pointed at the 2026
canonical roots. Use these paths instead:

| Old shim | Canonical |
|----------|-----------|
| `src/qcommon` | `engine/core` |
| `src/platform` | `engine/platform` |
| `src/client` | `runtime/client` |
| `src/server` | `runtime/server` |
| `src/game` | `runtime/game` |
| `src/cgame` | `runtime/cgame` |
| `src/ui` | `runtime/ui` |
| `src/world` | `modules/world` |
| `src/navigation` | `modules/navigation` |
| `src/physics` | `modules/physics` |
| `src/audio` | `modules/audio` |
| `src/botlib` | `modules/botlib` |
| `src/asm` | `engine/asm` |
| `src/extensions` | `extensions` |
| `src/renderers` | `renderers` |
| `src/external` | `third_party` |

Cross-domain layout bridges (`runtime/qcommon`, `engine/platform/client`, …)
remain for relative `#include` and MSVC path resolution — see
`scripts/layout_forwarding_symlinks.sh` and `docs/core/SHIM_REMOVAL_CHECKLIST.md`.
EOF

# Ensure layout bridges still exist; do not recreate src/* shims.
"${ROOT}/scripts/layout_forwarding_symlinks.sh"

echo "[5e] done — src/* forwarding shims removed"
