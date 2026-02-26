/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Engine-editor entity bridge.
Parses entity key/values from BSP entities lump to configure
engine systems (Director zones, DMM objects, projected lights,
cloth, skybox, navmesh, etc.) at map load time.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

void EntityBridge_ParseEntities(const char *entityString);
void EntityBridge_Clear(void);

#ifdef __cplusplus
}
#endif
