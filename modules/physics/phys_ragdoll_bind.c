/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Sidecar .rag loader for Soft Step ragdoll bone layouts.

Format (whitespace-separated lines):
  scale <f>
  bone <index> <tagName|*> <radius> <height> <ox> <oy> <oz> <parent>
Parent -1 = root. Tag names are stored for game/cgame MD3 bind; Soft Step
uses radius/height/offset/parent to build capsules + spherical joints.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_ragdoll_bind.h"

qboolean Phys_RagdollLoadDef( const char *pathOrModel, physRagdollDef_t *out ) {
	char path[MAX_QPATH];
	char *buf = NULL;
	int len;
	const char *p;

	if ( !out ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->scale = 1.0f;
	out->limbMass = 5.0f;
	out->jointStiffness = 0.8f;
	out->jointDamping = 0.4f;
	out->balanceForce = 100.0f;

	if ( !pathOrModel || !pathOrModel[0] ) {
		return qfalse;
	}

	if ( strstr( pathOrModel, ".rag" ) ) {
		Q_strncpyz( path, pathOrModel, sizeof( path ) );
	} else {
		Com_sprintf( path, sizeof( path ), "%s.rag", pathOrModel );
	}

	len = FS_ReadFile( path, (void **)&buf );
	if ( len <= 0 || !buf ) {
		return qfalse;
	}

	p = buf;
	while ( 1 ) {
		const char *tok = COM_ParseExt( &p, qtrue );
		physRagdollBoneDef_t *bone;
		int idx;

		if ( !tok[0] ) {
			break;
		}
		if ( tok[0] == '#' ) {
			while ( *p && *p != '\n' ) {
				p++;
			}
			continue;
		}
		if ( !Q_stricmp( tok, "scale" ) ) {
			tok = COM_ParseExt( &p, qfalse );
			out->scale = tok[0] ? (float)atof( tok ) : 1.0f;
			continue;
		}
		if ( !Q_stricmp( tok, "mass" ) || !Q_stricmp( tok, "limbMass" ) ) {
			tok = COM_ParseExt( &p, qfalse );
			out->limbMass = tok[0] ? (float)atof( tok ) : 5.0f;
			continue;
		}
		if ( Q_stricmp( tok, "bone" ) ) {
			continue;
		}

		tok = COM_ParseExt( &p, qfalse );
		idx = tok[0] ? atoi( tok ) : -1;
		if ( idx < 0 || idx >= PHYS_RAGDOLL_MAX_BONES ) {
			continue;
		}
		if ( idx >= out->numBones ) {
			out->numBones = idx + 1;
		}
		bone = &out->bones[idx];
		Com_Memset( bone, 0, sizeof( *bone ) );
		bone->parent = -1;
		bone->radius = 4.0f;
		bone->height = 12.0f;

		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] && tok[0] != '*' ) {
			Q_strncpyz( bone->tagName, tok, sizeof( bone->tagName ) );
		}

		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->radius = (float)atof( tok );
		}
		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->height = (float)atof( tok );
		}
		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->localOffset[0] = (float)atof( tok );
		}
		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->localOffset[1] = (float)atof( tok );
		}
		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->localOffset[2] = (float)atof( tok );
		}
		tok = COM_ParseExt( &p, qfalse );
		if ( tok[0] ) {
			bone->parent = atoi( tok );
		}
	}

	FS_FreeFile( buf );
	Q_strncpyz( out->modelPath, path, sizeof( out->modelPath ) );
	Com_Printf( "[physics] loaded ragdoll bind %s (%d bones)\n", path, out->numBones );
	return out->numBones > 0 ? qtrue : qfalse;
}
