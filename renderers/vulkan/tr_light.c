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
// tr_light.c

#include "tr_local.h"
#ifdef USE_VULKAN
#include "vk_raster_gi.h"
#endif

#define	DLIGHT_AT_RADIUS		16
// at the edge of a dlight's influence, this amount of light will be added

#define	DLIGHT_MINIMUM_RADIUS	16
// never calculate a range less than this to prevent huge light numbers

extern	cvar_t	*r_ambientScale;
extern	cvar_t	*r_directedScale;
extern	cvar_t	*r_intensity;
extern	cvar_t	*r_gamma;
extern	cvar_t	*r_dynamicLightScale;
extern	cvar_t	*r_lightGammaLink;
extern	cvar_t	*r_shWorldStrength;
extern	cvar_t	*r_classicLighting;

/*
===============
R_ClassicLightingActive

When true (default), preserve retail Q3 / classic mod lighting: disable chocolate-layer
overrides (SH world tint, PBR sun shadow, Forward+ overflow shade). Set r_classicLighting 0
for modern lighting features controlled by their individual cvars.
===============
*/
qboolean R_ClassicLightingActive( void ) {
	return ( r_classicLighting && r_classicLighting->integer );
}

/*
===============
R_EvalSH9_RGB
===============
*/
void R_EvalSH9_RGB( const vec3_t shCoeffs[SH_COEFF_COUNT], const vec3_t normal, vec3_t out ) {
	const float x = normal[0];
	const float y = normal[1];
	const float z = normal[2];
	const float basis[SH_COEFF_COUNT] = {
		1.0f,
		y,
		z,
		x,
		x * y,
		y * z,
		3.0f * z * z - 1.0f,
		x * z,
		x * x - y * y
	};
	int i;

	VectorClear( out );
	for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
		out[0] += shCoeffs[i][0] * basis[i];
		out[1] += shCoeffs[i][1] * basis[i];
		out[2] += shCoeffs[i][2] * basis[i];
	}
}

/*
===============
R_WorldSHVertexColor

Light-grid SH vertex color for world surfaces (matches entity directedScale).
Optional r_shWorldStrength blends toward identity white.
===============
*/
qboolean R_WorldSHVertexColor( const vec3_t position, const vec3_t normal, byte rgba[4] ) {
	vec3_t shCoeffs[SH_COEFF_COUNT];
	vec3_t shLight;
	float strength;
	float dirScale;
	int i;

	if ( !rgba || !tr.world ) {
		return qfalse;
	}

	if ( R_ClassicLightingActive() ) {
		rgba[0] = 255;
		rgba[1] = 255;
		rgba[2] = 255;
		rgba[3] = 255;
		return qfalse;
	}

	if ( !R_SampleLightGridSH( tr.world, position, shCoeffs ) ) {
		rgba[0] = 255;
		rgba[1] = 255;
		rgba[2] = 255;
		rgba[3] = 255;
		return qfalse;
	}

	R_EvalSH9_RGB( shCoeffs, normal, shLight );
	dirScale = ( r_directedScale ) ? r_directedScale->value : 1.0f;
	VectorScale( shLight, dirScale, shLight );

	strength = ( r_shWorldStrength ) ? r_shWorldStrength->value : 1.0f;
	strength = Com_Clamp( 0.0f, 2.0f, strength );

	for ( i = 0; i < 3; i++ ) {
		const float blended = 255.0f + ( shLight[i] - 255.0f ) * strength;
		rgba[i] = (byte)Com_Clamp( 0.0f, 255.0f, blended );
	}
	rgba[3] = 255;
	return qtrue;
}

/*
===============
R_DynamicLightUsesLegacyScale
===============
*/
qboolean R_DynamicLightUsesLegacyScale( void ) {
#ifdef USE_VULKAN
	if ( glConfig.deviceSupportsGamma || vk.fboActive ) {
		return qfalse;
	}
#else
	if ( glConfig.deviceSupportsGamma ) {
		return qfalse;
	}
#endif
	return qtrue;
}

/*
===============
R_DynamicLightExtraScale
===============
*/
static float R_DynamicLightExtraScale( void ) {
	if ( !r_dynamicLightScale ) {
		return 1.0f;
	}
	return Com_Clamp( 0.25f, 4.0f, r_dynamicLightScale->value );
}

