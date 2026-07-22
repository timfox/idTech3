/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
// tr_mesh.c: triangle model functions

#include "tr_local.h"
#include "vk_meshlets.h"

/*
=============
R_CullMD3SurfaceMeshlets

When r_meshlets 1, use bake-at-load local meshlets (or bake on miss) and frustum-cull
in world space. Returns qtrue if the surface should be drawn.
=============
*/
#define MESHLET_MD3_MAX_VERTS 512
#define MESHLET_MD3_MAX_TRIS  1024

static qboolean R_CullMD3SurfaceMeshlets( md3Surface_t *surface, int frame, const float entityAxis[3][3], const vec3_t entityOrigin )
{
	md3XyzNormal_t *xyz;
	md3Triangle_t *tri;
	vec3_t positions[MESHLET_MD3_MAX_VERTS];
	int indexes[MESHLET_MD3_MAX_TRIS * 3];
	const meshlet_t *meshlets = NULL;
	int visible[MESHLET_MAX_PER_SURFACE];
	int numVerts, i, mcount, vcount;

	if ( !R_Meshlets_Active() || !surface || surface->numVerts <= 0 || surface->numTriangles <= 0 ) {
		return qtrue;
	}
	if ( surface->numVerts > MESHLET_MD3_MAX_VERTS || surface->numTriangles > MESHLET_MD3_MAX_TRIS ) {
		return qtrue;
	}
	/*
	 * Meshlets bake bind-pose (frame 0) AABBs. Animated frames make frustum/LOD
	 * culls unsafe — fall back to drawing the full surface.
	 */
	if ( frame != 0 ) {
		return qtrue;
	}

	mcount = R_Meshlets_Lookup( surface, &meshlets );
	if ( mcount <= 0 || !meshlets ) {
		/* Miss: bake local (frame 0 bind pose) into cache */
		xyz = (md3XyzNormal_t *)( (byte *)surface + surface->ofsXyzNormals );
		tri = (md3Triangle_t *)( (byte *)surface + surface->ofsTriangles );
		numVerts = surface->numVerts;
		for ( i = 0; i < numVerts; i++ ) {
			positions[i][0] = xyz[i].xyz[0] * MD3_XYZ_SCALE;
			positions[i][1] = xyz[i].xyz[1] * MD3_XYZ_SCALE;
			positions[i][2] = xyz[i].xyz[2] * MD3_XYZ_SCALE;
		}
		for ( i = 0; i < surface->numTriangles; i++ ) {
			indexes[i * 3 + 0] = tri[i].indexes[0];
			indexes[i * 3 + 1] = tri[i].indexes[1];
			indexes[i * 3 + 2] = tri[i].indexes[2];
		}
		mcount = R_Meshlets_CacheLocal( surface, (const vec3_t *)positions, numVerts,
			indexes, surface->numTriangles * 3 );
		mcount = R_Meshlets_Lookup( surface, &meshlets );
		if ( mcount <= 0 || !meshlets ) {
			return qtrue;
		}
	}

	vcount = R_Meshlets_CullViewFrustumXform( meshlets, mcount, entityAxis, entityOrigin,
		visible, MESHLET_MAX_PER_SURFACE );
	if ( vcount > 0 && R_Meshlets_WantMdi() ) {
		meshlet_draw_cmd_t cmds[MESHLET_MAX_PER_SURFACE];
		R_Meshlets_PackIndirect( meshlets, visible, vcount, cmds, MESHLET_MAX_PER_SURFACE, 0 );
	}
	return ( vcount > 0 ) ? qtrue : qfalse;
}

