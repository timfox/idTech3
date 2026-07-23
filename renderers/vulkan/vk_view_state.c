#include "tr_local.h"
#include "vk.h"
#include "vk_temporal.h"
#include "vk_view_state.h"
#include "vk_frequency_aware.h"
#include "vk_upscale.h"
#include "vk_object_id.h"
#include "vk_oit_alpha.h"
#include <math.h>

typedef struct vkMvpPushConstants_s {
	float mvp[16];
	float prev_mvp[16];
	float reserved[8]; /* padding / future push data; size must match VkPushConstantRange in vk_init_device.c */
} vkMvpPushConstants_t;

/* OIT layouts: mvp + prevMvp + model (192 B) + fog/sun block (64 B) = 256 B. */
typedef struct vkOitPushConstants_s {
	float mvp[16];
	float prev_mvp[16];
	float model[16];
	int lightingDebug;
	int parityCompare;
	int fogMode;
	int fogDebug;
	float fogDensity;
	float coverageScale; /* <1 softens near-opaque tex alpha (Q3 glass often a≈1) */
	float sunDir[3];     /* world-space direction toward sun */
	float sunStrength;   /* 0 = sun term off */
	float sunColor[3];
	float sunAmbient;
	float cascadeCount; /* CSM cascade count (was _pad0) */
	int alphaPack;      /* enc | dbg<<8 | edge<<16 | emissive<<24 */
} vkOitPushConstants_t;

_Static_assert( sizeof( vkOitPushConstants_t ) == 256,
	"OIT push constants must stay 256 bytes (pipeline_layout_oit_* ranges)" );
_Static_assert( sizeof( vkMvpPushConstants_t ) == 160,
	"MVP push constants must match VkPushConstantRange in vk_init_device.c" );

static VkRect2D vk_scene_src_rect;
static qboolean vk_scene_src_rect_valid;

/*
 * Dynamic-object motion history keyed by spatial match (hModel + nearest origin),
 * not refdef slot index. Slot rematching across frames was the primary cause of
 * zero object velocity → stale TAA history ghosts on rotating/bobbing pickups.
 */
typedef enum {
	VK_MOTION_OK = 0,
	VK_MOTION_INVALID_NO_PREV,
	VK_MOTION_INVALID_TELEPORT,
	VK_MOTION_INVALID_MODEL_CHANGE,
	VK_MOTION_INVALID_SKIN_CHANGE,
	VK_MOTION_INVALID_TRANSFORM_JUMP,
	VK_MOTION_INVALID_SLOT_REUSE,
	VK_MOTION_INVALID_ANIM_NO_POSE,
	VK_MOTION_INVALID_FIRST_PERSON,
	VK_MOTION_INVALID_CUSTOM_SHADER,
	VK_MOTION_INVALID_STALE_PREV
} vkMotionInvalidReason_t;

typedef struct {
	qboolean		valid;
	vec3_t			origin;
	float			matrix[16];
	int				hModel;
	int				reType;
	int				skinNum;
	qhandle_t		customSkin;
	qhandle_t		customShader;
	int				frame;
	int				oldframe;
	uint32_t		generation;
	uint32_t		stableId;
	uint32_t		frameId;
	int				lastInvalidReason;
	uint32_t		prevAge;	/* temporal frames between matched prev record and this one (1 = healthy) */
} vkEntityMotionRecord_t;

static vkEntityMotionRecord_t vk_motion_prev[MAX_REFENTITIES];
static vkEntityMotionRecord_t vk_motion_curr[MAX_REFENTITIES];
static int vk_motion_prev_count;
static int vk_motion_curr_count;

/* Per-draw result consumed by vk_update_mvp for push-constant motion meta. */
static qboolean vk_motion_draw_invalid;
static uint32_t vk_motion_draw_stable_id;
static uint32_t vk_motion_draw_generation;
static int vk_motion_draw_invalid_reason;
static qboolean vk_motion_draw_is_dynamic;

#ifndef VK_MOTION_TELEPORT_UNITS
#define VK_MOTION_TELEPORT_UNITS 96.0f
#endif
#ifndef VK_MOTION_JUMP_UNITS
#define VK_MOTION_JUMP_UNITS 48.0f
#endif

typedef struct {
	qboolean valid;
	uint32_t frame;
	float weaponFovX;
	float weaponFovY;
	float worldFovX;
	float worldFovY;
	float zNear;
	float zFar;
	float jitterX;
	float jitterY;
	qboolean overrideEnabled;
	qboolean usingOverride;
	qboolean jitterApplied;
} vkViewmodelProjectionDiagnostic_t;

static vkViewmodelProjectionDiagnostic_t vk_viewmodel_projection_current;
static vkViewmodelProjectionDiagnostic_t vk_viewmodel_projection_previous;

uint32_t vk_get_render_target_width( void )
{
	if ( vk.fboActive && vk.mainColorWidth > 0u ) {
		return vk.mainColorWidth;
	}
	if ( vk.renderWidth > 0 ) {
		return vk.renderWidth;
	}
	if ( glConfig.vidWidth > 0 ) {
		return (uint32_t)glConfig.vidWidth;
	}
	return 1u;
}

uint32_t vk_get_render_target_height( void )
{
	if ( vk.fboActive && vk.mainColorHeight > 0u ) {
		return vk.mainColorHeight;
	}
	if ( vk.renderHeight > 0 ) {
		return vk.renderHeight;
	}
	if ( glConfig.vidHeight > 0 ) {
		return (uint32_t)glConfig.vidHeight;
	}
	return 1u;
}

static float vk_get_2d_logical_width( void )
{
	if ( glConfig.vidWidth > 0 ) {
		return (float)glConfig.vidWidth;
	}
	return (float)vk_get_render_target_width();
}

static float vk_get_2d_logical_height( void )
{
	if ( glConfig.vidHeight > 0 ) {
		return (float)glConfig.vidHeight;
	}
	return (float)vk_get_render_target_height();
}

void vk_reset_scene_src_rect_tracking( void )
{
	vk_scene_src_rect_valid = qfalse;
}

