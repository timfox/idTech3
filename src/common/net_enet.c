/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides ENet networking integration for enhanced multiplayer
networking capabilities. It wraps ENet functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_ENET
#include "enet/enet.h"

// CVar to control ENet usage
static cvar_t *net_enet;

// ENet host instance
static ENetHost *enet_host = NULL;
static qboolean enet_initialized = qfalse;

/*
=================
NET_ENet_Init_Lib
=================
Initialize ENet library
=================
*/
qboolean NET_ENet_Init_Lib(void)
{
	if (enet_initialized)
		return qtrue;
	
	if (enet_initialize() != 0)
	{
		Com_Printf("ERROR: Failed to initialize ENet\n");
		return qfalse;
	}
	
	enet_initialized = qtrue;
	return qtrue;
}

/*
=================
NET_ENet_Shutdown
=================
Shutdown ENet subsystem
=================
*/
void NET_ENet_Shutdown(void)
{
	if (enet_host)
	{
		enet_host_destroy(enet_host);
		enet_host = NULL;
	}
	
	if (enet_initialized)
	{
		enet_deinitialize();
		enet_initialized = qfalse;
	}
}

/*
=================
NET_ENet_CreateHost
=================
Create an ENet host
=================
*/
ENetHost *NET_ENet_CreateHost(const ENetAddress *address, size_t peerCount, size_t channelLimit, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth)
{
	if (!enet_initialized)
	{
		if (!NET_ENet_Init_Lib())
			return NULL;
	}
	
	return enet_host_create(address, peerCount, channelLimit, incomingBandwidth, outgoingBandwidth);
}

/*
=================
NET_ENet_DestroyHost
=================
Destroy an ENet host
=================
*/
void NET_ENet_DestroyHost(ENetHost *host)
{
	if (host)
	{
		enet_host_destroy(host);
		if (host == enet_host)
			enet_host = NULL;
	}
}

/*
=================
NET_ENet_Service
=================
Service ENet events
=================
*/
int NET_ENet_Service(ENetHost *host, ENetEvent *event, enet_uint32 timeout)
{
	if (!host || !event)
		return -1;
	
	return enet_host_service(host, event, timeout);
}

/*
=================
NET_ENet_Init
=================
Initialize ENet networking subsystem (CVar and library)
=================
*/
void NET_ENet_Init(void)
{
	net_enet = Cvar_Get("net_enet", "0", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(net_enet, "Enable ENet networking: 0 = UDP (default), 1 = ENet");
	
	if (net_enet->integer)
	{
		if (!NET_ENet_Init_Lib())
		{
			Com_Printf("WARNING: ENet initialization failed, falling back to UDP\n");
			Cvar_Set("net_enet", "0");
		}
		else
		{
			Com_Printf("ENet networking enabled\n");
		}
	}
}

#endif // USE_ENET

