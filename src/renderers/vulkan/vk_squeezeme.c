/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SqueezeMe — linear pose correctives + GCS + LBS, rendered via Mobile-GS.
Paper: arXiv:2412.15171v4 (Meta Codec Avatars Lab, 2025).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_mgs.h"
#include "vk_squeezeme.h"
#include "sqz_format.h"

#define SQZ_DEMO_GAUSSIANS_MAX 1024u

typedef struct {
	qboolean inUse;
	sqzAvatarAsset_t asset;
	char path[MAX_QPATH];
	float worldOrigin[3];
	float pose[SQZ_POSE_DIM];
} sqzAvatarInstance_t;

typedef struct {
	qboolean loaded;
	char mapName[MAX_QPATH];
	uint32_t totalGaussians;
	float animTime;
	sqzAvatarInstance_t avatars[SQZ_MAX_AVATARS];
} sqzState_t;

static sqzState_t sqz;

static cvar_t *r_squeezeme;
static cvar_t *r_squeezeme_tier;
static cvar_t *r_squeezeme_avatars;
static cvar_t *r_squeezeme_gcs;
static cvar_t *r_squeezeme_linear;
static cvar_t *r_squeezeme_asset;
static cvar_t *r_squeezeme_anim;
static cvar_t *r_squeezeme_debug;

static void SQZ_FreeAsset( sqzAvatarAsset_t *a )
{
	Com_Memset( a, 0, sizeof( *a ) );
}

