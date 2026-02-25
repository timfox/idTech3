/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Game frame integration -- per-frame tick for all gameplay subsystems.
Called from the client frame loop to drive physics, procedural
animation, navigation, and particles.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

void CL_InitGameSystems(void);
void CL_ShutdownGameSystems(void);
void CL_GameFrame(float frametime);

#ifdef __cplusplus
}
#endif