static float ProjectRadius( float r, vec3_t location )
{
	float pr;
	float dist;
	float c;
	vec3_t	p;
	float	projected[4];

	c = DotProduct( tr.viewParms.or.axis[0], tr.viewParms.or.origin );
	dist = DotProduct( tr.viewParms.or.axis[0], location ) - c;

	if ( dist <= 0 )
		return 0;

	p[0] = 0;
	p[1] = fabs( r );
	p[2] = -dist;

#if 0
	projected[0] = p[0] * tr.viewParms.projectionMatrix[0] +
					p[1] * tr.viewParms.projectionMatrix[4] +
					p[2] * tr.viewParms.projectionMatrix[8] +
					tr.viewParms.projectionMatrix[12];
#endif
	projected[1] = p[0] * tr.viewParms.projectionMatrix[1] +
					p[1] * tr.viewParms.projectionMatrix[5] +
					p[2] * tr.viewParms.projectionMatrix[9] +
					tr.viewParms.projectionMatrix[13];
#if 0
	projected[2] = p[0] * tr.viewParms.projectionMatrix[2] +
					p[1] * tr.viewParms.projectionMatrix[6] +
					p[2] * tr.viewParms.projectionMatrix[10] +
					tr.viewParms.projectionMatrix[14];
#endif
	projected[3] = p[0] * tr.viewParms.projectionMatrix[3] +
					p[1] * tr.viewParms.projectionMatrix[7] +
					p[2] * tr.viewParms.projectionMatrix[11] +
					tr.viewParms.projectionMatrix[15];

	pr = projected[1] / projected[3];

	if ( pr > 1.0f )
		pr = 1.0f;

	return pr;
}


