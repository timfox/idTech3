/*
===========================================================================
High-Throughput Raster Engine 1.1 — Slice A implementation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_ht_animation.h"
#include "vk_ht_throughput.h"

#include <math.h>

_Static_assert( sizeof( vkHtAnimPoseSample_t ) == sizeof( iqmTransform_t ),
	"vkHtAnimPoseSample_t must match iqmTransform_t layout" );

static cvar_t *r_htAnimation;
static cvar_t *r_iqmGpu;
static cvar_t *r_animLod;
static cvar_t *r_animLodStart;
static cvar_t *r_animLodEnd;
static cvar_t *r_animLodHysteresis;
static cvar_t *r_animCompress;
static cvar_t *r_animCompressMaxAng;
static cvar_t *r_animCompressMaxPos;
static cvar_t *r_animDebug;

static vkHtAnimationSkeleton_t s_skeletons[VK_HT_ANIM_MAX_SKELETONS];
static vkHtAnimationClip_t     s_clips[VK_HT_ANIM_MAX_CLIPS];
static vkHtAnimationInstance_t s_instances[VK_HT_ANIM_MAX_INSTANCES];
static vkHtDeformationOutput_t s_outputs[VK_HT_ANIM_MAX_INSTANCES];
static vkHtAnimationPose_t     s_poses[VK_HT_ANIM_MAX_INSTANCES];

static uint32_t s_frameIndex;

static vkHtAnimStats_t s_stats;
static qboolean s_cmds;
static qboolean s_logged;

/* Quantized sample storage for accepted clips (pooled, not per-frame). */
typedef struct {
	int16_t  tx, ty, tz;
	int16_t  qx, qy, qz, qw;
	uint8_t  sx, sy, sz;
	uint8_t  _pad;
} vkHtAnimQuantSample_t;

static vkHtAnimQuantSample_t *s_quantPool;
static uint32_t s_quantPoolCapacity;
static uint32_t s_quantPoolUsed;