qboolean vk_get_scene_src_rect( VkRect2D *out_rect )
{
	uint32_t maxW;
	uint32_t maxH;
	VkRect2D validated;

	if ( !out_rect || !vk_scene_src_rect_valid ) {
		return qfalse;
	}

	maxW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : vk_get_render_target_width();
	maxH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : vk_get_render_target_height();
	if ( maxW == 0u || maxH == 0u ) {
		vk_scene_src_rect_valid = qfalse;
		return qfalse;
	}

	validated = vk_scene_src_rect;
	if ( validated.offset.x < 0 ) {
		int32_t dx = -validated.offset.x;
		validated.offset.x = 0;
		validated.extent.width = ( validated.extent.width > (uint32_t)dx ) ?
			( validated.extent.width - (uint32_t)dx ) : 0u;
	}
	if ( validated.offset.y < 0 ) {
		int32_t dy = -validated.offset.y;
		validated.offset.y = 0;
		validated.extent.height = ( validated.extent.height > (uint32_t)dy ) ?
			( validated.extent.height - (uint32_t)dy ) : 0u;
	}
	if ( (uint32_t)validated.offset.x >= maxW || (uint32_t)validated.offset.y >= maxH ||
		validated.extent.width == 0u || validated.extent.height == 0u ) {
		vk_scene_src_rect_valid = qfalse;
		return qfalse;
	}
	if ( (uint32_t)validated.offset.x + validated.extent.width > maxW ) {
		validated.extent.width = maxW - (uint32_t)validated.offset.x;
	}
	if ( (uint32_t)validated.offset.y + validated.extent.height > maxH ) {
		validated.extent.height = maxH - (uint32_t)validated.offset.y;
	}
	if ( validated.extent.width == 0u || validated.extent.height == 0u ) {
		vk_scene_src_rect_valid = qfalse;
		return qfalse;
	}

	vk_scene_src_rect = validated;
	*out_rect = validated;
	return qtrue;
}

static void vk_get_viewport_rect( VkRect2D *r )
{
	if ( backEnd.projection2D ) {
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	} else {
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - ( backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight ) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void vk_get_viewport( VkViewport *viewport, Vk_Depth_Range depth_range )
{
	VkRect2D r;

	vk_get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
	}
}

void vk_get_scissor_rect( VkRect2D *r )
{
	if ( backEnd.viewParms.portalView != PV_NONE ) {
		const uint32_t targetWidth = vk_get_render_target_width();
		const uint32_t targetHeight = vk_get_render_target_height();
		r->offset.x = (int32_t)( (float)backEnd.viewParms.scissorX * vk.renderScaleX );
		r->offset.y = (int32_t)( (float)targetHeight -
			(float)( backEnd.viewParms.scissorY + backEnd.viewParms.scissorHeight ) * vk.renderScaleY );
		r->extent.width = (uint32_t)( (float)backEnd.viewParms.scissorWidth * vk.renderScaleX );
		r->extent.height = (uint32_t)( (float)backEnd.viewParms.scissorHeight * vk.renderScaleY );
		if ( r->offset.x < 0 ) {
			r->offset.x = 0;
		}
		if ( r->offset.y < 0 ) {
			r->offset.y = 0;
		}
		if ( (uint32_t)r->offset.x >= targetWidth || (uint32_t)r->offset.y >= targetHeight ) {
			r->extent.width = 0;
			r->extent.height = 0;
			return;
		}
		if ( (uint32_t)r->offset.x + r->extent.width > targetWidth ) {
			r->extent.width = targetWidth - (uint32_t)r->offset.x;
		}
		if ( (uint32_t)r->offset.y + r->extent.height > targetHeight ) {
			r->extent.height = targetHeight - (uint32_t)r->offset.y;
		}
	} else {
		const uint32_t targetWidth = vk_get_render_target_width();
		const uint32_t targetHeight = vk_get_render_target_height();
		vk_get_viewport_rect( r );

		if ( r->offset.x < 0 ) {
			r->offset.x = 0;
		}
		if ( r->offset.y < 0 ) {
			r->offset.y = 0;
		}

		if ( (uint32_t)r->offset.x >= targetWidth || (uint32_t)r->offset.y >= targetHeight ) {
			r->extent.width = 0;
			r->extent.height = 0;
			return;
		}

		if ( (uint32_t)r->offset.x + r->extent.width > targetWidth ) {
			r->extent.width = targetWidth - (uint32_t)r->offset.x;
		}
		if ( (uint32_t)r->offset.y + r->extent.height > targetHeight ) {
			r->extent.height = targetHeight - (uint32_t)r->offset.y;
		}
	}
}

void vk_get_projection_matrix_vk( const float *projection_matrix, float *projection_vk )
{
	Com_Memcpy( projection_vk, projection_matrix, sizeof( float ) * 16 );
	projection_vk[5] = -projection_matrix[5];
}

static void vk_get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D ) {
		float mvp0 = 2.0f / vk_get_2d_logical_width();
		float mvp5 = 2.0f / vk_get_2d_logical_height();

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
	} else {
		float proj[16];
		const float *projection = backEnd.useFirstPersonProjection
			? backEnd.firstPersonProjectionMatrix
			: backEnd.viewParms.projectionMatrix;
		vk_get_projection_matrix_vk( projection, proj );
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}

void vk_prime_gpu_morph_weights_current( void )
{
	unsigned int i;
	trRefEntity_t *ents;

	if ( !backEndData ) {
		return;
	}
	ents = backEndData->entities;
	for ( i = 0; i < (unsigned int)MAX_REFENTITIES; i++ ) {
		ents[i].morphGpuWeightsPrimedSingleUse = qfalse;
	}
}

void vk_snap_gpu_morph_weights_for_motion( void )
{
	unsigned int i, k;
	const unsigned int n = tr.refdef.num_entities;
	trRefEntity_t *ents;

	if ( n == 0 ) {
		return;
	}
	ents = tr.refdef.entities;
	if ( !ents ) {
		return;
	}
	for ( i = 0; i < n; i++ ) {
		trRefEntity_t *re = ents + i;
		int ch;
		for ( k = 0; k < (unsigned int)IQM_MORPH_TOP_K; k++ ) {
			re->morphGpuWeightPrev[k] = re->morphActiveWeight[k];
		}
		for ( ch = 0; ch < re->morphChannelCount && ch < IQM_MORPH_MAX_CHANNELS; ch++ ) {
			re->morphChannelWeightPrev[ch] = re->morphChannelWeights[ch];
		}
	}
}

void vk_begin_motion_frame( void )
{
	int i;

	/* Promote this frame's records to previous for the next frame's lookup. */
	vk_motion_prev_count = vk_motion_curr_count;
	if ( vk_motion_prev_count > MAX_REFENTITIES ) {
		vk_motion_prev_count = MAX_REFENTITIES;
	}
	for ( i = 0; i < vk_motion_prev_count; i++ ) {
		vk_motion_prev[i] = vk_motion_curr[i];
	}
	for ( ; i < MAX_REFENTITIES; i++ ) {
		vk_motion_prev[i].valid = qfalse;
	}
	vk_motion_curr_count = 0;
	Com_Memset( vk_motion_curr, 0, sizeof( vk_motion_curr ) );
	vk_motion_draw_invalid = qfalse;
	vk_motion_draw_stable_id = 0;
	vk_motion_draw_generation = 0;
	vk_motion_draw_invalid_reason = VK_MOTION_OK;
	vk_motion_draw_is_dynamic = qfalse;
}