/*
=============
R_CullModel
=============
*/
static int R_CullModel( md3Header_t *header, const trRefEntity_t *ent, vec3_t bounds[] ) {
	//vec3_t bounds[2];
	md3Frame_t	*oldFrame, *newFrame;
	int			i;

	// compute frame pointers
	newFrame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.frame;
	oldFrame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.oldframe;

	// calculate a bounding box in the current coordinate system
	for (i = 0 ; i < 3 ; i++) {
		bounds[0][i] = oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
	}

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;

			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;

			case CULL_CLIP:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			}
		}
		else
		{
			int sphereCull, sphereCullB;

			sphereCull  = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			if ( newFrame == oldFrame ) {
				sphereCullB = sphereCull;
			} else {
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == CULL_OUT )
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				else if ( sphereCull == CULL_IN )
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
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
R_ModelRadiusForLOD
Format-neutral bounding radius for LOD selection.
=================
*/
float R_ModelRadiusForLOD( const model_t *model, int frame ) {
	md3Frame_t *md3Frame;
	mdrHeader_t *mdr;
	mdrFrame_t *mdrframe;
	int frameSize;

	if ( !model ) {
		return 1.0f;
	}

	if ( model->type == MOD_MDR && model->modelData ) {
		mdr = (mdrHeader_t *)model->modelData;
		frameSize = (size_t)(&((mdrFrame_t *)0)->bones[mdr->numBones]);
		if ( frame < 0 ) {
			frame = 0;
		}
		mdrframe = (mdrFrame_t *)((byte *)mdr + mdr->ofsFrames + frameSize * frame);
		return RadiusFromBounds( mdrframe->bounds[0], mdrframe->bounds[1] );
	}

	if ( model->md3[0] ) {
		md3Frame = (md3Frame_t *)(((unsigned char *)model->md3[0]) + model->md3[0]->ofsFrames);
		if ( frame < 0 ) {
			frame = 0;
		}
		md3Frame += frame;
		return RadiusFromBounds( md3Frame->bounds[0], md3Frame->bounds[1] );
	}

	/* IQM / glTF / other without MD3 frames: conservative default */
	(void)frame;
	return 32.0f;
}

/*
=================
R_ComputeLOD
=================
*/
int R_ComputeLOD( trRefEntity_t *ent ) {
	float radius;
	float flod, lodscale;
	float projectedRadius;
	int lod;

	if ( tr.currentModel->numLods < 2 )
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		radius = R_ModelRadiusForLOD( tr.currentModel, ent->e.frame );

		if ( ( projectedRadius = ProjectRadius( radius, ent->e.origin ) ) != 0 )
		{
			lodscale = r_lodscale->value;
			if (lodscale > 20) lodscale = 20;
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		lod = myftol( flod );

		if ( lod < 0 )
		{
			lod = 0;
		}
		else if ( lod >= tr.currentModel->numLods )
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodbias->integer;
	
	if ( lod >= tr.currentModel->numLods )
		lod = tr.currentModel->numLods - 1;
	if ( lod < 0 )
		lod = 0;

	return lod;
}


/*
=================
R_ComputeFogNum
=================
*/
static int R_ComputeFogNum( md3Header_t *header, const trRefEntity_t *ent ) {
	int				i, j;
	const fog_t			*fog;
	md3Frame_t		*md3Frame;
	vec3_t			localOrigin;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	/* Non-normalized axis may cause scaling artifacts. */
	md3Frame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.frame;
	VectorAdd( ent->e.origin, md3Frame->localOrigin, localOrigin );
	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( localOrigin[j] - md3Frame->radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( localOrigin[j] + md3Frame->radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}


/*
=================
R_AddMD3Surfaces
=================
*/
void R_AddMD3Surfaces( trRefEntity_t *ent ) {
	vec3_t			bounds[2];
	int				i;
	md3Header_t		*header = NULL;
	md3Surface_t	*surface = NULL;
	md3Shader_t		*md3Shader = NULL;
	shader_t		*shader = NULL;
	int				cull;
	int				lod;
	int				fogNum;
	qboolean		personalModel;
	dlight_t		*dl;
	int				n;
	dlight_t		*dlights[ ARRAY_LEN( backEndData->dlights ) ];
	int				numDlights;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE);

	if ( !tr.currentModel->md3[0] || tr.currentModel->md3[0]->numFrames < 1 ) {
		return;
	}

	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		const int nf = tr.currentModel->md3[0]->numFrames;
		ent->e.frame %= nf;
		ent->e.oldframe %= nf;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ( (ent->e.frame >= tr.currentModel->md3[0]->numFrames) 
		|| (ent->e.frame < 0)
		|| (ent->e.oldframe >= tr.currentModel->md3[0]->numFrames)
		|| (ent->e.oldframe < 0) ) {
			ri.Printf( PRINT_DEVELOPER, "R_AddMD3Surfaces: no such frame %d to %d for '%s'\n",
				ent->e.oldframe, ent->e.frame,
				tr.currentModel->name );
			ent->e.frame = 0;
			ent->e.oldframe = 0;
	}

	//
	// compute LOD
	//
	lod = R_ComputeLOD( ent );

	header = tr.currentModel->md3[lod];

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	cull = R_CullModel( header, ent, bounds );
	if ( cull == CULL_OUT ) {
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if ( !personalModel || r_shadows->integer > 1 ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

	numDlights = 0;
	if ( r_dlightMode->integer >= 2 && ( !personalModel || tr.viewParms.portalView != PV_NONE ) ) {
		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );
		for ( n = 0; n < (int)tr.viewParms.num_dlights; n++ ) {
			dl = &tr.viewParms.dlights[ n ];
			if ( !R_LightCullBounds( dl, bounds[0], bounds[1] ) ) 
				dlights[ numDlights++ ] = dl;
		}
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_ComputeFogNum( header, ent );

	//
	// draw all surfaces
	//
	surface = (md3Surface_t *)( (byte *)header + header->ofsSurfaces );
	for ( i = 0 ; i < header->numSurfaces ; i++ ) {

		if ( ent->e.customShader ) {
			shader = R_GetShaderByHandle( ent->e.customShader );
		} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
			const skin_t *skin;
			int		j;

			skin = R_GetSkinByHandle( ent->e.customSkin );

			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
				// the names have both been lowercased
				if ( !strcmp( skin->surfaces[j].name, surface->name ) ) {
					shader = skin->surfaces[j].shader;
					break;
				}
			}
			if (shader == tr.defaultShader) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no shader for surface %s in skin %s\n", surface->name, skin->name);
			}
			else if (shader->defaultShader) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: shader %s in skin %s not found\n", shader->name, skin->name);
			}
		} else if ( surface->numShaders <= 0 ) {
			shader = tr.defaultShader;
		} else {
			md3Shader = (md3Shader_t *) ( (byte *)surface + surface->ofsShaders );
			md3Shader += ent->e.skinNum % surface->numShaders;
			shader = tr.shaders[ md3Shader->shaderIndex ];
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

		// don't add third_person objects if not viewing through a portal
		if ( !personalModel ) {
			if ( R_CullMD3SurfaceMeshlets( surface, ent->e.frame, ent->e.axis, ent->e.origin ) ) {
				R_AddDrawSurf( (void *)surface, shader, fogNum, 0 );
				tr.needScreenMap |= shader->hasScreenMap;
			}
		}

		if ( numDlights && shader->lightingStage >= 0 ) {
			for ( n = 0; n < numDlights; n++ ) {
				dl = dlights[ n ];
				tr.light = dl;
				R_AddLitSurf( (void *)surface, shader, fogNum );
			}
		}

		surface = (md3Surface_t *)( (byte *)surface + surface->ofsEnd );
	}
}