/* Binary loader (see scripts/sqz_pack_demo.py) */
static qboolean SQZ_LoadAssetBlob( const char *qpath, sqzAvatarAsset_t *out )
{
	byte *buf;
	int len;
	const byte *p;
	uint32_t hdr[7];

	if ( !out ) {
		return qfalse;
	}
	SQZ_FreeAsset( out );
	len = ri.FS_ReadFile( qpath, (void **)&buf );
	if ( len < 256 || !buf ) {
		return qfalse;
	}
	p = buf;
	if ( Q_strncmp( (const char *)p, SQZ_MAGIC, 4 ) != 0 ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}
	p += 4;
	Com_Memcpy( hdr, p, sizeof( hdr ) );
	p += sizeof( hdr );

	out->hasLinear = ( hdr[1] & 1u ) != 0;
	out->gcs64 = ( hdr[1] & 2u ) != 0;
	out->numJoints = hdr[2];
	out->gaussianCount = hdr[3];
	if ( hdr[0] != SQZ_VERSION ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	{
		size_t poseMeanSz = SQZ_POSE_DIM * sizeof( float );
		size_t poseBasisSz = SQZ_POSE_DIM * SQZ_POSE_BASIS_COLS * sizeof( float );
		size_t corrSz = (size_t)( SQZ_POSE_BASIS_COLS + 1 ) * SQZ_GCS_CELLS * SQZ_CHANNELS * sizeof( float );
		size_t tmplSz = SQZ_GCS_CELLS * SQZ_CHANNELS * sizeof( float );
		size_t maskSz = SQZ_GCS_CELLS;
		size_t wSz = SQZ_GCS_CELLS * SQZ_MAX_JOINTS * sizeof( float );
		size_t bindSz = SQZ_GCS_CELLS * 3 * sizeof( float );
		size_t total = poseMeanSz + poseBasisSz + corrSz + tmplSz + maskSz + wSz + bindSz;

		if ( (size_t)len < sizeof( hdr ) + total ) {
			ri.FS_FreeFile( buf );
			return qfalse;
		}
		Com_Memcpy( out->poseMean, p, poseMeanSz ); p += poseMeanSz;
		Com_Memcpy( out->poseBasis, p, poseBasisSz ); p += poseBasisSz;
		Com_Memcpy( out->correctivesBasis, p, corrSz ); p += corrSz;
		Com_Memcpy( out->templateMean, p, tmplSz ); p += tmplSz;
		Com_Memcpy( out->mask, p, maskSz ); p += maskSz;
		Com_Memcpy( out->jointWeights, p, wSz ); p += wSz;
		Com_Memcpy( out->bindPos, p, bindSz );
	}

	ri.FS_FreeFile( buf );
	if ( out->numJoints == 0 ) {
		out->numJoints = 12;
	}
	return qtrue;
}

static void SQZ_BuildProceduralAsset( sqzAvatarAsset_t *out )
{
	uint32_t cx, cy;
	float t = sqz.animTime;

	Com_Memset( out, 0, sizeof( *out ) );
	out->hasLinear = r_squeezeme_linear && r_squeezeme_linear->integer;
	out->gcs64 = r_squeezeme_gcs && r_squeezeme_gcs->integer;
	out->numJoints = 12;
	out->gaussianCount = SQZ_DEMO_GAUSSIANS_MAX;

	for ( cy = 0; cy < SQZ_GCS_GRID; cy++ ) {
		for ( cx = 0; cx < SQZ_GCS_GRID; cx++ ) {
			uint32_t idx = cy * SQZ_GCS_GRID + cx;
			float u = ( (float)cx + 0.5f ) / (float)SQZ_GCS_GRID;
			float v = ( (float)cy + 0.5f ) / (float)SQZ_GCS_GRID;
			float dx = u - 0.5f;
			float dy = v - 0.25f;
			float body = 1.0f - ( dx * dx * 4.0f + dy * dy * 1.5f );

			out->bindPos[idx * 3 + 0] = dx * 0.6f;
			out->bindPos[idx * 3 + 1] = dy * 1.75f;
			out->bindPos[idx * 3 + 2] = 0.1f * sinf( t + u * 6.28f );

			out->mask[idx] = ( body > 0.15f ) ? 1 : 0;
			out->jointWeights[idx * SQZ_MAX_JOINTS + 0] = 1.0f;

			out->templateMean[idx * SQZ_CHANNELS + 0] = dx * 0.02f; /* idx = cell */
			out->templateMean[idx * SQZ_CHANNELS + 1] = dy * 0.02f;
			out->templateMean[idx * SQZ_CHANNELS + 2] = 0.01f;
			out->templateMean[idx * SQZ_CHANNELS + 3] = 0.0f;
			out->templateMean[idx * SQZ_CHANNELS + 4] = 0.0f;
			out->templateMean[idx * SQZ_CHANNELS + 5] = 0.0f;
			out->templateMean[idx * SQZ_CHANNELS + 6] = 0.0f;
			out->templateMean[idx * SQZ_CHANNELS + 7] = 1.0f;
			out->templateMean[idx * SQZ_CHANNELS + 8] = 4.0f;
			out->templateMean[idx * SQZ_CHANNELS + 9] = 3.0f;
			out->templateMean[idx * SQZ_CHANNELS + 10] = 5.0f;
			out->templateMean[idx * SQZ_CHANNELS + 11] = 2.5f;
			out->templateMean[idx * SQZ_CHANNELS + 12] = 0.55f + 0.15f * body;
			out->templateMean[idx * SQZ_CHANNELS + 13] = 0.5f + 0.4f * u;
			out->templateMean[idx * SQZ_CHANNELS + 14] = 0.55f + 0.35f * v;
			out->templateMean[idx * SQZ_CHANNELS + 15] = 0.6f;
		}
	}

	if ( out->hasLinear ) {
		uint32_t k;

		Com_Memset( out->poseMean, 0, sizeof( out->poseMean ) );
		for ( k = 0; k < SQZ_POSE_DIM * SQZ_POSE_BASIS_COLS; k++ ) {
			out->poseBasis[k] = ( k % SQZ_POSE_DIM ) == ( k / SQZ_POSE_DIM ) ? 1.0f : 0.0f;
		}
		/* Tiny pose→z corrective on basis column 0 */
		out->correctivesBasis[SQZ_CHANNELS * 4 + 2] = 0.08f;
	}
}

static void SQZ_FillDemoPose( float *pose, float t )
{
	uint32_t j;

	Com_Memset( pose, 0, SQZ_POSE_DIM * sizeof( float ) );
	for ( j = 0; j < 12 && j * 4 + 3 < SQZ_POSE_DIM; j++ ) {
		float phase = t * 1.2f + (float)j * 0.5f;
		pose[j * 4 + 0] = 0.15f * sinf( phase );
		pose[j * 4 + 1] = 0.1f * cosf( phase * 0.7f );
		pose[j * 4 + 2] = 0.05f * sinf( phase * 1.3f );
		pose[j * 4 + 3] = 1.0f;
	}
}

static void SQZ_EvalLinearCorrectives( const sqzAvatarAsset_t *asset, const float *pose, float *cellsOut )
{
	float coeff[SQZ_POSE_BASIS_COLS + 1];
	uint32_t c, i, ch, cell;

	coeff[0] = 1.0f;
	for ( c = 0; c < SQZ_POSE_BASIS_COLS; c++ ) {
		float sum = 0.0f;
		for ( i = 0; i < SQZ_POSE_DIM; i++ ) {
			sum += ( pose[i] - asset->poseMean[i] ) * asset->poseBasis[i * SQZ_POSE_BASIS_COLS + c];
		}
		coeff[c + 1] = sum;
	}

	for ( cell = 0; cell < SQZ_GCS_CELLS; cell++ ) {
		for ( ch = 0; ch < SQZ_CHANNELS; ch++ ) {
			float v = 0.0f;
			uint32_t k;
			size_t base = (size_t)cell * SQZ_CHANNELS + (size_t)ch;

			for ( k = 0; k < (uint32_t)( SQZ_POSE_BASIS_COLS + 1 ); k++ ) {
				size_t idx = k * (size_t)SQZ_GCS_CELLS * SQZ_CHANNELS + base;
				v += coeff[k] * asset->correctivesBasis[idx];
			}
			cellsOut[base] = v;
		}
	}
}

static void SQZ_UpsampleGCSNearest( const float *cells64, float *cells256 )
{
	uint32_t oy, ox, sy, sx;

	for ( oy = 0; oy < SQZ_GCS_GRID * 4; oy++ ) {
		for ( ox = 0; ox < SQZ_GCS_GRID * 4; ox++ ) {
			uint32_t dstIdx = oy * ( SQZ_GCS_GRID * 4 ) + ox;
			sy = oy / 4;
			sx = ox / 4;
			Com_Memcpy( cells256 + dstIdx * SQZ_CHANNELS,
				cells64 + ( sy * SQZ_GCS_GRID + sx ) * SQZ_CHANNELS,
				SQZ_CHANNELS * sizeof( float ) );
		}
	}
}

static void SQZ_JointMatrix( uint32_t joint, const float *pose, float out[16] )
{
	float q[4];
	uint32_t base = joint * 4;

	Com_Memset( out, 0, 16 * sizeof( float ) );
	out[0] = out[5] = out[10] = out[15] = 1.0f;
	if ( base + 3 >= SQZ_POSE_DIM ) {
		return;
	}
	q[0] = pose[base];
	q[1] = pose[base + 1];
	q[2] = pose[base + 2];
	q[3] = pose[base + 3];
	/* Quaternion → 3x3 (column-major for vec multiply) */
	{
		float xx = q[0] * q[0], yy = q[1] * q[1], zz = q[2] * q[2];
		float xy = q[0] * q[1], xz = q[0] * q[2], yz = q[1] * q[2];
		float wx = q[3] * q[0], wy = q[3] * q[1], wz = q[3] * q[2];

		out[0] = 1.0f - 2.0f * ( yy + zz );
		out[1] = 2.0f * ( xy + wz );
		out[2] = 2.0f * ( xz - wy );
		out[4] = 2.0f * ( xy - wz );
		out[5] = 1.0f - 2.0f * ( xx + zz );
		out[6] = 2.0f * ( yz + wx );
		out[8] = 2.0f * ( xz + wy );
		out[9] = 2.0f * ( yz - wx );
		out[10] = 1.0f - 2.0f * ( xx + yy );
	}
}

static void SQZ_TransformPoint( const float m[16], const float p[3], float out[3] )
{
	out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2];
	out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2];
	out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2];
}

