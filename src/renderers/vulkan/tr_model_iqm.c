/*
===========================================================================
Copyright (C) 2011 Thilo Schulz <thilo@tjps.eu>
Copyright (C) 2011 Matthias Bentrup <matthias.bentrup@googlemail.com>
Copyright (C) 2011-2019 Zack Middleton <zturtleman@gmail.com>

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"

#define	LL(x) x=LittleLong(x)

// 3x4 identity matrix
static float identityMatrix[12] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0
};

static qboolean IQM_CheckRange( iqmHeader_t *header, int offset,
				int count, int size ) {
	// return true if the range specified by offset, count and size
	// doesn't fit into the file
	return ( count <= 0 ||
		 offset <= 0 ||
		 offset > (int)header->filesize ||
		 offset + count * size < 0 ||
		 offset + count * size > (int)header->filesize );
}
// "multiply" 3x4 matrices, these are assumed to be the top 3 rows
// of a 4x4 matrix with the last row = (0 0 0 1)
static void Matrix34Multiply( const float *a, const float *b, float *out ) {
	out[ 0] = a[0] * b[0] + a[1] * b[4] + a[ 2] * b[ 8];
	out[ 1] = a[0] * b[1] + a[1] * b[5] + a[ 2] * b[ 9];
	out[ 2] = a[0] * b[2] + a[1] * b[6] + a[ 2] * b[10];
	out[ 3] = a[0] * b[3] + a[1] * b[7] + a[ 2] * b[11] + a[ 3];
	out[ 4] = a[4] * b[0] + a[5] * b[4] + a[ 6] * b[ 8];
	out[ 5] = a[4] * b[1] + a[5] * b[5] + a[ 6] * b[ 9];
	out[ 6] = a[4] * b[2] + a[5] * b[6] + a[ 6] * b[10];
	out[ 7] = a[4] * b[3] + a[5] * b[7] + a[ 6] * b[11] + a[ 7];
	out[ 8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[ 8];
	out[ 9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[ 9];
	out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
	out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}
static void JointToMatrix( const quat_t rot, const vec3_t scale, const vec3_t trans,
			   float *mat ) {
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];

	mat[ 0] = scale[0] * (1.0f - (yy + zz));
	mat[ 1] = scale[0] * (xy - wz);
	mat[ 2] = scale[0] * (xz + wy);
	mat[ 3] = trans[0];
	mat[ 4] = scale[1] * (xy + wz);
	mat[ 5] = scale[1] * (1.0f - (xx + zz));
	mat[ 6] = scale[1] * (yz - wx);
	mat[ 7] = trans[1];
	mat[ 8] = scale[2] * (xz - wy);
	mat[ 9] = scale[2] * (yz + wx);
	mat[10] = scale[2] * (1.0f - (xx + yy));
	mat[11] = trans[2];
}
static void Matrix34Invert( const float *inMat, float *outMat ) {
	vec3_t trans;
	float invSqrLen, *v;

	outMat[ 0] = inMat[ 0]; outMat[ 1] = inMat[ 4]; outMat[ 2] = inMat[ 8];
	outMat[ 4] = inMat[ 1]; outMat[ 5] = inMat[ 5]; outMat[ 6] = inMat[ 9];
	outMat[ 8] = inMat[ 2]; outMat[ 9] = inMat[ 6]; outMat[10] = inMat[10];

	v = outMat + 0; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	v = outMat + 4; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	v = outMat + 8; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);

	trans[0] = inMat[ 3];
	trans[1] = inMat[ 7];
	trans[2] = inMat[11];

	outMat[ 3] = -DotProduct(outMat + 0, trans);
	outMat[ 7] = -DotProduct(outMat + 4, trans);
	outMat[11] = -DotProduct(outMat + 8, trans);
}
static void QuatSlerp(const quat_t from, const quat_t _to, float fraction, quat_t out) {
	float angle, cosAngle, sinAngle, backlerp, lerp;
	quat_t to;

	// cos() of angle
	cosAngle = from[0] * _to[0] + from[1] * _to[1] + from[2] * _to[2] + from[3] * _to[3];

	// negative handling is needed for taking shortest path (required for model joints)
	if ( cosAngle < 0.0f ) {
		cosAngle = -cosAngle;
		to[0] = - _to[0];
		to[1] = - _to[1];
		to[2] = - _to[2];
		to[3] = - _to[3];
	} else {
		QuatCopy( _to, to );
	}

	if ( cosAngle < 0.999999f ) {
		// spherical lerp (slerp)
		angle = acosf( cosAngle );
		sinAngle = sinf( angle );
		backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
		lerp = sinf( fraction * angle ) / sinAngle;
	} else {
		// linear lerp
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}


static vec_t QuatNormalize2( const quat_t v, quat_t out) {
	float	length;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3];

	if (length) {
		/* writing it this way allows gcc to recognize that rsqrt can be used with -ffast-math */
		const float ilength = 1.0f / sqrtf( length );
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
		out[3] = v[3]*ilength;
	} else {
		out[0] = out[1] = out[2] = 0;
		out[3] = -1;
	}

	return length;
}

typedef struct {
	char		name[IQM_MORPH_NAME_MAX];
	int			numVertexes;
} iqmMorphSurfaceDiskInfo_t;

static qboolean IQM_ReadU32LE( const byte **cursor, const byte *end, uint32_t *out ) {
	const byte *p;

	if ( (size_t)( end - *cursor ) < 4 ) {
		return qfalse;
	}

	p = *cursor;
	*out = (uint32_t)p[0] |
		( (uint32_t)p[1] << 8 ) |
		( (uint32_t)p[2] << 16 ) |
		( (uint32_t)p[3] << 24 );
	*cursor += 4;
	return qtrue;
}

static qboolean IQM_ReadFloatArrayLE( float *dst, const byte **cursor, const byte *end, size_t count ) {
	size_t i;

	if ( (size_t)( end - *cursor ) < count * sizeof( uint32_t ) ) {
		return qfalse;
	}

	for ( i = 0; i < count; i++ ) {
		uint32_t bits = (uint32_t)(*cursor)[0] |
			( (uint32_t)(*cursor)[1] << 8 ) |
			( (uint32_t)(*cursor)[2] << 16 ) |
			( (uint32_t)(*cursor)[3] << 24 );
		Com_Memcpy( &dst[i], &bits, sizeof( bits ) );
		*cursor += sizeof( bits );
	}
	return qtrue;
}

static qboolean R_LoadIQMMorphSidecar( iqmData_t *iqmData, const char *modelName ) {
	union {
		byte *b;
		void *v;
	} fileData;
	const byte *cursor;
	const byte *end;
	char sidecarPath[MAX_QPATH];
	size_t i;
	int fileSize;
	qboolean loaded = qfalse;
	uint32_t version;
	uint32_t numTargets;
	uint32_t numSurfaces;
	uint32_t flags;
	char *targetNames = NULL;
	iqmMorphSurfaceDiskInfo_t *surfaceInfos = NULL;
	int *surfaceMap = NULL;

	iqmData->morphNumTargets = 0;
	iqmData->morphTargets = NULL;
	iqmData->morphSurfaces = NULL;

	COM_StripExtension( modelName, sidecarPath, sizeof( sidecarPath ) );
	Q_strcat( sidecarPath, sizeof( sidecarPath ), ".morph" );

	fileSize = ri.FS_ReadFile( sidecarPath, &fileData.v );
	if ( !fileData.b ) {
		return qfalse;
	}

	cursor = fileData.b;
	end = fileData.b + fileSize;

	if ( (size_t)( end - cursor ) < 8 + 4 * sizeof( uint32_t ) ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has a truncated header, ignoring.\n", sidecarPath );
		goto cleanup;
	}

	if ( memcmp( cursor, "IQMMORPH", 8 ) != 0 ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has an invalid magic, ignoring.\n", sidecarPath );
		goto cleanup;
	}
	cursor += 8;

	if ( !IQM_ReadU32LE( &cursor, end, &version ) ||
		!IQM_ReadU32LE( &cursor, end, &numTargets ) ||
		!IQM_ReadU32LE( &cursor, end, &numSurfaces ) ||
		!IQM_ReadU32LE( &cursor, end, &flags ) ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has invalid metadata, ignoring.\n", sidecarPath );
		goto cleanup;
	}

	if ( version != 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has unsupported version %u, ignoring.\n",
			sidecarPath, (unsigned)version );
		goto cleanup;
	}
	if ( !( flags & 1u ) ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s is missing normal deltas (required), ignoring.\n", sidecarPath );
		goto cleanup;
	}
	if ( numTargets == 0 || numTargets > 1024 ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has invalid target count %u, ignoring.\n",
			sidecarPath, (unsigned)numTargets );
		goto cleanup;
	}
	if ( numSurfaces == 0 || numSurfaces > (uint32_t)iqmData->num_surfaces ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s has invalid surface count %u (model has %d), ignoring.\n",
			sidecarPath, (unsigned)numSurfaces, iqmData->num_surfaces );
		goto cleanup;
	}

	targetNames = (char *)ri.Hunk_AllocateTempMemory( (int)( numTargets * IQM_MORPH_NAME_MAX ) );
	surfaceInfos = (iqmMorphSurfaceDiskInfo_t *)ri.Hunk_AllocateTempMemory( (int)( numSurfaces * sizeof( *surfaceInfos ) ) );
	surfaceMap = (int *)ri.Hunk_AllocateTempMemory( (int)( numSurfaces * sizeof( *surfaceMap ) ) );
	if ( !targetNames || !surfaceInfos || !surfaceMap ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: temp allocation failed for %s, ignoring.\n", sidecarPath );
		goto cleanup;
	}

	for ( i = 0; i < numSurfaces; i++ ) {
		surfaceMap[i] = -1;
	}

	for ( i = 0; i < numTargets; i++ ) {
		if ( (size_t)( end - cursor ) < IQM_MORPH_NAME_MAX ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s target name table is truncated, ignoring.\n", sidecarPath );
			goto cleanup;
		}

		Q_strncpyz( targetNames + i * IQM_MORPH_NAME_MAX, (const char *)cursor, IQM_MORPH_NAME_MAX );
		Q_strlwr( targetNames + i * IQM_MORPH_NAME_MAX );
		cursor += IQM_MORPH_NAME_MAX;
	}

	for ( i = 0; i < numSurfaces; i++ ) {
		uint32_t diskVertexes;
		int j;
		int match = -1;

		if ( (size_t)( end - cursor ) < IQM_MORPH_NAME_MAX ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s surface table is truncated, ignoring.\n", sidecarPath );
			goto cleanup;
		}

		Q_strncpyz( surfaceInfos[i].name, (const char *)cursor, sizeof( surfaceInfos[i].name ) );
		Q_strlwr( surfaceInfos[i].name );
		cursor += IQM_MORPH_NAME_MAX;

		if ( !IQM_ReadU32LE( &cursor, end, &diskVertexes ) ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s surface table is truncated, ignoring.\n", sidecarPath );
			goto cleanup;
		}
		surfaceInfos[i].numVertexes = (int)diskVertexes;

		for ( j = 0; j < iqmData->num_surfaces; j++ ) {
			if ( !Q_stricmp( surfaceInfos[i].name, iqmData->surfaces[j].name ) ) {
				match = j;
				break;
			}
		}

		if ( match < 0 ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s surface '%s' not found in model, ignoring sidecar.\n",
				sidecarPath, surfaceInfos[i].name );
			goto cleanup;
		}
		if ( surfaceInfos[i].numVertexes != iqmData->surfaces[match].num_vertexes ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s surface '%s' vertex count mismatch (%d != %d), ignoring sidecar.\n",
				sidecarPath, surfaceInfos[i].name, surfaceInfos[i].numVertexes, iqmData->surfaces[match].num_vertexes );
			goto cleanup;
		}
		surfaceMap[i] = match;
	}

	iqmData->morphTargets = (iqmMorphTarget_t *)ri.Hunk_Alloc( (int)( numTargets * sizeof( iqmMorphTarget_t ) ), h_low );
	iqmData->morphSurfaces = (iqmMorphSurface_t *)ri.Hunk_Alloc( iqmData->num_surfaces * sizeof( iqmMorphSurface_t ), h_low );
	iqmData->morphNumTargets = (int)numTargets;
	for ( i = 0; i < (size_t)iqmData->num_surfaces; i++ ) {
		Com_Memset( &iqmData->morphSurfaces[i], 0, sizeof( iqmData->morphSurfaces[i] ) );
		Q_strncpyz( iqmData->morphSurfaces[i].name, iqmData->surfaces[i].name, sizeof( iqmData->morphSurfaces[i].name ) );
		iqmData->morphSurfaces[i].nameHash = (uint32_t)Com_GenerateHashValue( iqmData->morphSurfaces[i].name, 0x7fffffffU );
		iqmData->morphSurfaces[i].num_vertexes = iqmData->surfaces[i].num_vertexes;
	}
	for ( i = 0; i < numTargets; i++ ) {
		Q_strncpyz( iqmData->morphTargets[i].name, targetNames + i * IQM_MORPH_NAME_MAX, sizeof( iqmData->morphTargets[i].name ) );
		iqmData->morphTargets[i].nameHash = (uint32_t)Com_GenerateHashValue( iqmData->morphTargets[i].name, 0x7fffffffU );
	}

	for ( i = 0; i < numSurfaces; i++ ) {
		int modelSurface = surfaceMap[i];
		iqmMorphSurface_t *dstSurface = &iqmData->morphSurfaces[modelSurface];
		size_t floatsPerTarget = (size_t)surfaceInfos[i].numVertexes * 3u;
		size_t totalFloats = (size_t)numTargets * floatsPerTarget;
		size_t targetIndex;

		dstSurface->deltaPos = (float *)ri.Hunk_Alloc( (int)( totalFloats * sizeof( float ) ), h_low );
		dstSurface->deltaNorm = (float *)ri.Hunk_Alloc( (int)( totalFloats * sizeof( float ) ), h_low );

		for ( targetIndex = 0; targetIndex < numTargets; targetIndex++ ) {
			float *pos = dstSurface->deltaPos + targetIndex * floatsPerTarget;
			float *norm = dstSurface->deltaNorm + targetIndex * floatsPerTarget;
			if ( !IQM_ReadFloatArrayLE( pos, &cursor, end, floatsPerTarget ) ||
				!IQM_ReadFloatArrayLE( norm, &cursor, end, floatsPerTarget ) ) {
				ri.Printf( PRINT_WARNING, "R_LoadIQMMorphSidecar: %s payload is truncated, ignoring sidecar.\n", sidecarPath );
				iqmData->morphNumTargets = 0;
				iqmData->morphTargets = NULL;
				iqmData->morphSurfaces = NULL;
				goto cleanup;
			}
		}
	}

	loaded = qtrue;
	ri.Printf( PRINT_DEVELOPER, "R_LoadIQMMorphSidecar: loaded %u targets for %u surfaces from %s\n",
		(unsigned)numTargets, (unsigned)numSurfaces, sidecarPath );

