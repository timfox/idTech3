/*
===========================================================================
High-Throughput Raster Engine 1.1 — Slice A (skeletal throughput).

Canonical animation records, clip compression, pose ownership,
GPU-skin scheduling hooks, previous-frame validity, animation LOD.

Does not change modern_vulkan.cfg boot. Geometry-cache / compact tangents
are Slice B–C — stubs only here for ownership documentation.
===========================================================================
*/

#pragma once


#include "../common/tr_types.h"

#define VK_HT_ANIM_INVALID_ID       0u
#define VK_HT_ANIM_MAX_SKELETONS    256u
#define VK_HT_ANIM_MAX_CLIPS        512u
#define VK_HT_ANIM_MAX_INSTANCES    1024u
#define VK_HT_ANIM_MAX_BONES        128u
#define VK_HT_ANIM_NAME_MAX         64

typedef enum {
	VK_HT_ANIM_LOD_FULL = 0,
	VK_HT_ANIM_LOD_REDUCED_RATE,
	VK_HT_ANIM_LOD_REDUCED_SKELETON,
	VK_HT_ANIM_LOD_IMPOSTOR,
	VK_HT_ANIM_LOD_COUNT
} vkHtAnimLod_t;

typedef enum {
	VK_HT_BONE_IMPORTANCE_HELPER = 0,
	VK_HT_BONE_IMPORTANCE_DEFAULT,
	VK_HT_BONE_IMPORTANCE_END_EFFECTOR,
	VK_HT_BONE_IMPORTANCE_WEAPON_SOCKET,
	VK_HT_BONE_IMPORTANCE_FACIAL,
	VK_HT_BONE_IMPORTANCE_CRITICAL
} vkHtBoneImportance_t;

typedef enum {
	VK_HT_CLIP_COMPRESS_NONE = 0,
	VK_HT_CLIP_COMPRESS_QUANTIZED,
	VK_HT_CLIP_COMPRESS_FAILED_FALLBACK
} vkHtClipCompress_t;

typedef enum {
	VK_HT_DEFORM_NONE = 0,
	VK_HT_DEFORM_SKELETAL_CPU,
	VK_HT_DEFORM_SKELETAL_GPU_VS,
	VK_HT_DEFORM_MORPH_CPU,
	VK_HT_DEFORM_MORPH_PLUS_SKIN_GPU,
	VK_HT_DEFORM_GEOM_CACHE /* Slice C — reserved */
} vkHtDeformKind_t;

typedef enum {
	VK_HT_STREAM_RESIDENT = 0,
	VK_HT_STREAM_LOADING,
	VK_HT_STREAM_MISSING,
	VK_HT_STREAM_FALLBACK
} vkHtStreamState_t;

/* Layout-compatible with iqmTransform_t (translate/quat/scale). */
typedef struct vkHtAnimPoseSample_s {
	float translate[3];
	float rotate[4];
	float scale[3];
} vkHtAnimPoseSample_t;

typedef struct vkHtAnimCompressMetrics_s {
	float    maxAngularDeg;
	float    avgAngularDeg;
	float    maxPositional;
	float    endEffectorError;
	float    rootMotionError;
	uint32_t sourceBytes;
	uint32_t compressedBytes;
	uint32_t decodeOpsEstimate;
	qboolean withinTolerance;
	qboolean measured; /* qfalse until a real compress+compare runs */
} vkHtAnimCompressMetrics_t;

typedef struct vkHtAnimationSkeleton_s {
	uint32_t id;
	uint32_t generation;
	uint32_t topologySignature;
	uint32_t boneCount;
	uint32_t htResIndex;
	vkHtStreamState_t streamState;
	char     debugName[VK_HT_ANIM_NAME_MAX];
	qboolean alive;
} vkHtAnimationSkeleton_t;

typedef struct vkHtAnimationClip_s {
	uint32_t id;
	uint32_t generation;
	uint32_t skeletonId;
	uint32_t topologySignature;
	float    durationSec;
	float    sampleRate;
	uint32_t frameCount;
	uint32_t boneCount;
	vkHtClipCompress_t compressFormat;
	vkHtStreamState_t  streamState;
	uint32_t sourceBytes;
	uint32_t compressedBytes;
	uint32_t revision;
	qboolean looping;
	qboolean additive;
	qboolean fullPrecisionFallback;
	vkHtAnimCompressMetrics_t metrics;
	char     debugName[VK_HT_ANIM_NAME_MAX];
	qboolean alive;
} vkHtAnimationClip_t;

typedef struct vkHtAnimationPose_s {
	uint32_t id;
	uint32_t clipId;
	uint32_t generation;
	float    timeSec;
	float    prevTimeSec;
	uint32_t frame;
	uint32_t oldFrame;
	float    backlerp;
	qboolean prevValid;
	qboolean motionValid;
} vkHtAnimationPose_t;