/*
===============
vk_ht_animation_register_cvars
===============
*/
void vk_ht_animation_register_cvars( void )
{
	if ( r_htAnimation ) {
		return;
	}

	r_htAnimation = ri.Cvar_Get( "r_htAnimation", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_htAnimation, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_htAnimation,
		"High-Throughput Raster 1.1 Slice A animation/deformation hub (latched).\n"
		"Canonical clip records, IQM GPU skin scheduling, anim LOD, compression metrics.\n"
		"Opt-in only — does not change certified modern_vulkan.cfg boot." );
	ri.Cvar_SetGroup( r_htAnimation, CVG_RENDERER );

	r_iqmGpu = ri.Cvar_Get( "r_iqmGpu", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_iqmGpu, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_iqmGpu,
		"Enable IQM VS GPU skinning without requiring morph targets (same SSBO path as morph+skin).\n"
		"Default 0 preserves CPU skin; HT animation profile sets 1." );
	ri.Cvar_SetGroup( r_iqmGpu, CVG_RENDERER );

	r_animLod = ri.Cvar_Get( "r_animLod", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animLod, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_animLod,
		"When r_htAnimation 1: scale skeletal pose update rate by distance (with hysteresis)." );

	r_animLodStart = ri.Cvar_Get( "r_animLodStart", "600", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animLodStart, "0", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_animLodStart, "Distance where animation LOD begins reducing update rate." );

	r_animLodEnd = ri.Cvar_Get( "r_animLodEnd", "2400", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animLodEnd, "0", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_animLodEnd, "Distance where animation LOD reaches impostor/proxy tier." );

	r_animLodHysteresis = ri.Cvar_Get( "r_animLodHysteresis", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animLodHysteresis, "0", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_animLodHysteresis, "Frames to hold before committing an animation LOD change." );

	r_animCompress = ri.Cvar_Get( "r_animCompress", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animCompress, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_animCompress,
		"When r_htAnimation 1: allow quantized clip compression with measured error + full-precision fallback." );

	r_animCompressMaxAng = ri.Cvar_Get( "r_animCompressMaxAng", "2.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animCompressMaxAng, "0.01", "45", CV_FLOAT );
	ri.Cvar_SetDescription( r_animCompressMaxAng, "Max bone rotation error (degrees) for default-importance bones." );

	r_animCompressMaxPos = ri.Cvar_Get( "r_animCompressMaxPos", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animCompressMaxPos, "0.001", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_animCompressMaxPos, "Max bone translation error (world units) for default-importance bones." );

	r_animDebug = ri.Cvar_Get( "r_animDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_animDebug, "0", "3", CV_INTEGER );
}

qboolean vk_ht_animation_active( void )
{
	return ( r_htAnimation && r_htAnimation->integer ) ? qtrue : qfalse;
}

const vkHtAnimStats_t *vk_ht_animation_stats( void )
{
	return &s_stats;
}

/*
===============
Tolerance by bone importance (not a single global epsilon).
===============
*/
static void vk_ht_anim_bone_tolerance( vkHtBoneImportance_t imp, float *outAngDeg, float *outPos )
{
	float ang = r_animCompressMaxAng ? r_animCompressMaxAng->value : 2.0f;
	float pos = r_animCompressMaxPos ? r_animCompressMaxPos->value : 0.5f;

	switch ( imp ) {
	case VK_HT_BONE_IMPORTANCE_CRITICAL:
	case VK_HT_BONE_IMPORTANCE_WEAPON_SOCKET:
		*outAngDeg = ang * 0.25f;
		*outPos = pos * 0.25f;
		break;
	case VK_HT_BONE_IMPORTANCE_END_EFFECTOR:
		*outAngDeg = ang * 0.5f;
		*outPos = pos * 0.5f;
		break;
	case VK_HT_BONE_IMPORTANCE_FACIAL:
		*outAngDeg = ang * 0.35f;
		*outPos = pos * 0.35f;
		break;
	case VK_HT_BONE_IMPORTANCE_HELPER:
		*outAngDeg = ang * 2.0f;
		*outPos = pos * 2.0f;
		break;
	default:
		*outAngDeg = ang;
		*outPos = pos;
		break;
	}
}

static float vk_ht_anim_quat_angular_deg( const float a[4], const float b[4] )
{
	float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	if ( dot < 0.0f ) {
		dot = -dot;
	}
	if ( dot > 1.0f ) {
		dot = 1.0f;
	}
	return ( 2.0f * acosf( dot ) ) * ( 180.0f / (float)M_PI );
}

static void vk_ht_anim_quantize_sample( const vkHtAnimPoseSample_t *src, vkHtAnimQuantSample_t *dst,
	float tScale, float sScale )
{
	float q[4];
	float qlen;
	int i;

	dst->tx = (int16_t)Com_Clamp( -32767.0f, 32767.0f, src->translate[0] * tScale );
	dst->ty = (int16_t)Com_Clamp( -32767.0f, 32767.0f, src->translate[1] * tScale );
	dst->tz = (int16_t)Com_Clamp( -32767.0f, 32767.0f, src->translate[2] * tScale );

	q[0] = src->rotate[0];
	q[1] = src->rotate[1];
	q[2] = src->rotate[2];
	q[3] = src->rotate[3];
	qlen = sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] );
	if ( qlen > 1e-8f ) {
		q[0] /= qlen;
		q[1] /= qlen;
		q[2] /= qlen;
		q[3] /= qlen;
	}
	if ( q[3] < 0.0f ) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}
	dst->qx = (int16_t)Com_Clamp( -32767.0f, 32767.0f, q[0] * 32767.0f );
	dst->qy = (int16_t)Com_Clamp( -32767.0f, 32767.0f, q[1] * 32767.0f );
	dst->qz = (int16_t)Com_Clamp( -32767.0f, 32767.0f, q[2] * 32767.0f );
	dst->qw = (int16_t)Com_Clamp( -32767.0f, 32767.0f, q[3] * 32767.0f );

	for ( i = 0; i < 3; i++ ) {
		float s = src->scale[i] * sScale;
		if ( s < 0.0f ) {
			s = 0.0f;
		}
		if ( s > 255.0f ) {
			s = 255.0f;
		}
		( &dst->sx )[i] = (uint8_t)( s + 0.5f );
	}
	dst->_pad = 0;
}

