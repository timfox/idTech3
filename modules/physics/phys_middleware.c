/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_bullet.h"
#include "phys_events.h"
#include "phys_materials.h"
#include "phys_motor.h"
#include "phys_procedural_anim.h"
#include "phys_props.h"
#include "phys_volumes.h"
#include "phys_cloth.h"
#include "phys_solvers.h"
#include "phys_particles.h"
#include "phys_softblob.h"
#include "phys_fluid.h"
#include "phys_middleware.h"

#define PHYS_DEMO_RAGDOLL_MAX 8
#define PHYS_DEMO_PROP_MAX    64
#define PHYS_DEMO_VOLUME_MAX  16
#define PHYS_DEMO_CLOTH_MAX   8
#define PHYS_DEMO_ROPE_MAX    8

typedef struct physDemoRagdoll_s {
	qboolean            active;
	physRagdollHandle_t ragdoll;
	procAnimHandle_t    anim;
	physMotorHandle_t   motor;
} physDemoRagdoll_t;

static qboolean middlewareReady;
static physDemoRagdoll_t demoRagdolls[PHYS_DEMO_RAGDOLL_MAX];
static int demoRagdollLast = -1;
static physBodyHandle_t demoProps[PHYS_DEMO_PROP_MAX];
static int demoPropCount;
static physVolumeHandle_t demoVolumes[PHYS_DEMO_VOLUME_MAX];
static int demoVolumeCount;
static physShadowHandle_t demoShadow = -1;
static clothHandle_t demoCloths[PHYS_DEMO_CLOTH_MAX];
static int demoClothCount;
static physBodyHandle_t demoRopeAnchors[PHYS_DEMO_ROPE_MAX];
static int demoRopeCount;

static void PhysMiddleware_ClearDemoSlot( physDemoRagdoll_t *slot ) {
	if ( !slot || !slot->active ) {
		return;
	}
	if ( slot->motor >= 0 ) {
		PhysMotor_Destroy( slot->motor );
	}
	if ( slot->anim >= 0 ) {
		ProcAnim_Destroy( slot->anim );
	}
	if ( slot->ragdoll >= 0 ) {
		Phys_DestroyRagdoll( slot->ragdoll );
	}
	Com_Memset( slot, 0, sizeof( *slot ) );
	slot->ragdoll = -1;
	slot->anim = -1;
	slot->motor = -1;
}

static void PhysMiddleware_ClearAllDemoRagdolls( void ) {
	int i;

	for ( i = 0; i < PHYS_DEMO_RAGDOLL_MAX; i++ ) {
		PhysMiddleware_ClearDemoSlot( &demoRagdolls[i] );
	}
	demoRagdollLast = -1;
}

static void PhysMiddleware_LogImpact( const phys_event_t *ev, void *userData ) {
	(void)userData;
	if ( !ev ) {
		return;
	}
	if ( ev->type == PHYS_EVENT_IMPACT && ev->magnitude > 50.0f ) {
		Com_DPrintf( "PhysEvent impact: mag=%.1f matA=%d matB=%d\n",
			ev->magnitude, ev->matA, ev->matB );
	}
}

