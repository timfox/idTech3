#!/usr/bin/env bash
# Stock baseq3 contract: classic profile, QVM guards, server sprite disable.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'stockBaseq3' runtime/client/client.h || fail 'cls.stockBaseq3 missing'
grep -q 'CL_StockBaseq3Mode' runtime/client/client.h || fail 'CL_StockBaseq3Mode missing'
grep -q 'stock baseq3 mode' runtime/client/core/cl_cgame.c || fail 'stock baseq3 log missing'
grep -q 'CL_StockBaseq3Mode()' runtime/client/core/cl_gameframe.c || fail 'gameframe stock guard missing'
grep -q 'CL_StockBaseq3Mode()' runtime/client/core/cl_cgame.c || fail 'renderscene stock guard missing'
grep -q 'SV_ApplyClassicBaseq3ServerCvars' runtime/server/sv_init.c || fail 'server classic cvars missing'
grep -q 'cl_engineSprites 0' config/classic_baseq3.cfg || fail 'classic cfg must disable cl_engineSprites'
grep -q 'retail entity number mapping active' runtime/server/sv_game.c || fail 'qvm entity num mapping missing'
grep -q 'SV_UseLegacyNativeEntityNums' runtime/server/sv_game.c || fail 'entity num mapping helper missing'
grep -q 'cm_streamMerge 0' config/classic_baseq3.cfg || fail 'classic cfg must disable cm_streamMerge'

pass "stock baseq3 contract"