static void vk_ht_anim_dequantize_sample( const vkHtAnimQuantSample_t *src, vkHtAnimPoseSample_t *dst,
	float tScale, float sScale )
{
	float q[4];
	float qlen;

	dst->translate[0] = (float)src->tx / tScale;
	dst->translate[1] = (float)src->ty / tScale;
	dst->translate[2] = (float)src->tz / tScale;

	q[0] = (float)src->qx / 32767.0f;
	q[1] = (float)src->qy / 32767.0f;
	q[2] = (float)src->qz / 32767.0f;
	q[3] = (float)src->qw / 32767.0f;
	qlen = sqrtf( q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] );
	if ( qlen > 1e-8f ) {
		q[0] /= qlen;
		q[1] /= qlen;
		q[2] /= qlen;
		q[3] /= qlen;
	}
	dst->rotate[0] = q[0];
	dst->rotate[1] = q[1];
	dst->rotate[2] = q[2];
	dst->rotate[3] = q[3];

	dst->scale[0] = (float)src->sx / sScale;
	dst->scale[1] = (float)src->sy / sScale;
	dst->scale[2] = (float)src->sz / sScale;
}

static qboolean vk_ht_anim_ensure_quant_pool( uint32_t extra )
{
	uint32_t need = s_quantPoolUsed + extra;
	vkHtAnimQuantSample_t *neu;

	if ( need <= s_quantPoolCapacity ) {
		return qtrue;
	}
	if ( need < 1024u ) {
		need = 1024u;
	}
	neu = (vkHtAnimQuantSample_t *)ri.Hunk_Alloc( need * sizeof( *neu ), h_low );
	if ( !neu ) {
		return qfalse;
	}
	if ( s_quantPool && s_quantPoolUsed > 0 ) {
		Com_Memcpy( neu, s_quantPool, s_quantPoolUsed * sizeof( *neu ) );
	}
	s_quantPool = neu;
	s_quantPoolCapacity = need;
	return qtrue;
}

/*
===============
vk_ht_anim_register_skeleton
===============
*/
uint32_t vk_ht_anim_register_skeleton( const char *debugName, uint32_t boneCount, uint32_t topologySignature )
{
	uint32_t i;
	vkHtAnimationSkeleton_t *sk;

	if ( !vk_ht_animation_active() || boneCount == 0 || boneCount > VK_HT_ANIM_MAX_BONES ) {
		return VK_HT_ANIM_INVALID_ID;
	}
	for ( i = 1; i < VK_HT_ANIM_MAX_SKELETONS; i++ ) {
		if ( !s_skeletons[i].alive ) {
			sk = &s_skeletons[i];
			Com_Memset( sk, 0, sizeof( *sk ) );
			sk->id = i; /* stable slot id — never a raw pointer */
			sk->generation = 1;
			sk->topologySignature = topologySignature;
			sk->boneCount = boneCount;
			sk->streamState = VK_HT_STREAM_RESIDENT;
			sk->alive = qtrue;
			sk->htResIndex = vk_ht_res_alloc( VK_HT_RES_KIND_SKELETON );
			Q_strncpyz( sk->debugName, debugName ? debugName : "skeleton", sizeof( sk->debugName ) );
			s_stats.skeletonsAlive++;
			return sk->id;
		}
	}
	s_stats.overflow = qtrue;
	return VK_HT_ANIM_INVALID_ID;
}

uint32_t vk_ht_anim_register_clip( const char *debugName, uint32_t skeletonId,
	uint32_t frameCount, uint32_t boneCount, float sampleRate, qboolean looping )
{
	uint32_t i;
	vkHtAnimationClip_t *clip;

	if ( !vk_ht_animation_active() || frameCount == 0 || boneCount == 0 ) {
		return VK_HT_ANIM_INVALID_ID;
	}
	for ( i = 1; i < VK_HT_ANIM_MAX_CLIPS; i++ ) {
		if ( !s_clips[i].alive ) {
			clip = &s_clips[i];
			Com_Memset( clip, 0, sizeof( *clip ) );
			clip->id = i;
			clip->generation = 1;
			clip->skeletonId = skeletonId;
			clip->frameCount = frameCount;
			clip->boneCount = boneCount;
			clip->sampleRate = ( sampleRate > 0.0f ) ? sampleRate : 1.0f;
			clip->durationSec = (float)frameCount / clip->sampleRate;
			clip->compressFormat = VK_HT_CLIP_COMPRESS_NONE;
			clip->streamState = VK_HT_STREAM_RESIDENT;
			clip->looping = looping;
			clip->sourceBytes = frameCount * boneCount * (uint32_t)sizeof( vkHtAnimPoseSample_t );
			clip->compressedBytes = clip->sourceBytes;
			Q_strncpyz( clip->debugName, debugName ? debugName : "clip", sizeof( clip->debugName ) );
			clip->alive = qtrue;
			s_stats.clipsAlive++;
			return clip->id;
		}
	}
	s_stats.overflow = qtrue;
	return VK_HT_ANIM_INVALID_ID;
}

