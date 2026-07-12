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
#include "qfiles.h"
#include "phys_ragdoll_bind.h"

static void Phys_RagdollAxisToEuler( const vec3_t axis[3], vec3_t outDeg ) {
	/* Approximate yaw/pitch/roll from MD3 tag axes (forward = axis[0]). */
	float yaw, pitch;
	yaw = atan2f( axis[0][1], axis[0][0] ) * ( 180.0f / (float)M_PI );
	pitch = -asinf( Com_Clamp( -1.0f, 1.0f, axis[0][2] ) ) * ( 180.0f / (float)M_PI );
	outDeg[0] = pitch;
	outDeg[1] = yaw;
	outDeg[2] = 0.0f;
}

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

qboolean Phys_RagdollApplyMd3Frame( physRagdollHandle_t handle, const physRagdollDef_t *bind,
	const char *md3Path, int frame ) {
	byte *data = NULL;
	md3Header_t *hdr;
	md3Tag_t *tags;
	int len, t, b, numTags, numFrames;
	int applied = 0;

	if ( handle < 0 || !bind || !md3Path || !md3Path[0] || bind->numBones <= 0 ) {
		return qfalse;
	}

	len = FS_ReadFile( md3Path, (void **)&data );
	if ( len < (int)sizeof( md3Header_t ) || !data ) {
		return qfalse;
	}

	hdr = (md3Header_t *)data;
	if ( LittleLong( hdr->ident ) != MD3_IDENT || LittleLong( hdr->version ) != MD3_VERSION ) {
		FS_FreeFile( data );
		return qfalse;
	}
	numFrames = LittleLong( hdr->numFrames );
	numTags = LittleLong( hdr->numTags );
	if ( numFrames <= 0 || numTags <= 0 || frame < 0 || frame >= numFrames ) {
		FS_FreeFile( data );
		return qfalse;
	}
	if ( LittleLong( hdr->ofsTags ) + frame * numTags * (int)sizeof( md3Tag_t ) > len ) {
		FS_FreeFile( data );
		return qfalse;
	}

	tags = (md3Tag_t *)( data + LittleLong( hdr->ofsTags ) ) + frame * numTags;
	for ( b = 0; b < bind->numBones; b++ ) {
		const char *want = bind->bones[b].tagName;
		vec3_t pos, rot, world;
		if ( !want[0] ) {
			continue;
		}
		for ( t = 0; t < numTags; t++ ) {
			md3Tag_t tag = tags[t];
			/* tag name is char[]; origins need endian swap on BE (noop on LE) */
			if ( Q_stricmp( tag.name, want ) ) {
				continue;
			}
			pos[0] = tag.origin[0];
			pos[1] = tag.origin[1];
			pos[2] = tag.origin[2];
			world[0] = bind->rootPosition[0] + pos[0] * ( bind->scale > 0.0f ? bind->scale : 1.0f );
			world[1] = bind->rootPosition[1] + pos[1] * ( bind->scale > 0.0f ? bind->scale : 1.0f );
			world[2] = bind->rootPosition[2] + pos[2] * ( bind->scale > 0.0f ? bind->scale : 1.0f );
			Phys_RagdollAxisToEuler( tag.axis, rot );
			Phys_RagdollSetBoneAnimTarget( handle, b, world, rot );
			applied++;
			break;
		}
	}

	FS_FreeFile( data );
	if ( applied > 0 ) {
		Phys_RagdollBlendToAnimation( handle, 1.0f );
	}
	return applied > 0 ? qtrue : qfalse;
}

qboolean Phys_RagdollSpawnBoundEx( const char *modelOrRag, const vec3_t origin, physBoundRagdoll_t *out,
	qboolean startDead ) {
	physRagdollDef_t def;
	procAnimConfig_t cfg;
	qboolean loaded;

	if ( !out || !origin ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->ragdoll = -1;
	out->anim = -1;
	out->motor = -1;

	loaded = Phys_RagdollLoadDef( modelOrRag, &def );
	if ( !loaded ) {
		Com_Memset( &def, 0, sizeof( def ) );
		def.scale = 1.0f;
		def.limbMass = 5.0f;
	}
	VectorCopy( origin, def.rootPosition );
	def.entityNum = -1;

	out->ragdoll = Phys_CreateRagdoll( &def );
	if ( out->ragdoll < 0 ) {
		return qfalse;
	}

	ProcAnim_DefaultConfig( &cfg );
	out->anim = ProcAnim_Create( out->ragdoll, &cfg );
	if ( out->anim >= 0 ) {
		out->motor = PhysMotor_Create( out->anim, out->ragdoll );
		if ( startDead ) {
			ProcAnim_Kill( out->anim ); /* death → Soft Step ragdoll drive */
		}
	}

	/* If an MD3 exists next to the bind, seed pose from frame 0 tags. */
	if ( loaded && modelOrRag && modelOrRag[0] ) {
		char md3Path[MAX_QPATH];
		if ( strstr( modelOrRag, ".rag" ) ) {
			char base[MAX_QPATH];
			char *dot;
			Q_strncpyz( base, modelOrRag, sizeof( base ) );
			dot = strrchr( base, '.' );
			if ( dot ) {
				*dot = '\0';
			}
			Com_sprintf( md3Path, sizeof( md3Path ), "%s.md3", base );
		} else if ( strstr( modelOrRag, ".md3" ) ) {
			Q_strncpyz( md3Path, modelOrRag, sizeof( md3Path ) );
		} else {
			Com_sprintf( md3Path, sizeof( md3Path ), "%s.md3", modelOrRag );
		}
		Phys_RagdollApplyMd3Frame( out->ragdoll, &def, md3Path, 0 );
	}

	Com_Printf( "[physics] bound ragdoll=%d anim=%d motor=%d at (%.0f %.0f %.0f) bind=%s dead=%d\n",
		out->ragdoll, out->anim, out->motor, origin[0], origin[1], origin[2],
		loaded ? "yes" : "procedural", startDead ? 1 : 0 );
	return qtrue;
}

qboolean Phys_RagdollSpawnBound( const char *modelOrRag, const vec3_t origin, physBoundRagdoll_t *out ) {
	return Phys_RagdollSpawnBoundEx( modelOrRag, origin, out, qtrue );
}
