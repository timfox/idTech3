/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenGL backend: glTF primitive tessellation (CPU skin + morph). Shared loader
and R_AddGLTFSurfaces live in tr_model_gltf.c; Vulkan adds GPU/VBO paths there.
===========================================================================
*/

#include "tr_local.h"
#include <math.h>

static qboolean RB_GLTF_ImagePathLooksLikeNormalMap( const char *path ) {
	if ( !path || !path[0] ) {
		return qfalse;
	}
	if ( Q_stristr( path, "norm" ) || Q_stristr( path, "bump" ) || Q_stristr( path, "nmap" ) ) {
		return qtrue;
	}
	/* Q3-style: path/to/diffuse_n.tga next to diffuse.jpg */
	if ( Q_stristr( path, "_n." ) ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean RB_GLTF_ShaderUsesNormalMap( const shader_t *sh ) {
	int i, b;

	if ( !sh || sh->defaultShader ) {
		return qfalse;
	}
	for ( i = 0; i < sh->numUnfoggedPasses; i++ ) {
		const shaderStage_t *st = sh->stages[i];
		if ( !st || !st->active ) {
			continue;
		}
		for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
			const textureBundle_t *tb = &st->bundle[b];
			int a;
			for ( a = 0; a < tb->numImageAnimations && a < MAX_IMAGE_ANIMATIONS; a++ ) {
				const image_t *img = tb->image[a];
				if ( img && img->imgName && RB_GLTF_ImagePathLooksLikeNormalMap( img->imgName ) ) {
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

/*
MikkTSpace-style tangent basis from deformed positions + UV0 (matches Vulkan RB_GLTFRecomputeQtangentsForTessRange).
Fills tess.qtangent[w] for base <= w < base+numVerts.
*/
static void RB_GLTF_RecomputeQtangentsForTessRange( int vertBase, int numVerts, int indexStart, int numIndexes ) {
	float (*tanAcc)[3];
	float (*btAcc)[3];
	int tri, k;

	if ( numVerts <= 0 || numIndexes < 3 ) {
		return;
	}

	tanAcc = (float (*)[3])ri.Hunk_AllocateTempMemory( numVerts * sizeof( *tanAcc ) );
	btAcc = (float (*)[3])ri.Hunk_AllocateTempMemory( numVerts * sizeof( *btAcc ) );
	Com_Memset( tanAcc, 0, numVerts * sizeof( *tanAcc ) );
	Com_Memset( btAcc, 0, numVerts * sizeof( *btAcc ) );

	for ( tri = 0; tri < numIndexes / 3; tri++ ) {
		int ia = tess.indexes[indexStart + tri * 3 + 0];
		int ib = tess.indexes[indexStart + tri * 3 + 1];
		int ic = tess.indexes[indexStart + tri * 3 + 2];
		vec3_t e1, e2, sdir, tdir;
		vec2_t duv1, duv2;
		float denom, f;
		int corner;

		VectorSubtract( tess.xyz[ib], tess.xyz[ia], e1 );
		VectorSubtract( tess.xyz[ic], tess.xyz[ia], e2 );
		duv1[0] = tess.texCoords[0][ib][0] - tess.texCoords[0][ia][0];
		duv1[1] = tess.texCoords[0][ib][1] - tess.texCoords[0][ia][1];
		duv2[0] = tess.texCoords[0][ic][0] - tess.texCoords[0][ia][0];
		duv2[1] = tess.texCoords[0][ic][1] - tess.texCoords[0][ia][1];
		denom = duv1[0] * duv2[1] - duv2[0] * duv1[1];
		if ( fabsf( denom ) < 1e-12f ) {
			continue;
		}
		f = 1.0f / denom;
		sdir[0] = f * ( duv2[1] * e1[0] - duv1[1] * e2[0] );
		sdir[1] = f * ( duv2[1] * e1[1] - duv1[1] * e2[1] );
		sdir[2] = f * ( duv2[1] * e1[2] - duv1[1] * e2[2] );
		tdir[0] = f * ( -duv2[0] * e1[0] + duv1[0] * e2[0] );
		tdir[1] = f * ( -duv2[0] * e1[1] + duv1[0] * e2[1] );
		tdir[2] = f * ( -duv2[0] * e1[2] + duv1[0] * e2[2] );

		for ( corner = 0; corner < 3; corner++ ) {
			int vx = ( corner == 0 ) ? ia : ( ( corner == 1 ) ? ib : ic );
			int li;

			if ( vx < vertBase || vx >= vertBase + numVerts ) {
				continue;
			}
			li = vx - vertBase;
			for ( k = 0; k < 3; k++ ) {
				tanAcc[li][k] += sdir[k];
				btAcc[li][k] += tdir[k];
			}
		}
	}

	for ( k = 0; k < numVerts; k++ ) {
		int vi = vertBase + k;
		vec3_t n, t, b, up;
		float d;

		VectorCopy( tess.normal[vi], n );
		VectorNormalize( n );
		d = DotProduct( n, tanAcc[k] );
		t[0] = tanAcc[k][0] - n[0] * d;
		t[1] = tanAcc[k][1] - n[1] * d;
		t[2] = tanAcc[k][2] - n[2] * d;
		if ( VectorLength( t ) < 1e-8f ) {
			if ( fabsf( n[2] ) < 0.9f ) {
				VectorSet( up, 0.0f, 0.0f, 1.0f );
			} else {
				VectorSet( up, 1.0f, 0.0f, 0.0f );
			}
			CrossProduct( n, up, t );
		}
		VectorNormalize( t );
		CrossProduct( n, t, b );
		d = DotProduct( b, btAcc[k] );
		tess.qtangent[vi][0] = t[0];
		tess.qtangent[vi][1] = t[1];
		tess.qtangent[vi][2] = t[2];
		tess.qtangent[vi][3] = ( d < 0.0f ) ? -1.0f : 1.0f;
	}

	ri.Hunk_FreeTempMemory( btAcc );
	ri.Hunk_FreeTempMemory( tanAcc );
}

static int RB_GLTFMorphIndexByHash( const gltfMorphTarget_t *targets, int num, uint32_t hash ) {
	int i;
	char lower[MAX_QPATH];

	for ( i = 0; i < num; i++ ) {
		if ( !targets[i].name[0] ) {
			continue;
		}
		Q_strncpyz( lower, targets[i].name, sizeof( lower ) );
		Q_strlwr( lower );
		if ( (uint32_t)Com_GenerateHashValue( lower, 0x7fffffffU ) == hash ) {
			return i;
		}
	}
	return -1;
}

void RB_GLTFSurface( const surfaceType_t *surface ) {
	const srfGLTFPrimitive_t *surf = (const srfGLTFPrimitive_t *)surface;
	const gltfVertex_t *v;
	const trRefEntity_t *ent;
	int i, j;
	int base;
	float morphW[GLTF_MAX_MORPH_TARGETS];
	qboolean useMorph;
	const gltfModel_t *model;
	float jointMatrix[GLTF_MAX_JOINTS * 12];
	qboolean haveJoints;
	int animCur, animOld;
	float timeCur, timeOld, backlerp;
	float speed;

	ent = backEnd.currentEntity;
	model = ( tr.currentModel && tr.currentModel->modelData )
		? R_GetGLTFModelFromModelData( tr.currentModel->modelData ) : NULL;

	Com_Memset( morphW, 0, sizeof( morphW ) );
	useMorph = ( surf->numMorphTargets > 0 && surf->morphTargets != NULL && r_morph && r_morph->integer );

	speed = ( r_gltfAnim && r_gltfAnim->value > 0.0f ) ? r_gltfAnim->value : 1.0f;
	if ( ent->intShaderTime ) {
		timeCur = ent->e.shaderTime.i * 0.001f * speed;
	} else {
		timeCur = ent->e.shaderTime.f * speed;
	}
	if ( !timeCur && tr.refdef.time > 0 ) {
		timeCur = tr.refdef.time * 0.001f * speed;
	}
	timeOld = timeCur;
	backlerp = ent->e.backlerp;
	if ( backlerp < 0.0f ) {
		backlerp = 0.0f;
	} else if ( backlerp > 1.0f ) {
		backlerp = 1.0f;
	}

	animCur = ent->e.frame;
	animOld = ent->e.oldframe;
	if ( model && model->numAnimations > 0 ) {
		if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
			if ( animCur < 0 ) {
				animCur = 0;
			} else {
				animCur %= model->numAnimations;
			}
			if ( animOld < 0 ) {
				animOld = 0;
			} else {
				animOld %= model->numAnimations;
			}
		} else {
			if ( animCur < 0 || animCur >= model->numAnimations ) {
				animCur = 0;
			}
			if ( animOld < 0 || animOld >= model->numAnimations ) {
				animOld = animCur;
			}
		}
	} else {
		animCur = -1;
		animOld = -1;
	}

	if ( useMorph && model && animCur >= 0 ) {
		float wB[GLTF_MAX_MORPH_TARGETS];
		(void)R_SampleGLTFMeshMorphWeights( model, animCur, timeCur, surf->meshIndex, morphW, surf->numMorphTargets );
		if ( backlerp > 0.0f && animOld >= 0 && animOld != animCur ) {
			Com_Memset( wB, 0, sizeof( wB ) );
			(void)R_SampleGLTFMeshMorphWeights( model, animOld, timeOld, surf->meshIndex, wB, surf->numMorphTargets );
			for ( i = 0; i < surf->numMorphTargets; i++ ) {
				morphW[i] = morphW[i] * ( 1.0f - backlerp ) + wB[i] * backlerp;
			}
		}
	}
	if ( useMorph && ent->morphChannelCount > 0 ) {
		for ( i = 0; i < ent->morphChannelCount; i++ ) {
			int ti = RB_GLTFMorphIndexByHash( surf->morphTargets, surf->numMorphTargets, ent->morphChannelHashes[i] );
			if ( ti >= 0 ) {
				morphW[ti] += ent->morphChannelWeights[i];
			}
		}
	}
	if ( useMorph && model && surf->meshIndex >= 0 && surf->meshIndex < model->numMeshes ) {
		const gltfMesh_t *gm = &model->meshes[surf->meshIndex];
		int nw = gm->numDefaultMorphWeights;
		if ( nw > surf->numMorphTargets ) {
			nw = surf->numMorphTargets;
		}
		for ( i = 0; i < nw; i++ ) {
			morphW[i] += gm->defaultMorphWeights[i];
		}
	}

	haveJoints = ( qboolean )( surf->hasSkinning && model && model->skeleton.numJoints > 0 );
	if ( haveJoints ) {
		if ( animCur >= 0 && model->numAnimations > 0 ) {
			if ( backlerp > 0.001f && animOld >= 0 && animOld != animCur ) {
				R_ComputeGLTFJointMatricesBlend( model, animCur, timeCur, animOld, timeOld, backlerp, jointMatrix );
			} else {
				R_ComputeGLTFJointMatrices( model, animCur, timeCur, jointMatrix );
			}
		} else {
			R_ComputeGLTFJointMatrices( model, -1, 0.0f, jointMatrix );
		}
	}

	RB_CHECKOVERFLOW( surf->numVertices, surf->numIndices );

	base = tess.numVertexes;

	for ( i = 0; i < surf->numVertices; i++ ) {
		vec3_t pos, nrm, tan;
		float handed;
		float ndt, tanLen;
		v = &surf->vertices[i];

		VectorCopy( v->position, pos );
		VectorCopy( v->normal, nrm );
		VectorSet( tan, v->tangent[0], v->tangent[1], v->tangent[2] );
		handed = v->tangent[3];
		if ( useMorph ) {
			int ti;
			for ( ti = 0; ti < surf->numMorphTargets; ti++ ) {
				float w = morphW[ti];
				const gltfMorphTarget_t *mt;
				if ( w == 0.0f ) {
					continue;
				}
				mt = &surf->morphTargets[ti];
				if ( mt->deltaPosition ) {
					pos[0] += w * mt->deltaPosition[i * 3 + 0];
					pos[1] += w * mt->deltaPosition[i * 3 + 1];
					pos[2] += w * mt->deltaPosition[i * 3 + 2];
				}
				if ( mt->deltaNormal ) {
					nrm[0] += w * mt->deltaNormal[i * 3 + 0];
					nrm[1] += w * mt->deltaNormal[i * 3 + 1];
					nrm[2] += w * mt->deltaNormal[i * 3 + 2];
				}
				if ( mt->deltaTangent ) {
					tan[0] += w * mt->deltaTangent[i * 3 + 0];
					tan[1] += w * mt->deltaTangent[i * 3 + 1];
					tan[2] += w * mt->deltaTangent[i * 3 + 2];
				}
			}
			VectorNormalize( nrm );
		}

		if ( haveJoints ) {
			vec3_t spos, snrm, stan;
			float w0 = v->weights[0], w1 = v->weights[1], w2 = v->weights[2], w3 = v->weights[3];
			int j0 = v->joints[0], j1 = v->joints[1], j2 = v->joints[2], j3 = v->joints[3];
			float *m0, *m1, *m2, *m3;

			VectorClear( spos );
			VectorClear( snrm );
			VectorClear( stan );
			if ( w0 > 0 && j0 < model->skeleton.numJoints ) {
				m0 = &jointMatrix[j0 * 12];
				spos[0] += w0 * ( m0[0] * pos[0] + m0[1] * pos[1] + m0[2] * pos[2] + m0[3] );
				spos[1] += w0 * ( m0[4] * pos[0] + m0[5] * pos[1] + m0[6] * pos[2] + m0[7] );
				spos[2] += w0 * ( m0[8] * pos[0] + m0[9] * pos[1] + m0[10] * pos[2] + m0[11] );
				snrm[0] += w0 * ( m0[0] * nrm[0] + m0[1] * nrm[1] + m0[2] * nrm[2] );
				snrm[1] += w0 * ( m0[4] * nrm[0] + m0[5] * nrm[1] + m0[6] * nrm[2] );
				snrm[2] += w0 * ( m0[8] * nrm[0] + m0[9] * nrm[1] + m0[10] * nrm[2] );
				stan[0] += w0 * ( m0[0] * tan[0] + m0[1] * tan[1] + m0[2] * tan[2] );
				stan[1] += w0 * ( m0[4] * tan[0] + m0[5] * tan[1] + m0[6] * tan[2] );
				stan[2] += w0 * ( m0[8] * tan[0] + m0[9] * tan[1] + m0[10] * tan[2] );
			}
			if ( w1 > 0 && j1 < model->skeleton.numJoints ) {
				m1 = &jointMatrix[j1 * 12];
				spos[0] += w1 * ( m1[0] * pos[0] + m1[1] * pos[1] + m1[2] * pos[2] + m1[3] );
				spos[1] += w1 * ( m1[4] * pos[0] + m1[5] * pos[1] + m1[6] * pos[2] + m1[7] );
				spos[2] += w1 * ( m1[8] * pos[0] + m1[9] * pos[1] + m1[10] * pos[2] + m1[11] );
				snrm[0] += w1 * ( m1[0] * nrm[0] + m1[1] * nrm[1] + m1[2] * nrm[2] );
				snrm[1] += w1 * ( m1[4] * nrm[0] + m1[5] * nrm[1] + m1[6] * nrm[2] );
				snrm[2] += w1 * ( m1[8] * nrm[0] + m1[9] * nrm[1] + m1[10] * nrm[2] );
				stan[0] += w1 * ( m1[0] * tan[0] + m1[1] * tan[1] + m1[2] * tan[2] );
				stan[1] += w1 * ( m1[4] * tan[0] + m1[5] * tan[1] + m1[6] * tan[2] );
				stan[2] += w1 * ( m1[8] * tan[0] + m1[9] * tan[1] + m1[10] * tan[2] );
			}
			if ( w2 > 0 && j2 < model->skeleton.numJoints ) {
				m2 = &jointMatrix[j2 * 12];
				spos[0] += w2 * ( m2[0] * pos[0] + m2[1] * pos[1] + m2[2] * pos[2] + m2[3] );
				spos[1] += w2 * ( m2[4] * pos[0] + m2[5] * pos[1] + m2[6] * pos[2] + m2[7] );
				spos[2] += w2 * ( m2[8] * pos[0] + m2[9] * pos[1] + m2[10] * pos[2] + m2[11] );
				snrm[0] += w2 * ( m2[0] * nrm[0] + m2[1] * nrm[1] + m2[2] * nrm[2] );
				snrm[1] += w2 * ( m2[4] * nrm[0] + m2[5] * nrm[1] + m2[6] * nrm[2] );
				snrm[2] += w2 * ( m2[8] * nrm[0] + m2[9] * nrm[1] + m2[10] * nrm[2] );
				stan[0] += w2 * ( m2[0] * tan[0] + m2[1] * tan[1] + m2[2] * tan[2] );
				stan[1] += w2 * ( m2[4] * tan[0] + m2[5] * tan[1] + m2[6] * tan[2] );
				stan[2] += w2 * ( m2[8] * tan[0] + m2[9] * tan[1] + m2[10] * tan[2] );
			}
			if ( w3 > 0 && j3 < model->skeleton.numJoints ) {
				m3 = &jointMatrix[j3 * 12];
				spos[0] += w3 * ( m3[0] * pos[0] + m3[1] * pos[1] + m3[2] * pos[2] + m3[3] );
				spos[1] += w3 * ( m3[4] * pos[0] + m3[5] * pos[1] + m3[6] * pos[2] + m3[7] );
				spos[2] += w3 * ( m3[8] * pos[0] + m3[9] * pos[1] + m3[10] * pos[2] + m3[11] );
				snrm[0] += w3 * ( m3[0] * nrm[0] + m3[1] * nrm[1] + m3[2] * nrm[2] );
				snrm[1] += w3 * ( m3[4] * nrm[0] + m3[5] * nrm[1] + m3[6] * nrm[2] );
				snrm[2] += w3 * ( m3[8] * nrm[0] + m3[9] * nrm[1] + m3[10] * nrm[2] );
				stan[0] += w3 * ( m3[0] * tan[0] + m3[1] * tan[1] + m3[2] * tan[2] );
				stan[1] += w3 * ( m3[4] * tan[0] + m3[5] * tan[1] + m3[6] * tan[2] );
				stan[2] += w3 * ( m3[8] * tan[0] + m3[9] * tan[1] + m3[10] * tan[2] );
			}
			VectorNormalize( snrm );
			VectorCopy( spos, pos );
			VectorCopy( snrm, nrm );
			VectorCopy( stan, tan );
		}

		/* Re-orthonormalize tangent vs final normal (matches Vulkan r_gltfGpuTangentFix idea). */
		ndt = DotProduct( tan, nrm );
		tan[0] -= ndt * nrm[0];
		tan[1] -= ndt * nrm[1];
		tan[2] -= ndt * nrm[2];
		tanLen = VectorLength( tan );
		if ( tanLen < 1e-8f ) {
			VectorSet( tan, v->tangent[0], v->tangent[1], v->tangent[2] );
			ndt = DotProduct( tan, nrm );
			tan[0] -= ndt * nrm[0];
			tan[1] -= ndt * nrm[1];
			tan[2] -= ndt * nrm[2];
			tanLen = VectorLength( tan );
		}
		if ( tanLen >= 1e-8f ) {
			VectorScale( tan, 1.0f / tanLen, tan );
		} else {
			VectorSet( tan, v->tangent[0], v->tangent[1], v->tangent[2] );
			if ( VectorNormalize( tan ) == 0.0f ) {
				VectorSet( tan, 1.0f, 0.0f, 0.0f );
			}
		}

		tess.xyz[base + i][0] = pos[0];
		tess.xyz[base + i][1] = pos[1];
		tess.xyz[base + i][2] = pos[2];
		tess.normal[base + i][0] = nrm[0];
		tess.normal[base + i][1] = nrm[1];
		tess.normal[base + i][2] = nrm[2];
		tess.texCoords[0][base + i][0] = v->texCoord0[0];
		tess.texCoords[0][base + i][1] = v->texCoord0[1];
		tess.vertexColors[base + i].rgba[0] = (byte)( v->color[0] * 255 );
		tess.vertexColors[base + i].rgba[1] = (byte)( v->color[1] * 255 );
		tess.vertexColors[base + i].rgba[2] = (byte)( v->color[2] * 255 );
		tess.vertexColors[base + i].rgba[3] = (byte)( v->color[3] * 255 );

		tess.qtangent[base + i][0] = tan[0];
		tess.qtangent[base + i][1] = tan[1];
		tess.qtangent[base + i][2] = tan[2];
		tess.qtangent[base + i][3] = handed;
	}

	tess.numVertexes += surf->numVertices;

	{
		int idxBase = tess.numIndexes;
		for ( j = 0; j < surf->numIndices; j++ ) {
			tess.indexes[tess.numIndexes + j] = (glIndex_t)( base + surf->indices[j] );
		}
		tess.numIndexes += surf->numIndices;

		if ( r_gltfCpuQtangent && r_gltfCpuQtangent->integer &&
		     tess.shader && RB_GLTF_ShaderUsesNormalMap( tess.shader ) ) {
			RB_GLTF_RecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );
		}
	}
}