/*
===============
vk_ht_anim_compress_pose_tracks
===============
*/
qboolean vk_ht_anim_compress_pose_tracks(
	uint32_t clipId,
	const vkHtAnimPoseSample_t *sourcePoses,
	uint32_t frameCount,
	uint32_t boneCount,
	const vkHtBoneImportance_t *boneImportance,
	vkHtAnimCompressMetrics_t *outMetrics )
{
	vkHtAnimationClip_t *clip;
	vkHtAnimCompressMetrics_t metrics;
	uint32_t f, b, n;
	float tScale = 256.0f;
	float sScale = 64.0f;
	uint32_t quantBase;
	double angSum = 0.0;
	uint32_t angCount = 0;
	qboolean ok = qtrue;

	Com_Memset( &metrics, 0, sizeof( metrics ) );

	if ( !sourcePoses || frameCount == 0 || boneCount == 0 || boneCount > VK_HT_ANIM_MAX_BONES ) {
		if ( outMetrics ) {
			*outMetrics = metrics;
		}
		return qfalse;
	}

	metrics.sourceBytes = frameCount * boneCount * (uint32_t)sizeof( vkHtAnimPoseSample_t );
	metrics.measured = qtrue;
	s_stats.compressAttempts++;

	if ( !r_animCompress || !r_animCompress->integer || !vk_ht_animation_active() ) {
		metrics.compressedBytes = metrics.sourceBytes;
		metrics.withinTolerance = qtrue;
		if ( clipId > 0 && clipId < VK_HT_ANIM_MAX_CLIPS && s_clips[clipId].alive ) {
			s_clips[clipId].compressFormat = VK_HT_CLIP_COMPRESS_NONE;
			s_clips[clipId].metrics = metrics;
			s_clips[clipId].fullPrecisionFallback = qtrue;
		}
		s_stats.compressFallbacks++;
		if ( outMetrics ) {
			*outMetrics = metrics;
		}
		return qtrue;
	}

	n = frameCount * boneCount;
	if ( !vk_ht_anim_ensure_quant_pool( n ) ) {
		metrics.compressedBytes = metrics.sourceBytes;
		metrics.withinTolerance = qfalse;
		s_stats.compressFallbacks++;
		if ( outMetrics ) {
			*outMetrics = metrics;
		}
		return qfalse;
	}
	quantBase = s_quantPoolUsed;

	for ( f = 0; f < frameCount; f++ ) {
		for ( b = 0; b < boneCount; b++ ) {
			const vkHtAnimPoseSample_t *src = &sourcePoses[f * boneCount + b];
			vkHtAnimQuantSample_t *q = &s_quantPool[quantBase + f * boneCount + b];
			vkHtAnimPoseSample_t decoded;
			vkHtBoneImportance_t imp = boneImportance ? boneImportance[b] : VK_HT_BONE_IMPORTANCE_DEFAULT;
			float angTol, posTol, angErr, posErr;
			float ddx, ddy, ddz;

			vk_ht_anim_quantize_sample( src, q, tScale, sScale );
			vk_ht_anim_dequantize_sample( q, &decoded, tScale, sScale );

			angErr = vk_ht_anim_quat_angular_deg( src->rotate, decoded.rotate );
			ddx = decoded.translate[0] - src->translate[0];
			ddy = decoded.translate[1] - src->translate[1];
			ddz = decoded.translate[2] - src->translate[2];
			posErr = sqrtf( ddx * ddx + ddy * ddy + ddz * ddz );

			if ( angErr > metrics.maxAngularDeg ) {
				metrics.maxAngularDeg = angErr;
			}
			if ( posErr > metrics.maxPositional ) {
				metrics.maxPositional = posErr;
			}
			angSum += (double)angErr;
			angCount++;

			if ( b == 0 && posErr > metrics.rootMotionError ) {
				metrics.rootMotionError = posErr;
			}
			if ( ( imp == VK_HT_BONE_IMPORTANCE_END_EFFECTOR ||
				imp == VK_HT_BONE_IMPORTANCE_WEAPON_SOCKET ||
				imp == VK_HT_BONE_IMPORTANCE_CRITICAL ) &&
				posErr > metrics.endEffectorError ) {
				metrics.endEffectorError = posErr;
			}

			vk_ht_anim_bone_tolerance( imp, &angTol, &posTol );
			if ( angErr > angTol || posErr > posTol ) {
				ok = qfalse;
			}
		}
	}

	if ( angCount > 0 ) {
		metrics.avgAngularDeg = (float)( angSum / (double)angCount );
	}
	metrics.compressedBytes = n * (uint32_t)sizeof( vkHtAnimQuantSample_t );
	metrics.decodeOpsEstimate = n;
	metrics.withinTolerance = ok;

	s_quantPoolUsed = quantBase + n;

	if ( clipId > 0 && clipId < VK_HT_ANIM_MAX_CLIPS && s_clips[clipId].alive ) {
		clip = &s_clips[clipId];
		clip->metrics = metrics;
		clip->sourceBytes = metrics.sourceBytes;
		clip->compressedBytes = metrics.compressedBytes;
		clip->revision++;
		if ( ok ) {
			clip->compressFormat = VK_HT_CLIP_COMPRESS_QUANTIZED;
			clip->fullPrecisionFallback = qfalse;
			s_stats.compressAccepted++;
		} else {
			clip->compressFormat = VK_HT_CLIP_COMPRESS_FAILED_FALLBACK;
			clip->fullPrecisionFallback = qtrue;
			clip->compressedBytes = metrics.sourceBytes;
			s_stats.compressFallbacks++;
			/* Roll back pool use — keep full-precision owner. */
			s_quantPoolUsed = quantBase;
		}
	} else if ( ok ) {
		s_stats.compressAccepted++;
	} else {
		s_stats.compressFallbacks++;
		s_quantPoolUsed = quantBase;
	}

	if ( outMetrics ) {
		*outMetrics = metrics;
	}
	return ok;
}

