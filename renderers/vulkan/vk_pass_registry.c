/*
===========================================================================
Lightweight Spine pass / resource registry implementation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_pass_registry.h"
#include "vk_scene_pass.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "tr_render_mode_vk.h"
#include "vk_selective_reflection.h"
#include "vk_postfx.h"
#include <math.h>
#include <stdarg.h>

cvar_t *r_spineValidate;
cvar_t *r_spineCert;

#define VK_SPINE_MAX_OPEN 8
#define VK_SPINE_MAX_VIOLATIONS 16
#define VK_SPINE_CERT_BLACK_SETTLE_FRAMES 45
#define VK_SPINE_CERT_BLACK_STREAK 8
#define VK_SPINE_CERT_BLACK_LOG_LUM_FLOOR (-12.0f)

typedef struct {
	vkSpinePassId id;
	const char *name;
	vkSpinePassCategory category;
	vkSpinePhase phase;
	uint32_t viewClassMask; /* bit (1u << vkViewClass_t); ~0u = any */
	qboolean allowPhaseRegression; /* main resume / continuation */
	qboolean usesTemporalHistory;
	const vkSpineResourceEdge *reads;
	int readCount;
	const vkSpineResourceEdge *writes;
	int writeCount;
} vkSpinePassDesc;

typedef struct {
	qboolean alive;
	uint32_t generation;
	uint32_t width;
	uint32_t height;
	VkFormat format;
	vkSpinePassId lastWriter;
	vkSpinePassId lastReader;
	uint32_t lastWriteAccess;
	uint32_t lastReadAccess;
	qboolean historyValid;
	vkViewClass_t ownerViewClass;
	qboolean clearedThisFrame;
	qboolean barrierThisFrame;
	vkSpinePassId lastClearPass;
	vkSpinePassId lastBarrierPass;
	char lastBarrierReason[48];
	VkImageLayout layout;
	qboolean layoutKnown;
} vkSpineResourceRuntime;

typedef struct {
	qboolean initialized;
	uint32_t attachmentGeneration;
	uint32_t descriptorGeneration;
	qboolean descriptorsPendingRebound;
	uint32_t liveResourceCount;
	uint32_t liveResourceBaseline;
	qboolean liveBaselineArmed;
	uint32_t certFrameIndex;
	uint32_t blackStreak;
	qboolean oitResolvedThisFrame;
	qboolean oitSkippedThisFrame;
	vkSpinePhase currentPhase;
	vkSpinePhase highestPhase;
	vkSpinePassId openStack[VK_SPINE_MAX_OPEN];
	int openCount;
	vkSpinePassId lastBegun;
	vkSpinePassId lastEnded;
	uint32_t framePassMask[( VK_SPINE_PASS_COUNT + 31 ) / 32];
	vkSpineResourceRuntime resources[VK_SPINE_RES_COUNT];
	uint32_t violationCount;
	char lastViolation[128];
	char comboFallback[96];
	qboolean suppressTaaThisFrame;
	qboolean spine11CertActive;
} vkSpineRuntime;

static vkSpineRuntime s_spine;

/* ---- Declared edges (static; read-only tables) ---- */

