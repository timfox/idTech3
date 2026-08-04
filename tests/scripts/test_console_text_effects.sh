#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CONSOLE="$ROOT/runtime/client/shell/cl_console.c"
KEYS="$ROOT/runtime/client/core/cl_keys.c"

rg -q 'con_textEffects' "$CONSOLE"
rg -q 'Con_RuneColorTag' "$CONSOLE"
rg -q 'Con_RunePositionTag' "$CONSOLE"
rg -q 'CON_EFFECT_RANDOM_COLOR' "$CONSOLE"
rg -q 'Con_TextEffectColorAt' "$CONSOLE" "$KEYS"
rg -q '@red@' "$ROOT/docs/CONSOLE_TEXT_EFFECTS.md"
rg -q '~ddd~' "$ROOT/docs/CONSOLE_TEXT_EFFECTS.md"

echo "console text effects: PASS"