/*
===============
vk_ht_anim_acquire_instance
===============
*/
uint32_t vk_ht_anim_acquire_instance( int entityNum, int modelIndex, const char *debugName )
{
	uint32_t key;
	uint32_t i;
	vkHtAnimationInstance_t *inst;

	if ( !vk_ht_animation_active() ) {
		return VK_HT_ANIM_INVALID_ID;
	}

	key = ( (uint32_t)entityNum << 16 ) ^ (uint32_t)modelIndex;
	for ( i = 1; i < VK_HT_ANIM_MAX_INSTANCES; i++ ) {
		if ( s_instances[i].alive && s_instances[i].entityKey == key ) {
			return s_instances[i].id;
		}
	}
	for ( i = 1; i < VK_HT_ANIM_MAX_INSTANCES; i++ ) {
		if ( !s_instances[i].alive ) {
			inst = &s_instances[i];
			Com_Memset( inst, 0, sizeof( *inst ) );
			inst->id = i;
			inst->generation = 1;
			inst->entityKey = key;
			inst->entityNum = entityNum;
			inst->modelIndex = modelIndex;
			inst->lod = VK_HT_ANIM_LOD_FULL;
			inst->lodPending = VK_HT_ANIM_LOD_FULL;
			inst->deformKind = VK_HT_DEFORM_NONE;
			inst->alive = qtrue;
			Q_strncpyz( inst->debugName, debugName ? debugName : "anim", sizeof( inst->debugName ) );

			Com_Memset( &s_outputs[i], 0, sizeof( s_outputs[i] ) );
			s_outputs[i].id = i;
			s_outputs[i].instanceId = i;
			s_outputs[i].generation = 1;

			Com_Memset( &s_poses[i], 0, sizeof( s_poses[i] ) );
			s_poses[i].id = i;

			s_stats.instancesAlive++;
			return inst->id;
		}
	}
	s_stats.overflow = qtrue;
	return VK_HT_ANIM_INVALID_ID;
}