static uint32_t SQZ_BakeAvatar( const sqzAvatarInstance_t *inst, const float *pose,
	sqzMgsGaussian_t *out, uint32_t outMax, float bodyScale )
{
	const sqzAvatarAsset_t *asset = &inst->asset;
	float cells64[SQZ_GCS_CELLS * SQZ_CHANNELS];
	float cells256[SQZ_GCS_GRID * 4 * SQZ_GCS_GRID * 4 * SQZ_CHANNELS];
	float jointMats[SQZ_MAX_JOINTS * 16];
	uint32_t count = 0;
	uint32_t j;
	uint32_t fullGrid = SQZ_GCS_GRID * 4;

	Com_Memset( cells64, 0, sizeof( cells64 ) );
	for ( j = 0; j < SQZ_GCS_CELLS; j++ ) {
		Com_Memcpy( cells64 + j * SQZ_CHANNELS, asset->templateMean + j * SQZ_CHANNELS,
			SQZ_CHANNELS * sizeof( float ) );
	}
	if ( asset->hasLinear ) {
		float corr[SQZ_GCS_CELLS * SQZ_CHANNELS];
		SQZ_EvalLinearCorrectives( asset, pose, corr );
		for ( j = 0; j < SQZ_GCS_CELLS * SQZ_CHANNELS; j++ ) {
			cells64[j] += corr[j];
		}
	}

	if ( asset->gcs64 ) {
		SQZ_UpsampleGCSNearest( cells64, cells256 );
	} else {
		Com_Memcpy( cells256, cells64, sizeof( cells64 ) );
		fullGrid = SQZ_GCS_GRID;
	}

	for ( j = 0; j < asset->numJoints && j < SQZ_MAX_JOINTS; j++ ) {
		SQZ_JointMatrix( j, pose, jointMats + j * 16 );
	}

	{
		uint32_t gy, gx;
		for ( gy = 0; gy < fullGrid && count < outMax; gy++ ) {
			for ( gx = 0; gx < fullGrid && count < outMax; gx++ ) {
				uint32_t idx = gy * fullGrid + gx;
				uint32_t srcCell = ( asset->gcs64 ) ? ( gy / 4 ) * SQZ_GCS_GRID + ( gx / 4 ) : idx;
				const float *ch;
				float pos[3], posW[3];
				float skinM[16];
				sqzMgsGaussian_t *g;
				uint32_t jj;

				if ( !asset->mask[srcCell] ) {
					continue;
				}
				ch = cells256 + idx * SQZ_CHANNELS;
				Com_Memset( skinM, 0, sizeof( skinM ) );
				skinM[0] = skinM[5] = skinM[10] = skinM[15] = 1.0f;
				for ( jj = 0; jj < asset->numJoints && jj < SQZ_MAX_JOINTS; jj++ ) {
					float w = asset->jointWeights[srcCell * SQZ_MAX_JOINTS + jj];
					uint32_t k;
					if ( w <= 0.0f ) {
						continue;
					}
					for ( k = 0; k < 16; k++ ) {
						skinM[k] += w * jointMats[jj * 16 + k];
					}
				}
				pos[0] = asset->bindPos[srcCell * 3 + 0] + ch[0];
				pos[1] = asset->bindPos[srcCell * 3 + 1] + ch[1];
				pos[2] = asset->bindPos[srcCell * 3 + 2] + ch[2];
				SQZ_TransformPoint( skinM, pos, posW );

				g = &out[count++];
				g->position[0] = inst->worldOrigin[0] + posW[0] * bodyScale;
				g->position[1] = inst->worldOrigin[1] + posW[1] * bodyScale;
				g->position[2] = inst->worldOrigin[2] + posW[2] * bodyScale;
				g->scale[0] = ch[8];
				g->scale[1] = ch[9];
				g->scale[2] = ch[10];
				g->sigmaScale = ch[11];
				g->rotation[0] = ch[3];
				g->rotation[1] = ch[4];
				g->rotation[2] = ch[5];
				g->rotation[3] = ch[7] != 0.0f ? ch[7] : 1.0f;
				g->opacity = ch[12];
				g->color[0] = ch[13];
				g->color[1] = ch[14];
				g->color[2] = ch[15];
				g->pad = 0.0f;
			}
		}
	}
	return count;
}

