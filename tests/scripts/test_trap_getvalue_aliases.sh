#!/usr/bin/env bash
# Guard trap_GetValue alias coverage for documented extension traps.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

check_alias() {
	local file="$1"
	local key="$2"
	local symbol="$3"

	grep -q "\"$key\"" "$file" || fail "$file missing key $key"
	grep -q "$symbol" "$file" || fail "$file missing symbol $symbol"
}

CFILE="runtime/client/core/cl_cgame.c"
SFILE="runtime/server/sv_game.c"

check_alias "$CFILE" "trap_Phys_CreateBody" "CG_PHYS_CREATEBODY"
check_alias "$CFILE" "trap_Phys_DestroyBody" "CG_PHYS_DESTROYBODY"
check_alias "$CFILE" "trap_Phys_ApplyForceBody" "CG_PHYS_APPLYFORCEBODY"
check_alias "$CFILE" "trap_Phys_ApplyImpulse" "CG_PHYS_APPLYIMPULSE"
check_alias "$CFILE" "trap_Phys_GetBodyTransform" "CG_PHYS_GETBODYTRANSFORM"
check_alias "$CFILE" "trap_Phys_SetBodyTransform" "CG_PHYS_SETBODYTRANSFORM"
check_alias "$CFILE" "trap_Phys_SetBodyVelocity" "CG_PHYS_SETBODYVELOCITY"
check_alias "$CFILE" "trap_Phys_StepSimulation" "CG_PHYS_STEPSIMULATION"
check_alias "$CFILE" "trap_Phys_RayCast" "CG_PHYS_RAYCAST"
check_alias "$CFILE" "trap_Phys_LoadBSPCollision" "CG_PHYS_LOADBSPCOLLISION"
check_alias "$CFILE" "trap_EngineSpriteAddLocal" "CG_ENGINE_SPRITE_ADD_LOCAL"
check_alias "$CFILE" "trap_EngineDecalAddLocal" "CG_ENGINE_DECAL_ADD_LOCAL"

check_alias "$SFILE" "trap_EngineSpriteShaderIndex" "G_ENGINE_SPRITE_SHADER_INDEX"
check_alias "$SFILE" "trap_EngineSpriteSpawn" "G_ENGINE_SPRITE_SPAWN"
check_alias "$SFILE" "trap_EngineDecalShaderIndex" "G_ENGINE_DECAL_SHADER_INDEX"
check_alias "$SFILE" "trap_EngineDecalSpawn" "G_ENGINE_DECAL_SPAWN"
check_alias "$SFILE" "trap_Phys_CharacterCreate" "G_PHYS_CHARACTER_CREATE"
check_alias "$SFILE" "trap_Phys_CharacterMove" "G_PHYS_CHARACTER_MOVE"
check_alias "$SFILE" "trap_Loc_Lookup" "G_LOC_LOOKUP"

pass "trap_GetValue aliases cover documented extension traps"
