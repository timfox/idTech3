/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Lua scripting bindings for all engine systems.
Registers C functions into the Lua global table so game scripts
can call Director, NavMesh, Physics, Particles, Music, Face,
Horde, Dismember, Choreography, and Response systems.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void LuaBindings_RegisterAll(void *luaState);

#ifdef __cplusplus
}
#endif