vkHtAnimLod_t vk_ht_anim_select_lod( uint32_t instanceId, float distance, float projectedSize,
	qboolean visible, qboolean shadowVisible, qboolean cinematicImportant )
{
	vkHtAnimationInstance_t *inst;
	vkHtAnimLod_t want = VK_HT_ANIM_LOD_FULL;
	float start, end;
	int hold;

	if ( instanceId == 0 || instanceId >= VK_HT_ANIM_MAX_INSTANCES || !s_instances[instanceId].alive ) {
		return VK_HT_ANIM_LOD_FULL;
	}
	inst = &s_instances[instanceId];
	inst->distance = distance;
	inst->projectedSize = projectedSize;
	inst->visible = visible;
	inst->shadowVisible = shadowVisible;

	if ( !r_animLod || !r_animLod->integer || cinematicImportant ) {
		inst->lod = VK_HT_ANIM_LOD_FULL;
		inst->lodPending = VK_HT_ANIM_LOD_FULL;
		inst->lodHoldFrames = 0;
		s_stats.lodFull++;
		return inst->lod;
	}

	start = r_animLodStart ? r_animLodStart->value : 600.0f;
	end = r_animLodEnd ? r_animLodEnd->value : 2400.0f;
	if ( end <= start ) {
		end = start + 1.0f;
	}

	if ( !visible && !shadowVisible ) {
		want = VK_HT_ANIM_LOD_IMPOSTOR;
	} else if ( distance >= end || projectedSize < 4.0f ) {
		want = VK_HT_ANIM_LOD_IMPOSTOR;
	} else if ( distance >= ( start + end ) * 0.5f ) {
		want = VK_HT_ANIM_LOD_REDUCED_SKELETON;
	} else if ( distance >= start ) {
		want = VK_HT_ANIM_LOD_REDUCED_RATE;
	} else {
		want = VK_HT_ANIM_LOD_FULL;
	}

	hold = r_animLodHysteresis ? r_animLodHysteresis->integer : 8;
	if ( want != inst->lodPending ) {
		inst->lodPending = want;
		inst->lodHoldFrames = 0;
	} else {
		inst->lodHoldFrames++;
		if ( inst->lodHoldFrames >= hold && inst->lod != want ) {
			inst->lod = want;
		}
	}

	switch ( inst->lod ) {
	case VK_HT_ANIM_LOD_REDUCED_RATE: s_stats.lodReducedRate++; break;
	case VK_HT_ANIM_LOD_REDUCED_SKELETON: s_stats.lodReducedSkeleton++; break;
	case VK_HT_ANIM_LOD_IMPOSTOR: s_stats.lodImpostor++; break;
	default: s_stats.lodFull++; break;
	}
	return inst->lod;
}

qboolean vk_ht_anim_should_update_pose( uint32_t instanceId, uint32_t frameIndex )
{
	vkHtAnimationInstance_t *inst;
	uint32_t period;

	if ( instanceId == 0 || instanceId >= VK_HT_ANIM_MAX_INSTANCES || !s_instances[instanceId].alive ) {
		return qtrue;
	}
	inst = &s_instances[instanceId];

	switch ( inst->lod ) {
	case VK_HT_ANIM_LOD_REDUCED_RATE:
		period = 2;
		break;
	case VK_HT_ANIM_LOD_REDUCED_SKELETON:
		period = 3;
		break;
	case VK_HT_ANIM_LOD_IMPOSTOR:
		period = 8;
		break;
	default:
		period = 1;
		break;
	}

	if ( ( frameIndex % period ) == 0 || inst->lastUpdateFrame == 0 ) {
		inst->lastUpdateFrame = frameIndex;
		inst->poseSkipCounter = 0;
		s_stats.poseUpdates++;
		return qtrue;
	}
	inst->poseSkipCounter++;
	s_stats.poseSkips++;
	return qfalse;
}

void vk_ht_anim_note_deformation( uint32_t instanceId, vkHtDeformKind_t kind,
	qboolean motionValid, qboolean prevValid )
{
	vkHtAnimationInstance_t *inst;
	vkHtDeformationOutput_t *out;

	if ( instanceId == 0 || instanceId >= VK_HT_ANIM_MAX_INSTANCES || !s_instances[instanceId].alive ) {
		return;
	}
	inst = &s_instances[instanceId];
	out = &s_outputs[instanceId];

	inst->deformKind = kind;
	inst->previousOutputGen = inst->currentOutputGen;
	inst->currentOutputGen++;
	inst->prevDeformValid = prevValid;

	out->previousGeneration = out->generation;
	out->generation = inst->currentOutputGen;
	out->kind = kind;
	out->currentValid = qtrue;
	out->previousValid = prevValid;
	out->motionValid = motionValid;

	if ( !motionValid ) {
		s_stats.motionInvalidated++;
	}
}

