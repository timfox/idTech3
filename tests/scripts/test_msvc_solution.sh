#!/usr/bin/env bash
# MSVC solution must not list deprecated renderer2 (Vulkan-only shipping).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SLN="${ROOT}/engine/platform/win32/msvc2017/quake3e.sln"

fail() { echo "FAIL: $*" >&2; exit 1; }

[ -f "$SLN" ] || fail "missing quake3e.sln"

if rg -q 'renderer2\.vcxproj' "$SLN"; then
	fail "renderer2 must not be in quake3e.sln (deprecated OpenGL)"
fi

for proj in quake3e quake3e-ded botlib vulkan; do
	rg -q "\"${proj}\\.vcxproj\"" "$SLN" || fail "missing ${proj}.vcxproj in solution"
done

echo "test_msvc_solution: passed"