void vk_reset_motion_history( void )
{
	Com_Memset( vk_motion_prev, 0, sizeof( vk_motion_prev ) );
	Com_Memset( vk_motion_curr, 0, sizeof( vk_motion_curr ) );
	vk_motion_prev_count = 0;
	vk_motion_curr_count = 0;
	vk_motion_draw_invalid = qfalse;
	vk_motion_draw_stable_id = 0;
	vk_motion_draw_generation = 0;
	vk_motion_draw_invalid_reason = VK_MOTION_OK;
	vk_motion_draw_is_dynamic = qfalse;

	/* Invalidate first-person weapon prev MVP (cut / resize / weapon switch path). */
	vk.temporal.weaponMatricesValid = qfalse;
	vk.temporal.weaponMatricesHavePrev = qfalse;
	vk.temporal.weaponEntityId = 0;
	vk.temporal.weaponEntityIdPrev = 0;
	Com_Memset( &vk_viewmodel_projection_current, 0, sizeof( vk_viewmodel_projection_current ) );
	Com_Memset( &vk_viewmodel_projection_previous, 0, sizeof( vk_viewmodel_projection_previous ) );
	/* Phase 5/7: drop previous-matrix ownership so the next commit starts clean. */
	vk_prev_matrices_valid = qfalse;
	vk_prev_matrices_frame = 0u;
	vk_prev_jitter_valid = qfalse;
	vk_prev_jitter_x = 0.0f;
	vk_prev_jitter_y = 0.0f;
}

static const char *vk_motion_invalid_reason_name( int reason )
{
	switch ( reason ) {
		case VK_MOTION_OK: return "ok";
		case VK_MOTION_INVALID_NO_PREV: return "no_prev";
		case VK_MOTION_INVALID_TELEPORT: return "teleport";
		case VK_MOTION_INVALID_MODEL_CHANGE: return "model_change";
		case VK_MOTION_INVALID_SKIN_CHANGE: return "skin_change";
		case VK_MOTION_INVALID_TRANSFORM_JUMP: return "transform_jump";
		case VK_MOTION_INVALID_SLOT_REUSE: return "slot_reuse";
		case VK_MOTION_INVALID_ANIM_NO_POSE: return "anim_no_pose";
		case VK_MOTION_INVALID_FIRST_PERSON: return "first_person";
		case VK_MOTION_INVALID_CUSTOM_SHADER: return "custom_shader";
		case VK_MOTION_INVALID_STALE_PREV: return "stale_prev";
		default: return "?";
	}
}

/*
 * temporal_motion_status: per-entity dynamic-object motion + identity report.
 * Text realization of the requested helmet overlay — dump the same fields the
 * TAA resolve uses to accept/reject history so ghosting can be diagnosed live.
 */
void vk_motion_status_f( void )
{
	int i;
	int shown = 0;

	ri.Printf( PRINT_ALL, "Dynamic-object temporal motion (frame %u, objIdBuf=%s, prev=%s)\n",
		vk.temporal.frameIndex,
		vk_object_id_active() ? "on" : "off",
		vk.temporal.objectIdHasPrev ? "valid" : "none" );
	ri.Printf( PRINT_ALL, "  slot=%u curr=%d prev=%d\n",
		vk.temporal.objectIdIndex, vk_motion_curr_count, vk_motion_prev_count );
	ri.Printf( PRINT_ALL, "  %-4s %-6s %-6s %-8s %-6s %-6s %-5s %-14s\n",
		"idx", "model", "id16", "frameId", "gen", "prev", "age", "lastInvalid" );

	for ( i = 0; i < vk_motion_curr_count && i < MAX_REFENTITIES; i++ ) {
		const vkEntityMotionRecord_t *c = &vk_motion_curr[i];
		const char *ageColor;
		if ( !c->valid ) {
			continue;
		}
		/* Phase 5 overlay semantics: age 1 green, >1 red, no/invalid prev yellow. */
		ageColor = ( c->lastInvalidReason != VK_MOTION_OK ) ? S_COLOR_YELLOW :
			( c->prevAge == 1u ) ? S_COLOR_GREEN : S_COLOR_RED;
		ri.Printf( PRINT_ALL, "  %-4d %-6d %-6u %-8u %-6u %-6s %s%-5u" S_COLOR_WHITE " %-14s\n",
			i, c->hModel, (unsigned)( c->stableId & 0xFFFFu ), (unsigned)c->frameId,
			(unsigned)c->generation,
			c->lastInvalidReason == VK_MOTION_OK ? "yes" : "no",
			ageColor, (unsigned)c->prevAge,
			vk_motion_invalid_reason_name( c->lastInvalidReason ) );
		shown++;
	}
	if ( shown == 0 ) {
		ri.Printf( PRINT_ALL, "  (no dynamic objects this frame)\n" );
	}
}

static float vk_projection_fov_degrees( float scale )
{
	if ( fabsf( scale ) < 1e-6f ) {
		return 0.0f;
	}
	return 2.0f * atanf( 1.0f / fabsf( scale ) ) * 180.0f / (float)M_PI;
}

static void vk_capture_viewmodel_projection_diagnostic( const float *projection )
{
	vkViewmodelProjectionDiagnostic_t next;
	float jitterX = 0.0f;
	float jitterY = 0.0f;

	if ( !projection ) {
		return;
	}

	if ( vk_viewmodel_projection_current.valid &&
		vk_viewmodel_projection_current.frame != (uint32_t)tr.frameCount ) {
		vk_viewmodel_projection_previous = vk_viewmodel_projection_current;
	}

	Com_Memset( &next, 0, sizeof( next ) );
	R_Upscale_GetJitter( &jitterX, &jitterY );
	next.valid = qtrue;
	next.frame = (uint32_t)tr.frameCount;
	next.weaponFovX = vk_projection_fov_degrees( projection[0] );
	next.weaponFovY = vk_projection_fov_degrees( projection[5] );
	next.worldFovX = backEnd.viewParms.fovX;
	next.worldFovY = backEnd.viewParms.fovY;
	next.zNear = backEnd.useFirstPersonProjection
		? Com_Clamp( 0.01f, 8.0f, r_firstPersonZNear->value )
		: r_znear->value;
	next.zFar = backEnd.viewParms.zFar;
	next.jitterX = jitterX;
	next.jitterY = jitterY;
	next.overrideEnabled = ( r_firstPersonFovEnabled && r_firstPersonFovEnabled->integer )
		? qtrue : qfalse;
	next.usingOverride = backEnd.useFirstPersonProjection;
	/* The custom projection builder deliberately emits stable XY terms. */
	next.jitterApplied = ( !backEnd.useFirstPersonProjection &&
		( jitterX != 0.0f || jitterY != 0.0f ) ) ? qtrue : qfalse;
	vk_viewmodel_projection_current = next;
	if ( r_temporalDebug && r_temporalDebug->integer >= 16 ) {
		ri.Cvar_Set( "r_temporalOverlayInfo",
			va( "Temporal weapon: mode=%d FOV=%.1fx%.1f world=%.1fx%.1f z=%.2f/%.0f "
				"jitter=%.3f,%.3f MVPprev=%d hist={C%d D%d K%d W%d} frame=%u",
				r_weaponTemporalMode ? r_weaponTemporalMode->integer : 0,
				next.weaponFovX, next.weaponFovY, next.worldFovX, next.worldFovY,
				next.zNear, next.zFar, next.jitterX, next.jitterY,
				vk.temporal.weaponMatricesHavePrev ? 1 : 0,
				vk.temporal.prevColorValid ? 1 : 0,
				vk.temporal.prevDepthValid ? 1 : 0,
				vk.temporal.prevClassValid ? 1 : 0,
				vk.temporal.weaponHistoryValid ? 1 : 0,
				vk.temporal.frameIndex ) );
	}
}

