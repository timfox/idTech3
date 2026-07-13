#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'OPTION(USE_LIBTORRENT' CMakeLists.txt || fail "missing USE_LIBTORRENT option"
grep -q 'find_package(LibtorrentRasterbar' CMakeLists.txt || fail "missing libtorrent CMake package lookup"
grep -q 'pkg_check_modules(LIBTORRENT_RASTERBAR' CMakeLists.txt || fail "missing pkg-config fallback"
grep -q 'USE_LIBTORRENT=1' CMakeLists.txt || fail "missing client compile definition"
grep -q 'LIBTORRENT_TARGET' CMakeLists.txt || fail "missing libtorrent link target"
grep -q 'platform/cl_torrent.cpp' cmake/client/ClientSources.cmake || fail "missing client torrent source"
grep -q 'engine/content/torrent_manifest.cpp' CMakeLists.txt || fail "missing content manifest source"

grep -q 'torrent_enable' runtime/client/platform/cl_torrent.cpp || fail "missing torrent_enable cvar"
grep -q 'torrent_allowDht", "0"' runtime/client/platform/cl_torrent.cpp || fail "DHT must default off"
grep -q 'torrent_allowPex", "0"' runtime/client/platform/cl_torrent.cpp || fail "PEX must default off"
grep -q 'torrent_allowPublicTrackers", "0"' runtime/client/platform/cl_torrent.cpp || fail "public trackers must default off"
grep -q 'torrent_seedCompleted", "0"' runtime/client/platform/cl_torrent.cpp || fail "seeding must default off"
grep -q 'torrent_requireSignature", "1"' runtime/client/platform/cl_torrent.cpp || fail "signatures must default on"
grep -q 'torrent_checkmanifest' runtime/client/platform/cl_torrent.cpp || fail "missing manifest validation command"
grep -q 'CL_Torrent_IsPackageURL' runtime/client/media/cl_download.c || fail "download path does not route torrent package URLs"

grep -q 'TorrentManifest_IsAllowedPackagePath' engine/content/torrent_manifest.cpp || fail "missing package path allowlist"
grep -q 'component == \"..\"' engine/content/torrent_manifest.cpp || fail "missing traversal rejection"
grep -q 'manifest missing signature' engine/content/torrent_manifest.cpp || fail "missing signature requirement"
grep -q 'ext == \".pk3\"' engine/content/torrent_manifest.cpp || fail "missing pk3-only payload gate"

grep -q 'unit_torrent_manifest' CMakeLists.txt || fail "missing manifest unit test target"
grep -q 'Peer-Assisted Content Distribution' docs/TORRENT_CONTENT.md || fail "missing torrent content docs"
grep -q 'not a general BitTorrent client' docs/TORRENT_CONTENT.md || fail "docs must state feature scope"

pass "optional libtorrent content delivery controls, docs, and guards are present"