/*
===============
R_DynamicLightIntensityScale
===============
*/
float R_DynamicLightIntensityScale( void ) {
	float scale = R_DynamicLightExtraScale();

	if ( r_lightGammaLink && !r_lightGammaLink->integer ) {
		scale *= 2.0f * ( r_intensity ? r_intensity->value : 1.0f );
	} else {
		const float intensity = r_intensity ? r_intensity->value : 1.0f;
		const float gamma = r_gamma ? r_gamma->value : 1.0f;
		scale *= 2.0f * powf( intensity, gamma );
	}
	return scale;
}

/*
===============
R_DynamicLightColor
===============
*/
void R_DynamicLightColor( const dlight_t *dl, vec3_t out ) {
	VectorCopy( dl->color, out );
	if ( R_DynamicLightUsesLegacyScale() ) {
		VectorScale( out, R_DynamicLightIntensityScale(), out );
	} else {
		float scale = R_DynamicLightExtraScale();
		if ( r_lightGammaLink && !r_lightGammaLink->integer ) {
			scale *= 2.0f * ( r_intensity ? r_intensity->value : 1.0f );
		}
		if ( scale != 1.0f ) {
			VectorScale( out, scale, out );
		}
	}
}

static void R_ClearSHCoeffs( vec3_t shCoeffs[SH_COEFF_COUNT] ) {
	int i;

	for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
		VectorClear( shCoeffs[i] );
	}
}

static void R_AddSHSample( vec3_t shCoeffs[SH_COEFF_COUNT], const vec3_t dir, const vec3_t color, float weight ) {
	const float x = dir[0];
	const float y = dir[1];
	const float z = dir[2];
	const float basis[SH_COEFF_COUNT] = {
		1.0f,
		y,
		z,
		x,
		x * y,
		y * z,
		3.0f * z * z - 1.0f,
		x * z,
		x * x - y * y
	};
	int i;

	for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
		shCoeffs[i][0] += color[0] * basis[i] * weight;
		shCoeffs[i][1] += color[1] * basis[i] * weight;
		shCoeffs[i][2] += color[2] * basis[i] * weight;
	}
}

qboolean R_SampleLightGridSH( const world_t *world, const vec3_t position, vec3_t shCoeffs[SH_COEFF_COUNT] ) {
	vec3_t	lightOrigin;
	int		pos[3];
	int		i, j;
	byte	*gridData;
	float	frac[3];
	int		gridStep[3];
	float	totalFactor;

	R_ClearSHCoeffs( shCoeffs );
	if ( !world || !world->lightGridData ) {
		return qfalse;
	}

	VectorSubtract( position, world->lightGridOrigin, lightOrigin );
	for ( i = 0 ; i < 3 ; i++ ) {
		float	v;

		v = lightOrigin[i] * world->lightGridInverseSize[i];
		pos[i] = floor( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] > world->lightGridBounds[i] - 1 ) {
			pos[i] = world->lightGridBounds[i] - 1;
		}
	}

	gridStep[0] = 8;
	gridStep[1] = 8 * world->lightGridBounds[0];
	gridStep[2] = 8 * world->lightGridBounds[0] * world->lightGridBounds[1];
	gridData = world->lightGridData + pos[0] * gridStep[0]
		+ pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0.0f;
	for ( i = 0 ; i < 8 ; i++ ) {
		float	factor;
		byte	*data;
		int		lat, lng;
		vec3_t	normal;

		factor = 1.0f;
		data = gridData;
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( i & (1<<j) ) {
				if ( pos[j] + 1 > world->lightGridBounds[j] - 1 ) {
					break;
				}
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= (1.0f - frac[j]);
			}
		}

		if ( j != 3 ) {
			continue;
		}

		if ( !(data[0] + data[1] + data[2] + data[3] + data[4] + data[5]) ) {
			continue;
		}

		totalFactor += factor;

		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE/256);
		lng *= (FUNCTABLE_SIZE/256);

		normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
		normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

		{
			vec3_t ambientColor = {
				data[0] * r_ambientScale->value,
				data[1] * r_ambientScale->value,
				data[2] * r_ambientScale->value
			};
			vec3_t directedColor = {
				data[3] * r_directedScale->value,
				data[4] * r_directedScale->value,
				data[5] * r_directedScale->value
			};
			shCoeffs[0][0] += ambientColor[0] * factor;
			shCoeffs[0][1] += ambientColor[1] * factor;
			shCoeffs[0][2] += ambientColor[2] * factor;
			R_AddSHSample( shCoeffs, normal, directedColor, factor );
		}
	}

	if ( totalFactor > 0.0f && totalFactor < 0.99f ) {
		totalFactor = 1.0f / totalFactor;
		for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
			VectorScale( shCoeffs[i], totalFactor, shCoeffs[i] );
		}
	}

	return ( totalFactor > 0.0f );
}