cleanup:
	if ( targetNames ) {
		ri.Hunk_FreeTempMemory( targetNames );
	}
	if ( surfaceInfos ) {
		ri.Hunk_FreeTempMemory( surfaceInfos );
	}
	if ( surfaceMap ) {
		ri.Hunk_FreeTempMemory( surfaceMap );
	}
	if ( fileData.b ) {
		ri.FS_FreeFile( fileData.b );
	}
	return loaded;
}

/*
=================
R_LoadIQM

Load an IQM model and compute the joint matrices for every frame.
=================
*/
qboolean R_LoadIQM( model_t *mod, void *buffer, int filesize, const char *mod_name ) {
	iqmHeader_t		*header;
	iqmVertexArray_t	*vertexarray;
	iqmTriangle_t		*triangle;
	iqmMesh_t		*mesh;
	iqmJoint_t		*joint;
	iqmPose_t		*pose;
	iqmBounds_t		*bounds;
	unsigned short		*framedata;
	char			*str;
	int			i, j, k;
	iqmTransform_t		*transform;
	float			*mat, *matInv;
	size_t			size, joint_names;
	byte			*dataPtr;
	iqmData_t		*iqmData;
	srfIQModel_t		*surface;
	char			meshName[MAX_QPATH];
	int				vertexArrayFormat[IQM_COLOR+1];
	int				allocateInfluences;
	byte *blendIndexes;
	union {
		byte *b;
		float *f;
	} blendWeights;

	if( filesize < (int)sizeof(iqmHeader_t) ) {
		return qfalse;
	}

	header = (iqmHeader_t *)buffer;
	if( Q_strncmp( header->magic, IQM_MAGIC, sizeof(header->magic) ) ) {
		return qfalse;
	}

	LL( header->version );
	if( header->version != IQM_VERSION ) {
		ri.Printf(PRINT_WARNING, "R_LoadIQM: %s is a unsupported IQM version (%d), only version %d is supported.\n",
				mod_name, header->version, IQM_VERSION);
		return qfalse;
	}

	LL( header->filesize );
	if( (int)header->filesize > filesize || header->filesize > (16u<<20) ) {
		return qfalse;
	}

	LL( header->flags );
	LL( header->num_text );
	LL( header->ofs_text );
	LL( header->num_meshes );
	LL( header->ofs_meshes );
	LL( header->num_vertexarrays );
	LL( header->num_vertexes );
	LL( header->ofs_vertexarrays );
	LL( header->num_triangles );
	LL( header->ofs_triangles );
	LL( header->ofs_adjacency );
	LL( header->num_joints );
	LL( header->ofs_joints );
	LL( header->num_poses );
	LL( header->ofs_poses );
	LL( header->num_anims );
	LL( header->ofs_anims );
	LL( header->num_frames );
	LL( header->num_framechannels );
	LL( header->ofs_frames );
	LL( header->ofs_bounds );
	LL( header->num_comment );
	LL( header->ofs_comment );
	LL( header->num_extensions );
	LL( header->ofs_extensions );

	// check ioq3 joint limit
	if ( header->num_joints > IQM_MAX_JOINTS ) {
		ri.Printf(PRINT_WARNING, "R_LoadIQM: %s has more than %d joints (%d).\n",
				mod_name, IQM_MAX_JOINTS, header->num_joints);
		return qfalse;
	}

	for ( i = 0; i < (int)ARRAY_LEN( vertexArrayFormat ); i++ ) {
		vertexArrayFormat[i] = -1;
	}

	blendIndexes = NULL;
	blendWeights.b = NULL;

	allocateInfluences = 0;

	if ( header->num_meshes )
	{
		// check and swap vertex arrays
		if( IQM_CheckRange( header, header->ofs_vertexarrays,
				    header->num_vertexarrays,
				    sizeof(iqmVertexArray_t) ) ) {
			return qfalse;
		}
		vertexarray = (iqmVertexArray_t *)((byte *)header + header->ofs_vertexarrays);
		for( i = 0; i < (int)header->num_vertexarrays; i++, vertexarray++ ) {
			int	n, *intPtr;

			if( vertexarray->size <= 0 || vertexarray->size > 4 ) {
				return qfalse;
			}

			// total number of values
			n = header->num_vertexes * vertexarray->size;

			switch( vertexarray->format ) {
			case IQM_BYTE:
			case IQM_UBYTE:
				// 1 byte, no swapping necessary
				if( IQM_CheckRange( header, vertexarray->offset,
						    n, sizeof(byte) ) ) {
					return qfalse;
				}
				break;
			case IQM_INT:
			case IQM_UINT:
			case IQM_FLOAT:
				// 4-byte swap
				if( IQM_CheckRange( header, vertexarray->offset,
						    n, sizeof(float) ) ) {
					return qfalse;
				}
				intPtr = (int *)((byte *)header + vertexarray->offset);
				for( j = 0; j < n; j++, intPtr++ ) {
					LL( *intPtr );
				}
				break;
			default:
				// not supported
				return qfalse;
				break;
			}

			if( vertexarray->type < ARRAY_LEN( vertexArrayFormat ) ) {
				vertexArrayFormat[vertexarray->type] = vertexarray->format;
			}

			switch( vertexarray->type ) {
			case IQM_POSITION:
			case IQM_NORMAL:
				if( vertexarray->format != IQM_FLOAT ||
				    vertexarray->size != 3 ) {
					return qfalse;
				}
				break;
			case IQM_TANGENT:
				if( vertexarray->format != IQM_FLOAT ||
				    vertexarray->size != 4 ) {
					return qfalse;
				}
				break;
			case IQM_TEXCOORD:
				if( vertexarray->format != IQM_FLOAT ||
				    vertexarray->size != 2 ) {
					return qfalse;
				}
				break;
			case IQM_BLENDINDEXES:
				if( (vertexarray->format != IQM_INT &&
					 vertexarray->format != IQM_UBYTE) ||
					vertexarray->size != 4 ) {
					return qfalse;
				}
				blendIndexes = (byte*)header + vertexarray->offset;
				break;
			case IQM_BLENDWEIGHTS:
				if( (vertexarray->format != IQM_FLOAT &&
					 vertexarray->format != IQM_UBYTE) ||
				    vertexarray->size != 4 ) {
					return qfalse;
				}
				if( vertexarray->format == IQM_FLOAT ) {
					blendWeights.f = (float*)( (byte*)header + vertexarray->offset );
				} else {
					blendWeights.b = (byte*)header + vertexarray->offset;
				}
				break;
			case IQM_COLOR:
				if( vertexarray->format != IQM_UBYTE ||
				    vertexarray->size != 4 ) {
					return qfalse;
				}
				break;
			}
		}

		// check for required vertex arrays
		if( vertexArrayFormat[IQM_POSITION] == -1 || vertexArrayFormat[IQM_NORMAL] == -1 || vertexArrayFormat[IQM_TEXCOORD] == -1 ) {
			ri.Printf( PRINT_WARNING, "R_LoadIQM: %s is missing IQM_POSITION, IQM_NORMAL, and/or IQM_TEXCOORD array.\n", mod_name );
			return qfalse;
		}

		if( header->num_joints ) {
			if( vertexArrayFormat[IQM_BLENDINDEXES] == -1 || vertexArrayFormat[IQM_BLENDWEIGHTS] == -1 ) {
				ri.Printf( PRINT_WARNING, "R_LoadIQM: %s is missing IQM_BLENDINDEXES and/or IQM_BLENDWEIGHTS array.\n", mod_name );
				return qfalse;
			}
		} else {
			// ignore blend arrays if present
			vertexArrayFormat[IQM_BLENDINDEXES] = -1;
			vertexArrayFormat[IQM_BLENDWEIGHTS] = -1;
		}

		// opengl1 renderer doesn't use tangents
		vertexArrayFormat[IQM_TANGENT] = -1;

		// check and swap triangles
		if( IQM_CheckRange( header, header->ofs_triangles,
				    header->num_triangles, sizeof(iqmTriangle_t) ) ) {
			return qfalse;
		}
		triangle = (iqmTriangle_t *)((byte *)header + header->ofs_triangles);
		for( i = 0; i < (int)header->num_triangles; i++, triangle++ ) {
			LL( triangle->vertex[0] );
			LL( triangle->vertex[1] );
			LL( triangle->vertex[2] );

			if( triangle->vertex[0] > header->num_vertexes ||
			    triangle->vertex[1] > header->num_vertexes ||
			    triangle->vertex[2] > header->num_vertexes ) {
				return qfalse;
			}
		}

		// check and swap meshes
		if( IQM_CheckRange( header, header->ofs_meshes,
				    header->num_meshes, sizeof(iqmMesh_t) ) ) {
			return qfalse;
		}
		mesh = (iqmMesh_t *)((byte *)header + header->ofs_meshes);
		for( i = 0; i < (int)header->num_meshes; i++, mesh++) {
			LL( mesh->name );
			LL( mesh->material );
			LL( mesh->first_vertex );
			LL( mesh->num_vertexes );
			LL( mesh->first_triangle );
			LL( mesh->num_triangles );

			if ( mesh->name < header->num_text ) {
				Q_strncpyz( meshName, (char*)header + header->ofs_text + mesh->name, sizeof (meshName) );
			} else {
				meshName[0] = '\0';
			}

			// check ioq3 limits
			if ( mesh->num_vertexes >= SHADER_MAX_VERTEXES ) {
				ri.Printf( PRINT_WARNING, "R_LoadIQM: %s has more than %i verts on %s (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES - 1, meshName[0] ? meshName : "a surface",
					  mesh->num_vertexes );
				return qfalse;
			}
			if ( mesh->num_triangles*3 >= SHADER_MAX_INDEXES ) {
				ri.Printf( PRINT_WARNING, "R_LoadIQM: %s has more than %i triangles on %s (%i).\n",
					  mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, meshName[0] ? meshName : "a surface",
					  mesh->num_triangles );
				return qfalse;
			}

			if( mesh->first_vertex >= header->num_vertexes ||
			    mesh->first_vertex + mesh->num_vertexes > header->num_vertexes ||
			    mesh->first_triangle >= header->num_triangles ||
			    mesh->first_triangle + mesh->num_triangles > header->num_triangles ||
			    mesh->name >= header->num_text ||
			    mesh->material >= header->num_text ) {
				return qfalse;
			}

			// find number of unique blend influences per mesh
			if( header->num_joints ) {
				for( j = 0; j < (int)mesh->num_vertexes; j++ ) {
					int vtx = mesh->first_vertex + j;

					for( k = 0; k < j; k++ ) {
						int influence = mesh->first_vertex + k;

						if( *(int*)&blendIndexes[4*influence] != *(int*)&blendIndexes[4*vtx] ) {
							continue;
						}

						if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT ) {
							if ( blendWeights.f[4*influence+0] == blendWeights.f[4*vtx+0] &&
							     blendWeights.f[4*influence+1] == blendWeights.f[4*vtx+1] &&
							     blendWeights.f[4*influence+2] == blendWeights.f[4*vtx+2] &&
							     blendWeights.f[4*influence+3] == blendWeights.f[4*vtx+3] ) {
								break;
							}
						} else {
							if ( *(int*)&blendWeights.b[4*influence] == *(int*)&blendWeights.b[4*vtx] ) {
								break;
							}
						}
					}

					if ( k == j ) {
						allocateInfluences++;
					}
				}
			}
		}
	}

	if( header->num_poses != header->num_joints && header->num_poses != 0 ) {
		ri.Printf( PRINT_WARNING, "R_LoadIQM: %s has %d poses and %d joints, must have the same number or 0 poses\n",
			  mod_name, header->num_poses, header->num_joints );
		return qfalse;
	}

	joint_names = 0;

	if ( header->num_joints )
	{
		// check and swap joints
		if( IQM_CheckRange( header, header->ofs_joints,
					header->num_joints, sizeof(iqmJoint_t) ) ) {
			return qfalse;
		}
		joint = (iqmJoint_t *)((byte *)header + header->ofs_joints);
		for( i = 0; i < (int)header->num_joints; i++, joint++ ) {
			LL( joint->name );
			LL( joint->parent );
			LL( joint->translate[0] );
			LL( joint->translate[1] );
			LL( joint->translate[2] );
			LL( joint->rotate[0] );
			LL( joint->rotate[1] );
			LL( joint->rotate[2] );
			LL( joint->rotate[3] );
			LL( joint->scale[0] );
			LL( joint->scale[1] );
			LL( joint->scale[2] );

			if( joint->parent < -1 ||
				joint->parent >= (int)header->num_joints ||
				joint->name >= (unsigned)header->num_text ) {
				return qfalse;
			}
			joint_names += strlen( (char *)header + header->ofs_text +
						   joint->name ) + 1;
		}
	}

	if ( header->num_poses )
	{
		// check and swap poses
		if( IQM_CheckRange( header, header->ofs_poses,
					header->num_poses, sizeof(iqmPose_t) ) ) {
			return qfalse;
		}
		pose = (iqmPose_t *)((byte *)header + header->ofs_poses);
		for( i = 0; i < (int)header->num_poses; i++, pose++ ) {
			LL( pose->parent );
			LL( pose->mask );
			LL( pose->channeloffset[0] );
			LL( pose->channeloffset[1] );
			LL( pose->channeloffset[2] );
			LL( pose->channeloffset[3] );
			LL( pose->channeloffset[4] );
			LL( pose->channeloffset[5] );
			LL( pose->channeloffset[6] );
			LL( pose->channeloffset[7] );
			LL( pose->channeloffset[8] );
			LL( pose->channeloffset[9] );
			LL( pose->channelscale[0] );
			LL( pose->channelscale[1] );
			LL( pose->channelscale[2] );
			LL( pose->channelscale[3] );
			LL( pose->channelscale[4] );
			LL( pose->channelscale[5] );
			LL( pose->channelscale[6] );
			LL( pose->channelscale[7] );
			LL( pose->channelscale[8] );
			LL( pose->channelscale[9] );
		}
	}

	if (header->ofs_bounds)
	{
		// check and swap model bounds
		if(IQM_CheckRange(header, header->ofs_bounds,
				  header->num_frames, sizeof(*bounds)))
		{
			return qfalse;
		}
		bounds = (iqmBounds_t *) ((byte *) header + header->ofs_bounds);
		for(i = 0; i < (int)header->num_frames; i++)
		{
			LL(bounds->bbmin[0]);
			LL(bounds->bbmin[1]);
			LL(bounds->bbmin[2]);
			LL(bounds->bbmax[0]);
			LL(bounds->bbmax[1]);
			LL(bounds->bbmax[2]);

			bounds++;
		}
	}

	// allocate the model and copy the data
	size = sizeof(iqmData_t);
	if( header->num_meshes ) {
		size += header->num_meshes * sizeof( srfIQModel_t ); // surfaces
		size += header->num_triangles * 3 * sizeof(int);	// triangles
		size += header->num_vertexes * 3 * sizeof(float);	// positions
		size += header->num_vertexes * 2 * sizeof(float);	// texcoords
		size += header->num_vertexes * 3 * sizeof(float);	// normals

		if ( vertexArrayFormat[IQM_TANGENT] != -1 ) {
			size += header->num_vertexes * 4 * sizeof(float);	// tangents
		}

		if ( vertexArrayFormat[IQM_COLOR] != -1 ) {
			size += header->num_vertexes * 4 * sizeof(byte);	// colors
		}

		if ( allocateInfluences ) {
			size += header->num_vertexes * sizeof(int);			// influences
			size += allocateInfluences * 4 * sizeof(byte);		// influenceBlendIndexes

			if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_UBYTE ) {
				size += allocateInfluences * 4 * sizeof(byte);	// influenceBlendWeights
			} else if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT ) {
				size += allocateInfluences * 4 * sizeof(float);	// influenceBlendWeights
			}
		}
	}
	if( header->num_joints ) {
		size += joint_names;								// joint names
		size += header->num_joints * sizeof(int);			// joint parents
		size += header->num_joints * 12 * sizeof(float);	// bind joint matricies
		size += header->num_joints * 12 * sizeof(float);	// inverse bind joint matricies
	}
	if( header->num_poses ) {
		size += header->num_poses * header->num_frames * sizeof(iqmTransform_t); // pose transforms
	}
	if( header->ofs_bounds ) {
		size += header->num_frames * 6 * sizeof(float);		// model bounds
	} else if( header->num_meshes && header->num_frames == 0 ) {
		size += 6 * sizeof(float);							// model bounds
	}

	mod->type = MOD_IQM;
	iqmData = (iqmData_t *)ri.Hunk_Alloc( size, h_low );
	mod->modelData = iqmData;

	// fill header
	iqmData->num_vertexes = ( header->num_meshes > 0 ) ? header->num_vertexes : 0;
	iqmData->num_triangles = ( header->num_meshes > 0 ) ? header->num_triangles : 0;
	iqmData->num_frames   = header->num_frames;
	iqmData->num_surfaces = header->num_meshes;
	iqmData->num_joints   = header->num_joints;
	iqmData->num_poses    = header->num_poses;
	iqmData->blendWeightsType = vertexArrayFormat[IQM_BLENDWEIGHTS];

	dataPtr = (byte*)iqmData + sizeof(iqmData_t);
	if( header->num_meshes ) {
		iqmData->surfaces = (struct srfIQModel_s*)dataPtr;
		dataPtr += header->num_meshes * sizeof( srfIQModel_t );

		iqmData->triangles = (int*)dataPtr;
		dataPtr += header->num_triangles * 3 * sizeof(int);		// triangles

		iqmData->positions = (float*)dataPtr;
		dataPtr += header->num_vertexes * 3 * sizeof(float);	// positions

		iqmData->texcoords = (float*)dataPtr;
		dataPtr += header->num_vertexes * 2 * sizeof(float);	// texcoords

		iqmData->normals = (float*)dataPtr;
		dataPtr += header->num_vertexes * 3 * sizeof(float);	// normals

		if ( vertexArrayFormat[IQM_TANGENT] != -1 ) {
			iqmData->tangents = (float*)dataPtr;
			dataPtr += header->num_vertexes * 4 * sizeof(float);	// tangents
		}

		if ( vertexArrayFormat[IQM_COLOR] != -1 ) {
			iqmData->colors = (byte*)dataPtr;
			dataPtr += header->num_vertexes * 4 * sizeof(byte);		// colors
		}

		if ( allocateInfluences ) {
			iqmData->influences = (int*)dataPtr;
			dataPtr += header->num_vertexes * sizeof(int);			// influences

			iqmData->influenceBlendIndexes = (byte*)dataPtr;
			dataPtr += allocateInfluences * 4 * sizeof(byte);		// influenceBlendIndexes

			if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_UBYTE ) {
				iqmData->influenceBlendWeights.b = (byte*)dataPtr;
				dataPtr += allocateInfluences * 4 * sizeof(byte);	// influenceBlendWeights
			} else if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT ) {
				iqmData->influenceBlendWeights.f = (float*)dataPtr;
				dataPtr += allocateInfluences * 4 * sizeof(float);	// influenceBlendWeights
			}
		}
	}
	if( header->num_joints ) {
		iqmData->jointNames = (char*)dataPtr;
		dataPtr += joint_names;								// joint names

		iqmData->jointParents = (int*)dataPtr;
		dataPtr += header->num_joints * sizeof(int);		// joint parents

		iqmData->bindJoints = (float*)dataPtr;
		dataPtr += header->num_joints * 12 * sizeof(float);		// bind joint matricies

		iqmData->invBindJoints = (float*)dataPtr;
		dataPtr += header->num_joints * 12 * sizeof(float);		// inverse bind joint matricies
	}
	if( header->num_poses ) {
		iqmData->poses = (iqmTransform_t*)dataPtr;
		dataPtr += header->num_poses * header->num_frames * sizeof(iqmTransform_t); // pose transforms
	}
	if( header->ofs_bounds ) {
		iqmData->bounds = (float*)dataPtr;
		dataPtr += header->num_frames * 6 * sizeof(float);	// model bounds
	} else if( header->num_meshes && header->num_frames == 0 ) {
		iqmData->bounds = (float*)dataPtr;
		dataPtr += 6 * sizeof(float);						// model bounds
	}

	if( header->num_meshes )
	{
		// register shaders
		// overwrite the material offset with the shader index
		mesh = (iqmMesh_t *)((byte *)header + header->ofs_meshes);
		surface = iqmData->surfaces;
		str = (char *)header + header->ofs_text;
		for( i = 0; i < (int)header->num_meshes; i++, mesh++, surface++ ) {
			surface->surfaceType = SF_IQM;
			Q_strncpyz(surface->name, str + mesh->name, sizeof (surface->name));
			Q_strlwr(surface->name); // lowercase the surface name so skin compares are faster
			surface->shader = R_FindShader( str + mesh->material, LIGHTMAP_NONE, qtrue );
			if( surface->shader->defaultShader )
				surface->shader = tr.defaultShader;
			surface->data = iqmData;
			surface->first_vertex = mesh->first_vertex;
			surface->num_vertexes = mesh->num_vertexes;
			surface->first_triangle = mesh->first_triangle;
			surface->num_triangles = mesh->num_triangles;
			surface->morphSurfaceIndex = i;
		}

		// copy triangles
		triangle = (iqmTriangle_t *)((byte *)header + header->ofs_triangles);
		for( i = 0; i < (int)header->num_triangles; i++, triangle++ ) {
			iqmData->triangles[3*i+0] = triangle->vertex[0];
			iqmData->triangles[3*i+1] = triangle->vertex[1];
			iqmData->triangles[3*i+2] = triangle->vertex[2];
		}

		// copy vertexarrays and indexes
		vertexarray = (iqmVertexArray_t *)((byte *)header + header->ofs_vertexarrays);
		for( i = 0; i < (int)header->num_vertexarrays; i++, vertexarray++ ) {
			int	n;

			// skip disabled arrays
			if( vertexarray->type < ARRAY_LEN( vertexArrayFormat )
			    && vertexArrayFormat[vertexarray->type] == -1 )
				continue;

			// total number of values
			n = header->num_vertexes * vertexarray->size;

			switch( vertexarray->type ) {
			case IQM_POSITION:
				Com_Memcpy( iqmData->positions,
					    (byte *)header + vertexarray->offset,
					    n * sizeof(float) );
				break;
			case IQM_NORMAL:
				Com_Memcpy( iqmData->normals,
					    (byte *)header + vertexarray->offset,
					    n * sizeof(float) );
				break;
			case IQM_TANGENT:
				Com_Memcpy( iqmData->tangents,
					    (byte *)header + vertexarray->offset,
					    n * sizeof(float) );
				break;
			case IQM_TEXCOORD:
				Com_Memcpy( iqmData->texcoords,
					    (byte *)header + vertexarray->offset,
					    n * sizeof(float) );
				break;
			case IQM_BLENDINDEXES:
			case IQM_BLENDWEIGHTS:
				break;
			case IQM_COLOR:
				Com_Memcpy( iqmData->colors,
					    (byte *)header + vertexarray->offset,
					    n * sizeof(byte) );
				break;
			}
		}

		// find unique blend influences per mesh
		if( allocateInfluences ) {
			int vtx, influence, totalInfluences = 0;

			surface = iqmData->surfaces;
			for( i = 0; i < (int)header->num_meshes; i++, surface++ ) {
				surface->first_influence = totalInfluences;
				surface->num_influences = 0;

				for( j = 0; j < surface->num_vertexes; j++ ) {
					vtx = surface->first_vertex + j;

					for( k = 0; k < surface->num_influences; k++ ) {
						influence = surface->first_influence + k;

						if( *(int*)&iqmData->influenceBlendIndexes[4*influence] != *(int*)&blendIndexes[4*vtx] ) {
							continue;
						}

						if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT ) {
							if ( iqmData->influenceBlendWeights.f[4*influence+0] == blendWeights.f[4*vtx+0] &&
							     iqmData->influenceBlendWeights.f[4*influence+1] == blendWeights.f[4*vtx+1] &&
							     iqmData->influenceBlendWeights.f[4*influence+2] == blendWeights.f[4*vtx+2] &&
							     iqmData->influenceBlendWeights.f[4*influence+3] == blendWeights.f[4*vtx+3] ) {
								break;
							}
						} else {
							if ( *(int*)&iqmData->influenceBlendWeights.b[4*influence] == *(int*)&blendWeights.b[4*vtx] ) {
								break;
							}
						}
					}

					iqmData->influences[vtx] = surface->first_influence + k;

					if( k == surface->num_influences ) {
						influence = surface->first_influence + k;

						iqmData->influenceBlendIndexes[4*influence+0] = blendIndexes[4*vtx+0];
						iqmData->influenceBlendIndexes[4*influence+1] = blendIndexes[4*vtx+1];
						iqmData->influenceBlendIndexes[4*influence+2] = blendIndexes[4*vtx+2];
						iqmData->influenceBlendIndexes[4*influence+3] = blendIndexes[4*vtx+3];

						if( vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT ) {
							iqmData->influenceBlendWeights.f[4*influence+0] = blendWeights.f[4*vtx+0];
							iqmData->influenceBlendWeights.f[4*influence+1] = blendWeights.f[4*vtx+1];
							iqmData->influenceBlendWeights.f[4*influence+2] = blendWeights.f[4*vtx+2];
							iqmData->influenceBlendWeights.f[4*influence+3] = blendWeights.f[4*vtx+3];
						} else {
							iqmData->influenceBlendWeights.b[4*influence+0] = blendWeights.b[4*vtx+0];
							iqmData->influenceBlendWeights.b[4*influence+1] = blendWeights.b[4*vtx+1];
							iqmData->influenceBlendWeights.b[4*influence+2] = blendWeights.b[4*vtx+2];
							iqmData->influenceBlendWeights.b[4*influence+3] = blendWeights.b[4*vtx+3];
						}

						totalInfluences++;
						surface->num_influences++;
					}
				}
			}
		}
	}

	if( header->num_joints )
	{
		// copy joint names
		str = iqmData->jointNames;
		joint = (iqmJoint_t *)((byte *)header + header->ofs_joints);
		for( i = 0; i < (int)header->num_joints; i++, joint++ ) {
			char *name = (char *)header + header->ofs_text +
				joint->name;
			int len = strlen( name ) + 1;
			Com_Memcpy( str, name, len );
			str += len;
		}

		// copy joint parents
		joint = (iqmJoint_t *)((byte *)header + header->ofs_joints);
		for( i = 0; i < (int)header->num_joints; i++, joint++ ) {
			iqmData->jointParents[i] = joint->parent;
		}

		// calculate bind joint matrices and their inverses
		mat = iqmData->bindJoints;
		matInv = iqmData->invBindJoints;
		joint = (iqmJoint_t *)((byte *)header + header->ofs_joints);
		for( i = 0; i < (int)header->num_joints; i++, joint++ ) {
			float baseFrame[12], invBaseFrame[12];

			QuatNormalize2( joint->rotate, joint->rotate );

			JointToMatrix( joint->rotate, joint->scale, joint->translate, baseFrame );
			Matrix34Invert( baseFrame, invBaseFrame );

			if ( joint->parent >= 0 )
			{
				Matrix34Multiply( iqmData->bindJoints + 12 * joint->parent, baseFrame, mat );
				mat += 12;
				Matrix34Multiply( invBaseFrame, iqmData->invBindJoints + 12 * joint->parent, matInv );
				matInv += 12;
			}
			else
			{
				Com_Memcpy( mat, baseFrame,    sizeof(baseFrame)    );
				mat += 12;
				Com_Memcpy( matInv, invBaseFrame, sizeof(invBaseFrame) );
				matInv += 12;
			}
		}
	}

	if( header->num_poses )
	{
		// calculate pose transforms
		transform = iqmData->poses;
		framedata = (unsigned short *)((byte *)header + header->ofs_frames);
		for( i = 0; i < (int)header->num_frames; i++ ) {
			pose = (iqmPose_t *)((byte *)header + header->ofs_poses);
			for( j = 0; j < (int)header->num_poses; j++, pose++, transform++ ) {
				vec3_t	translate;
				quat_t	rotate;
				vec3_t	scale;

				translate[0] = pose->channeloffset[0];
				if( pose->mask & 0x001)
					translate[0] += *framedata++ * pose->channelscale[0];
				translate[1] = pose->channeloffset[1];
				if( pose->mask & 0x002)
					translate[1] += *framedata++ * pose->channelscale[1];
				translate[2] = pose->channeloffset[2];
				if( pose->mask & 0x004)
					translate[2] += *framedata++ * pose->channelscale[2];

				rotate[0] = pose->channeloffset[3];
				if( pose->mask & 0x008)
					rotate[0] += *framedata++ * pose->channelscale[3];
				rotate[1] = pose->channeloffset[4];
				if( pose->mask & 0x010)
					rotate[1] += *framedata++ * pose->channelscale[4];
				rotate[2] = pose->channeloffset[5];
				if( pose->mask & 0x020)
					rotate[2] += *framedata++ * pose->channelscale[5];
				rotate[3] = pose->channeloffset[6];
				if( pose->mask & 0x040)
					rotate[3] += *framedata++ * pose->channelscale[6];

				scale[0] = pose->channeloffset[7];
				if( pose->mask & 0x080)
					scale[0] += *framedata++ * pose->channelscale[7];
				scale[1] = pose->channeloffset[8];
				if( pose->mask & 0x100)
					scale[1] += *framedata++ * pose->channelscale[8];
				scale[2] = pose->channeloffset[9];
				if( pose->mask & 0x200)
					scale[2] += *framedata++ * pose->channelscale[9];

				VectorCopy( translate, transform->translate );
				QuatNormalize2( rotate, transform->rotate );
				VectorCopy( scale, transform->scale );
			}
		}
	}

	// copy model bounds
	if(header->ofs_bounds)
	{
		mat = iqmData->bounds;
		bounds = (iqmBounds_t *) ((byte *) header + header->ofs_bounds);
		for(i = 0; i < (int)header->num_frames; i++)
		{
			mat[0] = bounds->bbmin[0];
			mat[1] = bounds->bbmin[1];
			mat[2] = bounds->bbmin[2];
			mat[3] = bounds->bbmax[0];
			mat[4] = bounds->bbmax[1];
			mat[5] = bounds->bbmax[2];

			mat += 6;
			bounds++;
		}
	}
	else if( header->num_meshes && header->num_frames == 0 )
	{
		mat = iqmData->bounds;

		ClearBounds( &iqmData->bounds[0], &iqmData->bounds[3] );
		for ( i = 0 ; i < (int)header->num_vertexes ; i++ ) {
			AddPointToBounds( &iqmData->positions[i*3], &iqmData->bounds[0], &iqmData->bounds[3] );
		}
	}

	(void)R_LoadIQMMorphSidecar( iqmData, mod_name );

	return qtrue;
}