void vk_ht_anim_invalidate_previous( uint32_t instanceId, const char *reason )
{
	if ( instanceId == 0 || instanceId >= VK_HT_ANIM_MAX_INSTANCES || !s_instances[instanceId].alive ) {
		return;
	}
	s_instances[instanceId].prevDeformValid = qfalse;
	s_outputs[instanceId].previousValid = qfalse;
	s_outputs[instanceId].motionValid = qfalse;
	s_poses[instanceId].prevValid = qfalse;
	s_poses[instanceId].motionValid = qfalse;
	s_stats.motionInvalidated++;
	if ( r_animDebug && r_animDebug->integer && reason ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][ht-anim] invalidate prev inst=%u (%s)\n", instanceId, reason );
	}
}

qboolean vk_ht_anim_want_iqm_gpu_skin( qboolean morphActive, int numPoses )
{
	if ( numPoses <= 0 ) {
		return qfalse;
	}
	if ( morphActive ) {
		return qtrue;
	}
	if ( r_iqmGpu && r_iqmGpu->integer ) {
		return qtrue;
	}
	return qfalse;
}

void vk_ht_anim_count_skin_path( vkHtDeformKind_t kind )
{
	switch ( kind ) {
	case VK_HT_DEFORM_SKELETAL_GPU_VS:
		s_stats.gpuSkinDraws++;
		break;
	case VK_HT_DEFORM_MORPH_PLUS_SKIN_GPU:
		s_stats.morphPlusSkinDraws++;
		break;
	case VK_HT_DEFORM_SKELETAL_CPU:
	case VK_HT_DEFORM_MORPH_CPU:
		s_stats.cpuSkinDraws++;
		break;
	default:
		break;
	}
}

void vk_ht_animation_begin_frame( void )
{
	if ( !vk_ht_animation_active() ) {
		return;
	}
	s_frameIndex++;
	s_stats.lodFull = 0;
	s_stats.lodReducedRate = 0;
	s_stats.lodReducedSkeleton = 0;
	s_stats.lodImpostor = 0;
	s_stats.gpuSkinDraws = 0;
	s_stats.cpuSkinDraws = 0;
	s_stats.morphPlusSkinDraws = 0;
	s_stats.poseUpdates = 0;
	s_stats.poseSkips = 0;
}

void vk_ht_animation_end_frame( void )
{
}

void vk_ht_animation_init( void )
{
	vk_ht_animation_register_cvars();
	Com_Memset( s_skeletons, 0, sizeof( s_skeletons ) );
	Com_Memset( s_clips, 0, sizeof( s_clips ) );
	Com_Memset( s_instances, 0, sizeof( s_instances ) );
	Com_Memset( s_outputs, 0, sizeof( s_outputs ) );
	Com_Memset( s_poses, 0, sizeof( s_poses ) );
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
	s_quantPool = NULL;
	s_quantPoolCapacity = 0;
	s_quantPoolUsed = 0;
	s_frameIndex = 0;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "animation_status", vk_ht_animation_status_f );
		ri.Cmd_AddCommand( "animation_memory", vk_ht_animation_memory_f );
		ri.Cmd_AddCommand( "animation_profile", vk_ht_animation_profile_f );
		ri.Cmd_AddCommand( "deformation_status", vk_ht_deformation_status_f );
		s_cmds = qtrue;
	}

	if ( vk_ht_animation_active() && !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][ht-anim] High-Throughput Raster 1.1 Slice A active "
			"(records+compress+IQM GPU skin via r_iqmGpu + anim LOD). Not boot default.\n" );
		s_logged = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][iqm] GPU skin path: %s (r_iqmGpu)\n",
		( r_iqmGpu && r_iqmGpu->integer ) ? "on" : "off" );
}

void vk_ht_animation_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "animation_status" );
		ri.Cmd_RemoveCommand( "animation_memory" );
		ri.Cmd_RemoveCommand( "animation_profile" );
		ri.Cmd_RemoveCommand( "deformation_status" );
		s_cmds = qfalse;
	}
	Com_Memset( s_skeletons, 0, sizeof( s_skeletons ) );
	Com_Memset( s_clips, 0, sizeof( s_clips ) );
	Com_Memset( s_instances, 0, sizeof( s_instances ) );
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
	s_quantPool = NULL;
	s_quantPoolCapacity = 0;
	s_quantPoolUsed = 0;
	s_logged = qfalse;
}