typedef struct vkHtAnimationInstance_s {
	uint32_t id;
	uint32_t generation;
	uint32_t skeletonId;
	uint32_t clipId;
	uint32_t poseId;
	uint32_t entityKey; /* stable: (entityNum << 16) ^ modelIndex */
	int      entityNum;
	int      modelIndex;
	vkHtAnimLod_t lod;
	vkHtAnimLod_t lodPending;
	int      lodHoldFrames;
	vkHtDeformKind_t deformKind;
	qboolean visible;
	qboolean shadowVisible;
	qboolean prevDeformValid;
	uint32_t currentOutputGen;
	uint32_t previousOutputGen;
	float    projectedSize;
	float    distance;
	uint32_t lastUpdateFrame;
	uint32_t poseSkipCounter;
	char     debugName[VK_HT_ANIM_NAME_MAX];
	qboolean alive;
} vkHtAnimationInstance_t;

typedef struct vkHtDeformationOutput_s {
	uint32_t id;
	uint32_t instanceId;
	uint32_t generation;
	uint32_t previousGeneration;
	vkHtDeformKind_t kind;
	qboolean currentValid;
	qboolean previousValid;
	qboolean motionValid;
	float    boundsMins[3];
	float    boundsMaxs[3];
	qboolean boundsValid;
} vkHtDeformationOutput_t;

typedef struct vkHtAnimStats_s {
	uint32_t skeletonsAlive;
	uint32_t clipsAlive;
	uint32_t instancesAlive;
	uint32_t gpuSkinDraws;
	uint32_t cpuSkinDraws;
	uint32_t morphPlusSkinDraws;
	uint32_t lodFull;
	uint32_t lodReducedRate;
	uint32_t lodReducedSkeleton;
	uint32_t lodImpostor;
	uint32_t poseUpdates;
	uint32_t poseSkips;
	uint32_t motionInvalidated;
	uint32_t compressAttempts;
	uint32_t compressAccepted;
	uint32_t compressFallbacks;
	uint32_t budgetPoseMsHint; /* wall-clock not claimed; counter only */
	qboolean overflow;
} vkHtAnimStats_t;

void vk_ht_animation_register_cvars( void );
void vk_ht_animation_init( void );
void vk_ht_animation_shutdown( void );
void vk_ht_animation_begin_frame( void );
void vk_ht_animation_end_frame( void );

qboolean vk_ht_animation_active( void );
const vkHtAnimStats_t *vk_ht_animation_stats( void );

/* Canonical records — stable IDs; never raw file pointers as identity. */
uint32_t vk_ht_anim_register_skeleton( const char *debugName, uint32_t boneCount, uint32_t topologySignature );
uint32_t vk_ht_anim_register_clip( const char *debugName, uint32_t skeletonId,
	uint32_t frameCount, uint32_t boneCount, float sampleRate, qboolean looping );

/*
 * Compress IQM pose tracks (translate/quat/scale). Measures angular + positional
 * error against source. Falls back to full precision when tolerance fails.
 * metrics->measured is set only when source data is provided.
 */
qboolean vk_ht_anim_compress_pose_tracks(
	uint32_t clipId,
	const vkHtAnimPoseSample_t *sourcePoses, /* [frameCount * boneCount] */
	uint32_t frameCount,
	uint32_t boneCount,
	const vkHtBoneImportance_t *boneImportance, /* optional, boneCount */
	vkHtAnimCompressMetrics_t *outMetrics );

/* Instance + LOD + previous-frame validity. */
uint32_t vk_ht_anim_acquire_instance( int entityNum, int modelIndex, const char *debugName );
vkHtAnimLod_t vk_ht_anim_select_lod( uint32_t instanceId, float distance, float projectedSize,
	qboolean visible, qboolean shadowVisible, qboolean cinematicImportant );
qboolean vk_ht_anim_should_update_pose( uint32_t instanceId, uint32_t frameIndex );
void vk_ht_anim_note_deformation( uint32_t instanceId, vkHtDeformKind_t kind,
	qboolean motionValid, qboolean prevValid );
void vk_ht_anim_invalidate_previous( uint32_t instanceId, const char *reason );

/* IQM draw-path helpers (Slice A certified: VS GPU skin). */
qboolean vk_ht_anim_want_iqm_gpu_skin( qboolean morphActive, int numPoses );
void vk_ht_anim_count_skin_path( vkHtDeformKind_t kind );

void vk_ht_animation_status_f( void );
void vk_ht_animation_memory_f( void );
void vk_ht_animation_profile_f( void );
void vk_ht_deformation_status_f( void );