static void PhysMiddleware_Status_f( void ) {
	int demoActive = 0;
	int i;

	for ( i = 0; i < PHYS_DEMO_RAGDOLL_MAX; i++ ) {
		if ( demoRagdolls[i].active ) {
			demoActive++;
		}
	}

	Com_Printf( "Physics middleware status:\n" );
	Com_Printf( "  backend:      %s\n", Phys_GetBackendName() );
	Com_Printf( "  workers:      %d\n", Phys_GetWorkerCount() );
	Com_Printf( "  bodies:       %d\n", Phys_GetBodyCount() );
	Com_Printf( "  constraints:  %d\n", Phys_GetConstraintCount() );
	Com_Printf( "  ragdolls:     %d\n", Phys_GetRagdollCount() );
	Com_Printf( "  shadows:      %d\n", PhysProp_GetShadowCount() );
	Com_Printf( "  volumes:      %d\n", PhysVolume_GetActiveCount() );
	Com_Printf( "  procAnim:     %d active\n", ProcAnim_GetActiveCount() );
	Com_Printf( "  motors:       %d active\n", PhysMotor_GetActiveCount() );
	Com_Printf( "  cloth:        %d active\n", Cloth_GetActiveCount() );
	Com_Printf( "  particles:    %d active\n", PhysParticles_GetActiveCount() );
	Com_Printf( "  softblobs:    %d active\n", SoftBlob_GetActiveCount() );
	Com_Printf( "  fluid:        %d particles\n", PhysFluid_GetActiveCount() );
	Com_Printf( "  solvers:\n" );
	{
		int s;
		for ( s = 0; s < PhysSolvers_GetCount(); s++ ) {
			const char *name = PhysSolvers_GetName( s );
			Com_Printf( "    - %s: %s (active=%d)\n", name,
				PhysSolvers_IsEnabled( name ) ? "on" : "off",
				PhysSolvers_GetActiveCount( name ) );
		}
	}
	Com_Printf( "  demo ragdolls:%d / %d (last=%d)\n", demoActive, PHYS_DEMO_RAGDOLL_MAX, demoRagdollLast );
	Com_Printf( "  demo props:   %d\n", demoPropCount );
	Com_Printf( "  demo cloth:   %d\n", demoClothCount );
	Com_Printf( "  demo ropes:   %d\n", demoRopeCount );
	Com_Printf( "  event queue:  %d pending\n", PhysEvent_QueueDepth() );
}

static void PhysMiddleware_TrackProp( physBodyHandle_t body ) {
	if ( body < 0 || demoPropCount >= PHYS_DEMO_PROP_MAX ) {
		return;
	}
	demoProps[demoPropCount++] = body;
}

static void PhysMiddleware_ClearDemoProps( void ) {
	int i;

	for ( i = 0; i < demoPropCount; i++ ) {
		if ( demoProps[i] >= 0 ) {
			Phys_DestroyBody( demoProps[i] );
		}
	}
	demoPropCount = 0;
	if ( demoShadow >= 0 ) {
		PhysProp_DestroyShadow( demoShadow );
		demoShadow = -1;
	}
	for ( i = 0; i < demoVolumeCount; i++ ) {
		if ( demoVolumes[i] >= 0 ) {
			PhysVolume_Destroy( demoVolumes[i] );
		}
	}
	demoVolumeCount = 0;
}

