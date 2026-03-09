#include "tr_local.h"
#include "vk_skybox_hdr.h"
#include "vk_util.h"
#include "vk_volumetric_fog_color.h"

static void vk_apply_optional_tint( const cvar_t *tintCvar, vec3_t io, qboolean override ) {
	vec3_t tint;

	if ( !tintCvar || !vk_parse_fog_tint_string( tintCvar->string, tint ) ) {
		return;
	}

	if ( override ) {
		VectorCopy( tint, io );
		return;
	}

	io[0] *= tint[0];
	io[1] *= tint[1];
	io[2] *= tint[2];
}

static qboolean vk_get_ibl_fog_color( vec3_t out )
{
	int i;
	int bestIndex = -1;
	float bestDistSq = 0.0f;
	const float *pos = backEnd.viewParms.or.origin;
	vec4_t hdrSh[9];

	if ( !tr.cubemaps || tr.numCubemaps <= 0 ) {
		if ( SkyboxHDR_CopySHCoeffs( hdrSh ) ) {
			out[0] = hdrSh[0][0];
			out[1] = hdrSh[0][1];
			out[2] = hdrSh[0][2];
			vk_normalize_rgb_luma_safe( out );
			return qtrue;
		}
		return qfalse;
	}

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		vec3_t delta;
		float distSq;
		const cubemap_t *cube = &tr.cubemaps[i];

		if ( !cube->hasSHCoeffs ) {
			continue;
		}

		VectorSubtract( pos, cube->origin, delta );
		distSq = VectorLengthSquared( delta );

		if ( bestIndex == -1 || distSq < bestDistSq ) {
			bestIndex = i;
			bestDistSq = distSq;
		}
	}

	if ( bestIndex < 0 ) {
		return qfalse;
	}

	out[0] = tr.cubemaps[bestIndex].shCoeffs[0][0];
	out[1] = tr.cubemaps[bestIndex].shCoeffs[0][1];
	out[2] = tr.cubemaps[bestIndex].shCoeffs[0][2];
	vk_normalize_rgb_luma_safe( out );
	return qtrue;
}

void vk_get_volumetric_fog_color( vec4_t out )
{
	int i;
	vec3_t base;
	float maxc;
	const int colorMode = ( r_volumetricFogColorMode ) ? r_volumetricFogColorMode->integer : 0;
	qboolean foundFogVolume = qfalse;

	VectorCopy( tr.sunLight, base );
	maxc = MAX( base[0], MAX( base[1], base[2] ) );
	if ( maxc <= 0.0f ) {
		VectorSet( base, 1.0f, 1.0f, 1.0f );
	} else if ( maxc > 1.0f ) {
		VectorScale( base, 1.0f / maxc, base );
	}
	Vector4Set( out, base[0], base[1], base[2], 1.0f );

	if ( !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_apply_optional_tint( r_volumetricFogTint, out, qfalse );
		vk_apply_optional_tint( r_fogTint, out, qfalse );
		return;
	}

	if ( colorMode == 1 ) {
		vk_apply_optional_tint( r_volumetricFogTint, out, qtrue );
		vk_apply_optional_tint( r_fogTint, out, qfalse );
		out[3] = 1.0f;
		return;
	}

	for ( i = 1; i < tr.world->numfogs; i++ ) {
		const fog_t *fog = &tr.world->fogs[i];
		const float *o = backEnd.viewParms.or.origin;

		if ( o[0] < fog->bounds[0][0] || o[0] > fog->bounds[1][0] ) {
			continue;
		}
		if ( o[1] < fog->bounds[0][1] || o[1] > fog->bounds[1][1] ) {
			continue;
		}
		if ( o[2] < fog->bounds[0][2] || o[2] > fog->bounds[1][2] ) {
			continue;
		}

		Vector4Copy( fog->color, out );
		foundFogVolume = qtrue;
		break;
	}

	if ( colorMode == 2 && !foundFogVolume ) {
		vec3_t ibl;
		if ( vk_get_ibl_fog_color( ibl ) ) {
			out[0] = ibl[0];
			out[1] = ibl[1];
			out[2] = ibl[2];
			out[3] = 1.0f;
		}
	}

	vk_apply_optional_tint( r_volumetricFogTint, out, qfalse );
	vk_apply_optional_tint( r_fogTint, out, qfalse );
}
