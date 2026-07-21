#!/usr/bin/env bash
# Static wiring checks for Steam client API (no live Steam client required).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -f runtime/client/platform/cl_steam.c || fail "missing cl_steam.c"
test -f runtime/client/platform/cl_steam.h || fail "missing cl_steam.h"
test -f engine/core/steam_shared.c || fail "missing steam_shared.c"
test -f engine/core/steam_shared.h || fail "missing steam_shared.h"
test -f docs/STEAM.md || fail "missing docs/STEAM.md"
test -f steam_appid.txt || fail "missing steam_appid.txt"
test -f config/steamdeck.cfg || fail "missing config/steamdeck.cfg"

grep -q 'STEAMWORKS_SDK' CMakeLists.txt || fail "CMake missing STEAMWORKS_SDK cache"
grep -q 'CACHE PATH "Steamworks SDK root' CMakeLists.txt || fail "CMake missing STEAMWORKS_SDK CACHE PATH"
grep -q '\${STEAMWORKS_SDK}' CMakeLists.txt || fail "CMake HINTS missing \${STEAMWORKS_SDK}"
grep -q 'LANGUAGE CXX' CMakeLists.txt || fail "CMake missing LANGUAGE CXX for Steam sources"
grep -q 'file(COPY "${STEAMWORKS_LIBRARY}"' CMakeLists.txt || fail "CMake missing libsteam_api copy"
grep -q 'steam_appid.txt' CMakeLists.txt || fail "CMake missing steam_appid.txt copy"

grep -q 'steam|use-steam)' scripts/compile_engine.sh || fail "compile_engine.sh missing steam arg"
grep -q 'USE_STEAM=ON' scripts/compile_engine.sh || fail "compile_engine.sh missing USE_STEAM=ON"
grep -q 'libsteam_api.so' scripts/compile_engine.sh || fail "compile_engine.sh missing libsteam_api release copy"

grep -q 'Steam_Frame()' runtime/client/core/cl_frame.c || fail "CL_Frame missing Steam_Frame()"
grep -q 'Steam_Shutdown()' runtime/client/core/cl_lifecycle.c || fail "CL_Shutdown missing Steam_Shutdown()"

grep -q 'steam_status' runtime/client/platform/cl_steam.c || fail "missing steam_status command"
grep -q 'steam_achievement' runtime/client/platform/cl_steam.c || fail "missing steam_achievement command"
grep -q 'GameOverlayActivated_t' runtime/client/platform/cl_steam.c || fail "missing overlay callback"
grep -q 'cl_steamPauseOnOverlay' runtime/client/platform/cl_steam.c || fail "missing cl_steamPauseOnOverlay"
grep -q 'Steam_ClearAchievement' runtime/client/platform/cl_steam.c || fail "missing Steam_ClearAchievement"
grep -q 'SteamInput' runtime/client/platform/cl_steam.c || fail "missing Steam Input status path"
grep -q '#else /\* !USE_STEAM \*/' runtime/client/platform/cl_steam.c || fail "cl_steam.c missing USE_STEAM off stub branch"

grep -q 'extern "C"' engine/core/steam_shared.h || fail "steam_shared.h missing extern C"
grep -q 'SteamShared_Shutdown' engine/core/common.c || fail "common.c missing SteamShared_Shutdown"
grep -q 'SteamShared_Frame' engine/core/common.c || fail "common.c missing SteamShared_Frame"

grep -q 'STEAM.md' docs/DEVELOPMENT_SETUP.md || fail "DEVELOPMENT_SETUP.md missing STEAM.md link"
grep -q 'STEAM.md' docs/README.md || fail "docs/README.md missing STEAM.md link"

pass "Steam client API wiring (CMake, lifecycle, commands, docs) is present"
