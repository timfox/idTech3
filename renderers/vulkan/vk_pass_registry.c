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
#include <stdarg.h>

cvar_t *r_spineValidate;

#define VK_SPINE_MAX_OPEN 8
#define VK_SPINE_MAX_VIOLATIONS 16

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
	return ( r_spineValidate && r_spineValidate->integer > 0 ) ? qtrue : qfalse;
}

uint32_t vk_spine_attachment_generation( void )
{
	return s_spine.attachmentGeneration;
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
	s_spine.currentPhase = VK_SPINE_PHASE_FRAME_BEGIN;

	r_spineValidate = ri.Cvar_Get( "r_spineValidate", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spineValidate, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_spineValidate,
		"Spine pass/resource registry validation:\n"
		" 0 - ownership stamps only (cheap)\n"
		" 1 - validate phase order / stale reads (developer)\n"
		" 2 - same, PRINT_ALL on violations" );
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "pass_registry_status", vk_spine_status_f );
		ri.Cmd_AddCommand( "spine_status", vk_spine_status_f );
	}
	ri.Printf( PRINT_ALL, "[VK][spine] pass/resource registry ready (r_spineValidate=%d; combo soft-demote + DEVICE_LOST late-post context)\n",
		r_spineValidate ? r_spineValidate->integer : 0 );
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
	Com_Memset( s_spine.framePassMask, 0, sizeof( s_spine.framePassMask ) );
	for ( i = 0; i < VK_SPINE_RES_COUNT; i++ ) {
		/* Keep alive/generation/extent across frames; clear per-frame edge stamps. */
		s_spine.resources[i].clearedThisFrame = qfalse;
		s_spine.resources[i].barrierThisFrame = qfalse;
		if ( vk_spine_validate_enabled() ) {
			s_spine.resources[i].lastReader = VK_SPINE_PASS_NONE;
			s_spine.resources[i].lastReadAccess = 0u;
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
	if ( ( access & VK_SPINE_ACCESS_HISTORY_WRITE ) != 0u ) {
		rt->historyValid = qtrue;
		rt->ownerViewClass = vk_classify_current_view();
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
		vk.oit_reveal_image != VK_NULL_HANDLE, width, height, VK_FORMAT_R8_UNORM );
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

	/* Temporal histories invalidate across attachment recreate. */
	vk_spine_note_temporal_history( VK_SPINE_RES_TAA_HISTORY, qfalse );
	vk_spine_note_temporal_history( VK_SPINE_RES_AV_HISTORY, qfalse );
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
		} else if ( !s_warnedOitTaa ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW
				"[VK][spine] experimental combo: OIT + TAA (weapon-after on). "
				"Certified matrix keeps them separate; enable r_spineValidate 1 while testing.\n" );
			s_warnedOitTaa = qtrue;
			if ( vk_spine_validate_enabled() ) {
				vk_spine_record_violation(
					"feature combo: OIT + TAA both enabled (quality matrix; weapon-after-TAA required)" );
			}
		} else if ( vk_spine_validate_enabled() ) {
			vk_spine_record_violation(
				"feature combo: OIT + TAA both enabled (quality matrix; weapon-after-TAA required)" );
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
		"[VK][device_lost][spine] gen=%u phase=%s begun=%s ended=%s open=%d violations=%u last=%s combo=%s\n",
		s_spine.attachmentGeneration,
		vk_spine_phase_name( s_spine.currentPhase ),
		vk_spine_pass_name( s_spine.lastBegun ),
		vk_spine_pass_name( s_spine.lastEnded ),
		s_spine.openCount,
		s_spine.violationCount,
		s_spine.lastViolation[0] ? s_spine.lastViolation : "none",
		s_spine.comboFallback[0] ? s_spine.comboFallback : "none" );
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
	ri.Printf( PRINT_ALL, "validate  : r_spineValidate=%d enabled=%s\n",
		r_spineValidate ? r_spineValidate->integer : 0,
		vk_spine_validate_enabled() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "generation: attachments=%u activeExtent=%ux%u\n",
		s_spine.attachmentGeneration, w, h );
	ri.Printf( PRINT_ALL, "phase     : current=%s highest=%s\n",
		vk_spine_phase_name( s_spine.currentPhase ),
		vk_spine_phase_name( s_spine.highestPhase ) );
	ri.Printf( PRINT_ALL, "frame     : begun=%s ended=%s open=%d\n",
		vk_spine_pass_name( s_spine.lastBegun ),
		vk_spine_pass_name( s_spine.lastEnded ),
		s_spine.openCount );
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