static const vkSpineResourceEdge s_reads_world_opaque[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ | VK_SPINE_ACCESS_DEPTH_WRITE },
	{ VK_SPINE_RES_FORWARD_PLUS_LIGHTS, VK_SPINE_ACCESS_SAMPLED_READ | VK_SPINE_ACCESS_STORAGE_READ },
};
static const vkSpineResourceEdge s_writes_world_opaque[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_WRITE },
	{ VK_SPINE_RES_MOTION_VECTORS, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_gbuffer[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
};
static const vkSpineResourceEdge s_writes_gbuffer[] = {
	{ VK_SPINE_RES_GBUFFER_ALBEDO, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_GBUFFER_MATERIAL, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_MOTION_VECTORS, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_deferred_light[] = {
	{ VK_SPINE_RES_GBUFFER_ALBEDO, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_GBUFFER_MATERIAL, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
};
static const vkSpineResourceEdge s_writes_deferred_light[] = {
	{ VK_SPINE_RES_DEFERRED_LIGHTING, VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_av[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
	{ VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_MOTION_VECTORS, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_AV_HISTORY, VK_SPINE_ACCESS_HISTORY_READ },
};
static const vkSpineResourceEdge s_writes_av[] = {
	{ VK_SPINE_RES_AV_HISTORY, VK_SPINE_ACCESS_HISTORY_WRITE },
	{ VK_SPINE_RES_AV_FILTERED, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_raster_gi[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
	{ VK_SPINE_RES_GBUFFER_ALBEDO, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_AV_FILTERED, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_PROBE_GRID, VK_SPINE_ACCESS_STORAGE_READ },
};
static const vkSpineResourceEdge s_writes_raster_gi[] = {
	{ VK_SPINE_RES_PROBE_IRRADIANCE, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_SSGI_RADIANCE, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_SSGI_CONFIDENCE, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_RADIANCE_CLIPMAP, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_RADIANCE_CACHE_IRRADIANCE, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_INDIRECT_DIFFUSE, VK_SPINE_ACCESS_STORAGE_WRITE },
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_ssr[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
	{ VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_ssr[] = {
	{ VK_SPINE_RES_SSR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_oit_accum[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
	{ VK_SPINE_RES_FORWARD_PLUS_LIGHTS, VK_SPINE_ACCESS_STORAGE_READ },
};
static const vkSpineResourceEdge s_writes_wboit[] = {
	{ VK_SPINE_RES_OIT_ACCUM, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_OIT_REVEAL, VK_SPINE_ACCESS_COLOR_WRITE },
};
static const vkSpineResourceEdge s_writes_mboit_moments[] = {
	{ VK_SPINE_RES_OIT_MOMENTS, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_OIT_B0, VK_SPINE_ACCESS_COLOR_WRITE },
};
static const vkSpineResourceEdge s_writes_mboit_accum[] = {
	{ VK_SPINE_RES_OIT_ACCUM, VK_SPINE_ACCESS_COLOR_WRITE },
};
static const vkSpineResourceEdge s_reads_oit_resolve[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_OIT_ACCUM, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_OIT_REVEAL, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_OIT_MOMENTS, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_oit_resolve[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_taa[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_MOTION_VECTORS, VK_SPINE_ACCESS_SAMPLED_READ },
	/* History is optional on the first post-reset frame; stamped HISTORY_READ only when valid. */
	{ VK_SPINE_RES_TAA_HISTORY, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_REACTIVE_MASK, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_taa[] = {
	{ VK_SPINE_RES_TAA_HISTORY, VK_SPINE_ACCESS_HISTORY_WRITE },
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_reactive[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
	{ VK_SPINE_RES_MOTION_VECTORS, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_reactive[] = {
	{ VK_SPINE_RES_REACTIVE_MASK, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_bloom[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_bloom[] = {
	{ VK_SPINE_RES_BLOOM_CHAIN, VK_SPINE_ACCESS_COLOR_WRITE | VK_SPINE_ACCESS_TRANSFER_WRITE },
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_exposure[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_exposure[] = {
	{ VK_SPINE_RES_EXPOSURE_LUMINANCE, VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_HISTORY_WRITE },
};

static const vkSpineResourceEdge s_reads_smaa[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_smaa[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_writes_weapon[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_WRITE },
};

static const vkSpineResourceEdge s_writes_hud[] = {
	{ VK_SPINE_RES_SWAPCHAIN_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_reads_present[] = {
	{ VK_SPINE_RES_HDR_COLOR, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_present[] = {
	{ VK_SPINE_RES_SWAPCHAIN_COLOR, VK_SPINE_ACCESS_COLOR_WRITE },
};

static const vkSpineResourceEdge s_writes_light_pack[] = {
	{ VK_SPINE_RES_FORWARD_PLUS_LIGHTS, VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_TRANSFER_WRITE },
};
static const vkSpineResourceEdge s_reads_tile[] = {
	{ VK_SPINE_RES_FORWARD_PLUS_LIGHTS, VK_SPINE_ACCESS_STORAGE_READ },
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
};
static const vkSpineResourceEdge s_writes_tile[] = {
	{ VK_SPINE_RES_FORWARD_PLUS_LIGHTS, VK_SPINE_ACCESS_STORAGE_WRITE },
};

static const vkSpineResourceEdge s_writes_sun_shadow[] = {
	{ VK_SPINE_RES_SHADOW_SUN, VK_SPINE_ACCESS_DEPTH_WRITE },
};
static const vkSpineResourceEdge s_reads_froxel[] = {
	{ VK_SPINE_RES_DEPTH, VK_SPINE_ACCESS_DEPTH_READ },
	{ VK_SPINE_RES_SHADOW_SUN, VK_SPINE_ACCESS_SAMPLED_READ },
};
static const vkSpineResourceEdge s_writes_froxel[] = {
	{ VK_SPINE_RES_FROXEL_SCATTER, VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_HISTORY_WRITE },
};

#define VK_SPINE_VIEW_MAIN ( 1u << VK_VIEW_CLASS_MAIN_WORLD )
#define VK_SPINE_VIEW_WEAPON ( 1u << VK_VIEW_CLASS_WEAPON )
#define VK_SPINE_VIEW_UI ( 1u << VK_VIEW_CLASS_UI )
#define VK_SPINE_VIEW_ANY ( ~0u )

static const vkSpinePassDesc s_passes[VK_SPINE_PASS_COUNT] = {
	[VK_SPINE_PASS_NONE] = { VK_SPINE_PASS_NONE, "none", VK_SPINE_CAT_MAINTENANCE, VK_SPINE_PHASE_FRAME_BEGIN, 0, qfalse, qfalse, NULL, 0, NULL, 0 },
	[VK_SPINE_PASS_FRAME_PREP] = {
		VK_SPINE_PASS_FRAME_PREP, "frame_prep", VK_SPINE_CAT_SCENE_PREP, VK_SPINE_PHASE_FRAME_BEGIN,
		VK_SPINE_VIEW_ANY, qfalse, qfalse, NULL, 0, NULL, 0
	},
	[VK_SPINE_PASS_LIGHT_PACK] = {
		VK_SPINE_PASS_LIGHT_PACK, "light_pack", VK_SPINE_CAT_SCENE_PREP, VK_SPINE_PHASE_SCENE_PREP,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse, NULL, 0,
		s_writes_light_pack, (int)ARRAY_LEN( s_writes_light_pack )
	},
	[VK_SPINE_PASS_TILE_CONSTRUCT] = {
		VK_SPINE_PASS_TILE_CONSTRUCT, "tile_construct", VK_SPINE_CAT_FORWARD_PLUS, VK_SPINE_PHASE_SCENE_PREP,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_tile, (int)ARRAY_LEN( s_reads_tile ),
		s_writes_tile, (int)ARRAY_LEN( s_writes_tile )
	},
	[VK_SPINE_PASS_SUN_SHADOW] = {
		VK_SPINE_PASS_SUN_SHADOW, "sun_shadow", VK_SPINE_CAT_DEPTH, VK_SPINE_PHASE_WORLD_DEPTH,
		VK_SPINE_VIEW_MAIN, qtrue, qfalse,
		NULL, 0,
		s_writes_sun_shadow, (int)ARRAY_LEN( s_writes_sun_shadow )
	},
	[VK_SPINE_PASS_WORLD_OPAQUE] = {
		VK_SPINE_PASS_WORLD_OPAQUE, "world_opaque", VK_SPINE_CAT_OPAQUE_RASTER, VK_SPINE_PHASE_WORLD_OPAQUE,
		VK_SPINE_VIEW_MAIN | ( 1u << VK_VIEW_CLASS_PORTAL ) | ( 1u << VK_VIEW_CLASS_MIRROR ),
		qtrue, qfalse,
		s_reads_world_opaque, (int)ARRAY_LEN( s_reads_world_opaque ),
		s_writes_world_opaque, (int)ARRAY_LEN( s_writes_world_opaque )
	},
	[VK_SPINE_PASS_GBUFFER_FILL] = {
		VK_SPINE_PASS_GBUFFER_FILL, "gbuffer_fill", VK_SPINE_CAT_DEFERRED, VK_SPINE_PHASE_WORLD_OPAQUE,
		VK_SPINE_VIEW_MAIN, qtrue, qfalse,
		s_reads_gbuffer, (int)ARRAY_LEN( s_reads_gbuffer ),
		s_writes_gbuffer, (int)ARRAY_LEN( s_writes_gbuffer )
	},
	[VK_SPINE_PASS_DEFERRED_LIGHTING] = {
		VK_SPINE_PASS_DEFERRED_LIGHTING, "deferred_lighting", VK_SPINE_CAT_DEFERRED, VK_SPINE_PHASE_OPAQUE_LIGHTING,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_deferred_light, (int)ARRAY_LEN( s_reads_deferred_light ),
		s_writes_deferred_light, (int)ARRAY_LEN( s_writes_deferred_light )
	},
	[VK_SPINE_PASS_FORWARD_PLUS_OPAQUE] = {
		VK_SPINE_PASS_FORWARD_PLUS_OPAQUE, "forward_plus_opaque", VK_SPINE_CAT_FORWARD_PLUS, VK_SPINE_PHASE_OPAQUE_LIGHTING,
		VK_SPINE_VIEW_MAIN, qtrue, qfalse,
		s_reads_world_opaque, (int)ARRAY_LEN( s_reads_world_opaque ),
		s_writes_world_opaque, (int)ARRAY_LEN( s_writes_world_opaque )
	},
	[VK_SPINE_PASS_SSR] = {
		VK_SPINE_PASS_SSR, "ssr", VK_SPINE_CAT_POST, VK_SPINE_PHASE_SCREEN_SPACE,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_ssr, (int)ARRAY_LEN( s_reads_ssr ),
		s_writes_ssr, (int)ARRAY_LEN( s_writes_ssr )
	},
	[VK_SPINE_PASS_AMBIENT_VISIBILITY] = {
		VK_SPINE_PASS_AMBIENT_VISIBILITY, "ambient_visibility", VK_SPINE_CAT_POST, VK_SPINE_PHASE_SCREEN_SPACE,
		VK_SPINE_VIEW_MAIN, qfalse, qtrue,
		s_reads_av, (int)ARRAY_LEN( s_reads_av ),
		s_writes_av, (int)ARRAY_LEN( s_writes_av )
	},
	[VK_SPINE_PASS_RASTER_GI] = {
		VK_SPINE_PASS_RASTER_GI, "raster_gi", VK_SPINE_CAT_POST, VK_SPINE_PHASE_SCREEN_SPACE,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_raster_gi, (int)ARRAY_LEN( s_reads_raster_gi ),
		s_writes_raster_gi, (int)ARRAY_LEN( s_writes_raster_gi )
	},
	[VK_SPINE_PASS_FROXEL_VOLUME] = {
		VK_SPINE_PASS_FROXEL_VOLUME, "froxel_volume", VK_SPINE_CAT_POST, VK_SPINE_PHASE_SCREEN_SPACE,
		VK_SPINE_VIEW_MAIN, qtrue, qtrue,
		s_reads_froxel, (int)ARRAY_LEN( s_reads_froxel ),
		s_writes_froxel, (int)ARRAY_LEN( s_writes_froxel )
	},
	[VK_SPINE_PASS_TRANSPARENT_FORWARD_PLUS] = {
		VK_SPINE_PASS_TRANSPARENT_FORWARD_PLUS, "transparent_forward_plus", VK_SPINE_CAT_TRANSPARENCY,
		VK_SPINE_PHASE_WORLD_TRANSPARENCY, VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_oit_accum, (int)ARRAY_LEN( s_reads_oit_accum ),
		s_writes_world_opaque, (int)ARRAY_LEN( s_writes_world_opaque )
	},
	[VK_SPINE_PASS_WBOIT_ACCUM] = {
		VK_SPINE_PASS_WBOIT_ACCUM, "wboit_accum", VK_SPINE_CAT_OIT, VK_SPINE_PHASE_WORLD_TRANSPARENCY,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_oit_accum, (int)ARRAY_LEN( s_reads_oit_accum ),
		s_writes_wboit, (int)ARRAY_LEN( s_writes_wboit )
	},
	[VK_SPINE_PASS_MBOIT_MOMENTS] = {
		VK_SPINE_PASS_MBOIT_MOMENTS, "mboit_moments", VK_SPINE_CAT_OIT, VK_SPINE_PHASE_WORLD_TRANSPARENCY,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_oit_accum, (int)ARRAY_LEN( s_reads_oit_accum ),
		s_writes_mboit_moments, (int)ARRAY_LEN( s_writes_mboit_moments )
	},
	[VK_SPINE_PASS_MBOIT_ACCUM] = {
		VK_SPINE_PASS_MBOIT_ACCUM, "mboit_accum", VK_SPINE_CAT_OIT, VK_SPINE_PHASE_WORLD_TRANSPARENCY,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_oit_accum, (int)ARRAY_LEN( s_reads_oit_accum ),
		s_writes_mboit_accum, (int)ARRAY_LEN( s_writes_mboit_accum )
	},
	[VK_SPINE_PASS_OIT_RESOLVE] = {
		VK_SPINE_PASS_OIT_RESOLVE, "oit_resolve", VK_SPINE_CAT_OIT, VK_SPINE_PHASE_TRANSPARENCY_RESOLVE,
		VK_SPINE_VIEW_MAIN, qfalse, qfalse,
		s_reads_oit_resolve, (int)ARRAY_LEN( s_reads_oit_resolve ),
		s_writes_oit_resolve, (int)ARRAY_LEN( s_writes_oit_resolve )
	},
	[VK_SPINE_PASS_TEMPORAL_RECON] = {
		VK_SPINE_PASS_TEMPORAL_RECON, "temporal_recon", VK_SPINE_CAT_TEMPORAL, VK_SPINE_PHASE_TEMPORAL,
		VK_SPINE_VIEW_MAIN, qtrue, qtrue,
		s_reads_taa, (int)ARRAY_LEN( s_reads_taa ),
		s_writes_taa, (int)ARRAY_LEN( s_writes_taa )
	},
	[VK_SPINE_PASS_REACTIVE_MASK] = {
		VK_SPINE_PASS_REACTIVE_MASK, "reactive_mask", VK_SPINE_CAT_TEMPORAL, VK_SPINE_PHASE_TEMPORAL,
		VK_SPINE_VIEW_MAIN, qtrue, qfalse,
		s_reads_reactive, (int)ARRAY_LEN( s_reads_reactive ),
		s_writes_reactive, (int)ARRAY_LEN( s_writes_reactive )
	},
	[VK_SPINE_PASS_SMAA] = {
		VK_SPINE_PASS_SMAA, "smaa", VK_SPINE_CAT_POST, VK_SPINE_PHASE_POST,
		VK_SPINE_VIEW_ANY, qtrue, qfalse,
		s_reads_smaa, (int)ARRAY_LEN( s_reads_smaa ),
		s_writes_smaa, (int)ARRAY_LEN( s_writes_smaa )
	},
	[VK_SPINE_PASS_BLOOM] = {
		VK_SPINE_PASS_BLOOM, "bloom", VK_SPINE_CAT_POST, VK_SPINE_PHASE_POST,
		VK_SPINE_VIEW_ANY, qtrue, qfalse,
		s_reads_bloom, (int)ARRAY_LEN( s_reads_bloom ),
		s_writes_bloom, (int)ARRAY_LEN( s_writes_bloom )
	},
	[VK_SPINE_PASS_EYE_ADAPTATION] = {
		VK_SPINE_PASS_EYE_ADAPTATION, "eye_adaptation", VK_SPINE_CAT_POST, VK_SPINE_PHASE_POST,
		VK_SPINE_VIEW_MAIN, qtrue, qtrue,
		s_reads_exposure, (int)ARRAY_LEN( s_reads_exposure ),
		s_writes_exposure, (int)ARRAY_LEN( s_writes_exposure )
	},
	[VK_SPINE_PASS_WEAPON] = {
		VK_SPINE_PASS_WEAPON, "weapon", VK_SPINE_CAT_WEAPON, VK_SPINE_PHASE_WEAPON,
		VK_SPINE_VIEW_WEAPON | ( 1u << VK_VIEW_CLASS_NO_WORLD ), qtrue, qfalse,
		NULL, 0, s_writes_weapon, (int)ARRAY_LEN( s_writes_weapon )
	},
	[VK_SPINE_PASS_HUD_2D] = {
		VK_SPINE_PASS_HUD_2D, "hud_2d", VK_SPINE_CAT_UI, VK_SPINE_PHASE_UI,
		VK_SPINE_VIEW_UI | VK_SPINE_VIEW_ANY, qtrue, qfalse,
		NULL, 0, s_writes_hud, (int)ARRAY_LEN( s_writes_hud )
	},
	[VK_SPINE_PASS_PRESENTATION] = {
		VK_SPINE_PASS_PRESENTATION, "presentation", VK_SPINE_CAT_PRESENTATION, VK_SPINE_PHASE_PRESENT,
		VK_SPINE_VIEW_ANY, qtrue, qfalse,
		s_reads_present, (int)ARRAY_LEN( s_reads_present ),
		s_writes_present, (int)ARRAY_LEN( s_writes_present )
	},
	[VK_SPINE_PASS_HISTORY_MAINT] = {
		VK_SPINE_PASS_HISTORY_MAINT, "history_maint", VK_SPINE_CAT_MAINTENANCE, VK_SPINE_PHASE_FRAME_END,
		VK_SPINE_VIEW_ANY, qtrue, qtrue, NULL, 0, NULL, 0
	},
};

/* ---- Helpers ---- */

static void vk_spine_record_violation( const char *fmt, ... )
{
	va_list ap;
	char buf[128];

	va_start( ap, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );

	s_spine.violationCount++;
	Q_strncpyz( s_spine.lastViolation, buf, sizeof( s_spine.lastViolation ) );

	if ( !vk_spine_validate_enabled() ) {
		return;
	}
	if ( r_spineValidate && r_spineValidate->integer >= 2 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "[VK][spine] %s\n", buf );
	} else {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][spine] %s\n", buf );
	}
}

qboolean vk_spine_validate_enabled( void )
{
	if ( r_spineCert && r_spineCert->integer ) {
		return qtrue;
	}
	return ( r_spineValidate && r_spineValidate->integer > 0 ) ? qtrue : qfalse;
}

qboolean vk_spine_is_spine_1_1_combo( void )
{
	const int oit = r_oit ? r_oit->integer : 0;
	const int taa = r_taa ? r_taa->integer : 0;
	const int weaponAfter = r_temporalWeaponAfterTaa ? r_temporalWeaponAfterTaa->integer : 0;
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	const int aaMode = r_aaMode ? r_aaMode->integer : 0;
	const int cleanup = r_temporalSmaaCleanup ? r_temporalSmaaCleanup->integer : 0;

	/* WBOIT × Temporal Reconstruction × weapon-after on Unified Clustered. */
	return ( oit == 1 && taa && weaponAfter && mode == 3 && aaMode == 4 && !cleanup ) ? qtrue : qfalse;
}

qboolean vk_spine_cert_active( void )
{
	if ( !vk_spine_is_spine_1_1_combo() ) {
		return qfalse;
	}
	if ( r_spineCert && r_spineCert->integer ) {
		return qtrue;
	}
	/* Full cert matrix under validate still runs ownership asserts. */
	return vk_spine_validate_enabled();
}

uint32_t vk_spine_attachment_generation( void )
{
	return s_spine.attachmentGeneration;
}

uint32_t vk_spine_descriptor_generation( void )
{
	return s_spine.descriptorGeneration;
}

uint32_t vk_spine_violation_count( void )
{
	return s_spine.violationCount;
}

void vk_spine_reset_cert_counters( void )
{
	s_spine.violationCount = 0u;
	s_spine.lastViolation[0] = '\0';
	s_spine.blackStreak = 0u;
	s_spine.certFrameIndex = 0u;
	s_spine.liveBaselineArmed = qfalse;
	s_spine.liveResourceBaseline = 0u;
}

static uint32_t vk_spine_count_alive_resources( void )
{
	uint32_t n = 0u;
	vkSpineResourceId r;

	for ( r = (vkSpineResourceId)1; r < VK_SPINE_RES_COUNT; r++ ) {
		if ( s_spine.resources[r].alive ) {
			n++;
		}
	}
	return n;
}

static qboolean vk_spine_is_oit_resource( vkSpineResourceId res )
{
	return ( res == VK_SPINE_RES_OIT_ACCUM || res == VK_SPINE_RES_OIT_REVEAL ||
		res == VK_SPINE_RES_OIT_MOMENTS || res == VK_SPINE_RES_OIT_B0 ) ? qtrue : qfalse;
}

const char *vk_spine_phase_name( vkSpinePhase phase )
{
	switch ( phase ) {
	case VK_SPINE_PHASE_FRAME_BEGIN: return "frame_begin";
	case VK_SPINE_PHASE_SCENE_PREP: return "scene_prep";
	case VK_SPINE_PHASE_WORLD_DEPTH: return "world_depth";
	case VK_SPINE_PHASE_WORLD_OPAQUE: return "world_opaque";
	case VK_SPINE_PHASE_OPAQUE_LIGHTING: return "opaque_lighting";
	case VK_SPINE_PHASE_SCREEN_SPACE: return "screen_space";
	case VK_SPINE_PHASE_WORLD_TRANSPARENCY: return "world_transparency";
	case VK_SPINE_PHASE_TRANSPARENCY_RESOLVE: return "transparency_resolve";
	case VK_SPINE_PHASE_TEMPORAL: return "temporal";
	case VK_SPINE_PHASE_POST: return "post";
	case VK_SPINE_PHASE_WEAPON: return "weapon";
	case VK_SPINE_PHASE_UI: return "ui";
	case VK_SPINE_PHASE_PRESENT: return "present";
	case VK_SPINE_PHASE_FRAME_END: return "frame_end";
	default: return "unknown_phase";
	}
}

const char *vk_spine_pass_name( vkSpinePassId pass )
{
	if ( pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return "none";
	}
	return s_passes[pass].name ? s_passes[pass].name : "unnamed";
}

const char *vk_spine_resource_name( vkSpineResourceId res )
{
	switch ( res ) {
	case VK_SPINE_RES_SWAPCHAIN_COLOR: return "swapchain_color";
	case VK_SPINE_RES_DEPTH: return "depth";
	case VK_SPINE_RES_HDR_COLOR: return "hdr_color";
	case VK_SPINE_RES_MOTION_VECTORS: return "motion_vectors";
	case VK_SPINE_RES_GBUFFER_ALBEDO: return "gbuffer_albedo";
	case VK_SPINE_RES_GBUFFER_NORMAL: return "gbuffer_normal";
	case VK_SPINE_RES_GBUFFER_MATERIAL: return "gbuffer_material";
	case VK_SPINE_RES_DEFERRED_LIGHTING: return "deferred_lighting";
	case VK_SPINE_RES_SSAO: return "ssao";
	case VK_SPINE_RES_AV_HISTORY: return "av_history";
	case VK_SPINE_RES_AV_FILTERED: return "av_filtered";
	case VK_SPINE_RES_SSR: return "ssr";
	case VK_SPINE_RES_OIT_ACCUM: return "oit_accum";
	case VK_SPINE_RES_OIT_REVEAL: return "oit_reveal";
	case VK_SPINE_RES_OIT_MOMENTS: return "oit_moments";
	case VK_SPINE_RES_OIT_B0: return "oit_b0";
	case VK_SPINE_RES_TAA_HISTORY: return "taa_history";
	case VK_SPINE_RES_REACTIVE_MASK: return "reactive_mask";
	case VK_SPINE_RES_BLOOM_CHAIN: return "bloom_chain";
	case VK_SPINE_RES_EXPOSURE_LUMINANCE: return "exposure_luminance";
	case VK_SPINE_RES_SHADOW_SUN: return "shadow_sun";
	case VK_SPINE_RES_FROXEL_SCATTER: return "froxel_scatter";
	case VK_SPINE_RES_FORWARD_PLUS_LIGHTS: return "forward_plus_lights";
	case VK_SPINE_RES_PROBE_GRID: return "probe_grid";
	case VK_SPINE_RES_PROBE_IRRADIANCE: return "probe_irradiance";
	case VK_SPINE_RES_SSGI_RADIANCE: return "ssgi_radiance";
	case VK_SPINE_RES_SSGI_CONFIDENCE: return "ssgi_confidence";
	case VK_SPINE_RES_RADIANCE_CLIPMAP: return "radiance_clipmap";
	case VK_SPINE_RES_RADIANCE_CACHE_IRRADIANCE: return "radiance_cache_irradiance";
	case VK_SPINE_RES_INDIRECT_DIFFUSE: return "indirect_diffuse";
	default: return "none";
	}
}

vkSpinePassId vk_spine_last_writer( vkSpineResourceId res )
{
	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return VK_SPINE_PASS_NONE;
	}
	return s_spine.resources[res].lastWriter;
}

static vkSpinePassId vk_spine_lookup_named( const char *name )
{
	if ( !name || !name[0] ) {
		return VK_SPINE_PASS_NONE;
	}
	/* Scene-pass / pass_diag sticky names */
	if ( !Q_stricmp( name, "main" ) ) {
		return VK_SPINE_PASS_WORLD_OPAQUE;
	}
	if ( !Q_stricmp( name, "sun_shadow" ) ) {
		return VK_SPINE_PASS_SUN_SHADOW;
	}
	if ( !Q_stricmp( name, "post_bloom" ) ) {
		return VK_SPINE_PASS_BLOOM;
	}
	if ( !Q_stricmp( name, "ui_overlay" ) ) {
		return VK_SPINE_PASS_HUD_2D;
	}
	{
		vkSpinePassId i;
		for ( i = (vkSpinePassId)1; i < VK_SPINE_PASS_COUNT; i++ ) {
			if ( s_passes[i].name && !Q_stricmp( name, s_passes[i].name ) ) {
				return i;
			}
		}
	}
	return VK_SPINE_PASS_NONE;
}

static void vk_spine_mark_observed( vkSpinePassId pass )
{
	uint32_t word;
	uint32_t bit;

	if ( pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return;
	}
	word = (uint32_t)pass / 32u;
	bit = 1u << ( (uint32_t)pass % 32u );
	s_spine.framePassMask[word] |= bit;
}

static qboolean vk_spine_was_observed( vkSpinePassId pass )
{
	uint32_t word;
	uint32_t bit;

	if ( pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return qfalse;
	}
	word = (uint32_t)pass / 32u;
	bit = 1u << ( (uint32_t)pass % 32u );
	return ( s_spine.framePassMask[word] & bit ) != 0u ? qtrue : qfalse;
}

static void vk_spine_apply_declared_edges( vkSpinePassId pass, qboolean asWrites )
{
	const vkSpinePassDesc *desc;
	const vkSpineResourceEdge *edges;
	int count;
	int i;

	if ( pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return;
	}
	desc = &s_passes[pass];
	edges = asWrites ? desc->writes : desc->reads;
	count = asWrites ? desc->writeCount : desc->readCount;
	for ( i = 0; i < count; i++ ) {
		if ( asWrites ) {
			vk_spine_note_write( edges[i].resource, pass, edges[i].access );
		} else {
			vk_spine_note_read( edges[i].resource, pass, edges[i].access );
		}
	}
}

static void vk_spine_set_resource_alive( vkSpineResourceId res, qboolean alive, uint32_t w, uint32_t h, VkFormat fmt )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->alive = alive;
	if ( alive ) {
		rt->generation = s_spine.attachmentGeneration;
		rt->width = w;
		rt->height = h;
		rt->format = fmt;
	} else {
		rt->generation = 0u;
		rt->width = 0u;
		rt->height = 0u;
		rt->format = VK_FORMAT_UNDEFINED;
		rt->historyValid = qfalse;
		rt->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		rt->layoutKnown = qfalse;
	}
}

/* ---- Public lifecycle ---- */

void vk_spine_registry_init( void )
{
	Com_Memset( &s_spine, 0, sizeof( s_spine ) );
	s_spine.initialized = qtrue;
	s_spine.attachmentGeneration = 1u;
	s_spine.descriptorGeneration = 1u;
	s_spine.currentPhase = VK_SPINE_PHASE_FRAME_BEGIN;

	r_spineValidate = ri.Cvar_Get( "r_spineValidate", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spineValidate, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_spineValidate,
		"Spine pass/resource registry validation:\n"
		" 0 - ownership stamps only (cheap)\n"
		" 1 - validate phase order / stale reads (developer)\n"
		" 2 - same, PRINT_ALL on violations" );
	r_spineCert = ri.Cvar_Get( "r_spineCert", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spineCert, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_spineCert,
		"Spine 1.1 certification asserts (black-frame / descriptor gen / OIT×TAA ownership). "
		"Use with exec vulkan_overlay_spine_1_1_cert.cfg." );
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "pass_registry_status", vk_spine_status_f );
		ri.Cmd_AddCommand( "spine_status", vk_spine_status_f );
	}
	/* Attachments may already exist (init order: create_attachments before registry). */
	if ( vk.color_image != VK_NULL_HANDLE ) {
		uint32_t aw = 0, ah = 0;
		vk_get_active_render_extent( &aw, &ah );
		/* Stamp at gen 1 without double-bump: attachments_created increments. */
		s_spine.attachmentGeneration = 0u;
		vk_spine_attachments_created( aw, ah );
		/* vk_init_descriptors may run after us and will clear pending rebound. */
	}
	ri.Printf( PRINT_ALL,
		"[VK][spine] pass/resource registry ready (r_spineValidate=%d r_spineCert=%d; "
		"Spine 1.1 WBOIT×TAA×weapon cert + DEVICE_LOST late-post context)\n",
		r_spineValidate ? r_spineValidate->integer : 0,
		r_spineCert ? r_spineCert->integer : 0 );
}

void vk_spine_registry_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "pass_registry_status" );
		ri.Cmd_RemoveCommand( "spine_status" );
	}
	Com_Memset( &s_spine, 0, sizeof( s_spine ) );
}

void vk_spine_frame_begin( void )
{
	int i;

	if ( !s_spine.initialized ) {
		return;
	}
	s_spine.currentPhase = VK_SPINE_PHASE_FRAME_BEGIN;
	s_spine.highestPhase = VK_SPINE_PHASE_FRAME_BEGIN;
	s_spine.openCount = 0;
	s_spine.lastBegun = VK_SPINE_PASS_NONE;
	s_spine.lastEnded = VK_SPINE_PASS_NONE;
	s_spine.oitResolvedThisFrame = qfalse;
	s_spine.oitSkippedThisFrame = qfalse;
	s_spine.certFrameIndex++;
	Com_Memset( s_spine.framePassMask, 0, sizeof( s_spine.framePassMask ) );
	for ( i = 0; i < VK_SPINE_RES_COUNT; i++ ) {
		/* Keep alive/generation/extent across frames; clear per-frame edge stamps. */
		s_spine.resources[i].clearedThisFrame = qfalse;
		s_spine.resources[i].barrierThisFrame = qfalse;
		/* OIT targets are single-frame — never carry history validity. */
		if ( vk_spine_is_oit_resource( (vkSpineResourceId)i ) ) {
			s_spine.resources[i].historyValid = qfalse;
		}
		if ( vk_spine_validate_enabled() ) {
			s_spine.resources[i].lastReader = VK_SPINE_PASS_NONE;
			s_spine.resources[i].lastReadAccess = 0u;
		}
	}
	if ( vk_spine_validate_enabled() ) {
		if ( s_spine.descriptorsPendingRebound ) {
			vk_spine_record_violation(
				"attachments recreated without descriptor rebound (attGen=%u descGen=%u)",
				s_spine.attachmentGeneration, s_spine.descriptorGeneration );
		} else if ( s_spine.descriptorGeneration != s_spine.attachmentGeneration ) {
			vk_spine_record_violation(
				"stale descriptor generation %u (attachment %u)",
				s_spine.descriptorGeneration, s_spine.attachmentGeneration );
		}
	}
	vk_spine_pass_begin( VK_SPINE_PASS_FRAME_PREP );
	vk_spine_pass_end( VK_SPINE_PASS_FRAME_PREP );
	/* Sync tracked depth layout from the authoritative engine stamp. */
	if ( vk.depth_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_DEPTH, vk.depth_image_layout );
	}
	if ( vk.reactive_mask_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_REACTIVE_MASK, vk.reactive_mask_layout );
	}
	/* Keep registry history flags aligned with temporal ownership before TAA/AV consumers. */
	vk_spine_note_temporal_history( VK_SPINE_RES_TAA_HISTORY, vk.temporal.hasValidTAAHistory );
	vk_spine_validate_feature_combos();
}

void vk_spine_frame_end( void )
{
	if ( !s_spine.initialized ) {
		return;
	}
	if ( s_spine.openCount > 0 && vk_spine_validate_enabled() ) {
		vk_spine_record_violation( "frame_end with %d open pass(es); top=%s",
			s_spine.openCount, vk_spine_pass_name( s_spine.openStack[s_spine.openCount - 1] ) );
	}
	vk_spine_pass_begin( VK_SPINE_PASS_HISTORY_MAINT );
	/* Sync temporal history validity into registry. */
	vk_spine_note_temporal_history( VK_SPINE_RES_TAA_HISTORY,
		vk.temporal.hasValidTAAHistory );
	vk_spine_pass_end( VK_SPINE_PASS_HISTORY_MAINT );
	vk_spine_cert_check_black_frame();
	vk_spine_cert_check_resource_growth();
	s_spine.currentPhase = VK_SPINE_PHASE_FRAME_END;
	s_spine.openCount = 0;
}

void vk_spine_pass_begin( vkSpinePassId pass )
{
	const vkSpinePassDesc *desc;
	vkViewClass_t cls;

	if ( !s_spine.initialized || pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return;
	}
	desc = &s_passes[pass];
	s_spine.lastBegun = pass;
	vk_spine_mark_observed( pass );

	if ( s_spine.openCount < VK_SPINE_MAX_OPEN ) {
		s_spine.openStack[s_spine.openCount++] = pass;
	} else if ( vk_spine_validate_enabled() ) {
		vk_spine_record_violation( "open-pass stack overflow at %s", desc->name );
	}

	if ( vk_spine_validate_enabled() ) {
		if ( !desc->allowPhaseRegression && desc->phase < s_spine.highestPhase ) {
			vk_spine_record_violation( "phase regression: %s (%s) after highest %s",
				desc->name, vk_spine_phase_name( desc->phase ),
				vk_spine_phase_name( s_spine.highestPhase ) );
		}
		cls = vk_classify_current_view();
		if ( desc->viewClassMask != VK_SPINE_VIEW_ANY &&
			( desc->viewClassMask & ( 1u << (uint32_t)cls ) ) == 0u ) {
			vk_spine_record_violation( "pass %s not allowed for viewClass=%s",
				desc->name, vk_view_class_name( cls ) );
		}
	}

	s_spine.currentPhase = desc->phase;
	if ( desc->phase > s_spine.highestPhase ) {
		s_spine.highestPhase = desc->phase;
	}

	vk_spine_apply_declared_edges( pass, qfalse );
}

void vk_spine_pass_end( vkSpinePassId pass )
{
	if ( !s_spine.initialized || pass <= VK_SPINE_PASS_NONE || pass >= VK_SPINE_PASS_COUNT ) {
		return;
	}
	s_spine.lastEnded = pass;
	vk_spine_apply_declared_edges( pass, qtrue );

	if ( s_spine.openCount > 0 ) {
		if ( s_spine.openStack[s_spine.openCount - 1] == pass ) {
			s_spine.openCount--;
		} else if ( vk_spine_validate_enabled() ) {
			vk_spine_record_violation( "pass_end mismatch: expected %s got %s",
				vk_spine_pass_name( s_spine.openStack[s_spine.openCount - 1] ),
				vk_spine_pass_name( pass ) );
			/* Pop until match or empty to avoid sticky poison. */
			while ( s_spine.openCount > 0 && s_spine.openStack[s_spine.openCount - 1] != pass ) {
				s_spine.openCount--;
			}
			if ( s_spine.openCount > 0 ) {
				s_spine.openCount--;
			}
		}
	}
}

void vk_spine_pass_begin_named( const char *name, uint32_t width, uint32_t height )
{
	vkSpinePassId id = vk_spine_lookup_named( name );
	(void)width;
	(void)height;
	if ( id != VK_SPINE_PASS_NONE ) {
		vk_spine_pass_begin( id );
	}
}

void vk_spine_pass_end_named( const char *name )
{
	vkSpinePassId id = vk_spine_lookup_named( name );
	if ( id != VK_SPINE_PASS_NONE ) {
		vk_spine_pass_end( id );
	}
}

void vk_spine_note_write( vkSpineResourceId res, vkSpinePassId pass, uint32_t access )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->lastWriter = pass;
	rt->lastWriteAccess = access;
	if ( vk_spine_is_oit_resource( res ) &&
		( access & ( VK_SPINE_ACCESS_HISTORY_WRITE | VK_SPINE_ACCESS_HISTORY_READ ) ) != 0u ) {
		if ( vk_spine_validate_enabled() ) {
			vk_spine_record_violation(
				"OIT resource %s must remain single-frame (history write by %s forbidden)",
				vk_spine_resource_name( res ), vk_spine_pass_name( pass ) );
		}
		rt->historyValid = qfalse;
	} else if ( ( access & VK_SPINE_ACCESS_HISTORY_WRITE ) != 0u ) {
		rt->historyValid = qtrue;
		rt->ownerViewClass = vk_classify_current_view();
	}
	/* Weapon must never own TAA history (ordering ownership, not dual buffers). */
	if ( pass == VK_SPINE_PASS_WEAPON && res == VK_SPINE_RES_TAA_HISTORY &&
		vk_spine_validate_enabled() ) {
		vk_spine_record_violation( "weapon pass wrote TAA history (weapon must stay presentation-only)" );
	}
	if ( pass == VK_SPINE_PASS_OIT_RESOLVE && res == VK_SPINE_RES_HDR_COLOR ) {
		s_spine.oitResolvedThisFrame = qtrue;
	}
	if ( vk_spine_validate_enabled() && rt->alive &&
		rt->generation != 0u && rt->generation != s_spine.attachmentGeneration ) {
		vk_spine_record_violation( "write %s by %s: stale generation %u (current %u)",
			vk_spine_resource_name( res ), vk_spine_pass_name( pass ),
			rt->generation, s_spine.attachmentGeneration );
	}
}

void vk_spine_note_read( vkSpineResourceId res, vkSpinePassId pass, uint32_t access )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->lastReader = pass;
	rt->lastReadAccess = access;

	if ( !vk_spine_validate_enabled() ) {
		return;
	}
	if ( vk_spine_is_oit_resource( res ) &&
		( access & ( VK_SPINE_ACCESS_HISTORY_READ | VK_SPINE_ACCESS_HISTORY_WRITE ) ) != 0u ) {
		vk_spine_record_violation(
			"OIT resource %s treated as history by %s (single-frame only)",
			vk_spine_resource_name( res ), vk_spine_pass_name( pass ) );
	}
	/* Temporal Reconstruction must not sample raw OIT targets as current color. */
	if ( pass == VK_SPINE_PASS_TEMPORAL_RECON && vk_spine_is_oit_resource( res ) &&
		( access & ( VK_SPINE_ACCESS_SAMPLED_READ | VK_SPINE_ACCESS_HISTORY_READ ) ) != 0u ) {
		vk_spine_record_violation(
			"TAA sampled raw OIT %s (resolved HDR only)",
			vk_spine_resource_name( res ) );
	}
	if ( rt->alive && rt->generation != 0u && rt->generation != s_spine.attachmentGeneration ) {
		vk_spine_record_violation( "read %s by %s: stale generation %u (current %u)",
			vk_spine_resource_name( res ), vk_spine_pass_name( pass ),
			rt->generation, s_spine.attachmentGeneration );
	}
	if ( ( access & VK_SPINE_ACCESS_HISTORY_READ ) != 0u && !rt->historyValid ) {
		vk_spine_record_violation( "history read %s by %s while history invalid",
			vk_spine_resource_name( res ), vk_spine_pass_name( pass ) );
	}
	/* OIT resolve sampling requires accum/reveal cleared this frame when OIT ran. */
	if ( pass == VK_SPINE_PASS_OIT_RESOLVE &&
		( res == VK_SPINE_RES_OIT_ACCUM || res == VK_SPINE_RES_OIT_REVEAL ||
		  res == VK_SPINE_RES_OIT_MOMENTS || res == VK_SPINE_RES_OIT_B0 ) &&
		rt->alive && !rt->clearedThisFrame ) {
		vk_spine_record_violation( "OIT resolve read %s without clear this frame",
			vk_spine_resource_name( res ) );
	}
}

void vk_spine_note_clear( vkSpineResourceId res, vkSpinePassId pass )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->clearedThisFrame = qtrue;
	rt->lastClearPass = pass;
	/* A clear is also a write for ownership tracking. */
	rt->lastWriter = pass;
	rt->lastWriteAccess |= VK_SPINE_ACCESS_TRANSFER_WRITE | VK_SPINE_ACCESS_COLOR_WRITE;
	/* Common clear destinations. */
	if ( res == VK_SPINE_RES_REACTIVE_MASK ) {
		vk_spine_note_layout( res, VK_IMAGE_LAYOUT_GENERAL );
	} else if ( res == VK_SPINE_RES_SHADOW_SUN ) {
		vk_spine_note_layout( res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
	} else if ( res == VK_SPINE_RES_OIT_ACCUM || res == VK_SPINE_RES_OIT_REVEAL ||
		res == VK_SPINE_RES_OIT_MOMENTS || res == VK_SPINE_RES_OIT_B0 ) {
		vk_spine_note_layout( res, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	}
}

void vk_spine_note_barrier( vkSpineResourceId res, vkSpinePassId pass, const char *reason )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->barrierThisFrame = qtrue;
	rt->lastBarrierPass = pass;
	Q_strncpyz( rt->lastBarrierReason, reason ? reason : "barrier", sizeof( rt->lastBarrierReason ) );
	/* Infer common post-barrier layouts from reason tags used by producers. */
	if ( reason ) {
		if ( !Q_stricmp( reason, "shadow_to_sample" ) ) {
			vk_spine_note_layout( res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		} else if ( !Q_stricmpn( reason, "post-", 5 ) || !Q_stricmp( reason, "froxel_compute" ) ) {
			if ( res == VK_SPINE_RES_FROXEL_SCATTER ) {
				vk_spine_note_layout( res, VK_IMAGE_LAYOUT_GENERAL );
			} else {
				vk_spine_note_layout( res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
			}
		} else if ( !Q_stricmp( reason, "reactive_clear" ) ) {
			vk_spine_note_layout( res, VK_IMAGE_LAYOUT_GENERAL );
		}
	}
}

qboolean vk_spine_resource_cleared_this_frame( vkSpineResourceId res )
{
	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return qfalse;
	}
	return s_spine.resources[res].clearedThisFrame;
}

qboolean vk_spine_resource_barriered_this_frame( vkSpineResourceId res )
{
	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return qfalse;
	}
	return s_spine.resources[res].barrierThisFrame;
}

const char *vk_spine_layout_name( VkImageLayout layout )
{
	switch ( layout ) {
	case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
	case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT";
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_ATTACHMENT";
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_READ_ONLY";
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY";
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC";
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST";
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC";
	default: return "OTHER";
	}
}

void vk_spine_note_layout( vkSpineResourceId res, VkImageLayout layout )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	rt->layout = layout;
	rt->layoutKnown = qtrue;
}

void vk_spine_expect_layout( vkSpineResourceId res, VkImageLayout expected, vkSpinePassId pass, const char *where )
{
	vkSpineResourceRuntime *rt;

	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	rt = &s_spine.resources[res];
	if ( !vk_spine_validate_enabled() ) {
		return;
	}
	if ( !rt->layoutKnown ) {
		vk_spine_record_violation( "expect layout %s on %s by %s (%s): layout unknown",
			vk_spine_layout_name( expected ), vk_spine_resource_name( res ),
			vk_spine_pass_name( pass ), where ? where : "?" );
		return;
	}
	if ( rt->layout != expected ) {
		vk_spine_record_violation( "expect layout %s on %s by %s (%s): have %s",
			vk_spine_layout_name( expected ), vk_spine_resource_name( res ),
			vk_spine_pass_name( pass ), where ? where : "?",
			vk_spine_layout_name( rt->layout ) );
	}
}

VkImageLayout vk_spine_resource_layout( vkSpineResourceId res )
{
	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
	return s_spine.resources[res].layoutKnown ? s_spine.resources[res].layout : VK_IMAGE_LAYOUT_UNDEFINED;
}

void vk_spine_attachments_created( uint32_t width, uint32_t height )
{
	VkFormat colorFmt = vk.color_format;
	VkFormat depthFmt = vk.depth_format;

	if ( !s_spine.initialized ) {
		return;
	}
	s_spine.attachmentGeneration++;
	if ( s_spine.attachmentGeneration == 0u ) {
		s_spine.attachmentGeneration = 1u;
	}
	/* Recreated images invalidate every dependent descriptor until rebound. */
	s_spine.descriptorsPendingRebound = qtrue;
	s_spine.liveBaselineArmed = qfalse;

	vk_spine_set_resource_alive( VK_SPINE_RES_DEPTH, vk.depth_image != VK_NULL_HANDLE, width, height, depthFmt );
	vk_spine_set_resource_alive( VK_SPINE_RES_HDR_COLOR, vk.color_image != VK_NULL_HANDLE, width, height, colorFmt );
	vk_spine_set_resource_alive( VK_SPINE_RES_MOTION_VECTORS,
		vk.motion_vector_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_GBUFFER_ALBEDO,
		vk.deferred_gbuffer_albedo != VK_NULL_HANDLE, width, height, colorFmt );
	vk_spine_set_resource_alive( VK_SPINE_RES_GBUFFER_NORMAL,
		vk.deferred_gbuffer_normal != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16B16A16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_GBUFFER_MATERIAL,
		vk.deferred_gbuffer_material != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16B16A16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_DEFERRED_LIGHTING,
		vk.deferred_lighting_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16B16A16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_SSAO,
		vk.ssao_image != VK_NULL_HANDLE, width, height, vk.ssao_format );
	vk_spine_set_resource_alive( VK_SPINE_RES_SSR,
		vk.ssr_image != VK_NULL_HANDLE, width, height, colorFmt );
	vk_spine_set_resource_alive( VK_SPINE_RES_OIT_ACCUM,
		vk.oit_accum_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16B16A16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_OIT_REVEAL,
		vk.oit_reveal_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_OIT_MOMENTS,
		vk.oit_moments_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16G16B16A16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_OIT_B0,
		vk.oit_b0_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R16_SFLOAT );
	vk_spine_set_resource_alive( VK_SPINE_RES_TAA_HISTORY,
		vk.taa_history_image[0] != VK_NULL_HANDLE, width, height, colorFmt );
	vk_spine_set_resource_alive( VK_SPINE_RES_REACTIVE_MASK,
		vk.reactive_mask_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R8_UNORM );
	vk_spine_set_resource_alive( VK_SPINE_RES_BLOOM_CHAIN,
		vk.bloom_image[0] != VK_NULL_HANDLE, width, height, vk.bloom_format );
	vk_spine_set_resource_alive( VK_SPINE_RES_SHADOW_SUN,
		vk.sun_shadow_image != VK_NULL_HANDLE, 0u, 0u, VK_FORMAT_UNDEFINED );
	vk_spine_set_resource_alive( VK_SPINE_RES_FROXEL_SCATTER,
		vk.froxel_volume_image != VK_NULL_HANDLE, 0u, 0u, VK_FORMAT_UNDEFINED );
	vk_spine_set_resource_alive( VK_SPINE_RES_FORWARD_PLUS_LIGHTS,
		vk.forward_plus.buffer != VK_NULL_HANDLE, 0u, 0u, VK_FORMAT_UNDEFINED );

	/* Initial layout expectations after attachment create / layout init helpers. */
	if ( vk.depth_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_DEPTH, vk.depth_image_layout );
	}
	if ( vk.color_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_HDR_COLOR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.oit_accum_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_OIT_ACCUM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		vk_spine_note_layout( VK_SPINE_RES_OIT_REVEAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.oit_moments_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_OIT_MOMENTS, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		vk_spine_note_layout( VK_SPINE_RES_OIT_B0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.reactive_mask_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_REACTIVE_MASK, vk.reactive_mask_layout );
	}
	if ( vk.sun_shadow_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_SHADOW_SUN, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	}
	if ( vk.froxel_volume_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_FROXEL_SCATTER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.ssr_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_SSR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.bloom_image[0] != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_BLOOM_CHAIN, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( vk.motion_vector_image != VK_NULL_HANDLE ) {
		vk_spine_note_layout( VK_SPINE_RES_MOTION_VECTORS, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}

	/* Temporal histories invalidate across attachment recreate. */
	vk_spine_note_temporal_history( VK_SPINE_RES_TAA_HISTORY, qfalse );
	vk_spine_note_temporal_history( VK_SPINE_RES_AV_HISTORY, qfalse );

	s_spine.liveResourceCount = vk_spine_count_alive_resources();
}

void vk_spine_note_descriptors_rebound( void )
{
	if ( !s_spine.initialized ) {
		return;
	}
	s_spine.descriptorGeneration = s_spine.attachmentGeneration;
	s_spine.descriptorsPendingRebound = qfalse;
	s_spine.liveResourceCount = vk_spine_count_alive_resources();
}

void vk_spine_attachments_destroyed( void )
{
	int i;

	if ( !s_spine.initialized ) {
		return;
	}
	for ( i = 0; i < VK_SPINE_RES_COUNT; i++ ) {
		vk_spine_set_resource_alive( (vkSpineResourceId)i, qfalse, 0u, 0u, VK_FORMAT_UNDEFINED );
		s_spine.resources[i].lastWriter = VK_SPINE_PASS_NONE;
		s_spine.resources[i].lastReader = VK_SPINE_PASS_NONE;
	}
}

void vk_spine_note_temporal_history( vkSpineResourceId res, qboolean valid )
{
	if ( res <= VK_SPINE_RES_NONE || res >= VK_SPINE_RES_COUNT ) {
		return;
	}
	s_spine.resources[res].historyValid = valid;
	if ( !valid ) {
		/* Keep lastWriter for diagnostics. */
	}
}

void vk_spine_validate_feature_combos( void )
{
	qboolean avOn;
	qboolean ssaoOn;
	qboolean oitOn;
	qboolean taaOn;
	qboolean weaponAfter;
	static qboolean s_warnedDualAo;
	static qboolean s_warnedOitTaa;
	static qboolean s_warnedOitTaaWeapon;
	static qboolean s_warnedTaaWeapon;

	if ( !s_spine.initialized ) {
		return;
	}
	s_spine.comboFallback[0] = '\0';
	s_spine.suppressTaaThisFrame = qfalse;

	/* Dual owner: legacy SSAO and AV both requesting the AO signal. */
	avOn = ( ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ) >= 2 ) ? qtrue : qfalse;
	ssaoOn = ( r_ssao && r_ssao->integer ) ? qtrue : qfalse;
	if ( ssaoOn && avOn ) {
		Q_strncpyz( s_spine.comboFallback, "dual_ao_ssao_and_av", sizeof( s_spine.comboFallback ) );
		if ( !s_warnedDualAo ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW
				"[VK][spine] illegal combo: r_ssao + AV both on (one AO owner). "
				"Recovery: exec gfx_safe.cfg or seta r_ssao 0\n" );
			s_warnedDualAo = qtrue;
		}
		if ( vk_spine_validate_enabled() ) {
			vk_spine_record_violation(
				"feature combo: r_ssao and r_ambientVisibilityMode both on (one AO owner expected)" );
		}
	}

	oitOn = ( r_oit && r_oit->integer ) ? qtrue : qfalse;
	taaOn = ( r_taa && r_taa->integer ) ? qtrue : qfalse;
	weaponAfter = ( r_temporalWeaponAfterTaa && r_temporalWeaponAfterTaa->integer ) ? qtrue : qfalse;
	s_spine.spine11CertActive = vk_spine_is_spine_1_1_combo();
	if ( oitOn && taaOn ) {
		Q_strncpyz( s_spine.comboFallback, "oit_x_taa", sizeof( s_spine.comboFallback ) );
		if ( !weaponAfter ) {
			Q_strncpyz( s_spine.comboFallback, "oit_x_taa_without_weapon_after",
				sizeof( s_spine.comboFallback ) );
			/* Soft-demote world TAA this frame — weapon would poison history under OIT. */
			s_spine.suppressTaaThisFrame = qtrue;
			if ( !s_warnedOitTaaWeapon ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] illegal combo: OIT + TAA without r_temporalWeaponAfterTaa 1; "
					"suppressing world TAA this frame. Seta r_temporalWeaponAfterTaa 1 or disable OIT/TAA. "
					"Recovery: exec modern_vulkan_quality.cfg (OIT, no TAA) or vulkan_overlay_temporal_recon.cfg\n" );
				s_warnedOitTaaWeapon = qtrue;
			}
			if ( vk_spine_validate_enabled() ) {
				vk_spine_record_violation(
					"feature combo: OIT + TAA requires r_temporalWeaponAfterTaa 1" );
			}
		} else if ( s_spine.spine11CertActive ) {
			/* Spine 1.1 certified: mode3 + WBOIT + Temporal Reconstruction + weapon-after. */
			Q_strncpyz( s_spine.comboFallback, "spine_1_1_oit_taa_weapon",
				sizeof( s_spine.comboFallback ) );
			if ( !s_warnedOitTaa ) {
				ri.Printf( PRINT_ALL,
					"[VK][spine] Spine 1.1 certified combo active: mode3 + WBOIT + Temporal Reconstruction "
					"+ weapon-after-TAA (r_spineCert=%d)\n",
					r_spineCert ? r_spineCert->integer : 0 );
				s_warnedOitTaa = qtrue;
			}
			/* No violation — this is the certified ownership path. */
		} else if ( r_oit && r_oit->integer == 2 ) {
			/* MBOIT × TAA remains experimental until WBOIT cert soak passes. */
			Q_strncpyz( s_spine.comboFallback, "mboit_x_taa_experimental",
				sizeof( s_spine.comboFallback ) );
			if ( !s_warnedOitTaa ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] experimental combo: MBOIT + TAA (weapon-after on). "
					"Spine 1.1 cert targets WBOIT (r_oit 1) only; keep MBOIT off for cert.\n" );
				s_warnedOitTaa = qtrue;
			}
			if ( vk_spine_validate_enabled() && r_spineValidate && r_spineValidate->integer >= 2 ) {
				vk_spine_record_violation(
					"feature combo: MBOIT + TAA experimental (not Spine 1.1 certified)" );
			}
		} else {
			/* WBOIT+TAA+weapon but missing mode3/aaMode4 pins — experimental, no perpetual fail. */
			Q_strncpyz( s_spine.comboFallback, "oit_x_taa_experimental",
				sizeof( s_spine.comboFallback ) );
			if ( !s_warnedOitTaa ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] experimental combo: OIT + TAA (weapon-after on). "
					"For Spine 1.1 cert: exec vulkan_overlay_spine_1_1_cert.cfg\n" );
				s_warnedOitTaa = qtrue;
			}
		}
	} else if ( taaOn && !weaponAfter ) {
		Q_strncpyz( s_spine.comboFallback, "taa_without_weapon_after", sizeof( s_spine.comboFallback ) );
		if ( !s_warnedTaaWeapon ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW
				"[VK][spine] warning: TAA without r_temporalWeaponAfterTaa 1 (weapon can poison history)\n" );
			s_warnedTaaWeapon = qtrue;
		}
		if ( vk_spine_validate_enabled() ) {
			vk_spine_record_violation(
				"feature combo: TAA without r_temporalWeaponAfterTaa 1 (weapon can poison history)" );
		}
	}

	/* Spine 1.2: Hybrid1 × pathtrace dual lighting ownership. */
	{
		static qboolean s_warnedHybridPt;
		static qboolean s_warnedMode5Taa;
		const qboolean hybridOn = ( r_hybrid1 && r_hybrid1->integer > 0 ) ? qtrue : qfalse;
		const qboolean ptOn = ( r_pathtrace && r_pathtrace->integer > 0 ) ? qtrue : qfalse;
		const qboolean mode5 = R_RenderMode_IsPathTracedReference();
		const qboolean mode4 = R_RenderMode_IsSelectiveHybrid();

		if ( hybridOn && ptOn && !mode5 ) {
			Q_strncpyz( s_spine.comboFallback, "hybrid1_x_pathtrace",
				sizeof( s_spine.comboFallback ) );
			if ( !s_warnedHybridPt ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] illegal combo: Hybrid1 + pathtrace both on outside mode 5; "
					"PT demoted (no double lighting). Use r_renderMode 5 for exclusive PT reference, "
					"or disable r_pathtrace / r_hybrid1. Recovery: exec modern_vulkan.cfg\n" );
				s_warnedHybridPt = qtrue;
			}
			if ( vk_spine_validate_enabled() ) {
				vk_spine_record_violation(
					"feature combo: Hybrid1 and pathtrace both enabled (one lighting owner)" );
			}
		} else if ( mode5 && ptOn ) {
			Q_strncpyz( s_spine.comboFallback, "spine_1_2_pt_reference",
				sizeof( s_spine.comboFallback ) );
		} else if ( mode4 && hybridOn ) {
			Q_strncpyz( s_spine.comboFallback, "spine_1_2_selective_hybrid",
				sizeof( s_spine.comboFallback ) );
		}

		if ( mode5 && taaOn ) {
			if ( !s_warnedMode5Taa ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] mode 5 PT reference: prefer r_taa 0 / r_aaMode 0 for converged reference "
					"(world TAA trails fight path-trace accumulation)\n" );
				s_warnedMode5Taa = qtrue;
			}
		}
	}

	/* Selective Hybrid Reflections: Hybrid1 spec + SSR without SHR stacks energy. */
	{
		static qboolean s_warnedHybridSsr;
		const qboolean hybridSpec = ( r_hybrid1 && r_hybrid1->integer > 0 &&
			r_hybrid1_spec && r_hybrid1_spec->integer > 0 ) ? qtrue : qfalse;
		const qboolean ssrOn = PostFX_SSR_IsEnabled();
		const qboolean shrOn = vk_shr_active();

		if ( hybridSpec && ssrOn && !shrOn ) {
			Q_strncpyz( s_spine.comboFallback, "hybrid1_spec_x_ssr_without_shr",
				sizeof( s_spine.comboFallback ) );
			if ( !s_warnedHybridSsr ) {
				ri.Printf( PRINT_ALL, S_COLOR_YELLOW
					"[VK][spine] illegal combo: Hybrid1 specular + SSR without "
					"r_selectiveHybridReflection (duplicate reflection energy). "
					"Enable SHR or disable one source. Recovery: exec modern_vulkan.cfg\n" );
				s_warnedHybridSsr = qtrue;
			}
			if ( vk_spine_validate_enabled() ) {
				vk_spine_record_violation(
					"feature combo: hybrid1_spec + SSR without selective hybrid reflections" );
			}
		}
	}
}

void vk_spine_note_oit_skipped( void )
{
	if ( !s_spine.initialized ) {
		return;
	}
	s_spine.oitSkippedThisFrame = qtrue;
	/* Fallback scene-color producer remains HDR color_image (opaque / deferred result). */
	if ( vk.color_image != VK_NULL_HANDLE ) {
		vk_spine_note_write( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_WORLD_OPAQUE,
			VK_SPINE_ACCESS_COLOR_WRITE );
		vk_spine_note_layout( VK_SPINE_RES_HDR_COLOR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
}

void vk_spine_cert_check_taa_input( VkImageView taa_src )
{
	if ( !vk_spine_cert_active() || !vk_spine_validate_enabled() ) {
		return;
	}
	if ( taa_src == VK_NULL_HANDLE ) {
		return;
	}
	/* Key invariant: TAA consumes only resolved world color, never raw OIT targets. */
	if ( taa_src == vk.oit_accum_image_view ||
		taa_src == vk.oit_reveal_image_view ||
		taa_src == vk.oit_moments_image_view ||
		taa_src == vk.oit_b0_image_view ) {
		vk_spine_record_violation(
			"TAA current bound to raw OIT attachment (resolved HDR only)" );
	}
	if ( r_oit && r_oit->integer == 1 && !s_spine.oitSkippedThisFrame &&
		s_spine.oitResolvedThisFrame ) {
		/* After WBOIT resolve, OIT color-write layouts must not remain attachment-optimal. */
		if ( vk_spine_resource_layout( VK_SPINE_RES_OIT_ACCUM ) ==
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
			vk_spine_record_violation(
				"TAA begin while OIT accum still COLOR_ATTACHMENT (need resolve barrier)" );
		}
	}
}

void vk_spine_cert_check_black_frame( void )
{
	float avgLogLum;

	if ( !vk_spine_cert_active() || !( r_spineCert && r_spineCert->integer ) ) {
		return;
	}
	if ( s_spine.certFrameIndex < VK_SPINE_CERT_BLACK_SETTLE_FRAMES ) {
		s_spine.blackStreak = 0u;
		return;
	}
	if ( !vk.luminance_staging_ptr || !backEnd.doneWorldScene || tr.world == NULL ) {
		return;
	}
	avgLogLum = *(const float *)vk.luminance_staging_ptr;
	if ( !isfinite( avgLogLum ) ) {
		vk_spine_record_violation( "NaN/Inf luminance (avgLogLum=%g)", (double)avgLogLum );
		s_spine.blackStreak++;
		return;
	}
	if ( avgLogLum < VK_SPINE_CERT_BLACK_LOG_LUM_FLOOR ) {
		s_spine.blackStreak++;
		if ( s_spine.blackStreak >= VK_SPINE_CERT_BLACK_STREAK ) {
			vk_spine_record_violation(
				"black-frame streak: avgLogLum=%g for %u frames",
				(double)avgLogLum, s_spine.blackStreak );
		}
	} else {
		s_spine.blackStreak = 0u;
	}
}

void vk_spine_cert_check_resource_growth( void )
{
	uint32_t live;

	if ( !vk_spine_cert_active() || !( r_spineCert && r_spineCert->integer ) ) {
		return;
	}
	if ( s_spine.descriptorsPendingRebound ) {
		return;
	}
	live = vk_spine_count_alive_resources();
	s_spine.liveResourceCount = live;
	if ( !s_spine.liveBaselineArmed ) {
		if ( s_spine.certFrameIndex >= VK_SPINE_CERT_BLACK_SETTLE_FRAMES ) {
			s_spine.liveResourceBaseline = live;
			s_spine.liveBaselineArmed = qtrue;
		}
		return;
	}
	/* Attachment recreate resets baseline; otherwise growth is a leak signal. */
	if ( live > s_spine.liveResourceBaseline + 2u ) {
		vk_spine_record_violation(
			"resource growth: alive=%u baseline=%u (possible leak after recreate)",
			live, s_spine.liveResourceBaseline );
		s_spine.liveResourceBaseline = live;
	}
}

void vk_spine_cert_check_history_invalidated( uint32_t resetReasons )
{
	const uint32_t lifecycle =
		VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE |
		VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
		VK_TEMPORAL_RESET_WORLD_CHANGE |
		VK_TEMPORAL_RESET_RENDERER_INIT |
		VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE;

	if ( !vk_spine_validate_enabled() || resetReasons == 0u ) {
		return;
	}
	if ( ( resetReasons & lifecycle ) == 0u ) {
		return;
	}
	/* Sticky lifecycle resets must clear world TAA history. */
	if ( vk.temporal.hasValidTAAHistory ) {
		vk_spine_record_violation(
			"TAA history still valid after lifecycle reset (reasons=0x%x)",
			resetReasons );
	}
	vk_spine_note_temporal_history( VK_SPINE_RES_TAA_HISTORY, qfalse );
}

void vk_spine_cert_check_weapon_flush_order( qboolean taaRanThisFrame )
{
	if ( !vk_spine_cert_active() || !vk_spine_validate_enabled() ) {
		return;
	}
	if ( !taaRanThisFrame ) {
		return;
	}
	/* World Temporal Reconstruction must have been observed before weapon flush. */
	if ( !vk_spine_was_observed( VK_SPINE_PASS_TEMPORAL_RECON ) ) {
		vk_spine_record_violation(
			"weapon flush before Temporal Reconstruction (weapon must stay after world TAA)" );
	}
}

qboolean vk_spine_combo_suppress_taa( void )
{
	return s_spine.suppressTaaThisFrame;
}

const char *vk_spine_combo_fallback( void )
{
	return s_spine.comboFallback[0] ? s_spine.comboFallback : "none";
}

void vk_spine_dump_device_lost( void )
{
	vkSpineResourceId r;
	int shown = 0;

	if ( !s_spine.initialized ) {
		return;
	}
	ri.Printf( PRINT_ALL,
		"[VK][device_lost][spine] gen=%u descGen=%u phase=%s begun=%s ended=%s open=%d violations=%u last=%s combo=%s spine11=%s\n",
		s_spine.attachmentGeneration,
		s_spine.descriptorGeneration,
		vk_spine_phase_name( s_spine.currentPhase ),
		vk_spine_pass_name( s_spine.lastBegun ),
		vk_spine_pass_name( s_spine.lastEnded ),
		s_spine.openCount,
		s_spine.violationCount,
		s_spine.lastViolation[0] ? s_spine.lastViolation : "none",
		s_spine.comboFallback[0] ? s_spine.comboFallback : "none",
		s_spine.spine11CertActive ? "yes" : "no" );
	for ( r = (vkSpineResourceId)1; r < VK_SPINE_RES_COUNT && shown < 24; r++ ) {
		const vkSpineResourceRuntime *rt = &s_spine.resources[r];
		if ( !rt->alive && rt->lastWriter == VK_SPINE_PASS_NONE ) {
			continue;
		}
		ri.Printf( PRINT_ALL,
			"[VK][device_lost][spine]  %s alive=%d gen=%u %ux%u writer=%s hist=%d\n",
			vk_spine_resource_name( r ),
			rt->alive ? 1 : 0,
			rt->generation,
			rt->width, rt->height,
			vk_spine_pass_name( rt->lastWriter ),
			rt->historyValid ? 1 : 0 );
		shown++;
	}
}

void vk_spine_status_f( void )
{
	vkSpinePassId p;
	vkSpineResourceId r;
	uint32_t w = 0, h = 0;

	vk_get_active_render_extent( &w, &h );
	ri.Printf( PRINT_ALL, "======== Spine Pass/Resource Registry ========\n" );
	ri.Printf( PRINT_ALL, "validate  : r_spineValidate=%d r_spineCert=%d enabled=%s spine11=%s\n",
		r_spineValidate ? r_spineValidate->integer : 0,
		r_spineCert ? r_spineCert->integer : 0,
		vk_spine_validate_enabled() ? "yes" : "no",
		vk_spine_is_spine_1_1_combo() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "generation: attachments=%u descriptors=%u pendingRebound=%s live=%u baseline=%u extent=%ux%u\n",
		s_spine.attachmentGeneration,
		s_spine.descriptorGeneration,
		s_spine.descriptorsPendingRebound ? "yes" : "no",
		s_spine.liveResourceCount,
		s_spine.liveResourceBaseline,
		w, h );
	ri.Printf( PRINT_ALL, "phase     : current=%s highest=%s\n",
		vk_spine_phase_name( s_spine.currentPhase ),
		vk_spine_phase_name( s_spine.highestPhase ) );
	ri.Printf( PRINT_ALL, "frame     : begun=%s ended=%s open=%d certFrame=%u blackStreak=%u\n",
		vk_spine_pass_name( s_spine.lastBegun ),
		vk_spine_pass_name( s_spine.lastEnded ),
		s_spine.openCount,
		s_spine.certFrameIndex,
		s_spine.blackStreak );
	ri.Printf( PRINT_ALL, "violations: count=%u last=%s\n",
		s_spine.violationCount,
		s_spine.lastViolation[0] ? s_spine.lastViolation : "none" );
	ri.Printf( PRINT_ALL, "combo     : %s\n",
		s_spine.comboFallback[0] ? s_spine.comboFallback : "none" );

	ri.Printf( PRINT_ALL, "---- passes (declared / observed this frame) ----\n" );
	for ( p = (vkSpinePassId)1; p < VK_SPINE_PASS_COUNT; p++ ) {
		ri.Printf( PRINT_ALL, "  %-24s phase=%-20s obs=%s hist=%s\n",
			vk_spine_pass_name( p ),
			vk_spine_phase_name( s_passes[p].phase ),
			vk_spine_was_observed( p ) ? "yes" : "no",
			s_passes[p].usesTemporalHistory ? "yes" : "no" );
	}

	ri.Printf( PRINT_ALL, "---- resources ----\n" );
	for ( r = (vkSpineResourceId)1; r < VK_SPINE_RES_COUNT; r++ ) {
		const vkSpineResourceRuntime *rt = &s_spine.resources[r];
		ri.Printf( PRINT_ALL,
			"  %-22s alive=%s gen=%u %ux%u writer=%-20s reader=%-20s hist=%s clr=%s bar=%s lay=%s\n",
			vk_spine_resource_name( r ),
			rt->alive ? "yes" : "no",
			rt->generation,
			rt->width, rt->height,
			vk_spine_pass_name( rt->lastWriter ),
			vk_spine_pass_name( rt->lastReader ),
			rt->historyValid ? "valid" : "reset",
			rt->clearedThisFrame ? "yes" : "no",
			rt->barrierThisFrame ? ( rt->lastBarrierReason[0] ? rt->lastBarrierReason : "yes" ) : "no",
			rt->layoutKnown ? vk_spine_layout_name( rt->layout ) : "?" );
	}
	ri.Printf( PRINT_ALL, "===============================================\n" );
}