static void PhysMiddleware_SpawnBox_f( void ) {
	vec3_t origin, half;
	float mass;
	physBodyHandle_t body;

	VectorSet( origin, 0.0f, 0.0f, 64.0f );
	VectorSet( half, 8.0f, 8.0f, 8.0f );
	mass = 20.0f;
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		mass = (float)atof( Cmd_Argv( 4 ) );
	}
	body = PhysProp_CreateBox( origin, half, PHYS_BODY_DYNAMIC, mass, PHYS_MAT_WOOD );
	PhysMiddleware_TrackProp( body );
	Com_Printf( "phys_spawn_box: body=%d mass=%.1f at (%.0f %.0f %.0f)\n",
		body, mass, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnSphere_f( void ) {
	vec3_t origin;
	float radius, mass;
	physBodyHandle_t body;

	VectorSet( origin, 0.0f, 0.0f, 64.0f );
	radius = 10.0f;
	mass = 15.0f;
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		radius = (float)atof( Cmd_Argv( 4 ) );
	}
	body = PhysProp_CreateSphere( origin, radius, PHYS_BODY_DYNAMIC, mass, PHYS_MAT_METAL );
	PhysMiddleware_TrackProp( body );
	Com_Printf( "phys_spawn_sphere: body=%d r=%.1f at (%.0f %.0f %.0f)\n",
		body, radius, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnStatic_f( void ) {
	vec3_t origin, half;
	physBodyHandle_t body;

	VectorSet( origin, 0.0f, 0.0f, 0.0f );
	VectorSet( half, 32.0f, 32.0f, 4.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	body = PhysProp_CreateBox( origin, half, PHYS_BODY_STATIC, 0.0f, PHYS_MAT_CONCRETE );
	PhysMiddleware_TrackProp( body );
	Com_Printf( "phys_spawn_static: body=%d at (%.0f %.0f %.0f)\n",
		body, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnShadow_f( void ) {
	physShadowDef_t def;

	Com_Memset( &def, 0, sizeof( def ) );
	VectorSet( def.origin, 0.0f, 0.0f, 32.0f );
	VectorSet( def.halfExtents, 16.0f, 16.0f, 32.0f );
	def.shape = PHYS_SHAPE_BOX;
	def.entityNum = -1;
	def.allowMovement = qtrue;
	def.allowRotation = qtrue;
	if ( Cmd_Argc() >= 4 ) {
		def.origin[0] = (float)atof( Cmd_Argv( 1 ) );
		def.origin[1] = (float)atof( Cmd_Argv( 2 ) );
		def.origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( demoShadow >= 0 ) {
		PhysProp_DestroyShadow( demoShadow );
	}
	demoShadow = PhysProp_CreateShadow( &def );
	Com_Printf( "phys_spawn_shadow: shadow=%d body=%d (move with phys_shadow_pose)\n",
		demoShadow, PhysProp_GetShadowBody( demoShadow ) );
}

static void PhysMiddleware_ShadowPose_f( void ) {
	vec3_t origin, angles;

	if ( demoShadow < 0 ) {
		Com_Printf( "phys_shadow_pose: spawn a shadow first\n" );
		return;
	}
	VectorSet( origin, 0.0f, 0.0f, 32.0f );
	VectorSet( angles, 0.0f, 0.0f, 0.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 7 ) {
		angles[0] = (float)atof( Cmd_Argv( 4 ) );
		angles[1] = (float)atof( Cmd_Argv( 5 ) );
		angles[2] = (float)atof( Cmd_Argv( 6 ) );
	}
	PhysProp_SetShadowPose( demoShadow, origin, angles );
	Com_Printf( "phys_shadow_pose: (%.0f %.0f %.0f) angles (%.0f %.0f %.0f)\n",
		origin[0], origin[1], origin[2], angles[0], angles[1], angles[2] );
}

static void PhysMiddleware_ImpulseSphere_f( void ) {
	vec3_t center;
	float radius, magnitude;
	int n;

	VectorSet( center, 0.0f, 0.0f, 32.0f );
	radius = 128.0f;
	magnitude = 800.0f;
	if ( Cmd_Argc() >= 4 ) {
		center[0] = (float)atof( Cmd_Argv( 1 ) );
		center[1] = (float)atof( Cmd_Argv( 2 ) );
		center[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		radius = (float)atof( Cmd_Argv( 4 ) );
	}
	if ( Cmd_Argc() >= 6 ) {
		magnitude = (float)atof( Cmd_Argv( 5 ) );
	}
	n = Phys_ApplyImpulseRadius( center, radius, magnitude, 1.0f );
	Com_Printf( "phys_impulse_sphere: hit %d bodies (r=%.0f mag=%.0f)\n", n, radius, magnitude );
}

static void PhysMiddleware_SpawnBuoyancy_f( void ) {
	physVolumeDef_t def;
	physVolumeHandle_t h;

	Com_Memset( &def, 0, sizeof( def ) );
	def.type = PHYS_VOLUME_BUOYANCY;
	VectorSet( def.center, 0.0f, 0.0f, 0.0f );
	VectorSet( def.halfExtents, 128.0f, 128.0f, 48.0f );
	def.density = 1.0f;
	def.linearDrag = 0.4f;
	def.angularDrag = 0.2f;
	if ( Cmd_Argc() >= 4 ) {
		def.center[0] = (float)atof( Cmd_Argv( 1 ) );
		def.center[1] = (float)atof( Cmd_Argv( 2 ) );
		def.center[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	h = PhysVolume_Create( &def );
	if ( h >= 0 && demoVolumeCount < PHYS_DEMO_VOLUME_MAX ) {
		demoVolumes[demoVolumeCount++] = h;
	}
	Com_Printf( "phys_spawn_buoyancy: volume=%d\n", h );
}

static void PhysMiddleware_Debug_f( void ) {
	cvar_t *cv = Cvar_Get( "phys_debugDraw", "0", CVAR_ARCHIVE );
	int on = cv->integer ? 0 : 1;
	Cvar_Set( "phys_debugDraw", on ? "1" : "0" );
	Com_Printf( "phys_debug: collision wireframe %s\n", on ? "ON" : "OFF" );
}

static void PhysMiddleware_ClearProps_f( void ) {
	PhysMiddleware_ClearDemoProps();
	Com_Printf( "phys_clear_props: demo props / shadow / volumes cleared\n" );
}

/*
===============
phys_spawn_ragdoll [x y z]
Creates Bullet ragdoll + ProcAnim + motor for console demos.
===============
*/
static void PhysMiddleware_SpawnRagdoll_f( void ) {
	physRagdollDef_t def;
	procAnimConfig_t cfg;
	physDemoRagdoll_t *slot = NULL;
	int i;
	vec3_t origin;

	if ( !middlewareReady ) {
		Com_Printf( "PhysMiddleware not ready (phys_enabled?)\n" );
		return;
	}

	VectorSet( origin, 0.0f, 0.0f, 64.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}

	for ( i = 0; i < PHYS_DEMO_RAGDOLL_MAX; i++ ) {
		if ( !demoRagdolls[i].active ) {
			slot = &demoRagdolls[i];
			demoRagdollLast = i;
			break;
		}
	}
	if ( !slot ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_ragdoll: no free demo slots (max %d)\n", PHYS_DEMO_RAGDOLL_MAX );
		return;
	}

	Com_Memset( &def, 0, sizeof( def ) );
	VectorCopy( origin, def.rootPosition );
	def.scale = 1.0f;
	def.entityNum = -1;
	/* <=0 so Phys_CreateRagdoll applies phys_ragdoll_* cvars */
	def.jointStiffness = 0.0f;
	def.jointDamping = 0.0f;
	def.muscleStrength = 0.0f;
	def.limbMass = 5.0f;

	slot->ragdoll = Phys_CreateRagdoll( &def );
	if ( slot->ragdoll < 0 ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_ragdoll: Phys_CreateRagdoll failed\n" );
		return;
	}

	ProcAnim_DefaultConfig( &cfg );
	slot->anim = ProcAnim_Create( slot->ragdoll, &cfg );
	if ( slot->anim < 0 ) {
		Phys_DestroyRagdoll( slot->ragdoll );
		slot->ragdoll = -1;
		Com_Printf( S_COLOR_YELLOW "phys_spawn_ragdoll: ProcAnim_Create failed\n" );
		return;
	}

	slot->motor = PhysMotor_Create( slot->anim, slot->ragdoll );
	if ( slot->motor < 0 ) {
		ProcAnim_Destroy( slot->anim );
		Phys_DestroyRagdoll( slot->ragdoll );
		slot->anim = -1;
		slot->ragdoll = -1;
		Com_Printf( S_COLOR_YELLOW "phys_spawn_ragdoll: PhysMotor_Create failed\n" );
		return;
	}

	slot->active = qtrue;
	Com_Printf( "phys_spawn_ragdoll: slot %d ragdoll=%d anim=%d motor=%d at (%.0f %.0f %.0f)\n",
		demoRagdollLast, slot->ragdoll, slot->anim, slot->motor,
		origin[0], origin[1], origin[2] );
}

/*
===============
phys_hit_ragdoll [slot] [ix iy iz]
Applies an impulse through PhysMiddleware_DispatchHit.
Default: last spawned slot, impulse (200, 0, 100) on spine.
===============
*/
static void PhysMiddleware_HitRagdoll_f( void ) {
	physDemoRagdoll_t *slot;
	int slotIdx;
	int bone;
	vec3_t impulse;
	vec3_t point;
	physTransform_t xf;

	if ( !middlewareReady ) {
		Com_Printf( "PhysMiddleware not ready (phys_enabled?)\n" );
		return;
	}

	slotIdx = demoRagdollLast;
	VectorSet( impulse, 200.0f, 0.0f, 100.0f );
	bone = PROCANIM_BONE_SPINE;

	if ( Cmd_Argc() >= 2 ) {
		slotIdx = atoi( Cmd_Argv( 1 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		impulse[0] = (float)atof( Cmd_Argv( 2 ) );
		impulse[1] = (float)atof( Cmd_Argv( 3 ) );
		impulse[2] = (float)atof( Cmd_Argv( 4 ) );
	}
	if ( Cmd_Argc() >= 6 ) {
		bone = atoi( Cmd_Argv( 5 ) );
	}

	if ( slotIdx < 0 || slotIdx >= PHYS_DEMO_RAGDOLL_MAX || !demoRagdolls[slotIdx].active ) {
		Com_Printf( S_COLOR_YELLOW "phys_hit_ragdoll: invalid slot %d (spawn first)\n", slotIdx );
		return;
	}

	slot = &demoRagdolls[slotIdx];
	Phys_RagdollGetBoneTransform( slot->ragdoll, bone, &xf );
	VectorCopy( xf.position, point );
	PhysMiddleware_DispatchHit( -1, slot->anim, slot->motor, bone, 0, point, impulse );
	Phys_RagdollApplyImpact( slot->ragdoll, point, impulse, 40.0f );
	Com_Printf( "phys_hit_ragdoll: slot %d bone %d impulse (%.0f %.0f %.0f)\n",
		slotIdx, bone, impulse[0], impulse[1], impulse[2] );
}

static void PhysMiddleware_SpawnCompound_f( void ) {
	float centers[9];
	float halves[9];
	vec3_t origin;
	physBodyHandle_t body;

	VectorSet( origin, 0.0f, 0.0f, 0.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	/* L-shaped static compound: base + upright + wing. */
	centers[0] = origin[0];       centers[1] = origin[1];       centers[2] = origin[2];
	halves[0] = 48.0f;            halves[1] = 16.0f;            halves[2] = 8.0f;
	centers[3] = origin[0] + 32;  centers[4] = origin[1];       centers[5] = origin[2] + 24;
	halves[3] = 16.0f;            halves[4] = 16.0f;            halves[5] = 24.0f;
	centers[6] = origin[0];       centers[7] = origin[1] + 32;  centers[8] = origin[2];
	halves[6] = 16.0f;            halves[7] = 16.0f;            halves[8] = 8.0f;

	body = Phys_AddStaticCompoundBoxes( centers, halves, 3 );
	PhysMiddleware_TrackProp( body );
	Com_Printf( "phys_spawn_compound: body=%d (3 hulls) at (%.0f %.0f %.0f)\n",
		body, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnCloth_f( void ) {
	vec3_t origin, pinOff;
	clothConfig_t cfg;
	clothHandle_t h;
	int w = 12, ht = 12;
	float spacing = 4.0f;

	VectorSet( origin, 0.0f, 0.0f, 96.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( demoClothCount >= PHYS_DEMO_CLOTH_MAX ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_cloth: demo cloth slots full\n" );
		return;
	}
	Cloth_DefaultConfig( &cfg );
	cfg.windStrength = 40.0f;
	VectorSet( cfg.windDirection, 1.0f, 0.2f, 0.1f );
	h = Cloth_Create( w, ht, origin, spacing, &cfg );
	if ( h < 0 ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_cloth: create failed\n" );
		return;
	}
	VectorClear( pinOff );
	Cloth_PinEdge( h, 0, pinOff ); /* top edge pinned */
	demoCloths[demoClothCount++] = h;
	Com_Printf( "phys_spawn_cloth: cloth=%d %dx%d at (%.0f %.0f %.0f) (XPBD soft, collides with Phys_* world)\n",
		h, w, ht, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnRope_f( void ) {
	vec3_t start, end, dir, pos;
	physBodyDef_t def;
	physConstraintDef_t cdef;
	physBodyHandle_t prev = -1;
	physBodyHandle_t body;
	int segments = 8;
	int i;
	float len, step, radius = 3.0f;

	VectorSet( start, 0.0f, 0.0f, 128.0f );
	VectorSet( end, 0.0f, 0.0f, 32.0f );
	if ( Cmd_Argc() >= 4 ) {
		start[0] = (float)atof( Cmd_Argv( 1 ) );
		start[1] = (float)atof( Cmd_Argv( 2 ) );
		start[2] = (float)atof( Cmd_Argv( 3 ) );
		VectorCopy( start, end );
		end[2] -= 96.0f;
	}
	if ( Cmd_Argc() >= 7 ) {
		end[0] = (float)atof( Cmd_Argv( 4 ) );
		end[1] = (float)atof( Cmd_Argv( 5 ) );
		end[2] = (float)atof( Cmd_Argv( 6 ) );
	}
	if ( demoRopeCount >= PHYS_DEMO_ROPE_MAX ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_rope: demo rope slots full\n" );
		return;
	}

	VectorSubtract( end, start, dir );
	len = VectorNormalize( dir );
	if ( len < 8.0f ) {
		len = 96.0f;
		VectorSet( dir, 0.0f, 0.0f, -1.0f );
	}
	step = len / (float)segments;

	for ( i = 0; i <= segments; i++ ) {
		Com_Memset( &def, 0, sizeof( def ) );
		def.shape = PHYS_SHAPE_SPHERE;
		def.radius = radius;
		def.friction = 0.4f;
		def.restitution = 0.1f;
		VectorMA( start, step * (float)i, dir, pos );
		VectorCopy( pos, def.position );
		if ( i == 0 ) {
			def.type = PHYS_BODY_STATIC;
			def.mass = 0.0f;
		} else {
			def.type = PHYS_BODY_DYNAMIC;
			def.mass = 2.0f;
		}
		body = Phys_CreateBody( &def );
		PhysMiddleware_TrackProp( body );
		if ( i == 0 && demoRopeCount < PHYS_DEMO_ROPE_MAX ) {
			demoRopeAnchors[demoRopeCount++] = body;
		}
		if ( prev >= 0 && body >= 0 ) {
			Com_Memset( &cdef, 0, sizeof( cdef ) );
			cdef.type = PHYS_CONSTRAINT_DISTANCE;
			cdef.bodyA = prev;
			cdef.bodyB = body;
			cdef.softness = 0.35f;
			cdef.biasFactor = 0.7f;
			cdef.lowerLimit = step * 0.5f;
			cdef.upperLimit = step * 1.25f;
			cdef.disableCollision = qtrue;
			Phys_CreateConstraint( &cdef );
		}
		prev = body;
	}
	Com_Printf( "phys_spawn_rope: %d segments (distance joints) from (%.0f %.0f %.0f)\n",
		segments, start[0], start[1], start[2] );
}

static void PhysMiddleware_SpawnParticles_f( void ) {
	vec3_t origin;
	int count = 48;
	float speed = 280.0f;

	VectorSet( origin, 0.0f, 0.0f, 64.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		count = atoi( Cmd_Argv( 4 ) );
	}
	PhysParticles_CreateBurst( origin, count, speed, 2.0f );
	Com_Printf( "phys_spawn_particles: burst %d at (%.0f %.0f %.0f)\n",
		count, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnSoftBlob_f( void ) {
	vec3_t origin;
	physSoftBlobConfig_t cfg;
	physSoftBlobHandle_t h;
	int res = 4;
	float spacing = 10.0f;

	VectorSet( origin, 0.0f, 0.0f, 80.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	SoftBlob_DefaultConfig( &cfg );
	h = SoftBlob_CreateLattice( origin, res, spacing, &cfg );
	if ( h < 0 ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_softblob: create failed\n" );
		return;
	}
	SoftBlob_PinCorner( h, 0 );
	Com_Printf( "phys_spawn_softblob: blob=%d res=%d at (%.0f %.0f %.0f) (XPBD + Box3D collide)\n",
		h, res, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_SpawnFluid_f( void ) {
	vec3_t origin;
	physFluidConfig_t cfg;
	physFluidHandle_t h;
	int count = 64;
	float spacing = 8.0f;

	VectorSet( origin, 0.0f, 0.0f, 96.0f );
	if ( Cmd_Argc() >= 4 ) {
		origin[0] = (float)atof( Cmd_Argv( 1 ) );
		origin[1] = (float)atof( Cmd_Argv( 2 ) );
		origin[2] = (float)atof( Cmd_Argv( 3 ) );
	}
	if ( Cmd_Argc() >= 5 ) {
		count = atoi( Cmd_Argv( 4 ) );
	}
	PhysFluid_DefaultConfig( &cfg );
	h = PhysFluid_CreateBlob( origin, count, spacing, &cfg );
	if ( h < 0 ) {
		Com_Printf( S_COLOR_YELLOW "phys_spawn_fluid: create failed\n" );
		return;
	}
	Com_Printf( "phys_spawn_fluid: emitter=%d count=%d at (%.0f %.0f %.0f) (SPH + Soft Step couple)\n",
		h, count, origin[0], origin[1], origin[2] );
}

static void PhysMiddleware_Solvers_f( void ) {
	int i;
	if ( Cmd_Argc() >= 3 ) {
		const char *name = Cmd_Argv( 1 );
		const char *onoff = Cmd_Argv( 2 );
		qboolean en = ( !Q_stricmp( onoff, "1" ) || !Q_stricmp( onoff, "on" ) ) ? qtrue : qfalse;
		if ( PhysSolvers_SetEnabled( name, en ) ) {
			Com_Printf( "phys_solvers: %s -> %s\n", name, en ? "on" : "off" );
		} else {
			Com_Printf( S_COLOR_YELLOW "phys_solvers: unknown solver '%s'\n", name );
		}
		return;
	}
	Com_Printf( "Secondary solvers (post Soft Step), collide via Phys_*:\n" );
	for ( i = 0; i < PhysSolvers_GetCount(); i++ ) {
		const char *name = PhysSolvers_GetName( i );
		Com_Printf( "  %s  %s  active=%d\n", name,
			PhysSolvers_IsEnabled( name ) ? "ON " : "off",
			PhysSolvers_GetActiveCount( name ) );
	}
	Com_Printf( "usage: phys_solvers <name> on|off\n" );
}

static void PhysMiddleware_ClearRagdolls_f( void ) {
	PhysMiddleware_ClearAllDemoRagdolls();
	Com_Printf( "phys_clear_ragdolls: demo slots cleared\n" );
}

void PhysMiddleware_RegisterCommands( void ) {
	Cmd_AddCommand( "phys_status", PhysMiddleware_Status_f );
	Cmd_AddCommand( "phys_spawn_ragdoll", PhysMiddleware_SpawnRagdoll_f );
	Cmd_AddCommand( "phys_hit_ragdoll", PhysMiddleware_HitRagdoll_f );
	Cmd_AddCommand( "phys_clear_ragdolls", PhysMiddleware_ClearRagdolls_f );
	Cmd_AddCommand( "phys_spawn_box", PhysMiddleware_SpawnBox_f );
	Cmd_AddCommand( "phys_spawn_sphere", PhysMiddleware_SpawnSphere_f );
	Cmd_AddCommand( "phys_spawn_static", PhysMiddleware_SpawnStatic_f );
	Cmd_AddCommand( "phys_spawn_compound", PhysMiddleware_SpawnCompound_f );
	Cmd_AddCommand( "phys_spawn_cloth", PhysMiddleware_SpawnCloth_f );
	Cmd_AddCommand( "phys_spawn_rope", PhysMiddleware_SpawnRope_f );
	Cmd_AddCommand( "phys_spawn_particles", PhysMiddleware_SpawnParticles_f );
	Cmd_AddCommand( "phys_spawn_softblob", PhysMiddleware_SpawnSoftBlob_f );
	Cmd_AddCommand( "phys_spawn_fluid", PhysMiddleware_SpawnFluid_f );
	Cmd_AddCommand( "phys_solvers", PhysMiddleware_Solvers_f );
	Cmd_AddCommand( "phys_spawn_shadow", PhysMiddleware_SpawnShadow_f );
	Cmd_AddCommand( "phys_shadow_pose", PhysMiddleware_ShadowPose_f );
	Cmd_AddCommand( "phys_impulse_sphere", PhysMiddleware_ImpulseSphere_f );
	Cmd_AddCommand( "phys_spawn_buoyancy", PhysMiddleware_SpawnBuoyancy_f );
	Cmd_AddCommand( "phys_clear_props", PhysMiddleware_ClearProps_f );
	Cmd_AddCommand( "phys_debug", PhysMiddleware_Debug_f );
}

void PhysMiddleware_Init( void ) {
	int i;

	if ( middlewareReady ) {
		return;
	}
	for ( i = 0; i < PHYS_DEMO_RAGDOLL_MAX; i++ ) {
		demoRagdolls[i].ragdoll = -1;
		demoRagdolls[i].anim = -1;
		demoRagdolls[i].motor = -1;
	}
	demoPropCount = 0;
	demoVolumeCount = 0;
	demoClothCount = 0;
	demoRopeCount = 0;
	demoShadow = -1;
	PhysMat_Init();
	PhysEvent_Init();
	PhysMotor_Init();
	PhysProp_Init();
	PhysVolume_Init();
	PhysEvent_Subscribe( PHYS_EVENT_IMPACT, PhysMiddleware_LogImpact, NULL );
	PhysMiddleware_RegisterCommands();
	middlewareReady = qtrue;
	Com_Printf( "PhysMiddleware: gameplay physics layer ready "
		"(props / shadows / volumes / ragdolls)\n" );
}

void PhysMiddleware_Shutdown( void ) {
	if ( !middlewareReady ) {
		return;
	}
	PhysMiddleware_ClearAllDemoRagdolls();
	PhysMiddleware_ClearDemoProps();
	Cmd_RemoveCommand( "phys_status" );
	Cmd_RemoveCommand( "phys_spawn_ragdoll" );
	Cmd_RemoveCommand( "phys_hit_ragdoll" );
	Cmd_RemoveCommand( "phys_clear_ragdolls" );
	Cmd_RemoveCommand( "phys_spawn_box" );
	Cmd_RemoveCommand( "phys_spawn_sphere" );
	Cmd_RemoveCommand( "phys_spawn_static" );
	Cmd_RemoveCommand( "phys_spawn_compound" );
	Cmd_RemoveCommand( "phys_spawn_cloth" );
	Cmd_RemoveCommand( "phys_spawn_rope" );
	Cmd_RemoveCommand( "phys_spawn_particles" );
	Cmd_RemoveCommand( "phys_spawn_softblob" );
	Cmd_RemoveCommand( "phys_spawn_fluid" );
	Cmd_RemoveCommand( "phys_solvers" );
	Cmd_RemoveCommand( "phys_spawn_shadow" );
	Cmd_RemoveCommand( "phys_shadow_pose" );
	Cmd_RemoveCommand( "phys_impulse_sphere" );
	Cmd_RemoveCommand( "phys_spawn_buoyancy" );
	Cmd_RemoveCommand( "phys_clear_props" );
	Cmd_RemoveCommand( "phys_debug" );
	PhysEvent_UnsubscribeAll();
	PhysVolume_Shutdown();
	PhysProp_Shutdown();
	PhysMotor_Shutdown();
	PhysEvent_Shutdown();
	middlewareReady = qfalse;
}

void PhysMiddleware_DispatchHit( int entityNum, procAnimHandle_t anim, physMotorHandle_t motor,
	int bone, int damageType, const vec3_t point, const vec3_t impulse ) {
	phys_hit_event_t hit;
	phys_event_t ev;
	phys_impact_response_t response;
	float mag;

	if ( !middlewareReady ) {
		return;
	}

	PhysEvent_BuildHitFromImpulse( &hit, bone, damageType, point, impulse, 1.0f );
	mag = VectorLength( impulse );

	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = PHYS_EVENT_IMPACT;
	ev.entityNum = entityNum;
	ev.hit = hit;
	VectorCopy( point, ev.point );
	VectorCopy( impulse, ev.impulse );
	ev.magnitude = mag;
	PhysEvent_Post( &ev );

	if ( motor >= 0 ) {
		PhysMotor_ApplyHit( motor, &hit );
	} else if ( anim >= 0 ) {
		ProcAnim_ApplyImpact( anim, point, impulse, 24.0f );
	}

	PhysMat_ComputeImpactResponse( PHYS_MAT_FLESH, PHYS_MAT_DEFAULT, mag, 1.0f, &response );
	if ( response.shouldSplash ) {
		phys_event_t splash;
		Com_Memset( &splash, 0, sizeof( splash ) );
		splash.type = PHYS_EVENT_SPLASH;
		VectorCopy( point, splash.point );
		splash.magnitude = response.particleScale;
		PhysEvent_Post( &splash );
	}
}

void PhysMiddleware_Frame( float dt ) {
	if ( !middlewareReady ) {
		return;
	}

	/* shadows / volumes / procanim / motors tick as PRE_STEP solvers so Soft Step sees them */
	(void)dt;
	PhysEvent_DispatchQueued();
}