/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *or) {
	int		i;
	vec3_t	temp, temp2;

	for ( i = 0 ; i < count ; i++, dl++ ) {
		VectorSubtract( dl->origin, or->origin, temp );
		dl->transformed[0] = DotProduct( temp, or->axis[0] );
		dl->transformed[1] = DotProduct( temp, or->axis[1] );
		dl->transformed[2] = DotProduct( temp, or->axis[2] );
		if ( dl->linear ) {
			VectorSubtract( dl->origin2, or->origin, temp2 );
			dl->transformed2[0] = DotProduct( temp2, or->axis[0] );
			dl->transformed2[1] = DotProduct( temp2, or->axis[1] );
			dl->transformed2[2] = DotProduct( temp2, or->axis[2] );
		}
	}
}


/*
=============
R_DlightBmodel

Determine which dynamic lights may effect this bmodel
=============
*/
void R_DlightBmodel( bmodel_t *bmodel ) {
	int			i, j;
	const dlight_t	*dl;
	int			mask;
	msurface_t	*surf;

	// transform all the lights
	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.or );

	mask = 0;
	for ( i = 0; i < (int)R_NumSurfaceDlights( tr.refdef.num_dlights ); i++ ) {
		dl = &tr.refdef.dlights[i];

		// see if the point is close enough to the bounds to matter
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( dl->transformed[j] - bmodel->bounds[1][j] > dl->radius ) {
				break;
			}
			if ( bmodel->bounds[0][j] - dl->transformed[j] > dl->radius ) {
				break;
			}
		}
		if ( j < 3 ) {
			continue;
		}

		// we need to check this light
		mask |= 1 << i;
	}

	tr.currentEntity->needDlights = (mask != 0) ? 1 : 0;

	// set the dlight bits in all the surfaces
	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		surf = bmodel->firstSurface + i;

		if ( *surf->data == SF_FACE ) {
			((srfSurfaceFace_t *)surf->data)->dlightBits = mask;
		} else if ( *surf->data == SF_GRID ) {
			((srfGridMesh_t *)surf->data)->dlightBits = mask;
		} else if ( *surf->data == SF_TRIANGLES ) {
			((srfTriangles_t *)surf->data)->dlightBits = mask;
		}
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

extern	cvar_t	*r_ambientScale;
extern	cvar_t	*r_directedScale;
extern	cvar_t	*r_debugLight;

/*
=================
R_SetupEntityLightingGrid

=================
*/
static void R_SetupEntityLightingGrid( trRefEntity_t *ent, world_t *world ) {
	vec3_t	lightOrigin;
	int		pos[3];
	int		i, j;
	byte	*gridData;
	float	frac[3];
	int		gridStep[3];
	vec3_t	direction;
	float	totalFactor;

	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// separate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	VectorSubtract( lightOrigin, world->lightGridOrigin, lightOrigin );
	for ( i = 0 ; i < 3 ; i++ ) {
		float	v;

		v = lightOrigin[i]*world->lightGridInverseSize[i];
		pos[i] = floor( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] > world->lightGridBounds[i] - 1 ) {
			pos[i] = world->lightGridBounds[i] - 1;
		}
	}

	VectorClear( ent->ambientLight );
	VectorClear( ent->directedLight );
	VectorClear( direction );
	R_ClearSHCoeffs( ent->shCoeffs );
	ent->shLightingValid = qfalse;

	if ( !world->lightGridData ) {
		return; /* -nolight maps: no grid, leave lighting zeroed */
	}

	// trilerp the light value
	gridStep[0] = 8;
	gridStep[1] = 8 * world->lightGridBounds[0];
	gridStep[2] = 8 * world->lightGridBounds[0] * world->lightGridBounds[1];
	gridData = world->lightGridData + pos[0] * gridStep[0]
		+ pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0;
	for ( i = 0 ; i < 8 ; i++ ) {
		float	factor;
		byte	*data;
		int		lat, lng;
		vec3_t	normal;
		factor = 1.0;
		data = gridData;
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( i & (1<<j) ) {
				if ( pos[j] + 1 > world->lightGridBounds[j] - 1 ) {
					break; // ignore values outside lightgrid
				}
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= (1.0f - frac[j]);
			}
		}

		if ( j != 3 ) {
			continue;
		}

		if ( !(data[0]+data[1]+data[2]) ) {
			continue;	// ignore samples in walls
		}
		totalFactor += factor;

		ent->ambientLight[0] += factor * data[0];
		ent->ambientLight[1] += factor * data[1];
		ent->ambientLight[2] += factor * data[2];

		ent->directedLight[0] += factor * data[3];
		ent->directedLight[1] += factor * data[4];
		ent->directedLight[2] += factor * data[5];

		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE/256);
		lng *= (FUNCTABLE_SIZE/256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
		normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

		VectorMA( direction, factor, normal, direction );
		{
			vec3_t directedColor = { data[3], data[4], data[5] };
			R_AddSHSample( ent->shCoeffs, normal, directedColor, factor );
		}
	}

	qboolean hasSH = ( totalFactor > 0.0f );
	if ( totalFactor > 0 && totalFactor < 0.99 ) {
		totalFactor = 1.0f / totalFactor;
		VectorScale( ent->ambientLight, totalFactor, ent->ambientLight );
		VectorScale( ent->directedLight, totalFactor, ent->directedLight );
		for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
			VectorScale( ent->shCoeffs[i], totalFactor, ent->shCoeffs[i] );
		}
	}

	VectorScale( ent->ambientLight, r_ambientScale->value, ent->ambientLight );
	VectorScale( ent->directedLight, r_directedScale->value, ent->directedLight );
	for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
		VectorScale( ent->shCoeffs[i], r_directedScale->value, ent->shCoeffs[i] );
	}

	VectorNormalize2( direction, ent->lightDir );
	ent->shLightingValid = hasSH;
}


