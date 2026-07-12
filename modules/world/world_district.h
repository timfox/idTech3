/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

World districts + proxy meshes — USD-authored regions with low-poly proxy
residency and optional BSP sector streaming (cm_stream). Hybrid metadata
path aligned with FreeUSD EngineSceneSnapshot (id Tech 4–8 / Northlight-style
world partition, engine-original conventions).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define WORLD_DISTRICT_MAX       64
#define WORLD_DISTRICT_NAME_MAX  64
#define WORLD_DISTRICT_PATH_MAX  128

typedef enum {
	WD_STATE_UNLOADED = 0,
	WD_STATE_PROXY,
	WD_STATE_STREAMING,
	WD_STATE_LOADED
} worldDistrictState_t;

typedef struct worldDistrict_s {
	qboolean              active;
	char                  name[WORLD_DISTRICT_NAME_MAX];
	char                  manifestPath[WORLD_DISTRICT_PATH_MAX];
	char                  proxyMeshPath[WORLD_DISTRICT_PATH_MAX];
	char                  fullMeshPath[WORLD_DISTRICT_PATH_MAX];
	vec3_t                origin;
	vec3_t                boundsMin;
	vec3_t                boundsMax;
	int                   sectorX0;
	int                   sectorY0;
	int                   sectorX1;
	int                   sectorY1;
	worldDistrictState_t  state;
	qhandle_t             proxyModel;
	qhandle_t             fullModel;
} worldDistrict_t;

typedef qhandle_t ( *worldDistrictRegisterModel_f )( const char *path );
typedef void ( *worldDistrictOnUnload_f )( int index, const worldDistrict_t *d );

void     WorldDistrict_Init( void );
void     WorldDistrict_Shutdown( void );
void     WorldDistrict_SetRegisterModel( worldDistrictRegisterModel_f fn );
void     WorldDistrict_SetOnUnload( worldDistrictOnUnload_f fn );

qboolean WorldDistrict_LoadManifest( const char *usdaPath );
void     WorldDistrict_Import( int count, const worldDistrict_t *src, const char *path );
void     WorldDistrict_Clear( void );
int      WorldDistrict_GetCount( void );
const worldDistrict_t *WorldDistrict_Get( int index );
int      WorldDistrict_FindByName( const char *name );
int      WorldDistrict_FindAtPoint( const vec3_t point );

qboolean WorldDistrict_LoadProxy( int index );
qboolean WorldDistrict_LoadFull( int index );
void     WorldDistrict_Unload( int index );
void     WorldDistrict_UpdateView( const vec3_t viewOrigin, float loadRadius );

void     WorldDistrict_List( void );
void     WorldDistrict_Status( int index );

#ifdef __cplusplus
}
#endif