void vk_ht_animation_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== HT Animation 1.1 Slice A ===\n" );
	ri.Printf( PRINT_ALL, "active           : %s\n", vk_ht_animation_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "r_iqmGpu         : %d\n", r_iqmGpu ? r_iqmGpu->integer : 0 );
	ri.Printf( PRINT_ALL, "r_animLod        : %d\n", r_animLod ? r_animLod->integer : 0 );
	ri.Printf( PRINT_ALL, "skeletons        : %u\n", s_stats.skeletonsAlive );
	ri.Printf( PRINT_ALL, "clips            : %u\n", s_stats.clipsAlive );
	ri.Printf( PRINT_ALL, "instances        : %u\n", s_stats.instancesAlive );
	ri.Printf( PRINT_ALL, "gpuSkin draws    : %u\n", s_stats.gpuSkinDraws );
	ri.Printf( PRINT_ALL, "cpuSkin draws    : %u\n", s_stats.cpuSkinDraws );
	ri.Printf( PRINT_ALL, "morph+skin draws : %u\n", s_stats.morphPlusSkinDraws );
	ri.Printf( PRINT_ALL, "lod full/rate/skel/imp : %u/%u/%u/%u\n",
		s_stats.lodFull, s_stats.lodReducedRate, s_stats.lodReducedSkeleton, s_stats.lodImpostor );
	ri.Printf( PRINT_ALL, "pose update/skip : %u/%u\n", s_stats.poseUpdates, s_stats.poseSkips );
	ri.Printf( PRINT_ALL, "motion invalidate: %u\n", s_stats.motionInvalidated );
	ri.Printf( PRINT_ALL, "compress ok/fail : %u/%u (attempts %u)\n",
		s_stats.compressAccepted, s_stats.compressFallbacks, s_stats.compressAttempts );
	ri.Printf( PRINT_ALL, "ownership: pose=animation, skin=GPU VS (certified), "
		"material=shader, collision=gameplay\n" );
	ri.Printf( PRINT_ALL, "Slice B/C (compact tangents, geom-cache): not started\n" );
}

void vk_ht_animation_memory_f( void )
{
	uint32_t quantBytes = s_quantPoolUsed * (uint32_t)sizeof( vkHtAnimQuantSample_t );
	uint32_t tables = (uint32_t)( sizeof( s_skeletons ) + sizeof( s_clips ) +
		sizeof( s_instances ) + sizeof( s_outputs ) + sizeof( s_poses ) );

	ri.Printf( PRINT_ALL, "=== HT Animation memory ===\n" );
	ri.Printf( PRINT_ALL, "record tables    : %u bytes\n", tables );
	ri.Printf( PRINT_ALL, "quant pool used  : %u / %u samples (%u bytes)\n",
		s_quantPoolUsed, s_quantPoolCapacity, quantBytes );
	ri.Printf( PRINT_ALL, "Note: GPU skin SSBO payloads are transient per-draw (vk_alloc_storage).\n" );
}

void vk_ht_animation_profile_f( void )
{
	ri.Printf( PRINT_ALL, "=== HT Animation profile ===\n" );
	ri.Printf( PRINT_ALL, "Frame counters only — no invented ms timings.\n" );
	ri.Printf( PRINT_ALL, "poseUpdates=%u poseSkips=%u gpuSkin=%u cpuSkin=%u morphSkin=%u\n",
		s_stats.poseUpdates, s_stats.poseSkips, s_stats.gpuSkinDraws,
		s_stats.cpuSkinDraws, s_stats.morphPlusSkinDraws );
}

void vk_ht_deformation_status_f( void )
{
	uint32_t i;
	uint32_t shown = 0;

	ri.Printf( PRINT_ALL, "=== Deformation status (alive instances) ===\n" );
	for ( i = 1; i < VK_HT_ANIM_MAX_INSTANCES && shown < 32; i++ ) {
		const vkHtAnimationInstance_t *inst;
		const vkHtDeformationOutput_t *out;
		if ( !s_instances[i].alive ) {
			continue;
		}
		inst = &s_instances[i];
		out = &s_outputs[i];
		ri.Printf( PRINT_ALL,
			"  id=%u ent=%d model=%d lod=%d deform=%d curGen=%u prevGen=%u "
			"prevOk=%d motionOk=%d dist=%.1f '%s'\n",
			inst->id, inst->entityNum, inst->modelIndex, (int)inst->lod, (int)inst->deformKind,
			inst->currentOutputGen, inst->previousOutputGen,
			inst->prevDeformValid ? 1 : 0, out->motionValid ? 1 : 0,
			inst->distance, inst->debugName );
		shown++;
	}
	if ( shown == 0 ) {
		ri.Printf( PRINT_ALL, "  (none)\n" );
	}
}