/*
=============
R_CullIQM
=============
*/
static int R_CullIQM( const iqmData_t *data, const trRefEntity_t *ent ) {
	vec3_t		bounds[2];
	vec_t		*oldBounds, *newBounds;
	int		i;

	if (!data->bounds) {
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	}

	// compute bounds pointers
	oldBounds = data->bounds + 6*ent->e.oldframe;
	newBounds = data->bounds + 6*ent->e.frame;

	// calculate a bounding box in the current coordinate system
	for (i = 0 ; i < 3 ; i++) {
		bounds[0][i] = oldBounds[i] < newBounds[i] ? oldBounds[i] : newBounds[i];
		bounds[1][i] = oldBounds[i+3] > newBounds[i+3] ? oldBounds[i+3] : newBounds[i+3];
	}

	switch ( R_CullLocalBox( bounds ) )
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

/*
=================
R_ComputeIQMFogNum

=================
*/
static int R_ComputeIQMFogNum( const iqmData_t *data, const trRefEntity_t *ent ) {
	int			i, j;
	const fog_t			*fog;
	const vec_t		*bounds;
	const vec_t		defaultBounds[6] = { -8, -8, -8, 8, 8, 8 };
	vec3_t			diag, center;
	vec3_t			localOrigin;
	vec_t			radius;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	/* Non-normalized axis may cause scaling artifacts. */
	if (data->bounds) {
		bounds = data->bounds + 6*ent->e.frame;
	} else {
		bounds = defaultBounds;
	}
	VectorSubtract( bounds+3, bounds, diag );
	VectorMA( bounds, 0.5f, diag, center );
	VectorAdd( ent->e.origin, center, localOrigin );
	radius = 0.5f * VectorLength( diag );

	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( localOrigin[j] - radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( localOrigin[j] + radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

static int R_IQMFindMorphTargetByHash( const iqmData_t *data, uint32_t hash ) {
	int i;

	for ( i = 0; i < data->morphNumTargets; i++ ) {
		if ( data->morphTargets[i].nameHash == hash ) {
			return i;
		}
	}
	return -1;
}

static float R_IQMMorphLodFactor( const trRefEntity_t *ent ) {
	float start;
	float end;
	vec3_t toEntity;
	float dist;

	if ( !r_morphLodStart || !r_morphLodEnd ) {
		return 1.0f;
	}

	start = r_morphLodStart->value;
	end = r_morphLodEnd->value;
	if ( end <= start ) {
		return 1.0f;
	}

	VectorSubtract( ent->e.origin, tr.viewParms.or.origin, toEntity );
	dist = VectorLength( toEntity );

	if ( dist <= start ) {
		return 1.0f;
	}
	if ( dist >= end ) {
		return 0.0f;
	}
	return 1.0f - ( dist - start ) / ( end - start );
}

static void R_IQMInsertMorphTopK( int *count, int maxCount, int targetIndex, float weight,
	int *outIndices, float *outWeights, float *outAbsWeights ) {
	float absWeight = fabsf( weight );
	int insertPos;

	if ( absWeight <= 0.0001f ) {
		return;
	}

	if ( *count < maxCount ) {
		insertPos = ( *count )++;
	} else {
		if ( absWeight < outAbsWeights[maxCount - 1] ) {
			return;
		}
		if ( absWeight == outAbsWeights[maxCount - 1] && targetIndex > outIndices[maxCount - 1] ) {
			return;
		}
		insertPos = maxCount - 1;
	}

	while ( insertPos > 0 ) {
		if ( absWeight > outAbsWeights[insertPos - 1] ||
			( absWeight == outAbsWeights[insertPos - 1] && targetIndex < outIndices[insertPos - 1] ) ) {
			outIndices[insertPos] = outIndices[insertPos - 1];
			outWeights[insertPos] = outWeights[insertPos - 1];
			outAbsWeights[insertPos] = outAbsWeights[insertPos - 1];
			insertPos--;
		} else {
			break;
		}
	}

	outIndices[insertPos] = targetIndex;
	outWeights[insertPos] = weight;
	outAbsWeights[insertPos] = absWeight;
}

static void R_IQMSelectActiveMorphTargets( const iqmData_t *data, trRefEntity_t *ent ) {
	int i;
	int activeCount = 0;
	int maxActive;
	float lodFactor;
	int activeIndices[IQM_MORPH_TOP_K];
	float activeWeights[IQM_MORPH_TOP_K];
	float activeAbs[IQM_MORPH_TOP_K];
	static uint32_t breathHash;
	float breathWeight;
	int breathIndex;

	ent->morphActiveCount = 0;
	ent->morphDebugMaxAbsWeight = 0.0f;
	for ( i = 0; i < IQM_MORPH_TOP_K; i++ ) {
		ent->morphActiveTargetIndex[i] = -1;
		ent->morphActiveWeight[i] = 0.0f;
		activeIndices[i] = -1;
		activeWeights[i] = 0.0f;
		activeAbs[i] = 0.0f;
	}

	if ( !r_morph || !r_morph->integer || !data->morphTargets || !data->morphSurfaces || data->morphNumTargets <= 0 ) {
		return;
	}

	maxActive = r_morphMaxActive ? r_morphMaxActive->integer : IQM_MORPH_TOP_K;
	if ( maxActive < 1 ) {
		maxActive = 1;
	} else if ( maxActive > IQM_MORPH_TOP_K ) {
		maxActive = IQM_MORPH_TOP_K;
	}

	lodFactor = R_IQMMorphLodFactor( ent );
	if ( lodFactor <= 0.0f ) {
		return;
	}

	for ( i = 0; i < ent->morphChannelCount; i++ ) {
		int targetIndex = R_IQMFindMorphTargetByHash( data, ent->morphChannelHashes[i] );
		float effectiveWeight;
		if ( targetIndex < 0 ) {
			continue;
		}
		effectiveWeight = ent->morphChannelWeights[i] * lodFactor;
		R_IQMInsertMorphTopK( &activeCount, maxActive, targetIndex, effectiveWeight,
			activeIndices, activeWeights, activeAbs );
	}

	if ( r_morphBreath && r_morphBreath->integer ) {
		if ( !breathHash ) {
			breathHash = (uint32_t)Com_GenerateHashValue( "breath", 0x7fffffffU );
		}
		breathIndex = R_IQMFindMorphTargetByHash( data, breathHash );
		if ( breathIndex >= 0 ) {
			float freq = r_morphBreathFreq ? r_morphBreathFreq->value : 0.33f;
			float amp = r_morphBreathAmp ? r_morphBreathAmp->value : 0.25f;
			float t = (float)tr.refdef.time * 0.001f;
			breathWeight = sinf( t * freq * 6.28318530718f ) * amp * lodFactor;
			R_IQMInsertMorphTopK( &activeCount, maxActive, breathIndex, breathWeight,
				activeIndices, activeWeights, activeAbs );
		}
	}

	ent->morphActiveCount = activeCount;
	for ( i = 0; i < activeCount; i++ ) {
		ent->morphActiveTargetIndex[i] = activeIndices[i];
		ent->morphActiveWeight[i] = activeWeights[i];
	}
	if ( activeCount > 0 ) {
		ent->morphDebugMaxAbsWeight = activeAbs[0];
	}
}

#define IQM_GPU_META_TAG	0xAB000000u
#define IQM_GPU_INDEX_MASK	0x00000FFFu
#define IQM_GPU_MORPH_SHIFT	12

typedef struct {
	qboolean	enabled;
	int			skinCount;
	int			morphVertexCount;
	int			activeCount;
	float		weights[IQM_MORPH_TOP_K];
	float		prevWeights[IQM_MORPH_TOP_K];
	float		skinMatrices[SHADER_MAX_VERTEXES * 12];
	float		normalMatrices[SHADER_MAX_VERTEXES * 9];
	float		prevSkinMatrices[SHADER_MAX_VERTEXES * 12];
	float		prevNormalMatrices[SHADER_MAX_VERTEXES * 9];
	float		morphDeltas[SHADER_MAX_VERTEXES * IQM_MORPH_TOP_K * 6];
} iqmGpuBatchState_t;

static iqmGpuBatchState_t s_iqmGpuBatch;

static float R_IQMPackGpuMeta( int skinIndex, int morphIndex )
{
	uint32_t packed;
	float out;

	packed = IQM_GPU_META_TAG |
		( ( (uint32_t)morphIndex & IQM_GPU_INDEX_MASK ) << IQM_GPU_MORPH_SHIFT ) |
		( (uint32_t)skinIndex & IQM_GPU_INDEX_MASK );
	Com_Memcpy( &out, &packed, sizeof( out ) );
	return out;
}

void R_IQMBeginSurfaceBatch( void )
{
	int i;

	s_iqmGpuBatch.enabled = qfalse;
	s_iqmGpuBatch.skinCount = 0;
	s_iqmGpuBatch.morphVertexCount = 0;
	s_iqmGpuBatch.activeCount = 0;
	for ( i = 0; i < IQM_MORPH_TOP_K; i++ ) {
		s_iqmGpuBatch.weights[i] = 0.0f;
		s_iqmGpuBatch.prevWeights[i] = 0.0f;
	}
}

void R_IQMCommitSurfaceBatch( void )
{
	int i;
	size_t skinFloats;
	size_t skinBytes;
	size_t morphFloats;
	size_t morphBytes;
	uint32_t skinOffset;
	uint32_t morphOffset;
	float *skinPayload;
	float *morphPayload;

	if ( !s_iqmGpuBatch.enabled || s_iqmGpuBatch.skinCount <= 0 ||
		s_iqmGpuBatch.morphVertexCount <= 0 || s_iqmGpuBatch.activeCount <= 0 ) {
		vk_reset_iqm_storage_offsets();
		return;
	}

	skinFloats = 1u + 2u * ( (size_t)s_iqmGpuBatch.skinCount * 12u + (size_t)s_iqmGpuBatch.skinCount * 9u );
	skinBytes = skinFloats * sizeof( float );
	skinPayload = (float *)vk_alloc_storage( skinBytes, &skinOffset );
	if ( !skinPayload ) {
		vk_reset_iqm_storage_offsets();
		return;
	}

	skinPayload[0] = (float)s_iqmGpuBatch.skinCount;
	Com_Memcpy( skinPayload + 1, s_iqmGpuBatch.skinMatrices,
		(size_t)s_iqmGpuBatch.skinCount * 12u * sizeof( float ) );
	Com_Memcpy( skinPayload + 1 + (size_t)s_iqmGpuBatch.skinCount * 12u,
		s_iqmGpuBatch.normalMatrices, (size_t)s_iqmGpuBatch.skinCount * 9u * sizeof( float ) );
	Com_Memcpy( skinPayload + 1 + (size_t)s_iqmGpuBatch.skinCount * 12u + (size_t)s_iqmGpuBatch.skinCount * 9u,
		s_iqmGpuBatch.prevSkinMatrices, (size_t)s_iqmGpuBatch.skinCount * 12u * sizeof( float ) );
	Com_Memcpy( skinPayload + 1 + 2u * (size_t)s_iqmGpuBatch.skinCount * 12u + (size_t)s_iqmGpuBatch.skinCount * 9u,
		s_iqmGpuBatch.prevNormalMatrices, (size_t)s_iqmGpuBatch.skinCount * 9u * sizeof( float ) );

	morphFloats = (size_t)( 2 + 2u * (size_t)IQM_MORPH_TOP_K ) + (size_t)s_iqmGpuBatch.morphVertexCount * IQM_MORPH_TOP_K * 6u;
	morphBytes = morphFloats * sizeof( float );
	morphPayload = (float *)vk_alloc_storage( morphBytes, &morphOffset );
	if ( !morphPayload ) {
		vk_reset_iqm_storage_offsets();
		return;
	}

	morphPayload[0] = (float)s_iqmGpuBatch.morphVertexCount;
	morphPayload[1] = (float)s_iqmGpuBatch.activeCount;
	for ( i = 0; i < IQM_MORPH_TOP_K; i++ ) {
		morphPayload[2 + i] = s_iqmGpuBatch.weights[i];
		morphPayload[2 + IQM_MORPH_TOP_K + i] = s_iqmGpuBatch.prevWeights[i];
	}
	Com_Memcpy( morphPayload + 2 + 2u * (size_t)IQM_MORPH_TOP_K, s_iqmGpuBatch.morphDeltas,
		(size_t)s_iqmGpuBatch.morphVertexCount * IQM_MORPH_TOP_K * 6u * sizeof( float ) );

	vk_set_iqm_storage_offsets( skinOffset, morphOffset );
}

/*
=================
R_AddIQMSurfaces

Add all surfaces of this model
=================
*/
void R_AddIQMSurfaces( trRefEntity_t *ent ) {
	iqmData_t		*data;
	srfIQModel_t		*surface;
	int			i, j;
	qboolean		personalModel;
	int			cull;
	int			fogNum;
	shader_t		*shader;
	const skin_t			*skin;

	data = tr.currentModel->modelData;
	surface = data->surfaces;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE);

	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		ent->e.frame %= data->num_frames;
		ent->e.oldframe %= data->num_frames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ( (ent->e.frame >= data->num_frames) 
	     || (ent->e.frame < 0)
	     || (ent->e.oldframe >= data->num_frames)
	     || (ent->e.oldframe < 0) ) {
		ri.Printf( PRINT_DEVELOPER, "R_AddIQMSurfaces: no such frame %d to %d for '%s'\n",
			   ent->e.oldframe, ent->e.frame,
			   tr.currentModel->name );
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	cull = R_CullIQM ( data, ent );
	if ( cull == CULL_OUT ) {
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if ( !personalModel || r_shadows->integer > 1 ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_ComputeIQMFogNum( data, ent );
	R_IQMSelectActiveMorphTargets( data, ent );

	for ( i = 0 ; i < data->num_surfaces ; i++ ) {
		if(ent->e.customShader)
			shader = R_GetShaderByHandle( ent->e.customShader );
		else if(ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins)
		{
			skin = R_GetSkinByHandle(ent->e.customSkin);
			shader = tr.defaultShader;

			for(j = 0; j < skin->numSurfaces; j++)
			{
				if (!strcmp(skin->surfaces[j].name, surface->name))
				{
					shader = skin->surfaces[j].shader;
					break;
				}
			}
		} else {
			shader = surface->shader;
		}

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if ( !personalModel
			&& r_shadows->integer == 2 
			&& fogNum == 0
			&& !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) ) 
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (void *)surface, tr.shadowShader, 0, 0 );
		}

		// projection shadows work fine with personal models
		if ( r_shadows->integer == 3
			&& fogNum == 0
			&& (ent->e.renderfx & RF_SHADOW_PLANE )
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (void *)surface, tr.projectionShadowShader, 0, 0 );
		}

		if ( !personalModel ) {
			R_AddDrawSurf( (void *)surface, shader, fogNum, 0 );
			tr.needScreenMap |= shader->hasScreenMap;
		}

		surface++;
	}
}


static void ComputePoseMats( iqmData_t *data, int frame, int oldframe,
			      float backlerp, float *poseMats ) {
	iqmTransform_t relativeJoints[IQM_MAX_JOINTS];
	iqmTransform_t *relativeJoint;
	const iqmTransform_t *pose;
	const iqmTransform_t *oldpose;
	const int *jointParent;
	const float *invBindMat;
	float *poseMat, lerp;
	int i;

	relativeJoint = relativeJoints;

	// copy or lerp animation frame pose
	if ( oldframe == frame ) {
		pose = &data->poses[frame * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, pose++, relativeJoint++ ) {
			VectorCopy( pose->translate, relativeJoint->translate );
			QuatCopy( pose->rotate, relativeJoint->rotate );
			VectorCopy( pose->scale, relativeJoint->scale );
		}
	} else {
		lerp = 1.0f - backlerp;
		pose = &data->poses[frame * data->num_poses];
		oldpose = &data->poses[oldframe * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, oldpose++, pose++, relativeJoint++ ) {
			relativeJoint->translate[0] = oldpose->translate[0] * backlerp + pose->translate[0] * lerp;
			relativeJoint->translate[1] = oldpose->translate[1] * backlerp + pose->translate[1] * lerp;
			relativeJoint->translate[2] = oldpose->translate[2] * backlerp + pose->translate[2] * lerp;

			relativeJoint->scale[0] = oldpose->scale[0] * backlerp + pose->scale[0] * lerp;
			relativeJoint->scale[1] = oldpose->scale[1] * backlerp + pose->scale[1] * lerp;
			relativeJoint->scale[2] = oldpose->scale[2] * backlerp + pose->scale[2] * lerp;

			QuatSlerp( oldpose->rotate, pose->rotate, lerp, relativeJoint->rotate );
		}
	}

	// multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
	relativeJoint = relativeJoints;
	jointParent = data->jointParents;
	invBindMat = data->invBindJoints;
	poseMat = poseMats;
	for ( i = 0; i < data->num_poses; i++, relativeJoint++, jointParent++, invBindMat += 12, poseMat += 12 ) {
		float mat1[12], mat2[12];

		JointToMatrix( relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1 );

		if ( *jointParent >= 0 ) {
			Matrix34Multiply( &data->bindJoints[(*jointParent)*12], mat1, mat2 );
			Matrix34Multiply( mat2, invBindMat, mat1 );
			Matrix34Multiply( &poseMats[(*jointParent)*12], mat1, poseMat );
		} else {
			Matrix34Multiply( mat1, invBindMat, poseMat );
		}
	}
}

static void ComputeJointMats( iqmData_t *data, int frame, int oldframe,
			      float backlerp, float *mat ) {
	float	*mat1;
	int	i;

	if ( data->num_poses == 0 ) {
		Com_Memcpy( mat, data->bindJoints, data->num_joints * 12 * sizeof(float) );
		return;
	}

	ComputePoseMats( data, frame, oldframe, backlerp, mat );

	for( i = 0; i < data->num_joints; i++ ) {
		float outmat[12];
		mat1 = mat + 12 * i;

		Com_Memcpy(outmat, mat1, sizeof(outmat));

		Matrix34Multiply( outmat, data->bindJoints + 12*i, mat1 );
	}
}


/*
=================
RB_AddIQMSurfaces

Compute vertices for this model surface
=================
*/
void RB_IQMSurfaceAnim( const surfaceType_t *surface ) {
	const srfIQModel_t	*surf = (const srfIQModel_t *)surface;
	iqmData_t	*data = surf->data;
	const trRefEntity_t *ent = backEnd.currentEntity;
	const iqmMorphSurface_t *morphSurface = NULL;
	qboolean applyMorph = qfalse;
	qboolean useGpuMorphPath = qfalse;
	int gpuSkinBase = 0;
	int gpuMorphBase = 0;
	float		poseMats[IQM_MAX_JOINTS * 12];
	float		poseMatsPrev[IQM_MAX_JOINTS * 12];
	float		influenceVtxMat[SHADER_MAX_VERTEXES * 12];
	float		influenceNrmMat[SHADER_MAX_VERTEXES * 9];
	int		i;

	float		*xyz;
	float		*normal;
	float		*texCoords;
	byte		*color;
	vec4_t		*outXYZ;
	vec4_t		*outNormal;
	float		*outTexCoord;
	color4ub_t	*outColor;

	int	frame = data->num_frames ? backEnd.currentEntity->e.frame % data->num_frames : 0;
	int	oldframe = data->num_frames ? backEnd.currentEntity->e.oldframe % data->num_frames : 0;
	float	backlerp = backEnd.currentEntity->e.backlerp;

	int		*tri;
	glIndex_t	*ptr;
	glIndex_t	base;

	if ( data->morphSurfaces && surf->morphSurfaceIndex >= 0 && surf->morphSurfaceIndex < data->num_surfaces ) {
		morphSurface = &data->morphSurfaces[surf->morphSurfaceIndex];
	}
	applyMorph = ( ent && ent->morphActiveCount > 0 && morphSurface &&
		morphSurface->deltaPos && morphSurface->deltaNorm &&
		ent->morphActiveCount <= IQM_MORPH_TOP_K );
	useGpuMorphPath = ( applyMorph && data->num_poses > 0 );

	RB_CHECKOVERFLOW( surf->num_vertexes, surf->num_triangles * 3 );

	xyz = &data->positions[surf->first_vertex * 3];
	normal = &data->normals[surf->first_vertex * 3];
	texCoords = &data->texcoords[surf->first_vertex * 2];

	if ( data->colors ) {
		color = &data->colors[surf->first_vertex * 4];
	} else {
		color = NULL;
	}

	outXYZ = &tess.xyz[tess.numVertexes];
	outNormal = &tess.normal[tess.numVertexes];
	outTexCoord = &tess.texCoords[0][tess.numVertexes][0];
	outColor = &tess.vertexColors[tess.numVertexes];

	if ( useGpuMorphPath ) {
		if ( surf->num_influences > SHADER_MAX_VERTEXES || surf->num_vertexes > SHADER_MAX_VERTEXES ) {
			useGpuMorphPath = qfalse;
		}
		if ( s_iqmGpuBatch.skinCount + surf->num_influences > SHADER_MAX_VERTEXES ||
			s_iqmGpuBatch.morphVertexCount + surf->num_vertexes > SHADER_MAX_VERTEXES ) {
			useGpuMorphPath = qfalse;
		}
		if ( s_iqmGpuBatch.skinCount + surf->num_influences > (int)IQM_GPU_INDEX_MASK ||
			s_iqmGpuBatch.morphVertexCount + surf->num_vertexes > (int)IQM_GPU_INDEX_MASK ) {
			useGpuMorphPath = qfalse;
		}
		if ( s_iqmGpuBatch.enabled ) {
			if ( s_iqmGpuBatch.activeCount != ent->morphActiveCount ) {
				useGpuMorphPath = qfalse;
			} else {
				for ( i = 0; i < ent->morphActiveCount; i++ ) {
					if ( fabsf( s_iqmGpuBatch.weights[i] - ent->morphActiveWeight[i] ) > 0.0001f ) {
						useGpuMorphPath = qfalse;
						break;
					}
				}
			}
		}
		if ( useGpuMorphPath ) {
			int weightIndex;
			gpuSkinBase = s_iqmGpuBatch.skinCount;
			gpuMorphBase = s_iqmGpuBatch.morphVertexCount;
			if ( !s_iqmGpuBatch.enabled ) {
				s_iqmGpuBatch.enabled = qtrue;
				s_iqmGpuBatch.activeCount = ent->morphActiveCount;
				for ( weightIndex = 0; weightIndex < IQM_MORPH_TOP_K; weightIndex++ ) {
					s_iqmGpuBatch.weights[weightIndex] =
						( weightIndex < ent->morphActiveCount ) ? ent->morphActiveWeight[weightIndex] : 0.0f;
				}
			}
			for ( weightIndex = 0; weightIndex < IQM_MORPH_TOP_K; weightIndex++ ) {
				if ( s_iqmGpuBatch.activeCount == ent->morphActiveCount ) {
					s_iqmGpuBatch.prevWeights[weightIndex] =
						( weightIndex < ent->morphActiveCount ) ? ent->morphGpuWeightPrev[weightIndex] : 0.0f;
				} else {
					s_iqmGpuBatch.prevWeights[weightIndex] = s_iqmGpuBatch.weights[weightIndex];
				}
			}
		}
	}

	if ( data->num_poses > 0 ) {
		// compute interpolated joint matrices
		ComputePoseMats( data, frame, oldframe, backlerp, poseMats );
		if ( oldframe != frame ) {
			ComputePoseMats( data, oldframe, oldframe, 0.0f, poseMatsPrev );
		} else {
			Com_Memcpy( poseMatsPrev, poseMats, sizeof( poseMatsPrev ) );
		}

		// compute vertex blend influence matricies
		for( i = 0; i < surf->num_influences; i++ ) {
			int influence = surf->first_influence + i;
			float *vtxMat = &influenceVtxMat[12*i];
			float *nrmMat = &influenceNrmMat[9*i];
			int	j;
			float	blendWeights[4];

			if ( data->blendWeightsType == IQM_FLOAT ) {
				blendWeights[0] = data->influenceBlendWeights.f[4*influence + 0];
				blendWeights[1] = data->influenceBlendWeights.f[4*influence + 1];
				blendWeights[2] = data->influenceBlendWeights.f[4*influence + 2];
				blendWeights[3] = data->influenceBlendWeights.f[4*influence + 3];
			} else {
				blendWeights[0] = (float)data->influenceBlendWeights.b[4*influence + 0] / 255.0f;
				blendWeights[1] = (float)data->influenceBlendWeights.b[4*influence + 1] / 255.0f;
				blendWeights[2] = (float)data->influenceBlendWeights.b[4*influence + 2] / 255.0f;
				blendWeights[3] = (float)data->influenceBlendWeights.b[4*influence + 3] / 255.0f;
			}

			if ( blendWeights[0] <= 0.0f ) {
				// no blend joint, use identity matrix.
				vtxMat[0] = identityMatrix[0];
				vtxMat[1] = identityMatrix[1];
				vtxMat[2] = identityMatrix[2];
				vtxMat[3] = identityMatrix[3];
				vtxMat[4] = identityMatrix[4];
				vtxMat[5] = identityMatrix[5];
				vtxMat[6] = identityMatrix[6];
				vtxMat[7] = identityMatrix[7];
				vtxMat[8] = identityMatrix[8];
				vtxMat[9] = identityMatrix[9];
				vtxMat[10] = identityMatrix[10];
				vtxMat[11] = identityMatrix[11];
			} else {
				// compute the vertex matrix by blending the up to
				// four blend weights
				vtxMat[0] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 0];
				vtxMat[1] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 1];
				vtxMat[2] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 2];
				vtxMat[3] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 3];
				vtxMat[4] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 4];
				vtxMat[5] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 5];
				vtxMat[6] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 6];
				vtxMat[7] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 7];
				vtxMat[8] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 8];
				vtxMat[9] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 9];
				vtxMat[10] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 10];
				vtxMat[11] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 11];

				for ( j = 1; j < (int)ARRAY_LEN( blendWeights ); j++ ) {
					if ( blendWeights[j] <= 0.0f ) {
						break;
					}

					vtxMat[0] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 0];
					vtxMat[1] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 1];
					vtxMat[2] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 2];
					vtxMat[3] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 3];
					vtxMat[4] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 4];
					vtxMat[5] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 5];
					vtxMat[6] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 6];
					vtxMat[7] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 7];
					vtxMat[8] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 8];
					vtxMat[9] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 9];
					vtxMat[10] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 10];
					vtxMat[11] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 11];
				}
			}

			// compute the normal matrix as transpose of the adjoint
			// of the vertex matrix
			nrmMat[ 0] = vtxMat[ 5]*vtxMat[10] - vtxMat[ 6]*vtxMat[ 9];
			nrmMat[ 1] = vtxMat[ 6]*vtxMat[ 8] - vtxMat[ 4]*vtxMat[10];
			nrmMat[ 2] = vtxMat[ 4]*vtxMat[ 9] - vtxMat[ 5]*vtxMat[ 8];
			nrmMat[ 3] = vtxMat[ 2]*vtxMat[ 9] - vtxMat[ 1]*vtxMat[10];
			nrmMat[ 4] = vtxMat[ 0]*vtxMat[10] - vtxMat[ 2]*vtxMat[ 8];
			nrmMat[ 5] = vtxMat[ 1]*vtxMat[ 8] - vtxMat[ 0]*vtxMat[ 9];
			nrmMat[ 6] = vtxMat[ 1]*vtxMat[ 6] - vtxMat[ 2]*vtxMat[ 5];
			nrmMat[ 7] = vtxMat[ 2]*vtxMat[ 4] - vtxMat[ 0]*vtxMat[ 6];
			nrmMat[ 8] = vtxMat[ 0]*vtxMat[ 5] - vtxMat[ 1]*vtxMat[ 4];

			if ( useGpuMorphPath ) {
				float *vtxPrev;
				float *nrmPrev;
				float vtxMatPrev[12];
				float nrmMatPrev[9];
				int jp;

				Com_Memcpy( &s_iqmGpuBatch.skinMatrices[( gpuSkinBase + i ) * 12], vtxMat, 12 * sizeof( float ) );
				Com_Memcpy( &s_iqmGpuBatch.normalMatrices[( gpuSkinBase + i ) * 9], nrmMat, 9 * sizeof( float ) );

				vtxPrev = vtxMatPrev;
				if ( blendWeights[0] <= 0.0f ) {
					Com_Memcpy( vtxPrev, identityMatrix, 12 * sizeof( float ) );
				} else {
					vtxPrev[0] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 0];
					vtxPrev[1] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 1];
					vtxPrev[2] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 2];
					vtxPrev[3] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 3];
					vtxPrev[4] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 4];
					vtxPrev[5] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 5];
					vtxPrev[6] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 6];
					vtxPrev[7] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 7];
					vtxPrev[8] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 8];
					vtxPrev[9] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 9];
					vtxPrev[10] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 10];
					vtxPrev[11] = blendWeights[0] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + 0] + 11];
					for ( jp = 1; jp < (int)ARRAY_LEN( blendWeights ); jp++ ) {
						if ( blendWeights[jp] <= 0.0f ) {
							break;
						}
						vtxPrev[0] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 0];
						vtxPrev[1] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 1];
						vtxPrev[2] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 2];
						vtxPrev[3] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 3];
						vtxPrev[4] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 4];
						vtxPrev[5] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 5];
						vtxPrev[6] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 6];
						vtxPrev[7] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 7];
						vtxPrev[8] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 8];
						vtxPrev[9] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 9];
						vtxPrev[10] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 10];
						vtxPrev[11] += blendWeights[jp] * poseMatsPrev[12 * data->influenceBlendIndexes[4*influence + jp] + 11];
					}
				}
				nrmPrev = nrmMatPrev;
				nrmPrev[0] = vtxPrev[5]*vtxPrev[10] - vtxPrev[6]*vtxPrev[9];
				nrmPrev[1] = vtxPrev[6]*vtxPrev[8] - vtxPrev[4]*vtxPrev[10];
				nrmPrev[2] = vtxPrev[4]*vtxPrev[9] - vtxPrev[5]*vtxPrev[8];
				nrmPrev[3] = vtxPrev[2]*vtxPrev[9] - vtxPrev[1]*vtxPrev[10];
				nrmPrev[4] = vtxPrev[0]*vtxPrev[10] - vtxPrev[2]*vtxPrev[8];
				nrmPrev[5] = vtxPrev[1]*vtxPrev[8] - vtxPrev[0]*vtxPrev[9];
				nrmPrev[6] = vtxPrev[1]*vtxPrev[6] - vtxPrev[2]*vtxPrev[5];
				nrmPrev[7] = vtxPrev[2]*vtxPrev[4] - vtxPrev[0]*vtxPrev[6];
				nrmPrev[8] = vtxPrev[0]*vtxPrev[5] - vtxPrev[1]*vtxPrev[4];
				Com_Memcpy( &s_iqmGpuBatch.prevSkinMatrices[( gpuSkinBase + i ) * 12], vtxMatPrev, 12 * sizeof( float ) );
				Com_Memcpy( &s_iqmGpuBatch.prevNormalMatrices[( gpuSkinBase + i ) * 9], nrmMatPrev, 9 * sizeof( float ) );
			}
		}

		// transform vertexes and fill other data
			for( i = 0; i < surf->num_vertexes;
			     i++, xyz+=3, normal+=3, texCoords+=2,
			     outXYZ++, outNormal++, outTexCoord+=2 ) {
				int influence = data->influences[surf->first_vertex + i] - surf->first_influence;
				float *vtxMat = &influenceVtxMat[12*influence];
				float *nrmMat = &influenceNrmMat[9*influence];
				float morphedPos[3];
				float morphedNormal[3];
				int morphIndex;

				morphedPos[0] = xyz[0];
				morphedPos[1] = xyz[1];
				morphedPos[2] = xyz[2];
				morphedNormal[0] = normal[0];
				morphedNormal[1] = normal[1];
				morphedNormal[2] = normal[2];
				outTexCoord[0] = texCoords[0];
				outTexCoord[1] = texCoords[1];

				if ( useGpuMorphPath ) {
					size_t packedBase = (size_t)( gpuMorphBase + i ) * IQM_MORPH_TOP_K * 6u;
					int k;

					(*outXYZ)[0] = morphedPos[0];
					(*outXYZ)[1] = morphedPos[1];
					(*outXYZ)[2] = morphedPos[2];
					(*outXYZ)[3] = R_IQMPackGpuMeta( gpuSkinBase + influence, gpuMorphBase + i );

					(*outNormal)[0] = morphedNormal[0];
					(*outNormal)[1] = morphedNormal[1];
					(*outNormal)[2] = morphedNormal[2];
					(*outNormal)[3] = 0.0f;

					for ( k = 0; k < IQM_MORPH_TOP_K; k++ ) {
						size_t dst = packedBase + (size_t)k * 6u;

						s_iqmGpuBatch.morphDeltas[dst + 0] = 0.0f;
						s_iqmGpuBatch.morphDeltas[dst + 1] = 0.0f;
						s_iqmGpuBatch.morphDeltas[dst + 2] = 0.0f;
						s_iqmGpuBatch.morphDeltas[dst + 3] = 0.0f;
						s_iqmGpuBatch.morphDeltas[dst + 4] = 0.0f;
						s_iqmGpuBatch.morphDeltas[dst + 5] = 0.0f;

						if ( k < ent->morphActiveCount ) {
							int targetIndex = ent->morphActiveTargetIndex[k];
							size_t deltaOffset;
							const float *deltaPos;
							const float *deltaNorm;

							if ( targetIndex < 0 || targetIndex >= data->morphNumTargets ) {
								continue;
							}

							deltaOffset = ( (size_t)targetIndex * (size_t)surf->num_vertexes + (size_t)i ) * 3u;
							deltaPos = morphSurface->deltaPos + deltaOffset;
							deltaNorm = morphSurface->deltaNorm + deltaOffset;

							s_iqmGpuBatch.morphDeltas[dst + 0] = deltaPos[0];
							s_iqmGpuBatch.morphDeltas[dst + 1] = deltaPos[1];
							s_iqmGpuBatch.morphDeltas[dst + 2] = deltaPos[2];
							s_iqmGpuBatch.morphDeltas[dst + 3] = deltaNorm[0];
							s_iqmGpuBatch.morphDeltas[dst + 4] = deltaNorm[1];
							s_iqmGpuBatch.morphDeltas[dst + 5] = deltaNorm[2];
						}
					}
				} else {
					if ( applyMorph ) {
						for ( morphIndex = 0; morphIndex < ent->morphActiveCount; morphIndex++ ) {
							int targetIndex = ent->morphActiveTargetIndex[morphIndex];
							float weight = ent->morphActiveWeight[morphIndex];
							size_t deltaOffset;
							const float *deltaPos;
							const float *deltaNorm;

							if ( targetIndex < 0 || targetIndex >= data->morphNumTargets ) {
								continue;
							}
							deltaOffset = ( (size_t)targetIndex * (size_t)surf->num_vertexes + (size_t)i ) * 3u;
							deltaPos = morphSurface->deltaPos + deltaOffset;
							deltaNorm = morphSurface->deltaNorm + deltaOffset;
							morphedPos[0] += deltaPos[0] * weight;
							morphedPos[1] += deltaPos[1] * weight;
							morphedPos[2] += deltaPos[2] * weight;
							morphedNormal[0] += deltaNorm[0] * weight;
							morphedNormal[1] += deltaNorm[1] * weight;
							morphedNormal[2] += deltaNorm[2] * weight;
						}
						VectorNormalize( morphedNormal );
					}

					(*outXYZ)[0] =
						vtxMat[ 0] * morphedPos[0] +
						vtxMat[ 1] * morphedPos[1] +
						vtxMat[ 2] * morphedPos[2] +
						vtxMat[ 3];
					(*outXYZ)[1] =
						vtxMat[ 4] * morphedPos[0] +
						vtxMat[ 5] * morphedPos[1] +
						vtxMat[ 6] * morphedPos[2] +
						vtxMat[ 7];
					(*outXYZ)[2] =
						vtxMat[ 8] * morphedPos[0] +
						vtxMat[ 9] * morphedPos[1] +
						vtxMat[10] * morphedPos[2] +
						vtxMat[11];
					(*outXYZ)[3] = 0.0f;

					(*outNormal)[0] =
						nrmMat[ 0] * morphedNormal[0] +
						nrmMat[ 1] * morphedNormal[1] +
						nrmMat[ 2] * morphedNormal[2];
					(*outNormal)[1] =
						nrmMat[ 3] * morphedNormal[0] +
						nrmMat[ 4] * morphedNormal[1] +
						nrmMat[ 5] * morphedNormal[2];
					(*outNormal)[2] =
						nrmMat[ 6] * morphedNormal[0] +
						nrmMat[ 7] * morphedNormal[1] +
						nrmMat[ 8] * morphedNormal[2];
					(*outNormal)[3] = 0.0f;
				}
			}
			if ( useGpuMorphPath ) {
				s_iqmGpuBatch.skinCount += surf->num_influences;
				s_iqmGpuBatch.morphVertexCount += surf->num_vertexes;
			}
		} else {
			// copy vertexes and fill other data
			for( i = 0; i < surf->num_vertexes;
				i++, xyz+=3, normal+=3, texCoords+=2,
				outXYZ++, outNormal++, outTexCoord+=2 ) {
				float morphedPos[3];
				float morphedNormal[3];
				int morphIndex;

				morphedPos[0] = xyz[0];
				morphedPos[1] = xyz[1];
				morphedPos[2] = xyz[2];
				morphedNormal[0] = normal[0];
				morphedNormal[1] = normal[1];
				morphedNormal[2] = normal[2];
				if ( applyMorph ) {
					for ( morphIndex = 0; morphIndex < ent->morphActiveCount; morphIndex++ ) {
						int targetIndex = ent->morphActiveTargetIndex[morphIndex];
						float weight = ent->morphActiveWeight[morphIndex];
						size_t deltaOffset;
						const float *deltaPos;
						const float *deltaNorm;

						if ( targetIndex < 0 || targetIndex >= data->morphNumTargets ) {
							continue;
						}
						deltaOffset = ( (size_t)targetIndex * (size_t)surf->num_vertexes + (size_t)i ) * 3u;
						deltaPos = morphSurface->deltaPos + deltaOffset;
						deltaNorm = morphSurface->deltaNorm + deltaOffset;
						morphedPos[0] += deltaPos[0] * weight;
						morphedPos[1] += deltaPos[1] * weight;
						morphedPos[2] += deltaPos[2] * weight;
						morphedNormal[0] += deltaNorm[0] * weight;
						morphedNormal[1] += deltaNorm[1] * weight;
						morphedNormal[2] += deltaNorm[2] * weight;
					}
					VectorNormalize( morphedNormal );
				}

				outTexCoord[0] = texCoords[0];
				outTexCoord[1] = texCoords[1];

				(*outXYZ)[0] = morphedPos[0];
				(*outXYZ)[1] = morphedPos[1];
				(*outXYZ)[2] = morphedPos[2];
				(*outXYZ)[3] = 0.0f;

				(*outNormal)[0] = morphedNormal[0];
				(*outNormal)[1] = morphedNormal[1];
				(*outNormal)[2] = morphedNormal[2];
				(*outNormal)[3] = 0.0f;
			}
		}

	if ( color ) {
		Com_Memcpy( outColor, color, surf->num_vertexes * sizeof( outColor[0] ) );
	} else {
		Com_Memset( outColor, 0, surf->num_vertexes * sizeof( outColor[0] ) );
	}
	if ( r_morphDebug && r_morphDebug->integer && ent && ent->morphActiveCount > 0 ) {
		int v;
		float t = ent->morphDebugMaxAbsWeight;
		byte r, g;
		if ( t < 0.0f ) {
			t = 0.0f;
		} else if ( t > 1.0f ) {
			t = 1.0f;
		}
		r = (byte)( t * 255.0f );
		g = (byte)( ( 1.0f - t ) * 255.0f );
		for ( v = 0; v < surf->num_vertexes; v++ ) {
			outColor[v].rgba[0] = r;
			outColor[v].rgba[1] = g;
			outColor[v].rgba[2] = 32;
			outColor[v].rgba[3] = 255;
		}
	}

	tri = data->triangles + 3 * surf->first_triangle;
	ptr = &tess.indexes[tess.numIndexes];
	base = tess.numVertexes;

	for( i = 0; i < surf->num_triangles; i++ ) {
		*ptr++ = base + (*tri++ - surf->first_vertex);
		*ptr++ = base + (*tri++ - surf->first_vertex);
		*ptr++ = base + (*tri++ - surf->first_vertex);
	}

	tess.numIndexes += 3 * surf->num_triangles;
	tess.numVertexes += surf->num_vertexes;
}