int R_SQZ_EffectiveMgsTier( void )
{
	int tier = r_squeezeme_tier ? r_squeezeme_tier->integer : 2;
	if ( tier < 1 ) {
		tier = 1;
	}
	if ( tier > 3 ) {
		tier = 3;
	}
	return tier;
}

static void SQZ_Cmd_Status( void )
{
	int i, n = 0;
	for ( i = 0; i < SQZ_MAX_AVATARS; i++ ) {
		if ( sqz.avatars[i].inUse ) {
			n++;
		}
	}
	ri.Printf( PRINT_ALL,
		"[SQZ] active=%d loaded=%d avatars=%d gaussians=%u tier=%d gcs=%d linear=%d\n",
		R_SQZ_Active() ? 1 : 0,
		sqz.loaded ? 1 : 0,
		n,
		sqz.totalGaussians,
		R_SQZ_EffectiveMgsTier(),
		r_squeezeme_gcs ? r_squeezeme_gcs->integer : 0,
		r_squeezeme_linear ? r_squeezeme_linear->integer : 0 );
}

void R_SQZ_Init( void )
{
	r_squeezeme = ri.Cvar_Get( "r_squeezeme", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_squeezeme_tier = ri.Cvar_Get( "r_squeezeme_tier", "2", CVAR_ARCHIVE_ND );
	r_squeezeme_avatars = ri.Cvar_Get( "r_squeezeme_avatars", "1", CVAR_ARCHIVE_ND );
	r_squeezeme_gcs = ri.Cvar_Get( "r_squeezeme_gcs", "1", CVAR_ARCHIVE_ND );
	r_squeezeme_linear = ri.Cvar_Get( "r_squeezeme_linear", "1", CVAR_ARCHIVE_ND );
	r_squeezeme_asset = ri.Cvar_Get( "r_squeezeme_asset", "", CVAR_ARCHIVE_ND );
	r_squeezeme_anim = ri.Cvar_Get( "r_squeezeme_anim", "1", CVAR_ARCHIVE_ND );
	r_squeezeme_debug = ri.Cvar_Get( "r_squeezeme_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_squeezeme, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_squeezeme_tier, "1", "3", CV_INTEGER );
	ri.Cvar_CheckRange( r_squeezeme_avatars, "1", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_squeezeme,
		"SqueezeMe Gaussian full-body avatars (arXiv:2412.15171). 0=off, 1=on (latched; vid_restart)." );
	ri.Cvar_SetDescription( r_squeezeme_tier,
		"Mobile-GS splat tier when r_mgs=0 (1=mobile … 3=high)." );
	ri.Cvar_SetDescription( r_squeezeme_avatars,
		"Concurrent avatar instances (1–3, Quest-class target)." );
	ri.Cvar_SetDescription( r_squeezeme_gcs,
		"Gaussian corrective sharing: 64² UV cells upsampled 4× (paper GCS)." );
	ri.Cvar_SetDescription( r_squeezeme_linear,
		"Linear distilled pose correctives (DL_GCS) instead of CNN decoder." );
	ri.Cvar_SetDescription( r_squeezeme_asset,
		"Optional .sqz pack path (default: procedural demo avatar)." );

	ri.Cmd_AddCommand( "sqz_status", SQZ_Cmd_Status );

	if ( r_squeezeme->integer > 0 ) {
		ri.Printf( PRINT_ALL,
			"[SQZ] SqueezeMe enabled — linear+GCS avatars via Mobile-GS. See docs/SQUEEZEME.md\n" );
	}
}

void R_SQZ_Shutdown( void )
{
	int i;
	ri.Cmd_RemoveCommand( "sqz_status" );
	for ( i = 0; i < SQZ_MAX_AVATARS; i++ ) {
		SQZ_FreeAsset( &sqz.avatars[i].asset );
	}
	Com_Memset( &sqz, 0, sizeof( sqz ) );
}

void R_SQZ_OnMapLoad( const char *mapBaseName )
{
	int numAvatars, i;
	float originBase[3];

	Com_Memset( &sqz, 0, sizeof( sqz ) );
	if ( !r_squeezeme || r_squeezeme->integer <= 0 ) {
		return;
	}
	if ( mapBaseName && mapBaseName[0] ) {
		Q_strncpyz( sqz.mapName, mapBaseName, sizeof( sqz.mapName ) );
	}

	if ( tr.world ) {
		originBase[0] = tr.world->lightGridOrigin[0] + tr.world->lightGridSize[0] * tr.world->lightGridBounds[0] * 0.5f;
		originBase[1] = tr.world->lightGridOrigin[1] + tr.world->lightGridSize[1] * tr.world->lightGridBounds[1] * 0.5f;
		originBase[2] = tr.world->lightGridOrigin[2] + 96.0f;
	} else {
		VectorCopy( backEnd.viewParms.or.origin, originBase );
	}

	numAvatars = r_squeezeme_avatars ? r_squeezeme_avatars->integer : 1;
	if ( numAvatars < 1 ) {
		numAvatars = 1;
	}
	if ( numAvatars > SQZ_MAX_AVATARS ) {
		numAvatars = SQZ_MAX_AVATARS;
	}

	for ( i = 0; i < numAvatars; i++ ) {
		char path[MAX_QPATH];
		sqzAvatarInstance_t *inst = &sqz.avatars[i];

		inst->inUse = qtrue;
		inst->worldOrigin[0] = originBase[0] + (float)( i - 1 ) * 140.0f;
		inst->worldOrigin[1] = originBase[1];
		inst->worldOrigin[2] = originBase[2];

		if ( r_squeezeme_asset && r_squeezeme_asset->string[0] ) {
			Com_sprintf( path, sizeof( path ), "%s", r_squeezeme_asset->string );
		} else {
			Com_sprintf( path, sizeof( path ), "sqz/demo.sqz" );
		}
		Q_strncpyz( inst->path, path, sizeof( inst->path ) );
		if ( !SQZ_LoadAssetBlob( path, &inst->asset ) ) {
			SQZ_BuildProceduralAsset( &inst->asset );
			if ( r_squeezeme_debug && r_squeezeme_debug->integer ) {
				ri.Printf( PRINT_ALL, "[SQZ] Using procedural avatar %d (no '%s')\n", i, path );
			}
		}
		SQZ_FillDemoPose( inst->pose, 0.0f );
	}

	sqz.loaded = qtrue;
	ri.Printf( PRINT_ALL, "[SQZ] Loaded %d avatar(s) on map '%s' (tier %d)\n",
		numAvatars, sqz.mapName[0] ? sqz.mapName : "?", R_SQZ_EffectiveMgsTier() );
}

void R_SQZ_FrameUpdate( void )
{
	int i;
	static double lastFloatTime;
	double now = backEnd.refdef.floatTime;
	float dt = 0.016f;

	if ( !sqz.loaded || !r_squeezeme_anim || !r_squeezeme_anim->integer ) {
		return;
	}
	if ( lastFloatTime > 0.0 ) {
		dt = (float)( now - lastFloatTime );
		if ( dt <= 0.0f || dt > 0.5f ) {
			dt = 0.016f;
		}
	}
	lastFloatTime = now;
	sqz.animTime += dt;
	for ( i = 0; i < SQZ_MAX_AVATARS; i++ ) {
		if ( sqz.avatars[i].inUse ) {
			SQZ_FillDemoPose( sqz.avatars[i].pose, sqz.animTime + (float)i * 0.4f );
		}
	}
}

qboolean R_SQZ_Enabled( void )
{
	return ( r_squeezeme && r_squeezeme->integer > 0 ) ? qtrue : qfalse;
}

qboolean R_SQZ_Active( void )
{
	return ( R_SQZ_Enabled() && sqz.loaded ) ? qtrue : qfalse;
}

void vk_sqz_apply_after_geometry( void )
{
	sqzMgsGaussian_t *buf;
	uint32_t cap, total = 0;
	int i;
	int t;

	if ( !R_SQZ_Active() ) {
		return;
	}

	R_SQZ_FrameUpdate();

	t = R_SQZ_EffectiveMgsTier();
	cap = 256u;
	if ( t == 1 ) {
		cap = 256u;
	} else if ( t == 2 ) {
		cap = 1024u;
	} else {
		cap = 2048u;
	}
	buf = (sqzMgsGaussian_t *)ri.Malloc( (size_t)cap * sizeof( sqzMgsGaussian_t ) );

	for ( i = 0; i < SQZ_MAX_AVATARS; i++ ) {
		uint32_t n;
		if ( !sqz.avatars[i].inUse || total >= cap ) {
			continue;
		}
		n = SQZ_BakeAvatar( &sqz.avatars[i], sqz.avatars[i].pose, buf + total, cap - total, 180.0f );
		total += n;
	}

	if ( total > 0 && R_MGS_UploadGaussians( total, buf, sizeof( sqzMgsGaussian_t ) ) ) {
		sqz.totalGaussians = total;
		R_MGS_MarkLoaded( sqz.mapName, total );
		R_MGS_EnsurePipelines();
		vk_mgs_apply_after_geometry();
	} else if ( r_squeezeme_debug && r_squeezeme_debug->integer ) {
		ri.Printf( PRINT_WARNING, "[SQZ] bake/upload failed (count=%u)\n", total );
	}

	ri.Free( buf );
}