void vk_print_viewmodel_projection_f( void )
{
	const vkViewmodelProjectionDiagnostic_t *current = &vk_viewmodel_projection_current;
	const vkViewmodelProjectionDiagnostic_t *previous = &vk_viewmodel_projection_previous;

	ri.Printf( PRINT_ALL, "======== Viewmodel Projection ========\n" );
	if ( !current->valid ) {
		ri.Printf( PRINT_ALL, "effective state       : unavailable (no RF_FIRST_PERSON draw captured)\n" );
		ri.Printf( PRINT_ALL, "configured weapon FOV : %.3f deg horizontal (override=%s)\n",
			r_firstPersonFov ? r_firstPersonFov->value : 0.0f,
			( r_firstPersonFovEnabled && r_firstPersonFovEnabled->integer ) ? "enabled" : "disabled" );
		ri.Printf( PRINT_ALL, "configured z-near     : %.3f\n",
			r_firstPersonZNear ? r_firstPersonZNear->value : 0.0f );
		ri.Printf( PRINT_ALL, "======================================\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "effective weapon FOV  : %.3f deg horizontal\n", current->weaponFovX );
	ri.Printf( PRINT_ALL, "effective world FOV   : %.3f x %.3f deg (horizontal x vertical)\n",
		current->worldFovX, current->worldFovY );
	ri.Printf( PRINT_ALL, "aspect-adjusted FOV   : %.3f deg vertical (from %.3f deg horizontal)\n",
		current->weaponFovY, current->weaponFovX );
	ri.Printf( PRINT_ALL, "z-near / z-far        : %.3f / %.3f\n", current->zNear, current->zFar );
	ri.Printf( PRINT_ALL, "projection mode       : %s (r_firstPersonFovEnabled=%d)\n",
		current->usingOverride ? "custom horizontal weapon FOV" : "scene/world projection",
		current->overrideEnabled ? 1 : 0 );
#ifdef USE_VULKAN
	ri.Printf( PRINT_ALL, "reversed-Z state      : enabled (Vulkan 0..1 clip depth)\n" );
#else
	ri.Printf( PRINT_ALL, "reversed-Z state      : disabled\n" );
#endif
	ri.Printf( PRINT_ALL, "depth-range remap     : DEPTH_RANGE_WEAPON [0.600, 1.000]\n" );
	ri.Printf( PRINT_ALL, "jitter state          : current=(%.4f, %.4f) px appliedToWeapon=%s\n",
		current->jitterX, current->jitterY, current->jitterApplied ? "yes" : "no" );

	if ( previous->valid ) {
		ri.Printf( PRINT_ALL,
			"previous-frame values : frame=%u weaponFov=%.3f x %.3f worldFov=%.3f x %.3f "
			"z=%.3f/%.3f jitter=(%.4f, %.4f) mode=%s\n",
			previous->frame,
			previous->weaponFovX, previous->weaponFovY,
			previous->worldFovX, previous->worldFovY,
			previous->zNear, previous->zFar,
			previous->jitterX, previous->jitterY,
			previous->usingOverride ? "custom" : "scene" );
	} else {
		ri.Printf( PRINT_ALL, "previous-frame values : unavailable (history not captured yet)\n" );
	}
	ri.Printf( PRINT_ALL, "matrix provenance     : current and velocity history use the same effective weapon projection\n" );
	ri.Printf( PRINT_ALL, "======================================\n" );
}

static void vk_capture_weapon_matrices( void )
{
	const trRefEntity_t *ent = backEnd.currentEntity;
	const float *projection;
	uint32_t entityId;
	float fovDelta;

	if ( !ent || !( ent->e.renderfx & RF_FIRST_PERSON ) ) {
		return;
	}
	if ( backEnd.projection2D || backEnd.viewParms.portalView != PV_NONE ) {
		return;
	}

	projection = backEnd.useFirstPersonProjection
		? backEnd.firstPersonProjectionMatrix
		: backEnd.viewParms.projectionMatrix;
	vk_capture_viewmodel_projection_diagnostic( projection );

	entityId = (uint32_t)ent->e.hModel;
	/* Weapon switch or FOV jump → drop prev so velocity does not invent a teleport. */
	fovDelta = 0.0f;
	if ( vk.temporal.weaponMatricesValid ) {
		/* Projection [0] scales with cot(fov/2); large change ⇒ FOV cut. */
		fovDelta = fabsf( projection[0] - vk.temporal.weaponProjectionMatrix[0] );
	}
	if ( vk.temporal.weaponMatricesValid &&
		( entityId != vk.temporal.weaponEntityId || fovDelta > 0.02f ) ) {
		vk.temporal.weaponMatricesHavePrev = qfalse;
		vk_reset_weapon_history();
	} else if ( vk.temporal.weaponMatricesValid ) {
		Com_Memcpy( vk.temporal.weaponPrevViewMatrix, vk.temporal.weaponViewMatrix,
			sizeof( vk.temporal.weaponPrevViewMatrix ) );
		Com_Memcpy( vk.temporal.weaponPrevProjectionMatrix, vk.temporal.weaponProjectionMatrix,
			sizeof( vk.temporal.weaponPrevProjectionMatrix ) );
		Com_Memcpy( vk.temporal.weaponPrevModelMatrix, vk.temporal.weaponModelMatrix,
			sizeof( vk.temporal.weaponPrevModelMatrix ) );
		vk.temporal.weaponEntityIdPrev = vk.temporal.weaponEntityId;
		vk.temporal.weaponMatricesHavePrev = qtrue;
	}

	Com_Memcpy( vk.temporal.weaponViewMatrix, backEnd.viewParms.world.modelViewMatrix,
		sizeof( vk.temporal.weaponViewMatrix ) );
	/* Non-jittered projection (strip temporal upscale jitter from [8]/[9] if present). */
	Com_Memcpy( vk.temporal.weaponProjectionMatrix, projection,
		sizeof( vk.temporal.weaponProjectionMatrix ) );
	if ( !backEnd.useFirstPersonProjection ) {
		float jx = 0.0f, jy = 0.0f;
		const float width = (float)vk_get_render_target_width();
		const float height = (float)vk_get_render_target_height();
		R_Upscale_GetJitter( &jx, &jy );
		if ( ( jx != 0.0f || jy != 0.0f ) && width > 0.0f && height > 0.0f ) {
			vk.temporal.weaponProjectionMatrix[8] -= ( 2.0f * jx ) / width;
			vk.temporal.weaponProjectionMatrix[9] -= ( 2.0f * jy ) / height;
		}
	}
	Com_Memcpy( vk.temporal.weaponModelMatrix, backEnd.or.modelMatrix,
		sizeof( vk.temporal.weaponModelMatrix ) );
	vk.temporal.weaponEntityId = entityId;
	vk.temporal.weaponMatricesValid = qtrue;
}

static qboolean vk_entity_animates_this_frame( const trRefEntity_t *ent )
{
	if ( !ent ) {
		return qfalse;
	}
	if ( ent->e.frame != ent->e.oldframe ) {
		return qtrue;
	}
	if ( ent->e.backlerp > 0.001f ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean vk_draw_uses_gpu_deform_motion( void )
{
	if ( tess.gltfUseGpuPipeline ) {
		return qtrue;
	}
	if ( vk.cmd && vk.cmd->iqm_skin_offset != 0 ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean vk_entity_likely_gpu_deform_motion( const trRefEntity_t *ent )
{
	if ( !ent || ent->e.reType != RT_MODEL || !tr.currentModel ) {
		return qfalse;
	}
	if ( tr.currentModel->type == MOD_GLTF && r_gltfGpu && r_gltfGpu->integer ) {
		return qtrue;
	}
	if ( tr.currentModel->type == MOD_IQM && ent->morphActiveCount > 0 ) {
		const iqmData_t *data = (const iqmData_t *)tr.currentModel->modelData;
		return ( data && data->num_poses > 0 ) ? qtrue : qfalse;
	}
	return qfalse;
}

static qboolean vk_entity_has_gpu_deform_motion( const trRefEntity_t *ent )
{
	if ( vk_draw_uses_gpu_deform_motion() ) {
		return qtrue;
	}
	return vk_entity_likely_gpu_deform_motion( ent );
}

static float vk_motion_origin_dist_sq( const vec3_t a, const vec3_t b )
{
	const float dx = a[0] - b[0];
	const float dy = a[1] - b[1];
	const float dz = a[2] - b[2];
	return dx * dx + dy * dy + dz * dz;
}

static float vk_motion_matrix_origin_delta( const float *a, const float *b )
{
	const float dx = a[12] - b[12];
	const float dy = a[13] - b[13];
	const float dz = a[14] - b[14];
	return sqrtf( dx * dx + dy * dy + dz * dz );
}

static uint32_t vk_motion_hash_u32( uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

static uint32_t vk_motion_make_stable_id( const trRefEntity_t *ent, uint32_t generation )
{
	uint32_t h;

	if ( !ent ) {
		return generation;
	}
	h = (uint32_t)ent->e.hModel;
	h = vk_motion_hash_u32( h ^ (uint32_t)ent->e.reType * 0x9e3779b9u );
	h = vk_motion_hash_u32( h ^ (uint32_t)ent->e.skinNum );
	h = vk_motion_hash_u32( h ^ (uint32_t)ent->e.customSkin );
	h = vk_motion_hash_u32( h ^ (uint32_t)ent->e.customShader );
	h = vk_motion_hash_u32( h ^ (uint32_t)( (int)floorf( ent->e.origin[0] * 0.25f ) ) );
	h = vk_motion_hash_u32( h ^ (uint32_t)( (int)floorf( ent->e.origin[1] * 0.25f ) ) );
	h = vk_motion_hash_u32( h ^ (uint32_t)( (int)floorf( ent->e.origin[2] * 0.25f ) ) );
	return ( h & 0xfffffff0u ) | ( generation & 0xfu );
}

static int vk_motion_find_prev_match( const trRefEntity_t *ent, float *outDist )
{
	int i;
	int best = -1;
	float bestDist = VK_MOTION_TELEPORT_UNITS;
	float teleportLimit = VK_MOTION_TELEPORT_UNITS;

	if ( !ent || vk_motion_prev_count <= 0 ) {
		return -1;
	}

	for ( i = 0; i < vk_motion_prev_count; i++ ) {
		float d;
		if ( !vk_motion_prev[i].valid ) {
			continue;
		}
		if ( vk_motion_prev[i].hModel != ent->e.hModel ) {
			continue;
		}
		if ( vk_motion_prev[i].reType != (int)ent->e.reType ) {
			continue;
		}
		d = sqrtf( vk_motion_origin_dist_sq( ent->e.origin, vk_motion_prev[i].origin ) );
		if ( d < bestDist && d <= teleportLimit ) {
			bestDist = d;
			best = i;
		}
	}

	if ( outDist ) {
		*outDist = ( best >= 0 ) ? bestDist : 1e30f;
	}
	return best;
}

static void vk_motion_fill_nan_matrix( float *m )
{
	int i;
	for ( i = 0; i < 16; i++ ) {
		m[i] = 0.0f / 0.0f;
	}
}

static void vk_motion_resolve_entity( const trRefEntity_t *ent, float *outPrevModel, qboolean *outHavePrev )
{
	int match;
	float matchDist = 1e30f;
	vkEntityMotionRecord_t *curr;
	uint32_t generation = 1u;
	int reason = VK_MOTION_OK;
	qboolean havePrev = qfalse;
	uint32_t prevAge = 0u;
	int dupIndex = -1;
	int i;

	vk_motion_draw_invalid = qfalse;
	vk_motion_draw_stable_id = 0;
	vk_motion_draw_generation = 0;
	vk_motion_draw_invalid_reason = VK_MOTION_OK;
	vk_motion_draw_is_dynamic = qfalse;

	if ( outHavePrev ) {
		*outHavePrev = qfalse;
	}
	if ( !ent || !outPrevModel ) {
		return;
	}

	if ( ent->e.renderfx & RF_FIRST_PERSON ) {
		vk_motion_draw_invalid_reason = VK_MOTION_INVALID_FIRST_PERSON;
		return;
	}

	/*
	 * Phase 5/8 dedupe: multi-stage draws (depth prepass, G-buffer, lighting,
	 * OIT) resolve the same entity several times per frame. Reuse the record
	 * appended by the first stage instead of growing vk_motion_curr — repeated
	 * appends could exhaust MAX_REFENTITIES and starve later entities of
	 * motion records (stale TAA history for whatever draws last).
	 */
	for ( i = vk_motion_curr_count - 1; i >= 0; i-- ) {
		const vkEntityMotionRecord_t *c = &vk_motion_curr[i];
		if ( c->valid && c->hModel == ent->e.hModel && c->reType == (int)ent->e.reType &&
			c->frame == ent->e.frame && c->oldframe == ent->e.oldframe &&
			c->skinNum == ent->e.skinNum && c->customSkin == ent->e.customSkin &&
			c->customShader == ent->e.customShader &&
			VectorCompare( c->origin, ent->e.origin ) ) {
			dupIndex = i;
			break;
		}
	}

	match = vk_motion_find_prev_match( ent, &matchDist );

	if ( match < 0 ) {
		reason = VK_MOTION_INVALID_NO_PREV;
		generation = 1u;
	} else {
		const vkEntityMotionRecord_t *prev = &vk_motion_prev[match];
		generation = prev->generation ? prev->generation : 1u;
		prevAge = ( vk.temporal.frameIndex > prev->frameId ) ?
			( vk.temporal.frameIndex - prev->frameId ) : 0u;
		if ( prevAge != 1u ) {
			/*
			 * Phase 5: the matched "previous" record is not exactly one temporal
			 * frame old (skipped frame, promotion glitch). Reprojecting with it
			 * would scale the object's velocity by its age — reject instead.
			 */
			reason = VK_MOTION_INVALID_STALE_PREV;
			generation++;
		} else if ( prev->customSkin != ent->e.customSkin || prev->customShader != ent->e.customShader ||
			prev->skinNum != ent->e.skinNum ) {
			reason = VK_MOTION_INVALID_SKIN_CHANGE;
			generation++;
		} else if ( matchDist > VK_MOTION_TELEPORT_UNITS * 0.85f ) {
			reason = VK_MOTION_INVALID_TELEPORT;
			generation++;
		} else {
			const float jump = vk_motion_matrix_origin_delta( backEnd.or.modelMatrix, prev->matrix );
			if ( jump > VK_MOTION_JUMP_UNITS ) {
				reason = VK_MOTION_INVALID_TRANSFORM_JUMP;
				generation++;
			} else {
				havePrev = qtrue;
				Com_Memcpy( outPrevModel, prev->matrix, sizeof( float ) * 16 );
			}
		}
	}

	if ( havePrev && vk_entity_animates_this_frame( ent ) && !vk_entity_has_gpu_deform_motion( ent ) ) {
		if ( r_temporalCpuSkinPrev && r_temporalCpuSkinPrev->integer ) {
			havePrev = qfalse;
			reason = VK_MOTION_INVALID_ANIM_NO_POSE;
			generation++;
		} else {
			vk.temporal.unreliableMotionThisFrame = qtrue;
		}
	}

	if ( ent->e.customShader &&
		( !r_temporalCustomShaderMotion || !r_temporalCustomShaderMotion->integer ) ) {
		havePrev = qfalse;
		reason = VK_MOTION_INVALID_CUSTOM_SHADER;
	}

	if ( !havePrev ) {
		vk_motion_draw_invalid = qtrue;
		vk_motion_draw_invalid_reason = reason;
		vk_motion_fill_nan_matrix( outPrevModel );
	} else if ( outHavePrev ) {
		*outHavePrev = qtrue;
	}

	vk_motion_draw_generation = generation;
	vk_motion_draw_stable_id = vk_motion_make_stable_id( ent, generation );
	vk_motion_draw_is_dynamic = qtrue;

	if ( dupIndex >= 0 ) {
		/* Same entity resolved again this frame (later render stage): refresh in place. */
		curr = &vk_motion_curr[dupIndex];
		Com_Memcpy( curr->matrix, backEnd.or.modelMatrix, sizeof( curr->matrix ) );
		curr->generation = generation;
		curr->stableId = vk_motion_draw_stable_id;
		curr->frameId = vk.temporal.frameIndex;
		curr->lastInvalidReason = reason;
		curr->prevAge = prevAge;
	} else if ( vk_motion_curr_count < MAX_REFENTITIES ) {
		curr = &vk_motion_curr[vk_motion_curr_count++];
		Com_Memset( curr, 0, sizeof( *curr ) );
		curr->valid = qtrue;
		VectorCopy( ent->e.origin, curr->origin );
		Com_Memcpy( curr->matrix, backEnd.or.modelMatrix, sizeof( curr->matrix ) );
		curr->hModel = ent->e.hModel;
		curr->reType = (int)ent->e.reType;
		curr->skinNum = ent->e.skinNum;
		curr->customSkin = ent->e.customSkin;
		curr->customShader = ent->e.customShader;
		curr->frame = ent->e.frame;
		curr->oldframe = ent->e.oldframe;
		curr->generation = generation;
		curr->stableId = vk_motion_draw_stable_id;
		curr->frameId = vk.temporal.frameIndex;
		curr->lastInvalidReason = reason;
		curr->prevAge = prevAge;
	} else {
		static uint32_t warnedFrame = ~0u;
		if ( warnedFrame != vk.temporal.frameIndex ) {
			warnedFrame = vk.temporal.frameIndex;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][temporal] motion record table full (%d) — later entities lose object velocity this frame\n"
				S_COLOR_WHITE, MAX_REFENTITIES );
		}
	}

	if ( r_temporalDebug && r_temporalDebug->integer >= 2 && vk_motion_draw_invalid ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][temporal] entity motion invalid reason=%d hModel=%d gen=%u dist=%.1f\n",
			reason, ent->e.hModel, generation, matchDist );
	}
}

/*
 * Phase 7: rebase the previous frame's embedded projection jitter onto the
 * current frame's jitter. Both current and previous clip positions then carry
 * the SAME constant clip-space offset, which cancels exactly in
 * out_motion = currUV - prevUV — motion vectors stay jitter-free without
 * maintaining a second, non-jittered matrix set.
 */
static void vk_motion_rebase_prev_projection_jitter( float *prev_proj_gl )
{
	float jx = 0.0f, jy = 0.0f;
	uint32_t w, h;

	if ( !R_Upscale_WantTemporal() || !vk_prev_jitter_valid ) {
		return;
	}
	R_Upscale_GetJitter( &jx, &jy );
	if ( jx == vk_prev_jitter_x && jy == vk_prev_jitter_y ) {
		return;
	}
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) w = 1u;
	if ( h < 1u ) h = 1u;
	prev_proj_gl[8] += ( 2.0f * ( jx - vk_prev_jitter_x ) ) / (float)w;
	prev_proj_gl[9] += ( 2.0f * ( jy - vk_prev_jitter_y ) ) / (float)h;
}

static void vk_get_prev_mvp_transform( float *prev_mvp )
{
	float prev_model[16];
	float prev_model_view[16];
	float prev_proj_gl[16];
	float prev_proj[16];
	qboolean havePrev = qfalse;

	if ( backEnd.projection2D || !vk_prev_matrices_valid ) {
		vk_get_mvp_transform( prev_mvp );
		return;
	}

	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON ) &&
		vk.temporal.weaponMatricesHavePrev ) {
		myGlMultMatrix( vk.temporal.weaponPrevModelMatrix, vk.temporal.weaponPrevViewMatrix, prev_model_view );
		Com_Memcpy( prev_proj_gl, vk.temporal.weaponPrevProjectionMatrix, sizeof( prev_proj_gl ) );
		vk_motion_rebase_prev_projection_jitter( prev_proj_gl );
		vk_get_projection_matrix_vk( prev_proj_gl, prev_proj );
		myGlMultMatrix( prev_model_view, prev_proj, prev_mvp );
		vk_motion_draw_invalid = qfalse;
		vk_motion_draw_is_dynamic = qtrue;
		return;
	}

	Com_Memcpy( prev_model, backEnd.or.modelMatrix, sizeof( prev_model ) );
	vk_motion_draw_invalid = qfalse;
	vk_motion_draw_is_dynamic = qfalse;

	if ( backEnd.currentEntity && backEnd.currentEntity->e.reType == RT_MODEL &&
		!( backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON ) ) {
		vk_motion_resolve_entity( backEnd.currentEntity, prev_model, &havePrev );
	}

	myGlMultMatrix( prev_model, vk_prev_view_matrix, prev_model_view );
	Com_Memcpy( prev_proj_gl, vk_prev_projection_matrix, sizeof( prev_proj_gl ) );
	vk_motion_rebase_prev_projection_jitter( prev_proj_gl );
	vk_get_projection_matrix_vk( prev_proj_gl, prev_proj );
	myGlMultMatrix( prev_model_view, prev_proj, prev_mvp );
}