int R_IQMLerpTag( orientation_t *tag, iqmData_t *data,
		  int startFrame, int endFrame, 
		  float frac, const char *tagName ) {
	float	jointMats[IQM_MAX_JOINTS * 12];
	int	joint;
	char	*names = data->jointNames;

	// get joint number by reading the joint names
	for( joint = 0; joint < data->num_joints; joint++ ) {
		if( !strcmp( tagName, names ) )
			break;
		names += strlen( names ) + 1;
	}
	if( joint >= data->num_joints ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return qfalse;
	}

	ComputeJointMats( data, startFrame, endFrame, frac, jointMats );

	tag->axis[0][0] = jointMats[12 * joint + 0];
	tag->axis[1][0] = jointMats[12 * joint + 1];
	tag->axis[2][0] = jointMats[12 * joint + 2];
	tag->origin[0] = jointMats[12 * joint + 3];
	tag->axis[0][1] = jointMats[12 * joint + 4];
	tag->axis[1][1] = jointMats[12 * joint + 5];
	tag->axis[2][1] = jointMats[12 * joint + 6];
	tag->origin[1] = jointMats[12 * joint + 7];
	tag->axis[0][2] = jointMats[12 * joint + 8];
	tag->axis[1][2] = jointMats[12 * joint + 9];
	tag->axis[2][2] = jointMats[12 * joint + 10];
	tag->origin[2] = jointMats[12 * joint + 11];

	return qtrue;
}