/*
===============
LogLight
===============
*/
static void LogLight( const trRefEntity_t *ent ) {
	int	max1, max2;

	if ( !(ent->e.renderfx & RF_FIRST_PERSON ) ) {
		return;
	}

	max1 = ent->ambientLight[0];
	if ( ent->ambientLight[1] > max1 ) {
		max1 = ent->ambientLight[1];
	} else if ( ent->ambientLight[2] > max1 ) {
		max1 = ent->ambientLight[2];
	}

	max2 = ent->directedLight[0];
	if ( ent->directedLight[1] > max2 ) {
		max2 = ent->directedLight[1];
	} else if ( ent->directedLight[2] > max2 ) {
		max2 = ent->directedLight[2];
	}

	ri.Printf( PRINT_ALL, "amb:%i  dir:%i\n", max1, max2 );
}


/*
=================
R_SetupEntityLighting

Calculates all the lighting values that will be used
by the Calc_* functions
=================
*/
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
	int				i;
	const dlight_t		*dl;
	float			power;
	vec3_t			dir;
	float			d;
	vec3_t			lightDir;
	vec3_t			lightOrigin;
	vec3_t			shadowLightDir;

	// lighting calculations
	if ( ent->lightingCalculated ) {
		return;
	}
	ent->lightingCalculated = qtrue;

	//
	// trace a sample point down to find ambient light
	//
	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// separate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	// if NOWORLDMODEL, only use dynamic lights (menu system, etc)
	if ( !(refdef->rdflags & RDF_NOWORLDMODEL )
		&& tr.world->lightGridData ) {
		R_SetupEntityLightingGrid( ent, tr.world  );
	} else {
		ent->ambientLight[0] = ent->ambientLight[1] =
			ent->ambientLight[2] = tr.identityLight * 150;
		ent->directedLight[0] = ent->directedLight[1] =
			ent->directedLight[2] = tr.identityLight * 150;
		VectorCopy( tr.sunDirection, ent->lightDir );
		R_ClearSHCoeffs( ent->shCoeffs );
		ent->shLightingValid = qfalse;
	}

#ifdef USE_VULKAN
	/* Raster Ultra 1.3: dynamic objects — probe GI owns indirect diffuse when ready. */
	if ( vk_raster_gi_probes_ready() ) {
		vec3_t probeAmb;
		vec3_t sampleN;
		float conf = 0.0f;
		VectorCopy( ent->lightDir, sampleN );
		if ( VectorLength( sampleN ) < 0.1f ) {
			sampleN[2] = 1.0f;
		}
		VectorNormalize( sampleN );
		if ( vk_raster_gi_sample_entity( lightOrigin, sampleN, probeAmb, &conf ) && conf > 0.05f ) {
			float w = Com_Clamp( 0.0f, 1.0f, conf );
			/* Blend toward probe ambient; keep some lightgrid for continuity. */
			ent->ambientLight[0] = ent->ambientLight[0] * ( 1.0f - w * 0.75f ) + probeAmb[0] * w * 0.75f;
			ent->ambientLight[1] = ent->ambientLight[1] * ( 1.0f - w * 0.75f ) + probeAmb[1] * w * 0.75f;
			ent->ambientLight[2] = ent->ambientLight[2] * ( 1.0f - w * 0.75f ) + probeAmb[2] * w * 0.75f;
		}
	}