void vk_update_mvp( const float *m )
{
	vkMvpPushConstants_t push_constants;
	vkOitPushConstants_t oit_push;
	VkPipelineLayout layout;
	VkShaderStageFlags stage_flags;
	uint32_t push_bytes;
	qboolean oit_layout = qfalse;

	Com_Memset( &push_constants, 0, sizeof( push_constants ) );
	Com_Memset( &oit_push, 0, sizeof( oit_push ) );

	if ( m ) {
		Com_Memcpy( push_constants.mvp, m, sizeof( push_constants.mvp ) );
		Com_Memcpy( oit_push.mvp, m, sizeof( oit_push.mvp ) );
	} else {
		vk_get_mvp_transform( push_constants.mvp );
		vk_get_mvp_transform( oit_push.mvp );
	}
	vk_get_prev_mvp_transform( push_constants.prev_mvp );
	Com_Memcpy( oit_push.prev_mvp, push_constants.prev_mvp, sizeof( oit_push.prev_mvp ) );
	Com_Memcpy( oit_push.model, backEnd.or.modelMatrix, sizeof( oit_push.model ) );
	oit_push.lightingDebug = ri.Cvar_VariableIntegerValue( "r_oitLightingDebug" );
	oit_push.parityCompare = ri.Cvar_VariableIntegerValue( "r_oitParityCompare" );
	{
		/* IQ P0-E: with WBOIT/MBOIT active, never push fogMode 0 into accum (double-fog). */
		const int fogMode = ri.Cvar_VariableIntegerValue( "r_oitFogMode" );
		oit_push.fogMode = ( r_oit && r_oit->integer >= 1 && fogMode < 1 ) ? 1 : fogMode;
	}
	oit_push.fogDebug = ri.Cvar_VariableIntegerValue( "r_oitFogDebug" );
	{
		cvar_t *fogDen = ri.Cvar_Get( "r_oitFogDensity", "0.0", 0 );
		oit_push.fogDensity = fogDen ? fogDen->value : 0.0f;
	}
	/* Default: trust texture×vertex alpha. Soften when Q3 glass leaves alpha≈1. */
	oit_push.coverageScale = 1.0f;
	if ( tess.shader && tess.shader->sort >= SS_BLEND0 && tess.shader->name[0] ) {
		if ( Q_stristr( tess.shader->name, "glass" ) || Q_stristr( tess.shader->name, "window" ) ||
			Q_stristr( tess.shader->name, "trans" ) ) {
			oit_push.coverageScale = 0.32f;
		} else if ( Q_stristr( tess.shader->name, "water" ) || Q_stristr( tess.shader->name, "slime" ) ||
			Q_stristr( tess.shader->name, "lava" ) ) {
			oit_push.coverageScale = 0.48f;
		}
	}
	/* Sun term for WBOIT (Forward+ tiles lack directional). Unshadowed — CSM bind is next. */
	{
		vec3_t sunDir;
		float sunLen;
		VectorCopy( tr.sunDirection, sunDir );
		sunLen = VectorLength( sunDir );
		if ( sunLen > 1e-4f ) {
			VectorScale( sunDir, 1.0f / sunLen, sunDir );
		} else {
			sunDir[0] = 0.45f;
			sunDir[1] = 0.3f;
			sunDir[2] = 0.9f;
			VectorNormalize( sunDir );
		}
		oit_push.sunDir[0] = sunDir[0];
		oit_push.sunDir[1] = sunDir[1];
		oit_push.sunDir[2] = sunDir[2];
		oit_push.sunColor[0] = tr.sunLight[0];
		oit_push.sunColor[1] = tr.sunLight[1];
		oit_push.sunColor[2] = tr.sunLight[2];
		if ( oit_push.sunColor[0] + oit_push.sunColor[1] + oit_push.sunColor[2] < 1e-4f ) {
			oit_push.sunColor[0] = oit_push.sunColor[1] = oit_push.sunColor[2] = 1.0f;
		}
		oit_push.sunStrength = ( !R_ClassicLightingActive() && r_pbrSunShadow && r_pbrSunShadow->integer )
			? ( ( r_pbrSunShadowStrength ) ? Com_Clamp( 0.0f, 1.0f, r_pbrSunShadowStrength->value ) : 1.0f )
			: 0.65f;
		oit_push.sunAmbient = 0.28f;
		oit_push.cascadeCount = (float)( ( vk.sun_shadow_cascade_count > 0u ) ?
			( ( vk.sun_shadow_cascade_count > 4u ) ? 4u : vk.sun_shadow_cascade_count ) : 1u );
		{
			materialTransparencyInfo_t ainfo;
			oitSourceAlphaEncoding_t enc = OIT_SOURCE_ALPHA_STRAIGHT;
			if ( tess.shader ) {
				vk_oit_alpha_query_shader( tess.shader, &ainfo );
				enc = ainfo.sourceEncoding;
				vk_oit_alpha_note_route( enc, ainfo.path );
			}
			oit_push.alphaPack = vk_oit_alpha_pack_push( enc );
		}
	}
	vk_capture_weapon_matrices();
	push_constants.reserved[0] = ( tess.sdfUiEdge >= 0.0f ) ? tess.sdfUiEdge : 0.0f;
	if ( r_sdfScreenAa ) {
		push_constants.reserved[1] = Com_Clamp( 0.0f, 8.0f, r_sdfScreenAa->value );
	} else {
		push_constants.reserved[1] = 2.0f;
	}
	if ( tess.vectorCurveCount > 0 ) {
		push_constants.reserved[0] = (float)tess.vectorCurveStart;
		push_constants.reserved[1] = (float)tess.vectorCurveCount;
		push_constants.reserved[2] = (float)tess.vectorCurveTexWidth;
		push_constants.reserved[3] = 0.0f;
	} else if ( tess.subpixelShift >= 0.0f ) {
		push_constants.reserved[0] = tess.subpixelShift;
		push_constants.reserved[1] = tess.subpixelInvTexWidth;
		if ( r_fontGamma ) {
			push_constants.reserved[2] = Com_Clamp( 0.5f, 3.0f, r_fontGamma->value );
		} else {
			push_constants.reserved[2] = 1.0f;
		}
		if ( r_fontLcdWeight ) {
			push_constants.reserved[3] = Com_Clamp( 0.0f, 1.0f, r_fontLcdWeight->value );
		} else {
			push_constants.reserved[3] = 0.35f;
		}
	} else if ( tess.sdfUiEdge >= 0.0f ) {
		if ( r_sdfOutline ) {
			push_constants.reserved[2] = (float)r_sdfOutline->integer;
		}
		if ( r_sdfOutlineWidth ) {
			push_constants.reserved[3] = Com_Clamp( 0.01f, 0.25f, r_sdfOutlineWidth->value );
		}
		if ( r_fontGamma ) {
			push_constants.reserved[4] = Com_Clamp( 0.5f, 3.0f, r_fontGamma->value );
		} else {
			push_constants.reserved[4] = 1.0f;
		}
	}

	/* Stochastic alpha-clipped materials: reserved[6]=mode, reserved[7]=frame seed.
	 * Mode 2 (temporal hash) requires Temporal Reconstruction; otherwise fall back to
	 * screen-space hash (1) so coverage is not frozen without history. */
	if ( r_stochasticAlpha && r_stochasticAlpha->integer > 0 ) {
		int stochMode = r_stochasticAlpha->integer;
		if ( stochMode >= 2 && !vk_temporal_reconstruction_wanted() ) {
			stochMode = 1;
		}
		push_constants.reserved[6] = (float)stochMode;
		push_constants.reserved[7] = (float)( tr.frameCount & 1023 );
	} else if ( vk_frequency_aware_alpha_coverage() && tess.sdfUiEdge < 0.0f && tess.subpixelShift < 0.0f &&
		tess.vectorCurveCount <= 0 ) {
		/* Raster Ultra 1.12: coverage-preserving alpha via stoch_r4 (reserved[4]).
		 * Skip UI/SDF/vector font pushes that own reserved[0..4]. */
		push_constants.reserved[4] = 1.0f;
	}
	/* Visibility PrimID MRT: reserved[5] = monotonic draw id. */
	if ( vk.visibilityBufferDirectExport ) {
		push_constants.reserved[5] = (float)( backEnd.visDrawId++ );
	}

	/*
	 * Dynamic-object motion meta for fragment shaders (when UI/SDF/vector do not
	 * own reserved[2..3]): reserved[2]=motionInvalid, reserved[3]=objectId bits.
	 * NaN prevClip is the primary invalid signal; these floats aid debug / ID stamp.
	 */
	if ( tess.sdfUiEdge < 0.0f && tess.subpixelShift < 0.0f && tess.vectorCurveCount <= 0 &&
		vk_motion_draw_is_dynamic ) {
		push_constants.reserved[2] = vk_motion_draw_invalid ? 1.0f : 0.0f;
		/* reserved[3] = 16-bit stable object id (float bits) — only when the identity
		 * buffer is allocated, so disabled frames leave id 0 and gen_frag never stamps. */
		if ( vk_object_id_active() ) {
			union { uint32_t u; float f; } idBits;
			idBits.u = ( vk_motion_draw_stable_id & 0xFFFFu );
			if ( idBits.u == 0u ) {
				idBits.u = 1u; /* reserve 0 for background; avoid a live object hashing to 0 */
			}
			push_constants.reserved[3] = idBits.f;
		}
	}

	layout = vk.pipeline_layout;
	if ( backEnd.oitMomentsPass && vk.pipeline_layout_oit_moments != VK_NULL_HANDLE ) {
		layout = vk.pipeline_layout_oit_moments;
		oit_layout = qtrue;
	} else if ( backEnd.oitAccumPass && r_oit && r_oit->integer == 2 &&
		backEnd.oitBucketFilter != 2 &&
		vk.pipeline_layout_oit_accum_mboit != VK_NULL_HANDLE ) {
		layout = vk.pipeline_layout_oit_accum_mboit;
		oit_layout = qtrue;
	} else if ( backEnd.oitAccumPass && vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) {
		layout = vk.pipeline_layout_oit_accum;
		oit_layout = qtrue;
	}
	/*
	 * Pipeline layouts declare this push range for VERTEX|FRAGMENT (vk_init_device.c).
	 * Stages omitted from vkCmdPushConstants do not receive the update (Vulkan spec);
	 * frag_ui_sdf_text.frag reads sdfEdgeSmooth (reserved[0]) and r_sdfScreenAa (reserved[1]) after the two mat4s.
	 */
	stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	if ( oit_layout ) {
		push_bytes = (uint32_t)sizeof( oit_push );
		qvkCmdPushConstants( vk.cmd->command_buffer, layout, stage_flags, 0, push_bytes, &oit_push );
	} else {
		push_bytes = (uint32_t)sizeof( push_constants );
		qvkCmdPushConstants( vk.cmd->command_buffer, layout, stage_flags, 0, push_bytes, &push_constants );
	}

	vk.stats.push_size += push_bytes;
}

