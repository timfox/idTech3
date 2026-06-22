/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client cvar definitions and registration (split from cl_main.c).
===========================================================================
*/
#pragma once

#include "../../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *cl_noprint;
extern cvar_t *cl_motd;
extern cvar_t *rcon_client_password;
extern cvar_t *rconAddress;
extern cvar_t *cl_timeout;
extern cvar_t *cl_autoNudge;
extern cvar_t *cl_timeNudge;
extern cvar_t *cl_showTimeDelta;
extern cvar_t *cl_shownet;
extern cvar_t *cl_activeAction;
extern cvar_t *cl_motdString;
extern cvar_t *cl_conXOffset;
extern cvar_t *cl_conColor;
extern cvar_t *cl_inGameVideo;
extern cvar_t *cl_lanForcePackets;
extern cvar_t *cl_guidServerUniq;
extern cvar_t *cl_reconnectArgs;

void CL_InitCvars( void );

#ifdef __cplusplus
}
#endif