#endif

	// bonus items and view weapons have a fixed minimum add
	if ( 1 /* ent->e.renderfx & RF_MINLIGHT */ ) {
		// give everything a minimum light add
		ent->ambientLight[0] += tr.identityLight * 32;
		ent->ambientLight[1] += tr.identityLight * 32;
		ent->ambientLight[2] += tr.identityLight * 32;
	}

	//
	// modify the light by dynamic lights
	//
	d = VectorLength( ent->directedLight );
	VectorScale( ent->lightDir, d, lightDir );
	if ( r_dlightMode->integer == 2 ) {
		// only direct lights
		// but we need to deal with shadow light direction
		VectorCopy( lightDir, shadowLightDir );
		if ( r_shadows->integer == 2 ) {
			for ( i = 0 ; i < (int)refdef->num_dlights ; i++ ) {
				dl = &refdef->dlights[i];
				if ( dl->linear ) // no support for linear lights atm
					continue;
				VectorSubtract( dl->origin, lightOrigin, dir );
				d = VectorNormalize( dir );
				power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
				if ( d < DLIGHT_MINIMUM_RADIUS ) {
					d = DLIGHT_MINIMUM_RADIUS;
				}
				d = power / ( d * d );
				VectorMA( shadowLightDir, d, dir, shadowLightDir );
			}
		} // if ( r_shadows->integer == 2 )
	}  // if ( r_dlightMode->integer == 2 )
	else
	for ( i = 0 ; i < (int)refdef->num_dlights ; i++ ) {
		dl = &refdef->dlights[i];
		VectorSubtract( dl->origin, lightOrigin, dir );
		d = VectorNormalize( dir );

		power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
		if ( d < DLIGHT_MINIMUM_RADIUS ) {
			d = DLIGHT_MINIMUM_RADIUS;
		}
		d = power / ( d * d );

		VectorMA( ent->directedLight, d, dl->color, ent->directedLight );
		VectorMA( lightDir, d, dir, lightDir );
	}

	// clamp ambient
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( ent->ambientLight[i] > tr.identityLightByte ) {
			ent->ambientLight[i] = tr.identityLightByte;
		}
	}

	if ( r_debugLight->integer ) {
		LogLight( ent );
	}

	// save out the byte packet version
	((byte *)&ent->ambientLightInt)[0] = myftol( ent->ambientLight[0] ); // -EC-: don't use ri.ftol to avoid precision losses
	((byte *)&ent->ambientLightInt)[1] = myftol( ent->ambientLight[1] );
	((byte *)&ent->ambientLightInt)[2] = myftol( ent->ambientLight[2] );
	((byte *)&ent->ambientLightInt)[3] = 0xff;

	// transform the direction to local space
	VectorNormalize( lightDir );
	ent->lightDir[0] = DotProduct( lightDir, ent->e.axis[0] );
	ent->lightDir[1] = DotProduct( lightDir, ent->e.axis[1] );
	ent->lightDir[2] = DotProduct( lightDir, ent->e.axis[2] );

	if ( r_shadows->integer == 2 && r_dlightMode->integer == 2 ) {
		VectorNormalize( shadowLightDir );
		ent->shadowLightDir[0] = DotProduct( shadowLightDir, ent->e.axis[0] );
		ent->shadowLightDir[1] = DotProduct( shadowLightDir, ent->e.axis[1] );
		ent->shadowLightDir[2] = DotProduct( shadowLightDir, ent->e.axis[2] );
	}
}


/*
=================
R_LightForPoint
=================
*/
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir )
{
	trRefEntity_t ent;

	if ( tr.world->lightGridData == NULL )
	  return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy( point, ent.e.origin );
	R_SetupEntityLightingGrid( &ent, tr.world );
	VectorCopy(ent.ambientLight, ambientLight);
	VectorCopy(ent.directedLight, directedLight);
	VectorCopy(ent.lightDir, lightDir);

	return qtrue;
}

/*
=================
R_LightDirForPoint
=================
*/
int R_LightDirForPoint( vec3_t point, vec3_t lightDir, vec3_t normal, world_t *world )
{
	trRefEntity_t ent;

	if ( world->lightGridData == NULL )
	  return qfalse;

	Com_Memset( &ent, 0, sizeof(ent) );
	VectorCopy( point, ent.e.origin );
	R_SetupEntityLightingGrid( &ent, world );

	if (DotProduct( ent.lightDir, normal) > 0.2f )
		VectorCopy( ent.lightDir, lightDir );
	else
		VectorCopy( normal, lightDir );

	return qtrue;
}