void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		vk_get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		vk_get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}

	if ( !backEnd.projection2D &&
		( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ) {
		VkRect2D r;
		uint32_t maxW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
		uint32_t maxH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
		uint64_t area;
		uint64_t bestArea;

		vk_get_viewport_rect( &r );

		if ( r.offset.x < 0 ) {
			int dx = -r.offset.x;
			r.offset.x = 0;
			r.extent.width = ( r.extent.width > (uint32_t)dx ) ? ( r.extent.width - (uint32_t)dx ) : 0u;
		}
		if ( r.offset.y < 0 ) {
			int dy = -r.offset.y;
			r.offset.y = 0;
			r.extent.height = ( r.extent.height > (uint32_t)dy ) ? ( r.extent.height - (uint32_t)dy ) : 0u;
		}
		if ( (uint32_t)r.offset.x >= maxW || (uint32_t)r.offset.y >= maxH ) {
			return;
		}
		if ( (uint32_t)r.offset.x + r.extent.width > maxW ) {
			r.extent.width = maxW - (uint32_t)r.offset.x;
		}
		if ( (uint32_t)r.offset.y + r.extent.height > maxH ) {
			r.extent.height = maxH - (uint32_t)r.offset.y;
		}

		area = (uint64_t)r.extent.width * (uint64_t)r.extent.height;
		if ( area == 0 ) {
			return;
		}
		bestArea = vk_scene_src_rect_valid ? (uint64_t)vk_scene_src_rect.extent.width * (uint64_t)vk_scene_src_rect.extent.height : 0u;
		if ( !vk_scene_src_rect_valid || area > bestArea ) {
			vk_scene_src_rect = r;
			vk_scene_src_rect_valid = qtrue;
		}
	}
}

void vk_read_mvp_transform( float *mvp )
{
	vk_get_mvp_transform( mvp );
}

void vk_read_prev_mvp_transform( float *prev_mvp )
{
	vk_get_prev_mvp_transform( prev_mvp );
}
