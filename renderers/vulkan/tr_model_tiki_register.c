/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

TIKI registration: compose .tik into existing MD3/IQM/MDR/GLTF loaders.
Sidecar registry for animation aliases and allowlisted frame commands.
===========================================================================
*/

#include "tr_local.h"
#include "../common/tr_model_tiki.h"

#ifdef USE_TIKI

#define TIKI_SIDECAR_MAX 256

typedef struct tikiSidecar_s {
	qhandle_t handle;
	tikiDef_t def;
	qboolean used;
} tikiSidecar_t;

static tikiSidecar_t s_tikiSide[TIKI_SIDECAR_MAX];
static cvar_t *com_tiki;

static void R_Tiki_StoreSidecar( qhandle_t h, const tikiDef_t *def ) {
	int i;
	for ( i = 0; i < TIKI_SIDECAR_MAX; i++ ) {
		if ( !s_tikiSide[i].used ) {
			s_tikiSide[i].used = qtrue;
			s_tikiSide[i].handle = h;
			s_tikiSide[i].def = *def;
			return;
		}
		if ( s_tikiSide[i].handle == h ) {
			s_tikiSide[i].def = *def;
			return;
		}
	}
	ri.Printf( PRINT_WARNING, "TIKI: sidecar table full\n" );
}

const tikiDef_t *R_Tiki_GetSidecar( qhandle_t h ) {
	int i;
	for ( i = 0; i < TIKI_SIDECAR_MAX; i++ ) {
		if ( s_tikiSide[i].used && s_tikiSide[i].handle == h ) {
			return &s_tikiSide[i].def;
		}
	}
	return NULL;
}

extern qhandle_t R_RegisterIQM( const char *name, model_t *mod );
extern qhandle_t R_RegisterMDR( const char *name, model_t *mod );
extern qhandle_t R_RegisterMD3Public( const char *name, model_t *mod );
extern qboolean R_RegisterGLTF( const char *name, model_t *mod );

static qhandle_t R_Tiki_LoadMeshPath( const char *path, model_t *mod ) {
	const char *ext;

	if ( !path || !path[0] || !mod ) {
		return 0;
	}
	ext = COM_GetExtension( path );
	if ( !ext || !ext[0] ) {
		return 0;
	}
	if ( !Q_stricmp( ext, "md3" ) ) {
		return R_RegisterMD3Public( path, mod );
	}
	if ( !Q_stricmp( ext, "iqm" ) ) {
		return R_RegisterIQM( path, mod );
	}
	if ( !Q_stricmp( ext, "mdr" ) ) {
		return R_RegisterMDR( path, mod );
	}
	if ( !Q_stricmp( ext, "gltf" ) || !Q_stricmp( ext, "glb" ) ) {
		return R_RegisterGLTF( path, mod ) ? mod->index : 0;
	}
	/* .tan is animation only — not a render mesh */
	return 0;
}

qhandle_t R_RegisterTIKI( const char *name, model_t *mod ) {
	union {
		char *c;
		void *v;
	} buf;
	int len;
	tikiDef_t def;
	char err[128];
	qhandle_t h = 0;
	const char *meshPath;

	if ( !com_tiki ) {
		com_tiki = ri.Cvar_Get( "com_tiki", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_SetDescription( com_tiki, "Enable clean-room TIKI .tik model defs (USE_TIKI)." );
		ri.Printf( PRINT_ALL, "TIKI: loader ready (com_tiki=%d)\n", com_tiki->integer );
	}
	if ( !com_tiki->integer ) {
		return 0;
	}

	len = ri.FS_ReadFile( name, &buf.v );
	if ( len <= 0 || !buf.c ) {
		return 0;
	}

	if ( !R_Tiki_Parse( buf.c, len, &def, err, sizeof( err ) ) ) {
		ri.Printf( PRINT_WARNING, "TIKI: parse failed '%s': %s\n", name, err );
		ri.FS_FreeFile( buf.v );
		return 0;
	}
	ri.FS_FreeFile( buf.v );

	Q_strncpyz( def.name, name, sizeof( def.name ) );
	meshPath = def.mesh[0] ? def.mesh : ( def.skelmodel[0] ? def.skelmodel :
		( def.numLods > 0 ? def.lods[0].path : "" ) );

	if ( !meshPath[0] ) {
		ri.Printf( PRINT_WARNING, "TIKI: '%s' has no mesh path\n", name );
		return 0;
	}

	h = R_Tiki_LoadMeshPath( meshPath, mod );
	if ( !h ) {
		ri.Printf( PRINT_WARNING, "TIKI: failed to load mesh '%s' for '%s'\n", meshPath, name );
		return 0;
	}

	if ( def.numLods > 0 && mod->numLods < 2 ) {
		/* declare LOD count for R_ComputeLOD when mesh is single-LOD */
		int lodCount = def.numLods;
		if ( lodCount > MD3_MAX_LODS ) {
			lodCount = MD3_MAX_LODS;
		}
		if ( lodCount > mod->numLods ) {
			mod->numLods = lodCount;
		}
	}

	R_Tiki_StoreSidecar( h, &def );
	ri.Printf( PRINT_DEVELOPER, "TIKI: registered '%s' -> '%s' (anims=%d lods=%d)\n",
		name, meshPath, def.numAnims, def.numLods );
	return h;
}

#else /* !USE_TIKI */

qhandle_t R_RegisterTIKI( const char *name, model_t *mod ) {
	(void)name;
	(void)mod;
	return 0;
}

const tikiDef_t *R_Tiki_GetSidecar( qhandle_t h ) {
	(void)h;
	return NULL;
}

#endif /* USE_TIKI */
