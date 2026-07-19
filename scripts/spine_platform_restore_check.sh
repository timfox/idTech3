#!/usr/bin/env bash
# Cross-platform Spine restore-hook contract: every input backend + renderer export.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Spine platform restore-hook check ==="

PUB="$ROOT/renderers/common/tr_public.h"
STUB="$ROOT/renderers/common/tr_platform_renderer_stub.c"
VK_INIT="$ROOT/renderers/vulkan/tr_init.c"
PRES="$ROOT/renderers/vulkan/vk_presentation.c"
FRAME="$ROOT/renderers/vulkan/vk_frame_submit.c"

# Renderer export (optional NULL-checked from clients)
grep -q 'NotifyWindowRestored' "$PUB" || fail "refexport NotifyWindowRestored missing"
grep -q 're.NotifyWindowRestored = RE_NotifyWindowRestored' "$VK_INIT" || \
  fail "Vulkan GetRefAPI must wire NotifyWindowRestored"
grep -q 'Stub_NotifyWindowRestored' "$STUB" || fail "platform renderer stub must export NotifyWindowRestored"
grep -q 'vk_presentation_note_window_restored' "$PRES" "$VK_INIT" || \
  fail "presentation restore API must exist and be called from RE_NotifyWindowRestored"
# No CL_HasFocus on refimport (ABI hazard with dlopen / older clients)
if grep -q 'CL_HasFocus' "$PUB"; then
  fail "CL_HasFocus must not be on refimport_t (use re.NotifyWindowRestored from client)"
fi
pass "renderer export + stub; no refimport focus ABI"

# Minimize belt-and-suspenders remains in begin_frame
grep -q 'minimize_to_active' "$FRAME" || fail "begin_frame minimize→active restore missing"
pass "begin_frame minimize restore"

# SDL (primary desktop)
SDL_IN="$ROOT/engine/platform/sdl/sdl_input.c"
SDL_H="$ROOT/engine/platform/sdl/sdl_glw.h"
[[ -f "$SDL_IN" && -f "$SDL_H" ]] || fail "SDL input sources missing"
grep -q 'void IN_NotifyWindowRestored' "$SDL_H" || fail "SDL header missing IN_NotifyWindowRestored"
grep -q 'void IN_NotifyWindowRestored' "$SDL_IN" || fail "SDL implementation missing IN_NotifyWindowRestored"
grep -q 're.NotifyWindowRestored' "$SDL_IN" || fail "SDL FOCUS/RESTORED must call re.NotifyWindowRestored"
grep -q 'IN_NotifyWindowRestored' "$SDL_IN" || fail "SDL FOCUS/RESTORED must call IN_NotifyWindowRestored"
pass "SDL input restore hooks"

# Win32 (MSVC / non-SDL Windows)
WIN_IN="$ROOT/engine/platform/win32/win_input.c"
WIN_H="$ROOT/engine/platform/win32/win_local.h"
WIN_H2="$ROOT/engine/platform/platform/win32/win_local.h"
WIN_WND="$ROOT/engine/platform/win32/win_wndproc.c"
[[ -f "$WIN_IN" && -f "$WIN_H" && -f "$WIN_WND" ]] || fail "Win32 input sources missing"
grep -q 'IN_NotifyWindowRestored' "$WIN_H" || fail "win_local.h missing IN_NotifyWindowRestored"
grep -q 'IN_NotifyWindowRestored' "$WIN_H2" || fail "platform/win32/win_local.h missing IN_NotifyWindowRestored (MSVC bridge)"
grep -q 'void IN_NotifyWindowRestored' "$WIN_IN" || fail "Win32 implementation missing"
grep -q 's_wmv.mouseActive = qfalse' "$WIN_IN" || fail "Win32 restore must force mouse re-activate"
grep -q 're.NotifyWindowRestored' "$WIN_WND" || fail "Win32 WM_ACTIVATE must call re.NotifyWindowRestored"
pass "Win32 input restore hooks"

# Native X11/linux (USE_SDL=OFF)
LIN_H="$ROOT/engine/platform/unix/linux_local.h"
LIN_C="$ROOT/engine/platform/unix/linux_glimp.c"
[[ -f "$LIN_H" && -f "$LIN_C" ]] || fail "linux glimp sources missing"
grep -q 'IN_NotifyWindowRestored' "$LIN_H" || fail "linux_local.h missing IN_NotifyWindowRestored"
grep -q 'void IN_NotifyWindowRestored' "$LIN_C" || fail "linux_glimp.c missing IN_NotifyWindowRestored stub"
pass "Linux/X11 IN_NotifyWindowRestored stub"

echo "=== Spine platform restore-hook check PASSED ==="
