#!/usr/bin/env bash
# Wiring test: Studio chrome parity (menus, dock, Animation open flag, docs).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

HDR="${ROOT}/renderers/vulkan/inspector/vk_imgui.h"
CHROME="${ROOT}/renderers/vulkan/inspector/vk_imgui_chrome_panels.cpp"
STUDIO="${ROOT}/renderers/vulkan/inspector/vk_imgui_studio_panels.cpp"
INIT="${ROOT}/renderers/vulkan/inspector/vk_imgui.cpp"
DOC="${ROOT}/docs/IN_ENGINE_STUDIO_TOOLS.md"

[ -f "$HDR" ] || fail "missing vk_imgui.h"
[ -f "$CHROME" ] || fail "missing vk_imgui_chrome_panels.cpp"
[ -f "$STUDIO" ] || fail "missing vk_imgui_studio_panels.cpp"
[ -f "$DOC" ] || fail "missing IN_ENGINE_STUDIO_TOOLS.md"

rg -q 'studioAnimation' "$HDR" || fail "studioAnimation open flag missing in vk_imgui.h"
rg -q 'studioAnimation\.open' "$STUDIO" || fail "Animation panel must gate on studioAnimation.open"
rg -q 'MenuItem\( "Animation"' "$CHROME" || fail "Studio menu missing Animation item"
rg -q 'Studio / Entities' "$CHROME" || fail "Window/dock missing Studio / Entities"
rg -q 'Studio / Paint' "$CHROME" || fail "Window/dock missing Studio / Paint"
rg -q 'Studio / Animation' "$CHROME" || fail "Window/dock missing Studio / Animation"
rg -q 'DockBuilderDockWindow\( "Studio / Entities"' "$CHROME" || fail "dock reset missing Entities"
rg -q 'DockBuilderDockWindow\( "Studio / Paint"' "$CHROME" || fail "dock reset missing Paint"
rg -q 'DockBuilderDockWindow\( "Studio / Animation"' "$CHROME" || fail "dock reset missing Animation"
rg -q 'studioAnimation\.open = qtrue' "$INIT" || fail "init must open studioAnimation when r_studio_tools"
rg -q 'studioAnimation\.open = qtrue' "$CHROME" || fail "Developer enable / dock reset must open studioAnimation"

rg -q 'Studio / Session' "$DOC" || fail "docs missing Studio / Session"
rg -q 'Studio / Console' "$DOC" || fail "docs missing Studio / Console"
rg -q 'Studio / Entities' "$DOC" || fail "docs missing Studio / Entities"
rg -q 'Studio / Paint' "$DOC" || fail "docs missing Studio / Paint"
rg -q 'Studio / Animation' "$DOC" || fail "docs missing Studio / Animation"

echo "test_studio_chrome: passed"
