/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Box3D physics substrate — default first-class backend for Phys_* APIs.
Quake Z-up world space; Box3D gravity set to (0,0,phys_gravity).
Ragdoll + DMM stress grid live here so Euphoria-like / DMM middleware
share one substrate with props, volumes, and motors.
===========================================================================
*/

#ifdef USE_BOX3D_PHYSICS_IMPL

#include "box3d/box3d.h"
#include "box3d/collision.h"
#include "box3d/math_functions.h"
#include "box3d/types.h"

#include "phys_impl.h"
#include "phys_debugdraw.h"
#include "phys_events.h"
#include "q_shared.h"
#include "qcommon.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VALID_BODY(h) ((h) >= 0 && (h) < bx.bodyCount && bx.bodies[(h)].active)
#define VALID_CON(h)  ((h) >= 0 && (h) < bx.constraintCount && bx.constraints[(h)].active)
#define VALID_RAG(h)  ((h) >= 0 && (h) < bx.ragdollCount && bx.ragdolls[(h)].active)
#define VALID_DMM(h)  ((h) >= 0 && (h) < bx.dmmCount && bx.dmmObjects[(h)].active)
/* Soft Step userData: normal body = index+1; ragdoll bone = flag | (rag<<8) | bone */
#define BOX_UD_RAG_FLAG ((uintptr_t)1u << 30)
#define BOX_UD_PACK_RAG(r,b) ((void *)(uintptr_t)(BOX_UD_RAG_FLAG | (((uintptr_t)(r) & 0x3FFu) << 8) | ((uintptr_t)(b) & 0xFFu)))

static void box_decode_userdata( void *ud, int *bodyOut, int *ragOut, int *boneOut ) {
	uintptr_t v = (uintptr_t)(intptr_t)ud;
	if ( bodyOut ) {
		*bodyOut = -1;
	}
	if ( ragOut ) {
		*ragOut = -1;
	}
	if ( boneOut ) {
		*boneOut = -1;
	}
	if ( !v ) {
		return;
	}
	if ( v & BOX_UD_RAG_FLAG ) {
		if ( ragOut ) {
			*ragOut = (int)( ( v >> 8 ) & 0x3FFu );
		}
		if ( boneOut ) {
			*boneOut = (int)( v & 0xFFu );
		}
		return;
	}
	if ( bodyOut ) {
		*bodyOut = (int)v - 1;
	}
}

typedef struct {
	b3BodyId           bodyId;
	b3ShapeId          shapeId;
	b3ShapeId          shapes[8];
	int                shapeCount;
	b3MeshData        *meshData;
	b3CompoundData    *compoundData;
	b3HeightFieldData *heightFieldData;
	physBodyType_t     bodyType;
	int                materialId;
	qboolean           isSensor;
	qboolean           active;
} BoxBody;

typedef struct {
	b3JointId          jointId;
	physBodyHandle_t   bodyA;
	physBodyHandle_t   bodyB;
	physConstraintType_t type;
	float              breakForce;
	float              breakTorque;
	qboolean           active;
} BoxConstraint;

typedef struct {
	b3BodyId  bodyId;
	b3ShapeId shapeId;
} BoxRagBone;

typedef struct {
	BoxRagBone bones[PHYS_RAGDOLL_MAX_BONES];
	b3JointId  joints[PHYS_RAGDOLL_MAX_BONES];
	int        numBones;
	int        numJoints;
	float      muscleStiffness;
	float      muscleDamping;
	float      muscleStrength;
	float      balanceForce;
	float      reachForce;
	float      impactResponse;
	qboolean   balanceEnabled;
	b3Vec3     balanceTarget;
	float      animBlend;
	qboolean   hasAnimTargets;
	qboolean   animTargetValid[PHYS_RAGDOLL_MAX_BONES];
	b3Vec3     animTargetPos[PHYS_RAGDOLL_MAX_BONES];
	b3Quat     animTargetRot[PHYS_RAGDOLL_MAX_BONES];
	qboolean   active;
} BoxRagdoll;

typedef struct {
	float    strain;
	float    stress;
	float    plasticity;
} BoxDmmElement;

typedef struct {
	dmmMaterialType_t material;
	float             yieldStrength;
	float             fractureStrength;
	float             deformability;
	float             integrity;
	qboolean          fractured;
	BoxDmmElement    *elements;
	int               numElements;
	/* Soft Step rigid proxy + debris (DMM-like fracture companion) */
	physBodyHandle_t  body;
	physBodyHandle_t  fragments[32];
	int               numFragments;
	vec3_t            position;
	vec3_t            halfExtents;
	float             density;
	int               entityNum;
	qboolean          active;
} BoxDmmObject;

static struct {
	b3WorldId      worldId;
	BoxBody        bodies[PHYS_MAX_RIGID_BODIES];
	int            bodyCount;
	BoxConstraint  constraints[PHYS_MAX_CONSTRAINTS];
	int            constraintCount;
	BoxRagdoll     ragdolls[PHYS_MAX_RAGDOLLS];
	int            ragdollCount;
	BoxDmmObject   dmmObjects[PHYS_MAX_DMM_OBJECTS];
	int            dmmCount;
	b3MeshData    *meshes[256];
	int            meshCount;
	b3CompoundData *compounds[64];
	int            compoundCount;
	b3HeightFieldData *heightFields[64];
	int            heightFieldCount;
	qboolean       initialized;
	int            workerCount;
	b3Recording   *recording;
	b3Recording   *replayRec;
	b3RecPlayer   *replayPlayer;
	physSoftStepProfile_t lastProfile;
	unsigned       debugDrawFlags; /* bit0 shapes bit1 joints bit2 contacts bit3 bounds */
	PhysCustomFilterFn customFilterFn;
	void          *customFilterCtx;
	PhysPreSolveFn preSolveFn;
	void          *preSolveCtx;
	PhysFrictionMixFn frictionMixFn;
	PhysRestitutionMixFn restitutionMixFn;
} bx;

static void box_destroy_mesh( b3MeshData *mesh ) {
	int i;
	if ( !mesh ) {
		return;
	}
	for ( i = 0; i < bx.meshCount; i++ ) {
		if ( bx.meshes[i] == mesh ) {
			bx.meshes[i] = bx.meshes[bx.meshCount - 1];
			bx.meshes[bx.meshCount - 1] = NULL;
			bx.meshCount--;
			break;
		}
	}
	b3DestroyMesh( mesh );
}

static void box_destroy_all_meshes( void ) {
	int i;
	for ( i = 0; i < bx.meshCount; i++ ) {
		if ( bx.meshes[i] ) {
			b3DestroyMesh( bx.meshes[i] );
			bx.meshes[i] = NULL;
		}
	}
	bx.meshCount = 0;
}

static void box_destroy_compound( b3CompoundData *compound ) {
	int i;
	if ( !compound ) {
		return;
	}
	for ( i = 0; i < bx.compoundCount; i++ ) {
		if ( bx.compounds[i] == compound ) {
			bx.compounds[i] = bx.compounds[bx.compoundCount - 1];
			bx.compounds[bx.compoundCount - 1] = NULL;
			bx.compoundCount--;
			break;
		}
	}
	b3DestroyCompound( compound );
}

static void box_destroy_all_compounds( void ) {
	int i;
	for ( i = 0; i < bx.compoundCount; i++ ) {
		if ( bx.compounds[i] ) {
			b3DestroyCompound( bx.compounds[i] );
			bx.compounds[i] = NULL;
		}
	}
	bx.compoundCount = 0;
}

static void box_destroy_heightfield( b3HeightFieldData *hf ) {
	int i;
	if ( !hf ) {
		return;
	}
	for ( i = 0; i < bx.heightFieldCount; i++ ) {
		if ( bx.heightFields[i] == hf ) {
			bx.heightFields[i] = bx.heightFields[bx.heightFieldCount - 1];
			bx.heightFields[bx.heightFieldCount - 1] = NULL;
			bx.heightFieldCount--;
			break;
		}
	}
	b3DestroyHeightField( hf );
}

static void box_destroy_all_heightfields( void ) {
	int i;
	for ( i = 0; i < bx.heightFieldCount; i++ ) {
		if ( bx.heightFields[i] ) {
			b3DestroyHeightField( bx.heightFields[i] );
			bx.heightFields[i] = NULL;
		}
	}
	bx.heightFieldCount = 0;
}

static int box_resolve_workers( void ) {
	cvar_t *cv;
	int n;

	cv = Cvar_Get( "phys_workers", "0", CVAR_ARCHIVE );
	n = cv ? cv->integer : 0;
	if ( n <= 0 ) {
#if defined( _SC_NPROCESSORS_ONLN )
		n = (int)sysconf( _SC_NPROCESSORS_ONLN );
#else
		n = 4;
#endif
		if ( n > 8 ) {
			n = 8;
		}
		if ( n < 1 ) {
			n = 1;
		}
	}
	if ( n > B3_MAX_WORKERS ) {
		n = B3_MAX_WORKERS;
	}
	if ( n < 1 ) {
		n = 1;
	}
	return n;
}

static b3Vec3 v3( float x, float y, float z ) {
	b3Vec3 v = { x, y, z };
	return v;
}

static b3Vec3 from_vec3( const vec3_t in ) {
	return v3( in[0], in[1], in[2] );
}

static void to_vec3( b3Vec3 in, vec3_t out ) {
	out[0] = in.x;
	out[1] = in.y;
	out[2] = in.z;
}

static b3Quat quat_from_euler_deg( const vec3_t rotDeg ) {
	/* pitch,yaw,roll in degrees → ZYX intrinsic (matches Bullet path) */
	float pitch = rotDeg[0] * (float)( M_PI / 180.0 );
	float yaw = rotDeg[1] * (float)( M_PI / 180.0 );
	float roll = rotDeg[2] * (float)( M_PI / 180.0 );
	b3Quat qz = b3MakeQuatFromAxisAngle( b3Vec3_axisZ, yaw );
	b3Quat qy = b3MakeQuatFromAxisAngle( b3Vec3_axisY, pitch );
	b3Quat qx = b3MakeQuatFromAxisAngle( b3Vec3_axisX, roll );
	return b3MulQuat( qz, b3MulQuat( qy, qx ) );
}

/* Align local +X with dir (prismatic / wheel slide axis). */
static b3Quat quat_align_local_x( b3Vec3 dir ) {
	b3Matrix3 m;
	b3Vec3 ref, y, z;
	float len = b3Length( dir );
	if ( len < 1e-5f ) {
		return b3Quat_identity;
	}
	dir = b3MulSV( 1.0f / len, dir );
	ref = b3Vec3_axisZ;
	if ( fabsf( b3Dot( dir, ref ) ) > 0.95f ) {
		ref = b3Vec3_axisY;
	}
	y = b3Normalize( b3Cross( ref, dir ) );
	z = b3Cross( dir, y );
	m.cx = dir;
	m.cy = y;
	m.cz = z;
	return b3MakeQuatFromMatrix( &m );
}

static void apply_motion_locks( b3BodyId bodyId, int lockBits ) {
	b3MotionLocks locks;
	memset( &locks, 0, sizeof( locks ) );
	locks.linearX = ( lockBits & PHYS_LOCK_LIN_X ) != 0;
	locks.linearY = ( lockBits & PHYS_LOCK_LIN_Y ) != 0;
	locks.linearZ = ( lockBits & PHYS_LOCK_LIN_Z ) != 0;
	locks.angularX = ( lockBits & PHYS_LOCK_ANG_X ) != 0;
	locks.angularY = ( lockBits & PHYS_LOCK_ANG_Y ) != 0;
	locks.angularZ = ( lockBits & PHYS_LOCK_ANG_Z ) != 0;
	b3Body_SetMotionLocks( bodyId, locks );
}

static void euler_from_quat( b3Quat q, vec3_t outDeg ) {
	float angle;
	b3Vec3 axis = b3GetAxisAngle( &angle, q );
	outDeg[0] = axis.x * angle * (float)( 180.0 / M_PI );
	outDeg[1] = axis.y * angle * (float)( 180.0 / M_PI );
	outDeg[2] = axis.z * angle * (float)( 180.0 / M_PI );
}

static float volume_for_def( const physBodyDef_t *def ) {
	switch ( def->shape ) {
	case PHYS_SHAPE_SPHERE: {
		float r = def->radius > 0.0f ? def->radius : 8.0f;
		return ( 4.0f / 3.0f ) * (float)M_PI * r * r * r;
	}
	case PHYS_SHAPE_CAPSULE:
	case PHYS_SHAPE_CYLINDER: {
		float r = def->radius > 0.0f ? def->radius : 8.0f;
		float h = def->height > 0.0f ? def->height : 32.0f;
		return (float)M_PI * r * r * h + ( 4.0f / 3.0f ) * (float)M_PI * r * r * r;
	}
	case PHYS_SHAPE_CONVEX_HULL:
	case PHYS_SHAPE_BOX:
	default: {
		float hx = def->halfExtents[0] > 0.0f ? def->halfExtents[0] : 8.0f;
		float hy = def->halfExtents[1] > 0.0f ? def->halfExtents[1] : 8.0f;
		float hz = def->halfExtents[2] > 0.0f ? def->halfExtents[2] : 8.0f;
		return 8.0f * hx * hy * hz;
	}
	}
}

static void attach_shape( b3BodyId bodyId, const physBodyDef_t *def, float density, b3ShapeId *outShape ) {
	b3ShapeDef sd = b3DefaultShapeDef();
	sd.density = density;
	sd.baseMaterial.friction = def->friction > 0.0f ? def->friction : 0.5f;
	sd.baseMaterial.restitution = def->restitution;
	sd.baseMaterial.userMaterialId = (uint64_t)def->materialId;
	sd.isSensor = def->isSensor ? true : false;
	sd.enableSensorEvents = def->isSensor ? true : false;
	sd.enableHitEvents = !def->isSensor;
	sd.enableContactEvents = !def->isSensor;
	sd.enableCustomFiltering = true;
	sd.enablePreSolveEvents = true;
	if ( def->collisionGroup ) {
		sd.filter.categoryBits = (uint64_t)(unsigned)def->collisionGroup;
	}
	if ( def->collisionMask ) {
		sd.filter.maskBits = (uint64_t)(unsigned)def->collisionMask;
	}

	switch ( def->shape ) {
	case PHYS_SHAPE_SPHERE: {
		b3Sphere s;
		s.center = b3Vec3_zero;
		s.radius = def->radius > 0.0f ? def->radius : 8.0f;
		*outShape = b3CreateSphereShape( bodyId, &sd, &s );
		break;
	}
	case PHYS_SHAPE_CAPSULE: {
		b3Capsule c;
		float h = def->height > 0.0f ? def->height : 32.0f;
		float r = def->radius > 0.0f ? def->radius : 8.0f;
		c.center1 = v3( 0.0f, 0.0f, -0.5f * h );
		c.center2 = v3( 0.0f, 0.0f, 0.5f * h );
		c.radius = r;
		*outShape = b3CreateCapsuleShape( bodyId, &sd, &c );
		break;
	}
	case PHYS_SHAPE_CYLINDER: {
		/* Box3D cylinder hull is Y-up; rotate +90° pitch so height aligns with Quake Z. */
		float h = def->height > 0.0f ? def->height : 32.0f;
		float r = def->radius > 0.0f ? def->radius : 8.0f;
		b3HullData *cyl = b3CreateCylinder( h, r, 0.0f, 12 );
		b3Transform xf = b3Transform_identity;
		vec3_t rot;
		if ( !cyl ) {
			b3Capsule c;
			c.center1 = v3( 0.0f, 0.0f, -0.5f * h );
			c.center2 = v3( 0.0f, 0.0f, 0.5f * h );
			c.radius = r;
			*outShape = b3CreateCapsuleShape( bodyId, &sd, &c );
			break;
		}
		VectorSet( rot, 90.0f, 0.0f, 0.0f );
		xf.q = quat_from_euler_deg( rot );
		*outShape = b3CreateTransformedHullShape( bodyId, &sd, cyl, xf, b3Vec3_one );
		b3DestroyHull( cyl );
		break;
	}
	case PHYS_SHAPE_CONVEX_HULL: {
		b3Vec3 pts[64];
		int n = def->hullPointCount;
		int i;
		b3HullData *hull;
		if ( !def->hullPoints || n < 4 ) {
			float hx = def->halfExtents[0] > 0.0f ? def->halfExtents[0] : 8.0f;
			float hy = def->halfExtents[1] > 0.0f ? def->halfExtents[1] : 8.0f;
			float hz = def->halfExtents[2] > 0.0f ? def->halfExtents[2] : 8.0f;
			b3BoxHull box = b3MakeBoxHull( hx, hy, hz );
			*outShape = b3CreateHullShape( bodyId, &sd, &box.base );
			break;
		}
		if ( n > 64 ) {
			n = 64;
		}
		for ( i = 0; i < n; i++ ) {
			pts[i] = v3( def->hullPoints[i * 3 + 0], def->hullPoints[i * 3 + 1], def->hullPoints[i * 3 + 2] );
		}
		hull = b3CreateHull( pts, n, n );
		if ( !hull ) {
			b3BoxHull box = b3MakeBoxHull( 8.0f, 8.0f, 8.0f );
			*outShape = b3CreateHullShape( bodyId, &sd, &box.base );
			break;
		}
		*outShape = b3CreateHullShape( bodyId, &sd, hull );
		b3DestroyHull( hull );
		break;
	}
	case PHYS_SHAPE_BOX:
	default: {
		float hx = def->halfExtents[0] > 0.0f ? def->halfExtents[0] : 8.0f;
		float hy = def->halfExtents[1] > 0.0f ? def->halfExtents[1] : 8.0f;
		float hz = def->halfExtents[2] > 0.0f ? def->halfExtents[2] : 8.0f;
		b3BoxHull box = b3MakeBoxHull( hx, hy, hz );
		*outShape = b3CreateHullShape( bodyId, &sd, &box.base );
		break;
	}
	}
}

static void get_dmm_preset( dmmMaterialType_t mat, float *stiff, float *yield, float *frac ) {
	switch ( mat ) {
	case DMM_GLASS:       *stiff = 70.0f; *yield = 30.0f; *frac = 40.0f; break;
	case DMM_METAL_THIN:  *stiff = 90.0f; *yield = 80.0f; *frac = 120.0f; break;
	case DMM_CONCRETE:    *stiff = 40.0f; *yield = 20.0f; *frac = 35.0f; break;
	case DMM_WOOD:        *stiff = 25.0f; *yield = 15.0f; *frac = 25.0f; break;
	default:              *stiff = 30.0f; *yield = 20.0f; *frac = 40.0f; break;
	}
}

/* ========== lifecycle ========== */

qboolean Phys_Init_Impl( void ) {
	b3WorldDef worldDef;
	cvar_t *sleepCv;
	cvar_t *ccdCv;

	if ( bx.initialized ) {
		return qtrue;
	}

	memset( &bx, 0, sizeof( bx ) );
	worldDef = b3DefaultWorldDef();
	worldDef.gravity = v3( 0.0f, 0.0f, -800.0f );
	sleepCv = Cvar_Get( "phys_sleep", "1", CVAR_ARCHIVE );
	ccdCv = Cvar_Get( "phys_ccd", "1", CVAR_ARCHIVE );
	worldDef.enableSleep = !( sleepCv && sleepCv->integer == 0 );
	worldDef.enableContinuous = !( ccdCv && ccdCv->integer == 0 );
	bx.workerCount = box_resolve_workers();
	worldDef.workerCount = (uint32_t)bx.workerCount;
	bx.worldId = b3CreateWorld( &worldDef );
	if ( !b3World_IsValid( bx.worldId ) ) {
		Com_Printf( S_COLOR_RED "Box3D: b3CreateWorld failed\n" );
		return qfalse;
	}
	bx.initialized = qtrue;
	bx.debugDrawFlags = 0x3; /* shapes + joints */
	{
		cvar_t *hitCv = Cvar_Get( "phys_hitThreshold", "25", CVAR_ARCHIVE );
		cvar_t *maxSpd = Cvar_Get( "phys_maxLinearSpeed", "0", CVAR_ARCHIVE );
		cvar_t *specCv = Cvar_Get( "phys_speculative", "1", CVAR_ARCHIVE );
		cvar_t *tuneH = Cvar_Get( "phys_contactHertz", "0", CVAR_ARCHIVE );
		if ( hitCv ) {
			b3World_SetHitEventThreshold( bx.worldId, hitCv->value );
		}
		if ( maxSpd && maxSpd->value > 0.0f ) {
			b3World_SetMaximumLinearSpeed( bx.worldId, maxSpd->value );
		}
		if ( specCv ) {
			b3World_EnableSpeculative( bx.worldId, specCv->integer != 0 );
		}
		if ( tuneH && tuneH->value > 0.0f ) {
			cvar_t *tuneD = Cvar_Get( "phys_contactDamping", "0.7", CVAR_ARCHIVE );
			cvar_t *tuneS = Cvar_Get( "phys_contactSpeed", "0", CVAR_ARCHIVE );
			b3World_SetContactTuning( bx.worldId, tuneH->value,
				tuneD ? tuneD->value : 0.7f, tuneS ? tuneS->value : 0.0f );
		}
	}
	Com_Printf( "Box3D: world created (Z-up, Soft Step AAA, workers=%d, sleep=%d, ccd=%d)\n",
		bx.workerCount, worldDef.enableSleep ? 1 : 0, worldDef.enableContinuous ? 1 : 0 );
	return qtrue;
}

void Phys_Shutdown_Impl( void ) {
	int i;

	if ( !bx.initialized ) {
		return;
	}
	if ( bx.recording ) {
		b3World_StopRecording( bx.worldId );
		b3DestroyRecording( bx.recording );
		bx.recording = NULL;
	}
	if ( bx.replayPlayer ) {
		b3RecPlayer_Destroy( bx.replayPlayer );
		bx.replayPlayer = NULL;
	}
	if ( bx.replayRec ) {
		b3DestroyRecording( bx.replayRec );
		bx.replayRec = NULL;
	}
	for ( i = 0; i < bx.dmmCount; i++ ) {
		if ( bx.dmmObjects[i].elements ) {
			free( bx.dmmObjects[i].elements );
			bx.dmmObjects[i].elements = NULL;
		}
	}
	/* Destroy world (shapes/bodies) before cooked mesh/compound data. */
	b3DestroyWorld( bx.worldId );
	box_destroy_all_meshes();
	box_destroy_all_compounds();
	box_destroy_all_heightfields();
	memset( &bx, 0, sizeof( bx ) );
}

void Phys_StepSimulation_Impl( float dt ) {
	int subSteps;
	int workers;
	int r, b;
	cvar_t *subCv;
	cvar_t *sleepCv;
	cvar_t *ccdCv;

	if ( !bx.initialized ) {
		return;
	}
	if ( dt <= 0.0f ) {
		dt = 1.0f / 60.0f;
	}
	subCv = Cvar_Get( "phys_maxSubSteps", "4", CVAR_ARCHIVE );
	subSteps = subCv ? subCv->integer : 4;
	if ( subSteps < 1 ) {
		subSteps = 1;
	}
	if ( subSteps > 16 ) {
		subSteps = 16;
	}

	workers = box_resolve_workers();
	if ( workers != bx.workerCount ) {
		b3World_SetWorkerCount( bx.worldId, workers );
		bx.workerCount = workers;
	}
	sleepCv = Cvar_Get( "phys_sleep", "1", CVAR_ARCHIVE );
	ccdCv = Cvar_Get( "phys_ccd", "1", CVAR_ARCHIVE );
	b3World_EnableSleeping( bx.worldId, !( sleepCv && sleepCv->integer == 0 ) );
	b3World_EnableContinuous( bx.worldId, !( ccdCv && ccdCv->integer == 0 ) );

	b3World_Step( bx.worldId, dt, subSteps );

	{
		b3Profile prof = b3World_GetProfile( bx.worldId );
		b3Counters counters = b3World_GetCounters( bx.worldId );
		cvar_t *hitCv = Cvar_Get( "phys_hitThreshold", "25", CVAR_ARCHIVE );
		bx.lastProfile.stepMs = prof.step;
		bx.lastProfile.collideMs = prof.collide;
		bx.lastProfile.solveMs = prof.solve;
		bx.lastProfile.jointEventsMs = prof.jointEvents;
		bx.lastProfile.bodyCount = Phys_GetBodyCount_Impl();
		bx.lastProfile.constraintCount = Phys_GetConstraintCount_Impl();
		bx.lastProfile.contactCount = counters.contactCount;
		bx.lastProfile.shapeCount = counters.shapeCount;
		bx.lastProfile.islandCount = counters.islandCount;
		if ( hitCv ) {
			b3World_SetHitEventThreshold( bx.worldId, hitCv->value );
		}
	}

	for ( r = 0; r < bx.ragdollCount; r++ ) {
		BoxRagdoll *rag = &bx.ragdolls[r];
		float muscle;
		if ( !rag->active || !rag->balanceEnabled ) {
			continue;
		}
		/* Pelvis-only balance (Z-up horizontal plant) — not every bone */
		muscle = rag->muscleStiffness * ( rag->muscleStrength > 0.0f ? rag->muscleStrength : 1.0f );
		{
			b3Vec3 pos = b3Body_GetPosition( rag->bones[0].bodyId );
			b3Vec3 toTarget = b3Sub( rag->balanceTarget, pos );
			toTarget.z = 0.0f;
			if ( b3Length( toTarget ) > 0.1f ) {
				b3Vec3 force = b3MulSV( rag->balanceForce * muscle, b3Normalize( toTarget ) );
				b3Body_ApplyForceToCenter( rag->bones[0].bodyId, force, true );
			}
		}
	}

	/* Blend Soft Step bones toward MD3 / animation targets */
	for ( r = 0; r < bx.ragdollCount; r++ ) {
		BoxRagdoll *rag = &bx.ragdolls[r];
		float blend;
		if ( !rag->active || !rag->hasAnimTargets || rag->animBlend <= 0.0f ) {
			continue;
		}
		blend = rag->animBlend;
		{
			float muscle = rag->muscleStiffness * ( rag->muscleStrength > 0.0f ? rag->muscleStrength : 1.0f );
			for ( b = 0; b < rag->numBones; b++ ) {
				b3Vec3 pos, delta, force;
				b3Quat q, qErr;
				b3Vec3 torqueAxis;
				float angle;
				if ( !rag->animTargetValid[b] ) {
					continue;
				}
				pos = b3Body_GetPosition( rag->bones[b].bodyId );
				delta = b3Sub( rag->animTargetPos[b], pos );
				force = b3MulSV( blend * muscle * 40.0f, delta );
				b3Body_ApplyForceToCenter( rag->bones[b].bodyId, force, true );

				q = b3Body_GetRotation( rag->bones[b].bodyId );
				qErr = b3MulQuat( rag->animTargetRot[b], b3Conjugate( q ) );
				torqueAxis = b3GetAxisAngle( &angle, qErr );
				if ( angle > 0.001f || angle < -0.001f ) {
					b3Body_ApplyTorque( rag->bones[b].bodyId,
						b3MulSV( blend * muscle * 80.0f * angle, torqueAxis ), true );
				}
			}
		}
	}

	for ( r = 0; r < bx.dmmCount; r++ ) {
		BoxDmmObject *dmm = &bx.dmmObjects[r];
		float maxStrain = 0.0f;
		int e;
		cvar_t *dmmEn = Cvar_Get( "phys_dmm_enabled", "1", CVAR_ARCHIVE );
		if ( dmmEn && !dmmEn->integer ) {
			continue;
		}
		if ( !dmm->active || dmm->fractured || !dmm->elements ) {
			continue;
		}
		/* Accumulate Soft Step contact energy into stress grid */
		if ( dmm->body >= 0 && VALID_BODY( dmm->body ) ) {
			physContact_t contacts[16];
			int n = Phys_GetBodyContacts_Impl( dmm->body, contacts, 16 );
			int c;
			for ( c = 0; c < n; c++ ) {
				float energy = contacts[c].normalImpulse;
				float s = energy * ( 0.02f / ( dmm->deformability > 0.0f ? dmm->deformability : 1.0f ) );
				for ( e = 0; e < dmm->numElements; e++ ) {
					dmm->elements[e].strain += s * ( 0.15f + (float)( e % 7 ) * 0.01f );
				}
			}
		}
		for ( e = 0; e < dmm->numElements; e++ ) {
			if ( dmm->elements[e].strain > maxStrain ) {
				maxStrain = dmm->elements[e].strain;
			}
			if ( dmm->elements[e].strain > dmm->yieldStrength ) {
				dmm->elements[e].plasticity += ( dmm->elements[e].strain - dmm->yieldStrength ) * 0.01f;
			}
		}
		dmm->integrity = 1.0f - ( maxStrain / ( dmm->fractureStrength > 0.0f ? dmm->fractureStrength : 1.0f ) );
		if ( dmm->integrity <= 0.0f ) {
			dmm->fractured = qtrue;
			dmm->integrity = 0.0f;
			Com_Printf( "[physics] Soft Step DMM object %d fractured (spawn debris via Dmm_Fracture)\n", r );
		}
	}
}

void Phys_SetGravity_Impl( const vec3_t g ) {
	if ( !bx.initialized ) {
		return;
	}
	b3World_SetGravity( bx.worldId, from_vec3( g ) );
}

void Phys_ClearWorld_Impl( void ) {
	if ( !bx.initialized ) {
		return;
	}
	Phys_Shutdown_Impl();
	Phys_Init_Impl();
}

/* ========== rigid bodies ========== */

physBodyHandle_t Phys_CreateBody_Impl( const physBodyDef_t *def ) {
	b3BodyDef bd;
	BoxBody *pb;
	int idx;
	float mass;
	float vol;
	float density;

	if ( !bx.initialized || !def || bx.bodyCount >= PHYS_MAX_RIGID_BODIES ) {
		return -1;
	}

	idx = bx.bodyCount++;
	pb = &bx.bodies[idx];
	memset( pb, 0, sizeof( *pb ) );

	bd = b3DefaultBodyDef();
	switch ( def->type ) {
	case PHYS_BODY_STATIC:    bd.type = b3_staticBody; break;
	case PHYS_BODY_KINEMATIC: bd.type = b3_kinematicBody; break;
	default:                  bd.type = b3_dynamicBody; break;
	}
	bd.position = from_vec3( def->position );
	bd.rotation = quat_from_euler_deg( def->rotation );
	bd.linearDamping = def->linearDamping;
	bd.angularDamping = def->angularDamping;
	if ( def->type == PHYS_BODY_KINEMATIC ) {
		bd.enableSleep = false;
	}
	pb->bodyId = b3CreateBody( bx.worldId, &bd );
	b3Body_SetUserData( pb->bodyId, (void *)(intptr_t)( idx + 1 ) );

	mass = ( def->type == PHYS_BODY_STATIC || def->type == PHYS_BODY_KINEMATIC ) ? 0.0f : def->mass;
	if ( mass <= 0.0f && def->type == PHYS_BODY_DYNAMIC ) {
		mass = 10.0f;
	}
	vol = volume_for_def( def );
	density = ( mass > 0.0f && vol > 0.001f ) ? ( mass / vol ) : 0.0f;
	attach_shape( pb->bodyId, def, density, &pb->shapeId );
	pb->shapes[0] = pb->shapeId;
	pb->shapeCount = 1;

	/* gravityScale > 0 overrides Soft Step default; use Phys_SetBodyGravityScale(0) for zero-G */
	if ( def->gravityScale > 0.0f ) {
		b3Body_SetGravityScale( pb->bodyId, def->gravityScale );
	}
	if ( def->motionLocks ) {
		apply_motion_locks( pb->bodyId, def->motionLocks );
	}

	pb->bodyType = def->type;
	pb->materialId = def->materialId;
	pb->isSensor = def->isSensor;
	pb->active = qtrue;
	return idx;
}

void Phys_DestroyBody_Impl( physBodyHandle_t h ) {
	b3MeshData *mesh;
	b3CompoundData *compound;
	b3HeightFieldData *hf;
	if ( !VALID_BODY( h ) ) {
		return;
	}
	mesh = bx.bodies[h].meshData;
	compound = bx.bodies[h].compoundData;
	hf = bx.bodies[h].heightFieldData;
	b3DestroyBody( bx.bodies[h].bodyId );
	memset( &bx.bodies[h], 0, sizeof( bx.bodies[h] ) );
	if ( mesh ) {
		box_destroy_mesh( mesh );
	}
	if ( compound ) {
		box_destroy_compound( compound );
	}
	if ( hf ) {
		box_destroy_heightfield( hf );
	}
}

void Phys_GetBodyTransform_Impl( physBodyHandle_t h, physTransform_t *out ) {
	b3Vec3 p, lv, av;
	b3Quat q;

	if ( !out ) {
		return;
	}
	memset( out, 0, sizeof( *out ) );
	if ( !VALID_BODY( h ) ) {
		return;
	}
	p = b3Body_GetPosition( bx.bodies[h].bodyId );
	q = b3Body_GetRotation( bx.bodies[h].bodyId );
	lv = b3Body_GetLinearVelocity( bx.bodies[h].bodyId );
	av = b3Body_GetAngularVelocity( bx.bodies[h].bodyId );
	to_vec3( p, out->position );
	euler_from_quat( q, out->rotation );
	to_vec3( lv, out->linearVelocity );
	to_vec3( av, out->angularVelocity );
}

void Phys_SetBodyTransform_Impl( physBodyHandle_t h, const vec3_t pos, const vec3_t rot ) {
	b3Quat q;
	if ( !VALID_BODY( h ) ) {
		return;
	}
	if ( rot ) {
		q = quat_from_euler_deg( rot );
	} else {
		q = b3Body_GetRotation( bx.bodies[h].bodyId );
	}
	b3Body_SetTransform( bx.bodies[h].bodyId, from_vec3( pos ), q );
}

void Phys_SetBodyTargetTransform_Impl( physBodyHandle_t h, const vec3_t pos, const vec3_t rot, float timeStep ) {
	b3WorldTransform xf;
	if ( !VALID_BODY( h ) || !pos ) {
		return;
	}
	xf = b3Body_GetTransform( bx.bodies[h].bodyId );
	xf.p = from_vec3( pos );
	if ( rot ) {
		xf.q = quat_from_euler_deg( rot );
	}
	if ( timeStep <= 0.0f ) {
		timeStep = 1.0f / 60.0f;
	}
	b3Body_SetTargetTransform( bx.bodies[h].bodyId, xf, timeStep, true );
}

void Phys_SetBodyGravityScale_Impl( physBodyHandle_t h, float scale ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	b3Body_SetGravityScale( bx.bodies[h].bodyId, scale );
}

void Phys_SetBodyMotionLocks_Impl( physBodyHandle_t h, int lockBits ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	apply_motion_locks( bx.bodies[h].bodyId, lockBits );
}

void Phys_ApplyForce_Impl( physBodyHandle_t h, const vec3_t force, const vec3_t point ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	b3Body_ApplyForce( bx.bodies[h].bodyId, from_vec3( force ), from_vec3( point ), true );
}

void Phys_ApplyImpulse_Impl( physBodyHandle_t h, const vec3_t impulse, const vec3_t point ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	b3Body_ApplyLinearImpulse( bx.bodies[h].bodyId, from_vec3( impulse ), from_vec3( point ), true );
}

void Phys_ApplyTorque_Impl( physBodyHandle_t h, const vec3_t torque ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	b3Body_ApplyTorque( bx.bodies[h].bodyId, from_vec3( torque ), true );
}

void Phys_SetBodyVelocity_Impl( physBodyHandle_t h, const vec3_t linear, const vec3_t angular ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	if ( linear ) {
		b3Body_SetLinearVelocity( bx.bodies[h].bodyId, from_vec3( linear ) );
	}
	if ( angular ) {
		b3Body_SetAngularVelocity( bx.bodies[h].bodyId, from_vec3( angular ) );
	}
}

void Phys_SetBodyActive_Impl( physBodyHandle_t h, qboolean active ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	if ( active ) {
		b3Body_Enable( bx.bodies[h].bodyId );
	} else {
		b3Body_Disable( bx.bodies[h].bodyId );
	}
}

physBodyType_t Phys_GetBodyType_Impl( physBodyHandle_t h ) {
	if ( !VALID_BODY( h ) ) {
		return PHYS_BODY_STATIC;
	}
	return bx.bodies[h].bodyType;
}

qboolean Phys_IsBodyDynamic_Impl( physBodyHandle_t h ) {
	return ( VALID_BODY( h ) && bx.bodies[h].bodyType == PHYS_BODY_DYNAMIC ) ? qtrue : qfalse;
}

/* ========== constraints ========== */

physConstraintHandle_t Phys_CreateConstraint_Impl( const physConstraintDef_t *def ) {
	BoxConstraint *pc;
	int idx;
	b3Transform frameA, frameB;

	if ( !bx.initialized || !def || bx.constraintCount >= PHYS_MAX_CONSTRAINTS ) {
		return -1;
	}
	if ( !VALID_BODY( def->bodyA ) || !VALID_BODY( def->bodyB ) ) {
		return -1;
	}

	idx = bx.constraintCount++;
	pc = &bx.constraints[idx];
	memset( pc, 0, sizeof( *pc ) );
	pc->bodyA = def->bodyA;
	pc->bodyB = def->bodyB;
	pc->type = def->type;

	frameA.p = from_vec3( def->pivotA );
	frameA.q = b3Quat_identity;
	frameB.p = from_vec3( def->pivotB );
	frameB.q = b3Quat_identity;
	if ( def->axisA[0] != 0.0f || def->axisA[1] != 0.0f || def->axisA[2] != 0.0f ) {
		frameA.q = quat_align_local_x( from_vec3( def->axisA ) );
	}
	if ( def->axisB[0] != 0.0f || def->axisB[1] != 0.0f || def->axisB[2] != 0.0f ) {
		frameB.q = quat_align_local_x( from_vec3( def->axisB ) );
	}

	switch ( def->type ) {
	case PHYS_CONSTRAINT_HINGE: {
		b3RevoluteJointDef jd = b3DefaultRevoluteJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		jd.enableLimit = true;
		jd.lowerAngle = def->lowerLimit;
		jd.upperAngle = def->upperLimit;
		if ( def->enableMotor ) {
			jd.enableMotor = true;
			jd.motorSpeed = def->motorSpeed;
			jd.maxMotorTorque = def->maxMotorForce > 0.0f ? def->maxMotorForce : 1000.0f;
		}
		pc->jointId = b3CreateRevoluteJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_SLIDER: {
		b3PrismaticJointDef jd = b3DefaultPrismaticJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		jd.enableLimit = true;
		jd.lowerTranslation = def->lowerLimit;
		jd.upperTranslation = def->upperLimit;
		if ( def->softness > 0.0f ) {
			jd.enableSpring = true;
			jd.hertz = 4.0f / def->softness;
			if ( jd.hertz < 1.0f ) {
				jd.hertz = 1.0f;
			}
			if ( jd.hertz > 30.0f ) {
				jd.hertz = 30.0f;
			}
			jd.dampingRatio = def->biasFactor > 0.0f ? def->biasFactor : 0.7f;
		}
		if ( def->enableMotor ) {
			jd.enableMotor = true;
			jd.motorSpeed = def->motorSpeed;
			jd.maxMotorForce = def->maxMotorForce > 0.0f ? def->maxMotorForce : 5000.0f;
		}
		pc->jointId = b3CreatePrismaticJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_WHEEL: {
		b3WheelJointDef jd = b3DefaultWheelJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		jd.enableSuspensionSpring = true;
		jd.suspensionHertz = def->softness > 0.0f ? ( 4.0f / def->softness ) : 4.0f;
		jd.suspensionDampingRatio = def->biasFactor > 0.0f ? def->biasFactor : 0.7f;
		jd.enableSuspensionLimit = true;
		jd.lowerSuspensionLimit = def->lowerLimit;
		jd.upperSuspensionLimit = def->upperLimit;
		if ( def->enableMotor ) {
			jd.enableSpinMotor = true;
			jd.spinSpeed = def->motorSpeed;
			jd.maxSpinTorque = def->maxMotorForce > 0.0f ? def->maxMotorForce : 500.0f;
		}
		pc->jointId = b3CreateWheelJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_MOTOR: {
		b3MotorJointDef jd = b3DefaultMotorJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		jd.linearVelocity = b3MulSV( def->motorSpeed, b3Vec3_axisX );
		jd.maxVelocityForce = def->maxMotorForce > 0.0f ? def->maxMotorForce : 1000.0f;
		jd.maxVelocityTorque = jd.maxVelocityForce;
		pc->jointId = b3CreateMotorJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_DISTANCE: {
		b3DistanceJointDef jd = b3DefaultDistanceJointDef();
		b3Vec3 wa, wb, delta;
		float len;
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		wa = b3Body_GetWorldPoint( bx.bodies[def->bodyA].bodyId, frameA.p );
		wb = b3Body_GetWorldPoint( bx.bodies[def->bodyB].bodyId, frameB.p );
		delta = b3Sub( wb, wa );
		len = b3Length( delta );
		if ( len < 1.0f ) {
			len = 1.0f;
		}
		jd.length = len;
		jd.enableSpring = true;
		jd.hertz = def->softness > 0.0f ? ( 4.0f / def->softness ) : 8.0f;
		if ( jd.hertz < 1.0f ) {
			jd.hertz = 1.0f;
		}
		if ( jd.hertz > 30.0f ) {
			jd.hertz = 30.0f;
		}
		jd.dampingRatio = def->biasFactor > 0.0f ? def->biasFactor : 0.7f;
		jd.enableLimit = true;
		jd.minLength = def->lowerLimit > 0.0f ? def->lowerLimit : ( len * 0.5f );
		jd.maxLength = def->upperLimit > jd.minLength ? def->upperLimit : ( len * 1.5f );
		pc->jointId = b3CreateDistanceJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_FIXED:
	case PHYS_CONSTRAINT_GENERIC_6DOF: {
		b3WeldJointDef jd = b3DefaultWeldJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		pc->jointId = b3CreateWeldJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_FILTER: {
		b3FilterJointDef jd = b3DefaultFilterJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.collideConnected = false;
		pc->jointId = b3CreateFilterJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_PARALLEL: {
		b3ParallelJointDef jd = b3DefaultParallelJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		if ( def->springHertz > 0.0f ) {
			jd.hertz = def->springHertz;
		}
		if ( def->springDamping > 0.0f ) {
			jd.dampingRatio = def->springDamping;
		}
		if ( def->maxSpringTorque > 0.0f ) {
			jd.maxTorque = def->maxSpringTorque;
		}
		pc->jointId = b3CreateParallelJoint( bx.worldId, &jd );
		break;
	}
	case PHYS_CONSTRAINT_POINT:
	case PHYS_CONSTRAINT_CONE_TWIST:
	default: {
		b3SphericalJointDef jd = b3DefaultSphericalJointDef();
		jd.base.bodyIdA = bx.bodies[def->bodyA].bodyId;
		jd.base.bodyIdB = bx.bodies[def->bodyB].bodyId;
		jd.base.localFrameA = frameA;
		jd.base.localFrameB = frameB;
		jd.base.collideConnected = !def->disableCollision;
		jd.enableConeLimit = true;
		jd.coneAngle = def->coneAngle > 0.0f ? def->coneAngle : 0.5f;
		if ( def->twistLower != 0.0f || def->twistUpper != 0.0f ) {
			jd.enableTwistLimit = true;
			jd.lowerTwistAngle = def->twistLower;
			jd.upperTwistAngle = def->twistUpper;
		}
		if ( def->springHertz > 0.0f ) {
			jd.enableSpring = true;
			jd.hertz = def->springHertz;
			jd.dampingRatio = def->springDamping > 0.0f ? def->springDamping : 0.7f;
		}
		pc->jointId = b3CreateSphericalJoint( bx.worldId, &jd );
		break;
	}
	}

	pc->breakForce = def->breakForce;
	pc->breakTorque = def->breakTorque;
	b3Joint_SetUserData( pc->jointId, (void *)(intptr_t)( idx + 1 ) );
	if ( def->breakForce > 0.0f ) {
		b3Joint_SetForceThreshold( pc->jointId, def->breakForce );
	}
	if ( def->breakTorque > 0.0f ) {
		b3Joint_SetTorqueThreshold( pc->jointId, def->breakTorque );
	}
	pc->active = qtrue;
	return idx;
}

void Phys_DestroyConstraint_Impl( physConstraintHandle_t h ) {
	if ( !VALID_CON( h ) ) {
		return;
	}
	b3DestroyJoint( bx.constraints[h].jointId, true );
	memset( &bx.constraints[h], 0, sizeof( bx.constraints[h] ) );
}

void Phys_SetConstraintLimits_Impl( physConstraintHandle_t h, float lo, float hi ) {
	if ( !VALID_CON( h ) ) {
		return;
	}
	if ( bx.constraints[h].type == PHYS_CONSTRAINT_HINGE ) {
		b3RevoluteJoint_SetLimits( bx.constraints[h].jointId, lo, hi );
	} else if ( bx.constraints[h].type == PHYS_CONSTRAINT_SLIDER ) {
		b3PrismaticJoint_SetLimits( bx.constraints[h].jointId, lo, hi );
	}
}

void Phys_SetConstraintMotor_Impl( physConstraintHandle_t h, qboolean enable, float speed, float maxForce ) {
	b3JointId jid;
	if ( !VALID_CON( h ) ) {
		return;
	}
	jid = bx.constraints[h].jointId;
	switch ( bx.constraints[h].type ) {
	case PHYS_CONSTRAINT_HINGE:
		b3RevoluteJoint_EnableMotor( jid, enable ? true : false );
		b3RevoluteJoint_SetMotorSpeed( jid, speed );
		if ( maxForce > 0.0f ) {
			b3RevoluteJoint_SetMaxMotorTorque( jid, maxForce );
		}
		break;
	case PHYS_CONSTRAINT_SLIDER:
		b3PrismaticJoint_EnableMotor( jid, enable ? true : false );
		b3PrismaticJoint_SetMotorSpeed( jid, speed );
		if ( maxForce > 0.0f ) {
			b3PrismaticJoint_SetMaxMotorForce( jid, maxForce );
		}
		break;
	case PHYS_CONSTRAINT_WHEEL:
		b3WheelJoint_EnableSpinMotor( jid, enable ? true : false );
		b3WheelJoint_SetSpinMotorSpeed( jid, speed );
		if ( maxForce > 0.0f ) {
			b3WheelJoint_SetMaxSpinTorque( jid, maxForce );
		}
		break;
	case PHYS_CONSTRAINT_MOTOR:
		b3MotorJoint_SetLinearVelocity( jid, b3MulSV( speed, b3Vec3_axisX ) );
		if ( maxForce > 0.0f ) {
			b3MotorJoint_SetMaxVelocityForce( jid, maxForce );
			b3MotorJoint_SetMaxVelocityTorque( jid, maxForce );
		}
		(void)enable;
		break;
	default:
		break;
	}
}

void Phys_SetConstraintBreakForce_Impl( physConstraintHandle_t h, float force, float torque ) {
	if ( !VALID_CON( h ) ) {
		return;
	}
	bx.constraints[h].breakForce = force;
	bx.constraints[h].breakTorque = torque;
	if ( force > 0.0f ) {
		b3Joint_SetForceThreshold( bx.constraints[h].jointId, force );
	} else {
		b3Joint_SetForceThreshold( bx.constraints[h].jointId, FLT_MAX );
	}
	if ( torque > 0.0f ) {
		b3Joint_SetTorqueThreshold( bx.constraints[h].jointId, torque );
	} else {
		b3Joint_SetTorqueThreshold( bx.constraints[h].jointId, FLT_MAX );
	}
}

void Phys_SetWheelSteering_Impl( physConstraintHandle_t h, float angleRadians, float maxTorque ) {
	if ( !VALID_CON( h ) || bx.constraints[h].type != PHYS_CONSTRAINT_WHEEL ) {
		return;
	}
	b3WheelJoint_EnableSteering( bx.constraints[h].jointId, true );
	b3WheelJoint_SetTargetSteeringAngle( bx.constraints[h].jointId, angleRadians );
	if ( maxTorque > 0.0f ) {
		b3WheelJoint_SetMaxSteeringTorque( bx.constraints[h].jointId, maxTorque );
	}
}

void Phys_SetConstraintSpring_Impl( physConstraintHandle_t h, qboolean enable, float hertz, float dampingRatio ) {
	b3JointId jid;
	if ( !VALID_CON( h ) ) {
		return;
	}
	jid = bx.constraints[h].jointId;
	switch ( bx.constraints[h].type ) {
	case PHYS_CONSTRAINT_HINGE:
		b3RevoluteJoint_EnableSpring( jid, enable ? true : false );
		if ( hertz > 0.0f ) {
			b3RevoluteJoint_SetSpringHertz( jid, hertz );
		}
		if ( dampingRatio > 0.0f ) {
			b3RevoluteJoint_SetSpringDampingRatio( jid, dampingRatio );
		}
		break;
	case PHYS_CONSTRAINT_SLIDER:
		b3PrismaticJoint_EnableSpring( jid, enable ? true : false );
		if ( hertz > 0.0f ) {
			b3PrismaticJoint_SetSpringHertz( jid, hertz );
		}
		if ( dampingRatio > 0.0f ) {
			b3PrismaticJoint_SetSpringDampingRatio( jid, dampingRatio );
		}
		break;
	case PHYS_CONSTRAINT_DISTANCE:
		b3DistanceJoint_EnableSpring( jid, enable ? true : false );
		if ( hertz > 0.0f ) {
			b3DistanceJoint_SetSpringHertz( jid, hertz );
		}
		if ( dampingRatio > 0.0f ) {
			b3DistanceJoint_SetSpringDampingRatio( jid, dampingRatio );
		}
		break;
	case PHYS_CONSTRAINT_POINT:
	case PHYS_CONSTRAINT_CONE_TWIST:
		b3SphericalJoint_EnableSpring( jid, enable ? true : false );
		if ( hertz > 0.0f ) {
			b3SphericalJoint_SetSpringHertz( jid, hertz );
		}
		if ( dampingRatio > 0.0f ) {
			b3SphericalJoint_SetSpringDampingRatio( jid, dampingRatio );
		}
		break;
	case PHYS_CONSTRAINT_PARALLEL:
		if ( hertz > 0.0f ) {
			b3ParallelJoint_SetSpringHertz( jid, hertz );
		}
		if ( dampingRatio > 0.0f ) {
			b3ParallelJoint_SetSpringDampingRatio( jid, dampingRatio );
		}
		(void)enable;
		break;
	default:
		break;
	}
}

void Phys_SetSphericalLimits_Impl( physConstraintHandle_t h, float coneAngleRadians,
	float twistLowerRadians, float twistUpperRadians ) {
	b3JointId jid;
	if ( !VALID_CON( h ) ) {
		return;
	}
	if ( bx.constraints[h].type != PHYS_CONSTRAINT_POINT
		&& bx.constraints[h].type != PHYS_CONSTRAINT_CONE_TWIST ) {
		return;
	}
	jid = bx.constraints[h].jointId;
	if ( coneAngleRadians > 0.0f ) {
		b3SphericalJoint_EnableConeLimit( jid, true );
		b3SphericalJoint_SetConeLimit( jid, coneAngleRadians );
	}
	b3SphericalJoint_EnableTwistLimit( jid, true );
	b3SphericalJoint_SetTwistLimits( jid, twistLowerRadians, twistUpperRadians );
}

void Phys_GetConstraintReaction_Impl( physConstraintHandle_t h, vec3_t forceOut, vec3_t torqueOut ) {
	b3Vec3 f, t;
	if ( forceOut ) {
		VectorClear( forceOut );
	}
	if ( torqueOut ) {
		VectorClear( torqueOut );
	}
	if ( !VALID_CON( h ) ) {
		return;
	}
	f = b3Joint_GetConstraintForce( bx.constraints[h].jointId );
	t = b3Joint_GetConstraintTorque( bx.constraints[h].jointId );
	if ( forceOut ) {
		to_vec3( f, forceOut );
	}
	if ( torqueOut ) {
		to_vec3( t, torqueOut );
	}
}

/* ========== ragdoll (Euphoria substrate) ========== */

physRagdollHandle_t Phys_CreateRagdoll_Impl( const physRagdollDef_t *def ) {
	BoxRagdoll *rag;
	int idx;
	float scale;
	float limbMass;
	int b, j;
	/* Z-up bone layout (Quake) — fallback when def->numBones == 0 */
	struct { float radius; float height; float zOff; } boneSpec[] = {
		{ 8, 20, 0 }, { 6, 16, 28 }, { 5, 10, 48 },
		{ 4, 14, 20 }, { 3, 12, 10 }, { 4, 14, 20 }, { 3, 12, 10 },
		{ 4, 16, -10 }, { 3, 14, -28 }, { 4, 16, -10 }, { 3, 14, -28 },
	};
	int pairs[][2] = { {0,1},{1,2},{0,3},{3,4},{0,5},{5,6},{0,7},{7,8},{0,9},{9,10} };
	int useBind;

	if ( !bx.initialized || !def || bx.ragdollCount >= PHYS_MAX_RAGDOLLS ) {
		return -1;
	}

	idx = bx.ragdollCount++;
	rag = &bx.ragdolls[idx];
	memset( rag, 0, sizeof( *rag ) );
	rag->muscleStiffness = def->jointStiffness > 0 ? def->jointStiffness : 0.8f;
	rag->muscleDamping = def->jointDamping > 0 ? def->jointDamping : 0.4f;
	rag->muscleStrength = def->muscleStrength > 0 ? def->muscleStrength : 1.0f;
	rag->balanceForce = def->balanceForce > 0 ? def->balanceForce : 100.0f;
	rag->reachForce = def->reachForce > 0 ? def->reachForce : 1.0f;
	rag->impactResponse = def->impactResponse > 0 ? def->impactResponse : 1.0f;
	rag->active = qtrue;

	scale = def->scale > 0 ? def->scale : 1.0f;
	limbMass = def->limbMass > 0 ? def->limbMass : 5.0f;
	useBind = ( def->numBones > 0 && def->numBones <= PHYS_RAGDOLL_MAX_BONES ) ? 1 : 0;
	rag->numBones = useBind ? def->numBones : 11;

	for ( b = 0; b < rag->numBones; b++ ) {
		b3BodyDef bd = b3DefaultBodyDef();
		b3ShapeDef sd = b3DefaultShapeDef();
		b3Capsule cap;
		float h, r, vol;
		vec3_t origin;

		bd.type = b3_dynamicBody;
		origin[0] = def->rootPosition[0];
		origin[1] = def->rootPosition[1];
		origin[2] = def->rootPosition[2];
		if ( useBind ) {
			h = def->bones[b].height * scale;
			r = def->bones[b].radius * scale;
			origin[0] += def->bones[b].localOffset[0] * scale;
			origin[1] += def->bones[b].localOffset[1] * scale;
			origin[2] += def->bones[b].localOffset[2] * scale;
		} else {
			h = boneSpec[b].height * scale;
			r = boneSpec[b].radius * scale;
			origin[2] += boneSpec[b].zOff * scale;
		}
		if ( h <= 0.0f ) {
			h = 12.0f * scale;
		}
		if ( r <= 0.0f ) {
			r = 4.0f * scale;
		}
		bd.position = from_vec3( origin );
		bd.linearDamping = 0.15f;
		bd.angularDamping = 0.35f;
		bd.enableSleep = false;
		rag->bones[b].bodyId = b3CreateBody( bx.worldId, &bd );
		b3Body_SetUserData( rag->bones[b].bodyId, BOX_UD_PACK_RAG( idx, b ) );

		cap.center1 = v3( 0.0f, 0.0f, -0.5f * h );
		cap.center2 = v3( 0.0f, 0.0f, 0.5f * h );
		cap.radius = r;
		vol = (float)M_PI * r * r * h + ( 4.0f / 3.0f ) * (float)M_PI * r * r * r;
		sd.density = ( vol > 0.001f ) ? ( limbMass / vol ) : 1.0f;
		sd.baseMaterial.friction = 0.6f;
		if ( !def->selfCollision ) {
			sd.filter.groupIndex = -( idx + 1 );
		}
		rag->bones[b].shapeId = b3CreateCapsuleShape( rag->bones[b].bodyId, &sd, &cap );
	}

	if ( useBind ) {
		rag->numJoints = 0;
		for ( b = 0; b < rag->numBones; b++ ) {
			int parent = def->bones[b].parent;
			b3SphericalJointDef jd;
			if ( parent < 0 || parent >= rag->numBones || parent == b ) {
				continue;
			}
			jd = b3DefaultSphericalJointDef();
			jd.base.bodyIdA = rag->bones[parent].bodyId;
			jd.base.bodyIdB = rag->bones[b].bodyId;
			jd.base.localFrameA.p = b3Vec3_zero;
			jd.base.localFrameA.q = b3Quat_identity;
			jd.base.localFrameB.p = b3Vec3_zero;
			jd.base.localFrameB.q = b3Quat_identity;
			jd.base.collideConnected = false;
			jd.enableConeLimit = true;
			jd.coneAngle = 0.5f;
			jd.enableSpring = true;
			jd.hertz = 2.0f + rag->muscleStiffness * 8.0f;
			jd.dampingRatio = rag->muscleDamping;
			rag->joints[rag->numJoints++] = b3CreateSphericalJoint( bx.worldId, &jd );
		}
	} else {
		rag->numJoints = 10;
		for ( j = 0; j < 10; j++ ) {
			b3SphericalJointDef jd = b3DefaultSphericalJointDef();
			jd.base.bodyIdA = rag->bones[pairs[j][0]].bodyId;
			jd.base.bodyIdB = rag->bones[pairs[j][1]].bodyId;
			jd.base.localFrameA.p = b3Vec3_zero;
			jd.base.localFrameA.q = b3Quat_identity;
			jd.base.localFrameB.p = b3Vec3_zero;
			jd.base.localFrameB.q = b3Quat_identity;
			jd.base.collideConnected = false;
			jd.enableConeLimit = true;
			jd.coneAngle = 0.5f;
			jd.enableSpring = true;
			jd.hertz = 2.0f + rag->muscleStiffness * 8.0f;
			jd.dampingRatio = rag->muscleDamping;
			rag->joints[j] = b3CreateSphericalJoint( bx.worldId, &jd );
		}
	}
	return idx;
}

void Phys_DestroyRagdoll_Impl( physRagdollHandle_t h ) {
	BoxRagdoll *rag;
	int i;

	if ( !VALID_RAG( h ) ) {
		return;
	}
	rag = &bx.ragdolls[h];
	for ( i = 0; i < rag->numJoints; i++ ) {
		b3DestroyJoint( rag->joints[i], false );
	}
	for ( i = 0; i < rag->numBones; i++ ) {
		b3DestroyBody( rag->bones[i].bodyId );
	}
	memset( rag, 0, sizeof( *rag ) );
}

void Phys_RagdollApplyImpact_Impl( physRagdollHandle_t h, const vec3_t pt, const vec3_t imp, float radius ) {
	int b;
	float response;

	if ( !VALID_RAG( h ) ) {
		return;
	}
	response = bx.ragdolls[h].impactResponse > 0.0f ? bx.ragdolls[h].impactResponse : 1.0f;
	for ( b = 0; b < bx.ragdolls[h].numBones; b++ ) {
		b3Vec3 pos = b3Body_GetPosition( bx.ragdolls[h].bones[b].bodyId );
		b3Vec3 d = b3Sub( pos, from_vec3( pt ) );
		float dist = b3Length( d );
		float scale = response;
		if ( radius > 0.0f && dist > radius ) {
			continue;
		}
		if ( radius > 0.0f ) {
			scale = response * ( 1.0f - dist / radius );
		}
		b3Body_ApplyLinearImpulseToCenter( bx.ragdolls[h].bones[b].bodyId,
			v3( imp[0] * scale, imp[1] * scale, imp[2] * scale ), true );
	}
}

void Phys_RagdollSetBalance_Impl( physRagdollHandle_t h, qboolean en, const vec3_t tgt ) {
	if ( !VALID_RAG( h ) ) {
		return;
	}
	bx.ragdolls[h].balanceEnabled = en;
	if ( tgt ) {
		bx.ragdolls[h].balanceTarget = from_vec3( tgt );
	}
}

void Phys_RagdollReach_Impl( physRagdollHandle_t h, int limb, const vec3_t tgt, float str ) {
	b3Vec3 pos, dir;
	float strength;

	if ( !VALID_RAG( h ) || limb < 0 || limb >= bx.ragdolls[h].numBones ) {
		return;
	}
	strength = str * ( bx.ragdolls[h].reachForce > 0.0f ? bx.ragdolls[h].reachForce : 1.0f );
	strength *= ( bx.ragdolls[h].muscleStrength > 0.0f ? bx.ragdolls[h].muscleStrength : 1.0f );
	pos = b3Body_GetPosition( bx.ragdolls[h].bones[limb].bodyId );
	dir = b3Sub( from_vec3( tgt ), pos );
	if ( b3Length( dir ) > 0.1f ) {
		b3Body_ApplyForceToCenter( bx.ragdolls[h].bones[limb].bodyId,
			b3MulSV( strength, b3Normalize( dir ) ), true );
	}
}

void Phys_RagdollGetBoneTransform_Impl( physRagdollHandle_t h, int bone, physTransform_t *out ) {
	b3Vec3 p, lv, av;
	b3Quat q;

	if ( !out ) {
		return;
	}
	memset( out, 0, sizeof( *out ) );
	if ( !VALID_RAG( h ) || bone < 0 || bone >= bx.ragdolls[h].numBones ) {
		return;
	}
	p = b3Body_GetPosition( bx.ragdolls[h].bones[bone].bodyId );
	q = b3Body_GetRotation( bx.ragdolls[h].bones[bone].bodyId );
	lv = b3Body_GetLinearVelocity( bx.ragdolls[h].bones[bone].bodyId );
	av = b3Body_GetAngularVelocity( bx.ragdolls[h].bones[bone].bodyId );
	to_vec3( p, out->position );
	euler_from_quat( q, out->rotation );
	to_vec3( lv, out->linearVelocity );
	to_vec3( av, out->angularVelocity );
}

void Phys_RagdollSetMuscleStiffness_Impl( physRagdollHandle_t h, float s ) {
	int j;

	if ( !VALID_RAG( h ) ) {
		return;
	}
	bx.ragdolls[h].muscleStiffness = s;
	for ( j = 0; j < bx.ragdolls[h].numJoints; j++ ) {
		b3SphericalJoint_EnableSpring( bx.ragdolls[h].joints[j], true );
		b3SphericalJoint_SetSpringHertz( bx.ragdolls[h].joints[j], 2.0f + s * 8.0f );
		b3SphericalJoint_SetSpringDampingRatio( bx.ragdolls[h].joints[j], bx.ragdolls[h].muscleDamping );
	}
}

void Phys_RagdollBlendToAnimation_Impl( physRagdollHandle_t h, float blend ) {
	if ( !VALID_RAG( h ) ) {
		return;
	}
	bx.ragdolls[h].animBlend = blend < 0 ? 0 : ( blend > 1 ? 1 : blend );
}

void Phys_RagdollSetBoneAnimTarget_Impl( physRagdollHandle_t h, int bone,
	const vec3_t position, const vec3_t rotationDeg ) {
	vec3_t zeroRot = { 0, 0, 0 };
	if ( !VALID_RAG( h ) || bone < 0 || bone >= bx.ragdolls[h].numBones || !position ) {
		return;
	}
	bx.ragdolls[h].animTargetPos[bone] = from_vec3( position );
	bx.ragdolls[h].animTargetRot[bone] = quat_from_euler_deg( rotationDeg ? rotationDeg : zeroRot );
	bx.ragdolls[h].animTargetValid[bone] = qtrue;
	bx.ragdolls[h].hasAnimTargets = qtrue;
}

void Phys_RagdollClearAnimTargets_Impl( physRagdollHandle_t h ) {
	int i;
	if ( !VALID_RAG( h ) ) {
		return;
	}
	bx.ragdolls[h].hasAnimTargets = qfalse;
	for ( i = 0; i < PHYS_RAGDOLL_MAX_BONES; i++ ) {
		bx.ragdolls[h].animTargetValid[i] = qfalse;
	}
}

void Phys_RagdollApplyBoneTorque_Impl( physRagdollHandle_t h, int bone, const vec3_t torque ) {
	if ( !VALID_RAG( h ) || bone < 0 || bone >= bx.ragdolls[h].numBones || !torque ) {
		return;
	}
	b3Body_ApplyTorque( bx.ragdolls[h].bones[bone].bodyId, from_vec3( torque ), true );
}

int Phys_GetRagdollCount_Impl( void ) {
	int i, n = 0;
	for ( i = 0; i < bx.ragdollCount; i++ ) {
		if ( bx.ragdolls[i].active ) {
			n++;
		}
	}
	return n;
}

/* Phys_GetBodyContacts_Impl declared in phys_impl.h */

dmmObjectHandle_t Dmm_CreateObject_Impl( const dmmObjectDef_t *def ) {
	BoxDmmObject *dmm;
	physBodyDef_t bodyDef;
	int idx;
	int res;
	float stiff, yield, frac;
	cvar_t *resCv;
	cvar_t *dmmEn;

	if ( !bx.initialized || !def || bx.dmmCount >= PHYS_MAX_DMM_OBJECTS ) {
		return -1;
	}
	dmmEn = Cvar_Get( "phys_dmm_enabled", "1", CVAR_ARCHIVE );
	if ( dmmEn && !dmmEn->integer ) {
		Com_Printf( "[physics] DMM disabled (phys_dmm_enabled 0)\n" );
		return -1;
	}
	idx = bx.dmmCount++;
	dmm = &bx.dmmObjects[idx];
	memset( dmm, 0, sizeof( *dmm ) );
	dmm->material = def->material;
	dmm->deformability = def->deformability > 0 ? def->deformability : 1.0f;
	dmm->body = -1;
	dmm->numFragments = 0;
	dmm->entityNum = def->entityNum;
	VectorCopy( def->position, dmm->position );
	dmm->halfExtents[0] = def->dimensions[0] > 0.0f ? def->dimensions[0] * 0.5f : 16.0f;
	dmm->halfExtents[1] = def->dimensions[1] > 0.0f ? def->dimensions[1] * 0.5f : 16.0f;
	dmm->halfExtents[2] = def->dimensions[2] > 0.0f ? def->dimensions[2] * 0.5f : 16.0f;
	dmm->density = def->density > 0.0f ? def->density : 1.0f;
	if ( def->stiffness > 0 ) {
		stiff = def->stiffness;
		yield = def->yieldStrength;
		frac = def->fractureStrength;
	} else {
		get_dmm_preset( def->material, &stiff, &yield, &frac );
	}
	(void)stiff;
	dmm->yieldStrength = yield;
	dmm->fractureStrength = frac;
	dmm->integrity = 1.0f;
	resCv = Cvar_Get( "phys_dmm_resolution", "8", CVAR_ARCHIVE );
	res = def->gridResolution > 0 ? def->gridResolution : ( resCv ? resCv->integer : 8 );
	if ( res < 2 ) {
		res = 2;
	}
	if ( res > 16 ) {
		res = 16;
	}
	dmm->numElements = res * res * res;
	dmm->elements = (BoxDmmElement *)calloc( (size_t)dmm->numElements, sizeof( BoxDmmElement ) );

	/* Soft Step rigid proxy — DMM-like continuum is stress on this body */
	memset( &bodyDef, 0, sizeof( bodyDef ) );
	bodyDef.shape = PHYS_SHAPE_BOX;
	bodyDef.type = PHYS_BODY_DYNAMIC;
	VectorCopy( def->position, bodyDef.position );
	VectorCopy( def->rotation, bodyDef.rotation );
	VectorCopy( dmm->halfExtents, bodyDef.halfExtents );
	bodyDef.mass = dmm->halfExtents[0] * dmm->halfExtents[1] * dmm->halfExtents[2] * dmm->density * 0.001f;
	if ( bodyDef.mass < 1.0f ) {
		bodyDef.mass = 40.0f;
	}
	bodyDef.friction = 0.7f;
	bodyDef.restitution = 0.05f;
	bodyDef.linearDamping = 0.05f;
	bodyDef.angularDamping = 0.1f;
	bodyDef.materialId = (int)def->material;
	dmm->body = Phys_CreateBody_Impl( &bodyDef );
	dmm->active = qtrue;
	Com_Printf( "[physics] Soft Step DMM object %d body=%d mat=%d (stress grid %d)\n",
		idx, dmm->body, (int)def->material, dmm->numElements );
	return idx;
}

void Dmm_DestroyObject_Impl( dmmObjectHandle_t h ) {
	int i;
	if ( !VALID_DMM( h ) ) {
		return;
	}
	if ( bx.dmmObjects[h].body >= 0 ) {
		Phys_DestroyBody_Impl( bx.dmmObjects[h].body );
	}
	for ( i = 0; i < bx.dmmObjects[h].numFragments; i++ ) {
		if ( bx.dmmObjects[h].fragments[i] >= 0 ) {
			Phys_DestroyBody_Impl( bx.dmmObjects[h].fragments[i] );
		}
	}
	free( bx.dmmObjects[h].elements );
	memset( &bx.dmmObjects[h], 0, sizeof( bx.dmmObjects[h] ) );
}

void Dmm_ApplyForce_Impl( dmmObjectHandle_t h, const vec3_t force, const vec3_t point ) {
	if ( !VALID_DMM( h ) || bx.dmmObjects[h].body < 0 ) {
		return;
	}
	Phys_ApplyForce_Impl( bx.dmmObjects[h].body, force, point );
}

void Dmm_ApplyImpact_Impl( dmmObjectHandle_t h, const vec3_t point, const vec3_t direction, float energy ) {
	BoxDmmObject *dmm;
	float stress;
	int i;
	vec3_t impulse, dir;

	if ( !VALID_DMM( h ) || bx.dmmObjects[h].fractured ) {
		return;
	}
	dmm = &bx.dmmObjects[h];
	stress = energy / ( dmm->deformability > 0.0f ? dmm->deformability : 1.0f );
	for ( i = 0; i < dmm->numElements; i++ ) {
		dmm->elements[i].strain += stress * 0.5f;
		dmm->elements[i].stress = dmm->elements[i].strain * ( 1.0f - dmm->elements[i].plasticity );
	}
	if ( dmm->body >= 0 ) {
		VectorCopy( direction, dir );
		if ( VectorLength( dir ) < 0.001f ) {
			VectorSet( dir, 0.0f, 0.0f, 1.0f );
		} else {
			VectorNormalize( dir );
		}
		VectorScale( dir, energy * 0.15f, impulse );
		Phys_ApplyImpulse_Impl( dmm->body, impulse, point );
	}
}

void Dmm_GetState_Impl( dmmObjectHandle_t h, dmmState_t *out ) {
	BoxDmmObject *dmm;
	float maxStrain = 0.0f;
	int i;

	if ( !out ) {
		return;
	}
	memset( out, 0, sizeof( *out ) );
	if ( !VALID_DMM( h ) ) {
		return;
	}
	dmm = &bx.dmmObjects[h];
	for ( i = 0; i < dmm->numElements; i++ ) {
		if ( dmm->elements[i].strain > maxStrain ) {
			maxStrain = dmm->elements[i].strain;
		}
	}
	out->strain = maxStrain;
	out->stress = maxStrain;
	out->deformation = 1.0f - dmm->integrity;
	out->integrity = dmm->integrity;
	out->fractured = dmm->fractured;
	out->numFragments = dmm->numFragments;
}

qboolean Dmm_IsFractured_Impl( dmmObjectHandle_t h ) {
	return ( VALID_DMM( h ) && bx.dmmObjects[h].fractured ) ? qtrue : qfalse;
}

int Dmm_GetFragments_Impl( dmmObjectHandle_t h, physBodyHandle_t *fragments, int maxFragments ) {
	BoxDmmObject *dmm;
	int n, i;

	if ( !VALID_DMM( h ) || !fragments || maxFragments <= 0 ) {
		return 0;
	}
	dmm = &bx.dmmObjects[h];
	n = dmm->numFragments < maxFragments ? dmm->numFragments : maxFragments;
	for ( i = 0; i < n; i++ ) {
		fragments[i] = dmm->fragments[i];
	}
	return n;
}

void Dmm_SetMaterialParams_Impl( dmmObjectHandle_t h, float stiffness, float yield, float fracture ) {
	(void)stiffness;
	if ( !VALID_DMM( h ) ) {
		return;
	}
	bx.dmmObjects[h].yieldStrength = yield;
	bx.dmmObjects[h].fractureStrength = fracture;
}

/*
===============
Dmm_SpawnFragments_Impl
Internal Soft Step debris spawn used by enhanced Dmm_Fracture path.
===============
*/
int Dmm_SpawnFragments_Impl( dmmObjectHandle_t h, const vec3_t impactPoint, float energy ) {
	BoxDmmObject *dmm;
	cvar_t *fracCv;
	int f, count;
	float hx, hy, hz;

	if ( !VALID_DMM( h ) ) {
		return 0;
	}
	fracCv = Cvar_Get( "phys_dmm_fracture", "1", CVAR_ARCHIVE );
	if ( fracCv && !fracCv->integer ) {
		bx.dmmObjects[h].fractured = qtrue;
		return 0;
	}
	dmm = &bx.dmmObjects[h];
	dmm->fractured = qtrue;
	if ( dmm->body >= 0 ) {
		Phys_DestroyBody_Impl( dmm->body );
		dmm->body = -1;
	}
	hx = dmm->halfExtents[0];
	hy = dmm->halfExtents[1];
	hz = dmm->halfExtents[2];
	count = 8;
	if ( count > (int)( sizeof( dmm->fragments ) / sizeof( dmm->fragments[0] ) ) ) {
		count = (int)( sizeof( dmm->fragments ) / sizeof( dmm->fragments[0] ) );
	}
	for ( f = 0; f < count; f++ ) {
		physBodyDef_t fragDef;
		vec3_t offset, impulse, zero;
		float sx = ( f & 1 ) ? 0.5f : -0.5f;
		float sy = ( f & 2 ) ? 0.5f : -0.5f;
		float sz = ( f & 4 ) ? 0.5f : -0.5f;

		memset( &fragDef, 0, sizeof( fragDef ) );
		fragDef.shape = PHYS_SHAPE_BOX;
		fragDef.type = PHYS_BODY_DYNAMIC;
		fragDef.position[0] = dmm->position[0] + sx * hx * 0.5f;
		fragDef.position[1] = dmm->position[1] + sy * hy * 0.5f;
		fragDef.position[2] = dmm->position[2] + sz * hz * 0.5f;
		fragDef.halfExtents[0] = hx * 0.45f;
		fragDef.halfExtents[1] = hy * 0.45f;
		fragDef.halfExtents[2] = hz * 0.45f;
		fragDef.mass = 8.0f + (float)f;
		fragDef.friction = 0.6f;
		fragDef.restitution = 0.15f;
		dmm->fragments[f] = Phys_CreateBody_Impl( &fragDef );
		if ( dmm->fragments[f] >= 0 ) {
			VectorSet( offset, sx, sy, sz );
			VectorNormalize( offset );
			VectorScale( offset, energy * 0.08f * ( 0.5f + 0.1f * (float)f ), impulse );
			VectorClear( zero );
			Phys_ApplyImpulse_Impl( dmm->fragments[f], impulse, zero );
			(void)impactPoint;
		}
	}
	dmm->numFragments = count;
	Com_Printf( "[physics] Soft Step DMM %d → %d debris bodies\n", h, count );
	return count;
}

/* ========== queries ========== */

typedef struct {
	physRayResult_t *result;
	float            closest;
	qboolean         hit;
} boxRayCtx;

static b3QueryFilter box_make_query_filter( const physQueryFilter_t *filter ) {
	b3QueryFilter f = b3DefaultQueryFilter();
	if ( filter ) {
		if ( filter->categoryBits ) {
			f.categoryBits = (uint64_t)filter->categoryBits;
		}
		if ( filter->maskBits ) {
			f.maskBits = (uint64_t)filter->maskBits;
		}
	}
	return f;
}

static float box_ray_cb( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
	int triangleIndex, int childIndex, void *context ) {
	boxRayCtx *ctx = (boxRayCtx *)context;
	b3BodyId bodyId;
	void *ud;
	int handle;

	if ( fraction < ctx->closest ) {
		ctx->closest = fraction;
		ctx->hit = qtrue;
		to_vec3( point, ctx->result->hitPoint );
		to_vec3( normal, ctx->result->hitNormal );
		ctx->result->fraction = fraction;
		ctx->result->userMaterialId = (unsigned)userMaterialId;
		ctx->result->triangleIndex = triangleIndex;
		ctx->result->childIndex = childIndex;
		bodyId = b3Shape_GetBody( shapeId );
		ud = b3Body_GetUserData( bodyId );
		handle = (int)(intptr_t)ud - 1;
		ctx->result->body = handle;
		ctx->result->hit = qtrue;
	}
	return fraction;
}

qboolean Phys_RayCastFiltered_Impl( const vec3_t from, const vec3_t to, physRayResult_t *result,
	const physQueryFilter_t *filter ) {
	b3Vec3 origin, translation;
	b3QueryFilter qf;
	b3RayResult hit;

	if ( !result ) {
		return qfalse;
	}
	memset( result, 0, sizeof( *result ) );
	result->body = -1;
	result->triangleIndex = -1;
	result->childIndex = -1;
	if ( !bx.initialized ) {
		return qfalse;
	}
	origin = from_vec3( from );
	translation = b3Sub( from_vec3( to ), origin );
	qf = box_make_query_filter( filter );
	hit = b3World_CastRayClosest( bx.worldId, origin, translation, qf );
	if ( !hit.hit ) {
		return qfalse;
	}
	to_vec3( hit.point, result->hitPoint );
	to_vec3( hit.normal, result->hitNormal );
	result->fraction = hit.fraction;
	result->hit = qtrue;
	result->userMaterialId = (unsigned)hit.userMaterialId;
	result->triangleIndex = hit.triangleIndex;
	result->childIndex = hit.childIndex;
	if ( b3Shape_IsValid( hit.shapeId ) ) {
		void *ud = b3Body_GetUserData( b3Shape_GetBody( hit.shapeId ) );
		if ( ud ) {
			result->body = (int)(intptr_t)ud - 1;
		}
	}
	return qtrue;
}

qboolean Phys_RayCast_Impl( const vec3_t from, const vec3_t to, physRayResult_t *result ) {
	return Phys_RayCastFiltered_Impl( from, to, result, NULL );
}

qboolean Phys_ConvexSweepFiltered_Impl( const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result, const physQueryFilter_t *filter ) {
	b3Vec3 points[B3_MAX_SHAPE_CAST_POINTS];
	b3ShapeProxy proxy;
	b3Vec3 origin, translation;
	b3QueryFilter qf;
	b3Quat q;
	boxRayCtx ctx;
	int i;
	vec3_t rotZero = { 0, 0, 0 };

	if ( !result ) {
		return qfalse;
	}
	memset( result, 0, sizeof( *result ) );
	result->body = -1;
	result->triangleIndex = -1;
	result->childIndex = -1;
	if ( !bx.initialized || !shapeDef ) {
		return qfalse;
	}

	q = quat_from_euler_deg( rotation ? rotation : rotZero );
	proxy.points = points;
	proxy.count = 0;
	proxy.radius = 0.0f;

	switch ( shapeDef->shape ) {
	case PHYS_SHAPE_SPHERE: {
		points[0] = b3Vec3_zero;
		proxy.count = 1;
		proxy.radius = shapeDef->radius > 0.0f ? shapeDef->radius : 8.0f;
		break;
	}
	case PHYS_SHAPE_CAPSULE:
	case PHYS_SHAPE_CYLINDER: {
		float h = shapeDef->height > 0.0f ? shapeDef->height : 32.0f;
		float r = shapeDef->radius > 0.0f ? shapeDef->radius : 8.0f;
		b3Vec3 local1 = v3( 0.0f, 0.0f, -0.5f * h );
		b3Vec3 local2 = v3( 0.0f, 0.0f, 0.5f * h );
		points[0] = b3RotateVector( q, local1 );
		points[1] = b3RotateVector( q, local2 );
		proxy.count = 2;
		proxy.radius = r;
		break;
	}
	case PHYS_SHAPE_BOX:
	default: {
		float hx = shapeDef->halfExtents[0] > 0.0f ? shapeDef->halfExtents[0] : 8.0f;
		float hy = shapeDef->halfExtents[1] > 0.0f ? shapeDef->halfExtents[1] : 8.0f;
		float hz = shapeDef->halfExtents[2] > 0.0f ? shapeDef->halfExtents[2] : 8.0f;
		b3Vec3 corners[8] = {
			{ -hx, -hy, -hz }, { hx, -hy, -hz }, { -hx, hy, -hz }, { hx, hy, -hz },
			{ -hx, -hy,  hz }, { hx, -hy,  hz }, { -hx, hy,  hz }, { hx, hy,  hz },
		};
		for ( i = 0; i < 8; i++ ) {
			points[i] = b3RotateVector( q, corners[i] );
		}
		proxy.count = 8;
		proxy.radius = 0.0f;
		break;
	}
	}

	origin = from_vec3( from );
	translation = b3Sub( from_vec3( to ), origin );
	qf = box_make_query_filter( filter );
	ctx.result = result;
	ctx.closest = 1.0f;
	ctx.hit = qfalse;
	b3World_CastShape( bx.worldId, origin, &proxy, translation, qf, box_ray_cb, &ctx );
	return ctx.hit;
}

qboolean Phys_ConvexSweep_Impl( const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result ) {
	return Phys_ConvexSweepFiltered_Impl( shapeDef, from, to, rotation, result, NULL );
}

typedef struct {
	physBodyHandle_t *results;
	int               maxResults;
	int               count;
} boxOverlapCtx;

static bool box_overlap_cb( b3ShapeId shapeId, void *context ) {
	boxOverlapCtx *ctx = (boxOverlapCtx *)context;
	b3BodyId bodyId = b3Shape_GetBody( shapeId );
	void *ud = b3Body_GetUserData( bodyId );
	int handle = (int)(intptr_t)ud - 1;
	int i;

	if ( handle < 0 || ctx->count >= ctx->maxResults ) {
		return ctx->count < ctx->maxResults;
	}
	for ( i = 0; i < ctx->count; i++ ) {
		if ( ctx->results[i] == handle ) {
			return true;
		}
	}
	ctx->results[ctx->count++] = handle;
	return ctx->count < ctx->maxResults;
}

int Phys_OverlapShapeFiltered_Impl( const vec3_t center, float radius, physBodyHandle_t *results, int maxResults,
	const physQueryFilter_t *filter ) {
	b3Vec3 points[1];
	b3ShapeProxy proxy;
	b3QueryFilter qf;
	boxOverlapCtx ctx;

	if ( !bx.initialized || !results || maxResults <= 0 ) {
		return 0;
	}
	points[0] = b3Vec3_zero;
	proxy.points = points;
	proxy.count = 1;
	proxy.radius = radius > 0.0f ? radius : 8.0f;
	qf = box_make_query_filter( filter );
	ctx.results = results;
	ctx.maxResults = maxResults;
	ctx.count = 0;
	b3World_OverlapShape( bx.worldId, from_vec3( center ), &proxy, qf, box_overlap_cb, &ctx );
	return ctx.count;
}

int Phys_OverlapShape_Impl( const vec3_t center, float radius, physBodyHandle_t *results, int maxResults ) {
	return Phys_OverlapShapeFiltered_Impl( center, radius, results, maxResults, NULL );
}

int Phys_OverlapSphere_Impl( const vec3_t center, float radius, physBodyHandle_t *results, int maxResults ) {
	/* Exact Soft Step sphere overlap (not AABB broadphase). */
	return Phys_OverlapShapeFiltered_Impl( center, radius, results, maxResults, NULL );
}

int Phys_OverlapBox_Impl( const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults ) {
	b3AABB aabb;
	b3QueryFilter filter;
	boxOverlapCtx ctx;

	if ( !bx.initialized || !results || maxResults <= 0 || !halfExtents ) {
		return 0;
	}
	aabb.lowerBound = v3( center[0] - halfExtents[0], center[1] - halfExtents[1], center[2] - halfExtents[2] );
	aabb.upperBound = v3( center[0] + halfExtents[0], center[1] + halfExtents[1], center[2] + halfExtents[2] );
	filter = b3DefaultQueryFilter();
	ctx.results = results;
	ctx.maxResults = maxResults;
	ctx.count = 0;
	b3World_OverlapAABB( bx.worldId, aabb, filter, box_overlap_cb, &ctx );
	return ctx.count;
}

static qboolean box_fill_contact_point( b3ContactId contactId, physBodyHandle_t selfBody,
	physContact_t *out ) {
	b3ContactData data;
	const b3Manifold *m;
	b3ManifoldPoint mp;
	b3BodyId bodyA, bodyB;
	void *ud;
	int other = -1;
	b3Pos pos;

	if ( !out || !b3Contact_IsValid( contactId ) ) {
		return qfalse;
	}
	data = b3Contact_GetData( contactId );
	if ( !data.manifolds || data.manifoldCount <= 0 || data.manifolds[0].pointCount <= 0 ) {
		return qfalse;
	}
	m = &data.manifolds[0];
	mp = m->points[0];
	bodyA = b3Shape_GetBody( data.shapeIdA );
	bodyB = b3Shape_GetBody( data.shapeIdB );
	ud = b3Body_GetUserData( bodyA );
	if ( ud && ( (int)(intptr_t)ud - 1 ) == selfBody ) {
		ud = b3Body_GetUserData( bodyB );
		other = ud ? (int)(intptr_t)ud - 1 : -1;
		pos = b3Add( b3Body_GetPosition( bodyA ), mp.anchorA );
	} else {
		ud = b3Body_GetUserData( bodyA );
		other = ud ? (int)(intptr_t)ud - 1 : -1;
		pos = b3Add( b3Body_GetPosition( bodyB ), mp.anchorB );
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->otherBody = other;
	to_vec3( pos, out->point );
	to_vec3( m->normal, out->normal );
	out->normalImpulse = mp.totalNormalImpulse;
	out->separation = mp.separation;
	return qtrue;
}

int Phys_GetBodyContacts_Impl( physBodyHandle_t body, physContact_t *out, int maxOut ) {
	b3ContactData contacts[32];
	int n, i, written = 0;

	if ( !VALID_BODY( body ) || !out || maxOut <= 0 ) {
		return 0;
	}
	n = b3Body_GetContactData( bx.bodies[body].bodyId, contacts, 32 );
	for ( i = 0; i < n && written < maxOut; i++ ) {
		if ( box_fill_contact_point( contacts[i].contactId, body, &out[written] ) ) {
			written++;
		}
	}
	return written;
}

void Phys_SetHitEventThreshold_Impl( float approachSpeed ) {
	if ( !bx.initialized ) {
		return;
	}
	if ( approachSpeed < 0.0f ) {
		approachSpeed = 0.0f;
	}
	b3World_SetHitEventThreshold( bx.worldId, approachSpeed );
	Com_Printf( "[physics] Soft Step hit threshold=%.1f\n", approachSpeed );
}

static void box_hex_to_rgb( b3HexColor color, vec3_t out ) {
	unsigned rgb = (unsigned)color & 0xFFFFFFu;
	out[0] = ( ( rgb >> 16 ) & 0xFFu ) / 255.0f;
	out[1] = ( ( rgb >> 8 ) & 0xFFu ) / 255.0f;
	out[2] = ( rgb & 0xFFu ) / 255.0f;
}

static void box_draw_segment( b3Pos p1, b3Pos p2, b3HexColor color, void *context ) {
	vec3_t a, b, rgb;
	(void)context;
	to_vec3( p1, a );
	to_vec3( p2, b );
	box_hex_to_rgb( color, rgb );
	PhysDebug_AddLine( a, b, rgb );
}

static void box_draw_bounds( b3AABB aabb, b3HexColor color, void *context ) {
	vec3_t rgb, c000, c001, c010, c011, c100, c101, c110, c111;
	(void)context;
	box_hex_to_rgb( color, rgb );
	to_vec3( aabb.lowerBound, c000 );
	VectorSet( c001, aabb.lowerBound.x, aabb.lowerBound.y, aabb.upperBound.z );
	VectorSet( c010, aabb.lowerBound.x, aabb.upperBound.y, aabb.lowerBound.z );
	VectorSet( c011, aabb.lowerBound.x, aabb.upperBound.y, aabb.upperBound.z );
	VectorSet( c100, aabb.upperBound.x, aabb.lowerBound.y, aabb.lowerBound.z );
	VectorSet( c101, aabb.upperBound.x, aabb.lowerBound.y, aabb.upperBound.z );
	VectorSet( c110, aabb.upperBound.x, aabb.upperBound.y, aabb.lowerBound.z );
	to_vec3( aabb.upperBound, c111 );
	PhysDebug_AddLine( c000, c100, rgb );
	PhysDebug_AddLine( c000, c010, rgb );
	PhysDebug_AddLine( c000, c001, rgb );
	PhysDebug_AddLine( c111, c011, rgb );
	PhysDebug_AddLine( c111, c101, rgb );
	PhysDebug_AddLine( c111, c110, rgb );
	PhysDebug_AddLine( c100, c110, rgb );
	PhysDebug_AddLine( c100, c101, rgb );
	PhysDebug_AddLine( c010, c110, rgb );
	PhysDebug_AddLine( c010, c011, rgb );
	PhysDebug_AddLine( c001, c101, rgb );
	PhysDebug_AddLine( c001, c011, rgb );
}

static void box_draw_box( b3Vec3 extents, b3WorldTransform transform, b3HexColor color, void *context ) {
	b3Vec3 corners[8];
	vec3_t pts[8], rgb;
	int i;
	static const int edges[12][2] = {
		{ 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
		{ 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};
	(void)context;
	box_hex_to_rgb( color, rgb );
	for ( i = 0; i < 8; i++ ) {
		b3Vec3 local = {
			( i & 1 ) ? extents.x : -extents.x,
			( i & 2 ) ? extents.y : -extents.y,
			( i & 4 ) ? extents.z : -extents.z,
		};
		corners[i] = b3TransformPoint( transform, local );
		to_vec3( corners[i], pts[i] );
	}
	for ( i = 0; i < 12; i++ ) {
		PhysDebug_AddLine( pts[edges[i][0]], pts[edges[i][1]], rgb );
	}
}

static void box_draw_point( b3Pos p, float size, b3HexColor color, void *context ) {
	vec3_t c, a, b;
	float rgb[3];
	float s = size > 0.0f ? size : 2.0f;
	(void)context;
	box_hex_to_rgb( color, rgb );
	to_vec3( p, c );
	VectorSet( a, c[0] - s, c[1], c[2] );
	VectorSet( b, c[0] + s, c[1], c[2] );
	PhysDebug_AddLine( a, b, rgb );
	VectorSet( a, c[0], c[1] - s, c[2] );
	VectorSet( b, c[0], c[1] + s, c[2] );
	PhysDebug_AddLine( a, b, rgb );
	VectorSet( a, c[0], c[1], c[2] - s );
	VectorSet( b, c[0], c[1], c[2] + s );
	PhysDebug_AddLine( a, b, rgb );
}

void Phys_DebugDraw_Impl( void ) {
	b3DebugDraw draw;
	b3AABB bounds;
	unsigned flags;

	if ( !bx.initialized ) {
		return;
	}
	flags = bx.debugDrawFlags ? bx.debugDrawFlags : 0x3u;
	{
		cvar_t *cv = Cvar_Get( "phys_debugContacts", "0", CVAR_ARCHIVE );
		if ( cv && cv->integer ) {
			flags |= 0x4u;
		}
	}
	draw = b3DefaultDebugDraw();
	draw.DrawSegmentFcn = box_draw_segment;
	draw.DrawBoundsFcn = box_draw_bounds;
	draw.DrawBoxFcn = box_draw_box;
	draw.DrawPointFcn = box_draw_point;
	draw.drawShapes = ( flags & 0x1u ) != 0;
	draw.drawJoints = ( flags & 0x2u ) != 0;
	draw.drawContacts = ( flags & 0x4u ) != 0;
	draw.drawBounds = ( flags & 0x8u ) != 0;
	draw.drawMass = ( flags & 0x10u ) != 0;
	draw.drawSleep = ( flags & 0x20u ) != 0;
	bounds = b3World_GetBounds( bx.worldId );
	draw.drawingBounds = bounds;
	b3World_Draw( bx.worldId, &draw, B3_DEFAULT_MASK_BITS );
}

void Phys_ProcessContactEvents_Impl( void ) {
	b3ContactEvents events;
	b3SensorEvents sensors;
	int i;

	if ( !bx.initialized ) {
		return;
	}

	events = b3World_GetContactEvents( bx.worldId );
	for ( i = 0; i < events.hitCount; i++ ) {
		b3ContactHitEvent *hit = &events.hitEvents[i];
		vec3_t point, normal, impulse;
		int bodyA = -1, bodyB = -1;
		int ragA = -1, boneA = -1, ragB = -1, boneB = -1;
		void *ud;
		float mag;
		phys_event_t pe;

		ud = b3Body_GetUserData( b3Shape_GetBody( hit->shapeIdA ) );
		box_decode_userdata( ud, &bodyA, &ragA, &boneA );
		ud = b3Body_GetUserData( b3Shape_GetBody( hit->shapeIdB ) );
		box_decode_userdata( ud, &bodyB, &ragB, &boneB );
		to_vec3( hit->point, point );
		to_vec3( hit->normal, normal );
		mag = hit->approachSpeed;
		if ( mag < 25.0f ) {
			continue;
		}
		impulse[0] = normal[0] * mag;
		impulse[1] = normal[1] * mag;
		impulse[2] = normal[2] * mag;
		Com_Memset( &pe, 0, sizeof( pe ) );
		pe.type = PHYS_EVENT_IMPACT;
		pe.entityNum = -1;
		pe.bodyA = bodyA;
		pe.bodyB = bodyB;
		pe.ragdoll = ragA >= 0 ? ragA : ragB;
		pe.bone = ragA >= 0 ? boneA : boneB;
		pe.matA = bodyA >= 0 ? bx.bodies[bodyA].materialId : 0;
		pe.matB = bodyB >= 0 ? bx.bodies[bodyB].materialId : 0;
		VectorCopy( point, pe.point );
		VectorCopy( normal, pe.normal );
		VectorCopy( impulse, pe.impulse );
		pe.magnitude = mag;
		if ( pe.ragdoll >= 0 ) {
			PhysEvent_BuildHitFromImpulse( &pe.hit, pe.bone, 0, point, impulse, 1.0f );
		}
		PhysEvent_Post( &pe );
	}

	/* Soft Step contact begin/end (non-sensor rigid contacts) */
	for ( i = 0; i < events.beginCount; i++ ) {
		b3ContactBeginTouchEvent *ev = &events.beginEvents[i];
		phys_event_t pe;
		physContact_t contact;
		int bodyA = -1, bodyB = -1;
		int ragA = -1, boneA = -1, ragB = -1, boneB = -1;
		void *ud;
		if ( !b3Shape_IsValid( ev->shapeIdA ) || !b3Shape_IsValid( ev->shapeIdB ) ) {
			continue;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->shapeIdA ) );
		box_decode_userdata( ud, &bodyA, &ragA, &boneA );
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->shapeIdB ) );
		box_decode_userdata( ud, &bodyB, &ragB, &boneB );
		Com_Memset( &pe, 0, sizeof( pe ) );
		pe.type = PHYS_EVENT_CONTACT_BEGIN;
		pe.bodyA = bodyA;
		pe.bodyB = bodyB;
		pe.ragdoll = ragA >= 0 ? ragA : ragB;
		pe.bone = ragA >= 0 ? boneA : boneB;
		if ( bodyA >= 0 ) {
			pe.matA = bx.bodies[bodyA].materialId;
		}
		if ( bodyB >= 0 ) {
			pe.matB = bx.bodies[bodyB].materialId;
		}
		if ( bodyA >= 0 && box_fill_contact_point( ev->contactId, bodyA, &contact ) ) {
			VectorCopy( contact.point, pe.point );
			VectorCopy( contact.normal, pe.normal );
			pe.magnitude = contact.normalImpulse;
		} else if ( b3Contact_IsValid( ev->contactId ) ) {
			b3ContactData data = b3Contact_GetData( ev->contactId );
			if ( data.manifolds && data.manifoldCount > 0 && data.manifolds[0].pointCount > 0 ) {
				b3Pos pos = b3Add( b3Body_GetPosition( b3Shape_GetBody( data.shapeIdA ) ),
					data.manifolds[0].points[0].anchorA );
				to_vec3( pos, pe.point );
				to_vec3( data.manifolds[0].normal, pe.normal );
				pe.magnitude = data.manifolds[0].points[0].totalNormalImpulse;
			}
		}
		PhysEvent_Post( &pe );
	}
	for ( i = 0; i < events.endCount; i++ ) {
		b3ContactEndTouchEvent *ev = &events.endEvents[i];
		phys_event_t pe;
		int bodyA = -1, bodyB = -1;
		int ragA = -1, boneA = -1, ragB = -1, boneB = -1;
		void *ud;
		if ( !b3Shape_IsValid( ev->shapeIdA ) || !b3Shape_IsValid( ev->shapeIdB ) ) {
			continue;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->shapeIdA ) );
		box_decode_userdata( ud, &bodyA, &ragA, &boneA );
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->shapeIdB ) );
		box_decode_userdata( ud, &bodyB, &ragB, &boneB );
		Com_Memset( &pe, 0, sizeof( pe ) );
		pe.type = PHYS_EVENT_CONTACT_END;
		pe.bodyA = bodyA;
		pe.bodyB = bodyB;
		pe.ragdoll = ragA >= 0 ? ragA : ragB;
		pe.bone = ragA >= 0 ? boneA : boneB;
		PhysEvent_Post( &pe );
	}

	/* Soft Step sensors → MOTION_ENTER/EXIT (triggers / pickups / AOE) */
	sensors = b3World_GetSensorEvents( bx.worldId );
	for ( i = 0; i < sensors.beginCount; i++ ) {
		b3SensorBeginTouchEvent *ev = &sensors.beginEvents[i];
		phys_event_t pe;
		int sensorBody = -1, visitorBody = -1;
		void *ud;
		if ( !b3Shape_IsValid( ev->sensorShapeId ) || !b3Shape_IsValid( ev->visitorShapeId ) ) {
			continue;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->sensorShapeId ) );
		if ( ud ) {
			sensorBody = (int)(intptr_t)ud - 1;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->visitorShapeId ) );
		if ( ud ) {
			visitorBody = (int)(intptr_t)ud - 1;
		}
		Com_Memset( &pe, 0, sizeof( pe ) );
		pe.type = PHYS_EVENT_MOTION_ENTER;
		pe.bodyA = sensorBody;
		pe.bodyB = visitorBody;
		PhysEvent_Post( &pe );
	}
	for ( i = 0; i < sensors.endCount; i++ ) {
		b3SensorEndTouchEvent *ev = &sensors.endEvents[i];
		phys_event_t pe;
		int sensorBody = -1, visitorBody = -1;
		void *ud;
		if ( !b3Shape_IsValid( ev->sensorShapeId ) || !b3Shape_IsValid( ev->visitorShapeId ) ) {
			continue;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->sensorShapeId ) );
		if ( ud ) {
			sensorBody = (int)(intptr_t)ud - 1;
		}
		ud = b3Body_GetUserData( b3Shape_GetBody( ev->visitorShapeId ) );
		if ( ud ) {
			visitorBody = (int)(intptr_t)ud - 1;
		}
		Com_Memset( &pe, 0, sizeof( pe ) );
		pe.type = PHYS_EVENT_MOTION_EXIT;
		pe.bodyA = sensorBody;
		pe.bodyB = visitorBody;
		PhysEvent_Post( &pe );
	}

	/* Joint force/torque thresholds → BREAK + destroy */
	{
		b3JointEvents joints = b3World_GetJointEvents( bx.worldId );
		for ( i = 0; i < joints.count; i++ ) {
			void *ud = joints.jointEvents[i].userData;
			int ch;
			phys_event_t pe;
			if ( !ud ) {
				ud = b3Joint_GetUserData( joints.jointEvents[i].jointId );
			}
			if ( !ud ) {
				continue;
			}
			ch = (int)(intptr_t)ud - 1;
			if ( !VALID_CON( ch ) ) {
				continue;
			}
			Com_Memset( &pe, 0, sizeof( pe ) );
			pe.type = PHYS_EVENT_BREAK;
			pe.bodyA = bx.constraints[ch].bodyA;
			pe.bodyB = bx.constraints[ch].bodyB;
			PhysEvent_Post( &pe );
			Phys_DestroyConstraint_Impl( ch );
		}
	}

	/* Soft Step body move / sleep notifications */
	{
		b3BodyEvents bodies = b3World_GetBodyEvents( bx.worldId );
		for ( i = 0; i < bodies.moveCount; i++ ) {
			b3BodyMoveEvent *ev = &bodies.moveEvents[i];
			int bh = -1;
			void *ud = ev->userData;
			if ( !ud ) {
				ud = b3Body_GetUserData( ev->bodyId );
			}
			if ( ud ) {
				bh = (int)(intptr_t)ud - 1;
			}
			if ( !VALID_BODY( bh ) ) {
				continue;
			}
			if ( ev->fellAsleep ) {
				phys_event_t pe;
				Com_Memset( &pe, 0, sizeof( pe ) );
				pe.type = PHYS_EVENT_BODY_SLEEP;
				pe.bodyA = bh;
				pe.matA = bx.bodies[bh].materialId;
				to_vec3( ev->transform.p, pe.point );
				PhysEvent_Post( &pe );
			}
		}
		bx.lastProfile.contactHitCount = events.hitCount;
	}
}

void Phys_SetBodyMaterial_Impl( physBodyHandle_t h, int materialId ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	bx.bodies[h].materialId = materialId;
}

int Phys_GetBodyMaterial_Impl( physBodyHandle_t h ) {
	if ( !VALID_BODY( h ) ) {
		return 0;
	}
	return bx.bodies[h].materialId;
}

void Phys_SetBodyFriction_Impl( physBodyHandle_t h, float friction ) {
	BoxBody *pb;
	int i;
	if ( !VALID_BODY( h ) ) {
		return;
	}
	pb = &bx.bodies[h];
	for ( i = 0; i < pb->shapeCount; i++ ) {
		if ( b3Shape_IsValid( pb->shapes[i] ) ) {
			b3Shape_SetFriction( pb->shapes[i], friction );
		}
	}
	if ( pb->shapeCount == 0 && b3Shape_IsValid( pb->shapeId ) ) {
		b3Shape_SetFriction( pb->shapeId, friction );
	}
}

void Phys_SetBodyRestitution_Impl( physBodyHandle_t h, float restitution ) {
	BoxBody *pb;
	int i;
	if ( !VALID_BODY( h ) ) {
		return;
	}
	pb = &bx.bodies[h];
	for ( i = 0; i < pb->shapeCount; i++ ) {
		if ( b3Shape_IsValid( pb->shapes[i] ) ) {
			b3Shape_SetRestitution( pb->shapes[i], restitution );
		}
	}
	if ( pb->shapeCount == 0 && b3Shape_IsValid( pb->shapeId ) ) {
		b3Shape_SetRestitution( pb->shapeId, restitution );
	}
}

int Phys_GetBodyCount_Impl( void ) {
	int i, n = 0;
	for ( i = 0; i < bx.bodyCount; i++ ) {
		if ( bx.bodies[i].active ) {
			n++;
		}
	}
	return n;
}

int Phys_GetConstraintCount_Impl( void ) {
	int i, n = 0;
	for ( i = 0; i < bx.constraintCount; i++ ) {
		if ( bx.constraints[i].active ) {
			n++;
		}
	}
	return n;
}

physBodyHandle_t Phys_AddStaticTriMesh_Impl( const float *verts, int numVerts, const int *indices, int numIndices ) {
	b3MeshDef meshDef;
	b3MeshData *mesh;
	b3BodyDef bd;
	b3ShapeDef sd;
	BoxBody *pb;
	int idx;
	int triCount;
	b3Vec3 *bverts = NULL;
	int32_t *bindices = NULL;
	int i;

	if ( !bx.initialized || !verts || numVerts < 3 || !indices || numIndices < 3 ) {
		return -1;
	}
	if ( bx.bodyCount >= PHYS_MAX_RIGID_BODIES || bx.meshCount >= (int)( sizeof( bx.meshes ) / sizeof( bx.meshes[0] ) ) ) {
		return -1;
	}

	triCount = numIndices / 3;
	if ( triCount < 1 ) {
		return -1;
	}

	bverts = (b3Vec3 *)malloc( (size_t)numVerts * sizeof( b3Vec3 ) );
	bindices = (int32_t *)malloc( (size_t)triCount * 3 * sizeof( int32_t ) );
	if ( !bverts || !bindices ) {
		free( bverts );
		free( bindices );
		return -1;
	}
	for ( i = 0; i < numVerts; i++ ) {
		bverts[i] = v3( verts[i * 3 + 0], verts[i * 3 + 1], verts[i * 3 + 2] );
	}
	for ( i = 0; i < triCount * 3; i++ ) {
		bindices[i] = (int32_t)indices[i];
	}

	memset( &meshDef, 0, sizeof( meshDef ) );
	meshDef.vertices = bverts;
	meshDef.vertexCount = numVerts;
	meshDef.indices = bindices;
	meshDef.triangleCount = triCount;
	meshDef.weldVertices = true;
	meshDef.identifyEdges = true;
	meshDef.useMedianSplit = true;

	mesh = b3CreateMesh( &meshDef, NULL, 0 );
	free( bverts );
	free( bindices );
	if ( !mesh ) {
		Com_Printf( S_COLOR_YELLOW "Box3D: b3CreateMesh failed (%d tris)\n", triCount );
		return -1;
	}

	idx = bx.bodyCount++;
	pb = &bx.bodies[idx];
	memset( pb, 0, sizeof( *pb ) );

	bd = b3DefaultBodyDef();
	bd.type = b3_staticBody;
	pb->bodyId = b3CreateBody( bx.worldId, &bd );
	b3Body_SetUserData( pb->bodyId, (void *)(intptr_t)( idx + 1 ) );

	sd = b3DefaultShapeDef();
	sd.baseMaterial.friction = 0.8f;
	sd.baseMaterial.restitution = 0.2f;
	sd.enableHitEvents = true;
	sd.enableContactEvents = true;
	sd.enableCustomFiltering = true;
	sd.enablePreSolveEvents = true;
	pb->shapeId = b3CreateMeshShape( pb->bodyId, &sd, mesh, b3Vec3_one );
	pb->meshData = mesh;
	bx.meshes[bx.meshCount++] = mesh;
	pb->bodyType = PHYS_BODY_STATIC;
	pb->materialId = 0;
	pb->active = qtrue;
	return idx;
}

physBodyHandle_t Phys_AddStaticHeightField_Impl( const float *heights, int countX, int countY,
	float cellSize, float heightScale, const vec3_t origin ) {
	b3HeightFieldDef hfDef;
	b3HeightFieldData *hf;
	b3BodyDef bd;
	b3ShapeDef sd;
	BoxBody *pb;
	int idx;
	int cellCount;
	uint8_t *materials = NULL;
	float *ownedHeights = NULL;
	float minH, maxH;
	int i;
	vec3_t rot;

	if ( !bx.initialized || !heights || countX < 2 || countY < 2 ) {
		return -1;
	}
	if ( bx.bodyCount >= PHYS_MAX_RIGID_BODIES
		|| bx.heightFieldCount >= (int)( sizeof( bx.heightFields ) / sizeof( bx.heightFields[0] ) ) ) {
		return -1;
	}
	if ( cellSize <= 0.0f ) {
		cellSize = 32.0f;
	}
	if ( heightScale <= 0.0f ) {
		heightScale = 1.0f;
	}

	cellCount = ( countX - 1 ) * ( countY - 1 );
	materials = (uint8_t *)malloc( (size_t)cellCount );
	ownedHeights = (float *)malloc( (size_t)countX * (size_t)countY * sizeof( float ) );
	if ( !materials || !ownedHeights ) {
		free( materials );
		free( ownedHeights );
		return -1;
	}
	memset( materials, 0, (size_t)cellCount );
	minH = maxH = heights[0];
	for ( i = 0; i < countX * countY; i++ ) {
		ownedHeights[i] = heights[i];
		if ( heights[i] < minH ) {
			minH = heights[i];
		}
		if ( heights[i] > maxH ) {
			maxH = heights[i];
		}
	}
	if ( maxH - minH < 1.0f ) {
		maxH = minH + 1.0f;
	}

	memset( &hfDef, 0, sizeof( hfDef ) );
	hfDef.heights = ownedHeights;
	hfDef.materialIndices = materials;
	/* Box3D heightfields are Y-up; Quake Z heights become local Y. */
	hfDef.scale = v3( cellSize, heightScale, cellSize );
	hfDef.countX = countX;
	hfDef.countZ = countY;
	hfDef.globalMinimumHeight = minH;
	hfDef.globalMaximumHeight = maxH;
	hfDef.clockwiseWinding = false;

	hf = b3CreateHeightField( &hfDef );
	free( materials );
	free( ownedHeights );
	if ( !hf ) {
		Com_Printf( S_COLOR_YELLOW "Box3D: b3CreateHeightField failed (%dx%d)\n", countX, countY );
		return -1;
	}

	idx = bx.bodyCount++;
	pb = &bx.bodies[idx];
	memset( pb, 0, sizeof( *pb ) );

	bd = b3DefaultBodyDef();
	bd.type = b3_staticBody;
	if ( origin ) {
		bd.position = from_vec3( origin );
	}
	/* +90° pitch: local Y (height) → world Z */
	VectorSet( rot, 90.0f, 0.0f, 0.0f );
	bd.rotation = quat_from_euler_deg( rot );

	pb->bodyId = b3CreateBody( bx.worldId, &bd );
	b3Body_SetUserData( pb->bodyId, (void *)(intptr_t)( idx + 1 ) );

	sd = b3DefaultShapeDef();
	sd.baseMaterial.friction = 0.8f;
	sd.baseMaterial.restitution = 0.1f;
	sd.enableHitEvents = true;
	sd.enableContactEvents = true;
	pb->shapeId = b3CreateHeightFieldShape( pb->bodyId, &sd, hf );
	pb->heightFieldData = hf;
	bx.heightFields[bx.heightFieldCount++] = hf;
	pb->bodyType = PHYS_BODY_STATIC;
	pb->materialId = 0;
	pb->active = qtrue;
	Com_Printf( "[physics] Box3D heightfield %dx%d cell=%.1f at (%.0f %.0f %.0f)\n",
		countX, countY, cellSize,
		origin ? origin[0] : 0.0f, origin ? origin[1] : 0.0f, origin ? origin[2] : 0.0f );
	return idx;
}

int Phys_ApplyImpulseRadius_Impl( const vec3_t center, float radius, float magnitude, float falloff ) {
	b3ExplosionDef def;
	physBodyHandle_t hits[128];
	int count;
	int i;
	int affected = 0;

	if ( !bx.initialized || !center || radius <= 0.0f || magnitude == 0.0f ) {
		return 0;
	}

	count = Phys_OverlapSphere_Impl( center, radius, hits, 128 );
	for ( i = 0; i < count; i++ ) {
		if ( Phys_IsBodyDynamic_Impl( hits[i] ) ) {
			affected++;
		}
	}

	def = b3DefaultExplosionDef();
	def.position = from_vec3( center );
	def.radius = radius;
	/* Box3D falloff is distance past radius; map our exponent into a soft shell. */
	if ( falloff <= 0.0f ) {
		def.falloff = 0.0f;
	} else {
		def.falloff = radius / falloff;
		if ( def.falloff < 1.0f ) {
			def.falloff = 1.0f;
		}
	}
	/* Quake-scale impulse → area impulse; tuned for prop masses ~10. */
	def.impulsePerArea = magnitude * 0.05f;
	b3World_Explode( bx.worldId, &def );
	return affected;
}

physBodyHandle_t Phys_AddStaticCompoundBoxes_Impl( const float *centersXYZ, const float *halfExtentsXYZ, int count ) {
	b3CompoundHullDef *hullDefs = NULL;
	b3BoxHull *boxes = NULL;
	b3CompoundDef cdef;
	b3CompoundData *compound;
	b3BodyDef bd;
	b3ShapeDef sd;
	BoxBody *pb;
	int idx;
	int i;
	int maxChildren = (int)( sizeof( bx.compounds ) / sizeof( bx.compounds[0] ) );

	if ( !bx.initialized || !centersXYZ || !halfExtentsXYZ || count < 1 ) {
		return -1;
	}
	if ( bx.bodyCount >= PHYS_MAX_RIGID_BODIES || bx.compoundCount >= maxChildren ) {
		return -1;
	}
	if ( count > 4096 ) {
		count = 4096;
	}

	hullDefs = (b3CompoundHullDef *)calloc( (size_t)count, sizeof( b3CompoundHullDef ) );
	boxes = (b3BoxHull *)calloc( (size_t)count, sizeof( b3BoxHull ) );
	if ( !hullDefs || !boxes ) {
		free( hullDefs );
		free( boxes );
		return -1;
	}

	for ( i = 0; i < count; i++ ) {
		float hx = halfExtentsXYZ[i * 3 + 0];
		float hy = halfExtentsXYZ[i * 3 + 1];
		float hz = halfExtentsXYZ[i * 3 + 2];
		if ( hx < 0.5f ) {
			hx = 0.5f;
		}
		if ( hy < 0.5f ) {
			hy = 0.5f;
		}
		if ( hz < 0.5f ) {
			hz = 0.5f;
		}
		boxes[i] = b3MakeBoxHull( hx, hy, hz );
		hullDefs[i].hull = &boxes[i].base;
		hullDefs[i].transform.p = v3( centersXYZ[i * 3 + 0], centersXYZ[i * 3 + 1], centersXYZ[i * 3 + 2] );
		hullDefs[i].transform.q = b3Quat_identity;
		hullDefs[i].material = b3DefaultSurfaceMaterial();
		hullDefs[i].material.friction = 0.8f;
		hullDefs[i].material.restitution = 0.2f;
	}

	memset( &cdef, 0, sizeof( cdef ) );
	cdef.hulls = hullDefs;
	cdef.hullCount = count;
	compound = b3CreateCompound( &cdef );
	free( hullDefs );
	free( boxes );
	if ( !compound ) {
		Com_Printf( S_COLOR_YELLOW "Box3D: b3CreateCompound failed (%d hulls)\n", count );
		return -1;
	}

	idx = bx.bodyCount++;
	pb = &bx.bodies[idx];
	memset( pb, 0, sizeof( *pb ) );
	bd = b3DefaultBodyDef();
	bd.type = b3_staticBody;
	pb->bodyId = b3CreateBody( bx.worldId, &bd );
	b3Body_SetUserData( pb->bodyId, (void *)(intptr_t)( idx + 1 ) );
	sd = b3DefaultShapeDef();
	sd.enableHitEvents = true;
	sd.enableContactEvents = true;
	pb->shapeId = b3CreateCompoundShape( pb->bodyId, &sd, compound );
	pb->compoundData = compound;
	bx.compounds[bx.compoundCount++] = compound;
	pb->bodyType = PHYS_BODY_STATIC;
	pb->active = qtrue;
	return idx;
}

#define BOX_MOVER_MAX_PLANES 24

typedef struct {
	b3CollisionPlane planes[BOX_MOVER_MAX_PLANES];
	int              count;
	qboolean        grounded;
} boxMoverPlaneCtx;

static bool box_mover_plane_cb( b3ShapeId shapeId, const b3PlaneResult *planeResults, int planeCount, void *context ) {
	boxMoverPlaneCtx *ctx = (boxMoverPlaneCtx *)context;
	int i;

	(void)shapeId;
	for ( i = 0; i < planeCount && ctx->count < BOX_MOVER_MAX_PLANES; i++ ) {
		ctx->planes[ctx->count].plane = planeResults[i].plane;
		ctx->planes[ctx->count].pushLimit = FLT_MAX;
		ctx->planes[ctx->count].push = 0.0f;
		ctx->planes[ctx->count].clipVelocity = true;
		if ( planeResults[i].plane.normal.z > 0.7f ) {
			ctx->grounded = qtrue;
		}
		ctx->count++;
	}
	return ctx->count < BOX_MOVER_MAX_PLANES;
}

qboolean Phys_MoverStep_Impl( vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut ) {
	b3Capsule mover;
	b3Pos originPos;
	b3Vec3 wish, translation, vel;
	b3QueryFilter filter;
	boxMoverPlaneCtx planeCtx;
	b3PlaneSolverResult solved;
	float h;
	float r;
	float speed;
	int iter;
	qboolean grounded;

	if ( !bx.initialized || !origin || !velocity ) {
		return qfalse;
	}
	if ( dt <= 0.0f ) {
		dt = 1.0f / 60.0f;
	}
	r = radius > 0.0f ? radius : 16.0f;
	h = height > 0.0f ? height : 56.0f;
	grounded = groundedOut ? *groundedOut : qfalse;

	vel = from_vec3( velocity );
	if ( wishDir ) {
		wish = from_vec3( wishDir );
		if ( b3Length( wish ) > 0.001f ) {
			wish = b3Normalize( wish );
			speed = wishSpeed > 0.0f ? wishSpeed : 320.0f;
			vel.x = wish.x * speed;
			vel.y = wish.y * speed;
		} else {
			vel.x = 0.0f;
			vel.y = 0.0f;
		}
	} else {
		vel.x = 0.0f;
		vel.y = 0.0f;
	}

	if ( jump && grounded ) {
		vel.z = 280.0f;
		grounded = qfalse;
	}
	vel.z -= 800.0f * dt;

	originPos = from_vec3( origin );
	mover.center1 = v3( 0.0f, 0.0f, -0.5f * h );
	mover.center2 = v3( 0.0f, 0.0f, 0.5f * h );
	mover.radius = r;
	filter = b3DefaultQueryFilter();
	translation = b3MulSV( dt, vel );

	for ( iter = 0; iter < 4; iter++ ) {
		float fraction;
		b3Vec3 delta;
		float tol = 0.25f;

		memset( &planeCtx, 0, sizeof( planeCtx ) );
		b3World_CollideMover( bx.worldId, originPos, &mover, filter, box_mover_plane_cb, &planeCtx );
		solved = b3SolvePlanes( translation, planeCtx.planes, planeCtx.count );
		delta = solved.delta;
		fraction = b3World_CastMover( bx.worldId, originPos, &mover, delta, filter, NULL, NULL );
		delta = b3MulSV( fraction, delta );
		originPos = b3Add( originPos, delta );
		translation = b3Sub( translation, delta );
		if ( b3LengthSquared( delta ) < tol * tol ) {
			break;
		}
	}

	/* Opt-in stair step using phys_stepHeight (CastMover companion). */
	{
		cvar_t *stepCv = Cvar_Get( "phys_stepHeight", "18", CVAR_ARCHIVE );
		float stepH = stepCv && stepCv->value > 0.0f ? stepCv->value : 0.0f;
		float horiz = sqrtf( vel.x * vel.x + vel.y * vel.y );
		if ( stepH > 0.0f && horiz > 1.0f ) {
			b3Pos raised = originPos;
			b3Vec3 forward = v3( vel.x, vel.y, 0.0f );
			float frac;
			raised.z += stepH;
			if ( b3Length( forward ) > 0.001f ) {
				forward = b3Normalize( forward );
				forward = b3MulSV( fminf( horiz * dt, r * 2.0f ), forward );
				frac = b3World_CastMover( bx.worldId, raised, &mover, forward, filter, NULL, NULL );
				if ( frac > 0.01f ) {
					b3Pos stepped = b3Add( raised, b3MulSV( frac, forward ) );
					b3Vec3 down = v3( 0.0f, 0.0f, -stepH - 2.0f );
					float drop = b3World_CastMover( bx.worldId, stepped, &mover, down, filter, NULL, NULL );
					originPos = b3Add( stepped, b3MulSV( drop, down ) );
				}
			}
		}
	}

	memset( &planeCtx, 0, sizeof( planeCtx ) );
	b3World_CollideMover( bx.worldId, originPos, &mover, filter, box_mover_plane_cb, &planeCtx );
	vel = b3ClipVector( vel, planeCtx.planes, planeCtx.count );
	grounded = planeCtx.grounded;

	to_vec3( originPos, origin );
	to_vec3( vel, velocity );
	if ( groundedOut ) {
		*groundedOut = grounded;
	}
	return qtrue;
}

int Phys_GetWorkerCount_Impl( void ) {
	if ( !bx.initialized ) {
		return 0;
	}
	return b3World_GetWorkerCount( bx.worldId );
}

int Phys_AttachShape_Impl( physBodyHandle_t body, const physBodyDef_t *shapeDef ) {
	BoxBody *pb;
	b3ShapeId sid;
	float density = 0.0f;

	if ( !VALID_BODY( body ) || !shapeDef ) {
		return -1;
	}
	pb = &bx.bodies[body];
	if ( pb->shapeCount >= (int)( sizeof( pb->shapes ) / sizeof( pb->shapes[0] ) ) ) {
		return -1;
	}
	if ( pb->bodyType == PHYS_BODY_DYNAMIC && shapeDef->mass > 0.0f ) {
		float vol = volume_for_def( shapeDef );
		density = ( vol > 0.001f ) ? ( shapeDef->mass / vol ) : 1.0f;
	}
	attach_shape( pb->bodyId, shapeDef, density, &sid );
	pb->shapes[pb->shapeCount] = sid;
	if ( pb->shapeCount == 0 ) {
		pb->shapeId = sid;
	}
	pb->shapeCount++;
	if ( pb->bodyType == PHYS_BODY_DYNAMIC ) {
		b3Body_ApplyMassFromShapes( pb->bodyId );
	}
	return pb->shapeCount - 1;
}

void Phys_DestroyAttachedShape_Impl( physBodyHandle_t body, int shapeIndex ) {
	BoxBody *pb;
	int i;

	if ( !VALID_BODY( body ) || shapeIndex < 0 ) {
		return;
	}
	pb = &bx.bodies[body];
	if ( shapeIndex >= pb->shapeCount ) {
		return;
	}
	b3DestroyShape( pb->shapes[shapeIndex], true );
	for ( i = shapeIndex; i < pb->shapeCount - 1; i++ ) {
		pb->shapes[i] = pb->shapes[i + 1];
	}
	pb->shapeCount--;
	pb->shapeId = pb->shapeCount > 0 ? pb->shapes[0] : (b3ShapeId){0};
}

void Phys_SetBodyFilter_Impl( physBodyHandle_t body, int categoryBits, int maskBits ) {
	Phys_SetBodyFilterEx_Impl( body, categoryBits, maskBits, 0 );
}

void Phys_SetBodyFilterEx_Impl( physBodyHandle_t body, int categoryBits, int maskBits, int groupIndex ) {
	BoxBody *pb;
	b3Filter filter;
	int i;

	if ( !VALID_BODY( body ) ) {
		return;
	}
	pb = &bx.bodies[body];
	filter = b3DefaultFilter();
	if ( categoryBits ) {
		filter.categoryBits = (uint64_t)(unsigned)categoryBits;
	}
	if ( maskBits ) {
		filter.maskBits = (uint64_t)(unsigned)maskBits;
	}
	filter.groupIndex = groupIndex;
	for ( i = 0; i < pb->shapeCount; i++ ) {
		if ( b3Shape_IsValid( pb->shapes[i] ) ) {
			b3Shape_SetFilter( pb->shapes[i], filter, true );
		}
	}
	if ( pb->shapeCount == 0 && b3Shape_IsValid( pb->shapeId ) ) {
		b3Shape_SetFilter( pb->shapeId, filter, true );
	}
}

void Phys_GetSoftStepProfile_Impl( physSoftStepProfile_t *out ) {
	if ( !out ) {
		return;
	}
	*out = bx.lastProfile;
}

void Phys_StartRecording_Impl( void ) {
	cvar_t *recCv;
	if ( !bx.initialized ) {
		return;
	}
	recCv = Cvar_Get( "phys_record", "0", CVAR_ARCHIVE );
	if ( !recCv || !recCv->integer ) {
		Com_Printf( "[physics] set phys_record 1 before Phys_StartRecording\n" );
		return;
	}
	if ( bx.recording ) {
		b3World_StopRecording( bx.worldId );
		b3DestroyRecording( bx.recording );
		bx.recording = NULL;
	}
	bx.recording = b3CreateRecording( 8 * 1024 * 1024 );
	if ( !bx.recording ) {
		Com_Printf( S_COLOR_YELLOW "Box3D: recording alloc failed\n" );
		return;
	}
	b3World_StartRecording( bx.worldId, bx.recording );
	Com_Printf( "[physics] Soft Step recording started (phys_record)\n" );
}

void Phys_StopRecording_Impl( const char *path ) {
	if ( !bx.initialized || !bx.recording ) {
		return;
	}
	b3World_StopRecording( bx.worldId );
	if ( path && path[0] ) {
		if ( b3SaveRecordingToFile( bx.recording, path ) ) {
			Com_Printf( "[physics] Soft Step recording saved to %s\n", path );
		} else {
			Com_Printf( S_COLOR_YELLOW "Box3D: failed to save recording %s\n", path );
		}
	}
	b3DestroyRecording( bx.recording );
	bx.recording = NULL;
}

qboolean Phys_ValidateReplay_Impl( const char *path ) {
	b3Recording *rec;
	const uint8_t *data;
	int size;
	int workers;
	bool ok;

	if ( !path || !path[0] ) {
		Com_Printf( S_COLOR_YELLOW "phys_replay: need a recording path\n" );
		return qfalse;
	}
	rec = b3LoadRecordingFromFile( path );
	if ( !rec ) {
		Com_Printf( S_COLOR_YELLOW "phys_replay: failed to load %s\n", path );
		return qfalse;
	}
	data = b3Recording_GetData( rec );
	size = b3Recording_GetSize( rec );
	workers = bx.initialized ? bx.workerCount : 1;
	if ( workers < 1 ) {
		workers = 1;
	}
	ok = b3ValidateReplay( data, size, workers );
	b3DestroyRecording( rec );
	Com_Printf( "[physics] Soft Step replay %s: %s (workers=%d, bytes=%d)\n",
		path, ok ? "PASS" : "FAIL", workers, size );
	return ok ? qtrue : qfalse;
}

void Phys_DumpWorld_Impl( void ) {
	if ( !bx.initialized ) {
		Com_Printf( "phys_dump: Soft Step world not initialized\n" );
		return;
	}
	b3World_DumpMemoryStats( bx.worldId );
	Com_Printf( "[physics] Soft Step DumpMemoryStats (see stdout)\n" );
}

/* ========== Soft Step AAA surfaces ========== */

static bool box_custom_filter_cb( b3ShapeId shapeIdA, b3ShapeId shapeIdB, void *context ) {
	int bodyA = -1, bodyB = -1, ragA = -1, boneA = -1, ragB = -1, boneB = -1;
	(void)context;
	if ( !bx.customFilterFn ) {
		return true;
	}
	box_decode_userdata( b3Body_GetUserData( b3Shape_GetBody( shapeIdA ) ), &bodyA, &ragA, &boneA );
	box_decode_userdata( b3Body_GetUserData( b3Shape_GetBody( shapeIdB ) ), &bodyB, &ragB, &boneB );
	return bx.customFilterFn( bodyA, bodyB, bx.customFilterCtx ) ? true : false;
}

static bool box_presolve_cb( b3ShapeId shapeIdA, b3ShapeId shapeIdB, b3Pos point, b3Vec3 normal, void *context ) {
	int bodyA = -1, bodyB = -1, ragA = -1, boneA = -1, ragB = -1, boneB = -1;
	vec3_t pt, nrm;
	(void)context;
	if ( !bx.preSolveFn ) {
		return true;
	}
	box_decode_userdata( b3Body_GetUserData( b3Shape_GetBody( shapeIdA ) ), &bodyA, &ragA, &boneA );
	box_decode_userdata( b3Body_GetUserData( b3Shape_GetBody( shapeIdB ) ), &bodyB, &ragB, &boneB );
	to_vec3( point, pt );
	to_vec3( normal, nrm );
	return bx.preSolveFn( bodyA, bodyB, pt, nrm, bx.preSolveCtx ) ? true : false;
}

qboolean Phys_GetClosestPoint_Impl( physBodyHandle_t body, const vec3_t target, vec3_t closestOut, float *distanceOut ) {
	b3Vec3 closest;
	float dist;

	if ( closestOut ) {
		VectorClear( closestOut );
	}
	if ( distanceOut ) {
		*distanceOut = 0.0f;
	}
	if ( !VALID_BODY( body ) || !target ) {
		return qfalse;
	}
	dist = b3Body_GetClosestPoint( bx.bodies[body].bodyId, &closest, from_vec3( target ) );
	if ( closestOut ) {
		to_vec3( closest, closestOut );
	}
	if ( distanceOut ) {
		*distanceOut = dist;
	}
	return qtrue;
}

qboolean Phys_SphereTimeOfImpact_Impl( const vec3_t from, const vec3_t to, float radius,
	physBodyHandle_t againstBody, physRayResult_t *result ) {
	physBodyDef_t def;
	qboolean hit;

	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_SPHERE;
	def.radius = radius > 0.0f ? radius : 8.0f;
	hit = Phys_ConvexSweepFiltered_Impl( &def, from, to, NULL, result, NULL );
	if ( hit && againstBody >= 0 && result && result->body != againstBody ) {
		/* Soft Step world cast hit something else — treat as miss for targeted TOI */
		Com_Memset( result, 0, sizeof( *result ) );
		result->body = -1;
		return qfalse;
	}
	return hit;
}

void Phys_SetCustomFilterCallback_Impl( PhysCustomFilterFn fn, void *userData ) {
	if ( !bx.initialized ) {
		return;
	}
	bx.customFilterFn = fn;
	bx.customFilterCtx = userData;
	b3World_SetCustomFilterCallback( bx.worldId, fn ? box_custom_filter_cb : NULL, NULL );
	Com_Printf( "[physics] Soft Step custom filter callback %s\n", fn ? "set" : "cleared" );
}

void Phys_SetPreSolveCallback_Impl( PhysPreSolveFn fn, void *userData ) {
	if ( !bx.initialized ) {
		return;
	}
	bx.preSolveFn = fn;
	bx.preSolveCtx = userData;
	b3World_SetPreSolveCallback( bx.worldId, fn ? box_presolve_cb : NULL, NULL );
	Com_Printf( "[physics] Soft Step pre-solve callback %s\n", fn ? "set" : "cleared" );
}

void Phys_SetBodyContinuous_Impl( physBodyHandle_t body, qboolean enable ) {
	if ( !VALID_BODY( body ) ) {
		return;
	}
	b3Body_SetBullet( bx.bodies[body].bodyId, enable ? true : false );
}

void Phys_SetBodySleepEnabled_Impl( physBodyHandle_t body, qboolean enable ) {
	if ( !VALID_BODY( body ) ) {
		return;
	}
	b3Body_EnableSleep( bx.bodies[body].bodyId, enable ? true : false );
}

void Phys_SetBodySleepThreshold_Impl( physBodyHandle_t body, float linearThreshold ) {
	if ( !VALID_BODY( body ) ) {
		return;
	}
	b3Body_SetSleepThreshold( bx.bodies[body].bodyId, linearThreshold );
}

void Phys_SetContactTuning_Impl( float hertz, float dampingRatio, float contactSpeed ) {
	if ( !bx.initialized ) {
		return;
	}
	b3World_SetContactTuning( bx.worldId, hertz, dampingRatio, contactSpeed );
}

void Phys_SetMaxLinearSpeed_Impl( float maxSpeed ) {
	if ( !bx.initialized ) {
		return;
	}
	b3World_SetMaximumLinearSpeed( bx.worldId, maxSpeed );
}

void Phys_EnableSpeculative_Impl( qboolean enable ) {
	if ( !bx.initialized ) {
		return;
	}
	b3World_EnableSpeculative( bx.worldId, enable ? true : false );
}

void Phys_SetDebugDrawFlags_Impl( unsigned flags ) {
	bx.debugDrawFlags = flags;
}

qboolean Phys_UpdateStaticTriMesh_Impl( physBodyHandle_t body, const float *verts, int numVerts,
	const int *indices, int numIndices ) {
	b3MeshDef meshDef;
	b3MeshData *mesh;
	b3MeshData *old;
	b3Vec3 *bverts = NULL;
	int32_t *bindices = NULL;
	int triCount, i;

	if ( !VALID_BODY( body ) || !bx.bodies[body].meshData || !b3Shape_IsValid( bx.bodies[body].shapeId ) ) {
		return qfalse;
	}
	if ( !verts || numVerts < 3 || !indices || numIndices < 3 ) {
		return qfalse;
	}
	triCount = numIndices / 3;
	if ( triCount < 1 ) {
		return qfalse;
	}
	if ( bx.meshCount >= (int)( sizeof( bx.meshes ) / sizeof( bx.meshes[0] ) ) ) {
		return qfalse;
	}

	bverts = (b3Vec3 *)malloc( (size_t)numVerts * sizeof( b3Vec3 ) );
	bindices = (int32_t *)malloc( (size_t)triCount * 3 * sizeof( int32_t ) );
	if ( !bverts || !bindices ) {
		free( bverts );
		free( bindices );
		return qfalse;
	}
	for ( i = 0; i < numVerts; i++ ) {
		bverts[i] = v3( verts[i * 3 + 0], verts[i * 3 + 1], verts[i * 3 + 2] );
	}
	for ( i = 0; i < triCount * 3; i++ ) {
		bindices[i] = (int32_t)indices[i];
	}
	memset( &meshDef, 0, sizeof( meshDef ) );
	meshDef.vertices = bverts;
	meshDef.vertexCount = numVerts;
	meshDef.indices = bindices;
	meshDef.triangleCount = triCount;
	meshDef.weldVertices = true;
	meshDef.identifyEdges = true;
	meshDef.useMedianSplit = true;
	mesh = b3CreateMesh( &meshDef, NULL, 0 );
	free( bverts );
	free( bindices );
	if ( !mesh ) {
		return qfalse;
	}

	old = bx.bodies[body].meshData;
	b3Shape_SetMesh( bx.bodies[body].shapeId, mesh, b3Vec3_one );
	bx.bodies[body].meshData = mesh;
	bx.meshes[bx.meshCount++] = mesh;
	if ( old ) {
		box_destroy_mesh( old );
	}
	b3World_RebuildStaticTree( bx.worldId );
	return qtrue;
}

void Phys_RebuildStaticTree_Impl( void ) {
	if ( !bx.initialized ) {
		return;
	}
	b3World_RebuildStaticTree( bx.worldId );
}

qboolean Phys_ReplayOpen_Impl( const char *path ) {
	const uint8_t *data;
	int size;
	int workers;

	Phys_ReplayClose_Impl();
	if ( !path || !path[0] ) {
		return qfalse;
	}
	bx.replayRec = b3LoadRecordingFromFile( path );
	if ( !bx.replayRec ) {
		Com_Printf( S_COLOR_YELLOW "phys_replay_open: failed to load %s\n", path );
		return qfalse;
	}
	data = b3Recording_GetData( bx.replayRec );
	size = b3Recording_GetSize( bx.replayRec );
	workers = bx.workerCount > 0 ? bx.workerCount : 1;
	bx.replayPlayer = b3RecPlayer_Create( data, size, workers );
	if ( !bx.replayPlayer ) {
		b3DestroyRecording( bx.replayRec );
		bx.replayRec = NULL;
		Com_Printf( S_COLOR_YELLOW "phys_replay_open: RecPlayer create failed\n" );
		return qfalse;
	}
	Com_Printf( "[physics] Soft Step RecPlayer open %s frames=%d\n",
		path, b3RecPlayer_GetFrameCount( bx.replayPlayer ) );
	return qtrue;
}

void Phys_ReplayClose_Impl( void ) {
	if ( bx.replayPlayer ) {
		b3RecPlayer_Destroy( bx.replayPlayer );
		bx.replayPlayer = NULL;
	}
	if ( bx.replayRec ) {
		b3DestroyRecording( bx.replayRec );
		bx.replayRec = NULL;
	}
}

qboolean Phys_ReplayStep_Impl( void ) {
	if ( !bx.replayPlayer ) {
		return qfalse;
	}
	return b3RecPlayer_StepFrame( bx.replayPlayer ) ? qtrue : qfalse;
}

void Phys_ReplaySeek_Impl( int frame ) {
	if ( !bx.replayPlayer ) {
		return;
	}
	b3RecPlayer_SeekFrame( bx.replayPlayer, frame );
}

int Phys_ReplayGetFrame_Impl( void ) {
	return bx.replayPlayer ? b3RecPlayer_GetFrame( bx.replayPlayer ) : -1;
}

int Phys_ReplayGetFrameCount_Impl( void ) {
	return bx.replayPlayer ? b3RecPlayer_GetFrameCount( bx.replayPlayer ) : 0;
}

qboolean Phys_ReplayHasDiverged_Impl( void ) {
	return ( bx.replayPlayer && b3RecPlayer_HasDiverged( bx.replayPlayer ) ) ? qtrue : qfalse;
}

qboolean Phys_ReplayIsOpen_Impl( void ) {
	return bx.replayPlayer ? qtrue : qfalse;
}

void Phys_SetHingeTargetAngle_Impl( physConstraintHandle_t handle, float targetRadians ) {
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_HINGE ) {
		return;
	}
	b3RevoluteJoint_SetTargetAngle( bx.constraints[handle].jointId, targetRadians );
}

void Phys_SetSliderTarget_Impl( physConstraintHandle_t handle, float targetTranslation ) {
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_SLIDER ) {
		return;
	}
	b3PrismaticJoint_SetTargetTranslation( bx.constraints[handle].jointId, targetTranslation );
}

void Phys_SetDistanceLength_Impl( physConstraintHandle_t handle, float length ) {
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_DISTANCE ) {
		return;
	}
	b3DistanceJoint_SetLength( bx.constraints[handle].jointId, length );
}

void Phys_SetWheelSuspension_Impl( physConstraintHandle_t handle, float hertz, float dampingRatio,
	float lower, float upper ) {
	b3JointId jid;
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_WHEEL ) {
		return;
	}
	jid = bx.constraints[handle].jointId;
	if ( hertz > 0.0f ) {
		b3WheelJoint_SetSuspensionHertz( jid, hertz );
	}
	if ( dampingRatio > 0.0f ) {
		b3WheelJoint_SetSuspensionDampingRatio( jid, dampingRatio );
	}
	b3WheelJoint_SetSuspensionLimits( jid, lower, upper );
}

void Phys_SetWheelSpin_Impl( physConstraintHandle_t handle, float speed, float maxTorque ) {
	b3JointId jid;
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_WHEEL ) {
		return;
	}
	jid = bx.constraints[handle].jointId;
	b3WheelJoint_EnableSpinMotor( jid, true );
	b3WheelJoint_SetSpinMotorSpeed( jid, speed );
	if ( maxTorque > 0.0f ) {
		b3WheelJoint_SetMaxSpinTorque( jid, maxTorque );
	}
}

void Phys_SetMotorVelocities_Impl( physConstraintHandle_t handle, const vec3_t linearVelocity,
	const vec3_t angularVelocity, float maxForce, float maxTorque ) {
	b3JointId jid;
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_MOTOR ) {
		return;
	}
	jid = bx.constraints[handle].jointId;
	if ( linearVelocity ) {
		b3MotorJoint_SetLinearVelocity( jid, from_vec3( linearVelocity ) );
	}
	if ( angularVelocity ) {
		b3MotorJoint_SetAngularVelocity( jid, from_vec3( angularVelocity ) );
	}
	if ( maxForce > 0.0f ) {
		b3MotorJoint_SetMaxVelocityForce( jid, maxForce );
	}
	if ( maxTorque > 0.0f ) {
		b3MotorJoint_SetMaxVelocityTorque( jid, maxTorque );
	}
}

void Phys_SetSphericalTarget_Impl( physConstraintHandle_t handle, const vec3_t rotationDeg ) {
	b3JointId jid;
	b3Quat q;
	vec3_t zero = { 0, 0, 0 };
	if ( !VALID_CON( handle ) || bx.constraints[handle].type != PHYS_CONSTRAINT_CONE_TWIST ) {
		return;
	}
	jid = bx.constraints[handle].jointId;
	q = quat_from_euler_deg( rotationDeg ? rotationDeg : zero );
	b3SphericalJoint_EnableMotor( jid, true );
	b3SphericalJoint_SetTargetRotation( jid, q );
}

void Phys_SetBodyDamping_Impl( physBodyHandle_t body, float linearDamping, float angularDamping ) {
	if ( !VALID_BODY( body ) ) {
		return;
	}
	if ( linearDamping >= 0.0f ) {
		b3Body_SetLinearDamping( bx.bodies[body].bodyId, linearDamping );
	}
	if ( angularDamping >= 0.0f ) {
		b3Body_SetAngularDamping( bx.bodies[body].bodyId, angularDamping );
	}
}

void Phys_SetBodyType_Impl( physBodyHandle_t body, physBodyType_t type ) {
	b3BodyType bt;
	if ( !VALID_BODY( body ) ) {
		return;
	}
	switch ( type ) {
	case PHYS_BODY_DYNAMIC:   bt = b3_dynamicBody; break;
	case PHYS_BODY_KINEMATIC: bt = b3_kinematicBody; break;
	case PHYS_BODY_STATIC:
	default:                  bt = b3_staticBody; break;
	}
	b3Body_SetType( bx.bodies[body].bodyId, bt );
	bx.bodies[body].bodyType = type;
}

void Phys_ApplyWind_Impl( physBodyHandle_t body, const vec3_t wind, float drag, float lift, float maxSpeed ) {
	b3ShapeId shapes[8];
	int n, i;
	if ( !VALID_BODY( body ) || !wind ) {
		return;
	}
	n = b3Body_GetShapes( bx.bodies[body].bodyId, shapes, 8 );
	for ( i = 0; i < n; i++ ) {
		b3Shape_ApplyWind( shapes[i], from_vec3( wind ), drag, lift, maxSpeed, true );
	}
}

int Phys_Explode_Impl( const vec3_t center, float radius, float impulsePerArea, float falloff,
	unsigned maskBits ) {
	b3ExplosionDef def;
	if ( !bx.initialized || !center || radius <= 0.0f || impulsePerArea == 0.0f ) {
		return 0;
	}
	def = b3DefaultExplosionDef();
	def.position = from_vec3( center );
	def.radius = radius;
	def.impulsePerArea = impulsePerArea;
	if ( falloff <= 0.0f ) {
		def.falloff = 0.0f;
	} else {
		def.falloff = radius / falloff;
		if ( def.falloff < 1.0f ) {
			def.falloff = 1.0f;
		}
	}
	if ( maskBits ) {
		def.maskBits = (uint64_t)maskBits;
	}
	b3World_Explode( bx.worldId, &def );
	return 1;
}

typedef struct {
	physRayResult_t *results;
	int              maxResults;
	int              count;
} boxRayAllCtx;

static float box_ray_all_cb( b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction,
	uint64_t userMaterialId, int triangleIndex, int childIndex, void *context ) {
	boxRayAllCtx *ctx = (boxRayAllCtx *)context;
	physRayResult_t *r;
	int body = -1, rag = -1, bone = -1;
	int i, insert;

	if ( !ctx || !ctx->results || ctx->maxResults <= 0 ) {
		return fraction;
	}
	box_decode_userdata( b3Body_GetUserData( b3Shape_GetBody( shapeId ) ), &body, &rag, &bone );

	/* Insert sorted by fraction (stable ascending). */
	insert = ctx->count;
	if ( ctx->count < ctx->maxResults ) {
		ctx->count++;
	} else if ( fraction >= ctx->results[ctx->count - 1].fraction ) {
		return fraction; /* farther than worst kept hit */
	} else {
		insert = ctx->count - 1;
	}
	for ( i = 0; i < insert; i++ ) {
		if ( fraction < ctx->results[i].fraction ) {
			insert = i;
			break;
		}
	}
	if ( insert < ctx->count - 1 ) {
		memmove( &ctx->results[insert + 1], &ctx->results[insert],
			(size_t)( ctx->count - 1 - insert ) * sizeof( physRayResult_t ) );
	}
	r = &ctx->results[insert];
	memset( r, 0, sizeof( *r ) );
	r->hit = qtrue;
	r->fraction = fraction;
	r->body = body;
	r->userMaterialId = (unsigned)userMaterialId;
	r->triangleIndex = triangleIndex;
	r->childIndex = childIndex;
	to_vec3( point, r->hitPoint );
	to_vec3( normal, r->hitNormal );
	return 1.0f; /* continue collecting all hits */
}

int Phys_RayCastAll_Impl( const vec3_t from, const vec3_t to, physRayResult_t *results, int maxResults,
	const physQueryFilter_t *filter ) {
	b3Vec3 origin, translation;
	b3QueryFilter qf;
	boxRayAllCtx ctx;

	if ( !results || maxResults <= 0 ) {
		return 0;
	}
	memset( results, 0, (size_t)maxResults * sizeof( physRayResult_t ) );
	if ( !bx.initialized || !from || !to ) {
		return 0;
	}
	origin = from_vec3( from );
	translation = b3Sub( from_vec3( to ), origin );
	qf = box_make_query_filter( filter );
	ctx.results = results;
	ctx.maxResults = maxResults;
	ctx.count = 0;
	b3World_CastRay( bx.worldId, origin, translation, qf, box_ray_all_cb, &ctx );
	return ctx.count;
}

static float box_friction_mix_cb( float frictionA, uint64_t userMaterialIdA, float frictionB, uint64_t userMaterialIdB ) {
	if ( bx.frictionMixFn ) {
		return bx.frictionMixFn( frictionA, (unsigned)userMaterialIdA, frictionB, (unsigned)userMaterialIdB );
	}
	return sqrtf( frictionA * frictionB );
}

static float box_restitution_mix_cb( float restitutionA, uint64_t userMaterialIdA, float restitutionB,
	uint64_t userMaterialIdB ) {
	if ( bx.restitutionMixFn ) {
		return bx.restitutionMixFn( restitutionA, (unsigned)userMaterialIdA, restitutionB, (unsigned)userMaterialIdB );
	}
	return restitutionA > restitutionB ? restitutionA : restitutionB;
}

void Phys_SetFrictionCallback_Impl( PhysFrictionMixFn fn ) {
	if ( !bx.initialized ) {
		return;
	}
	bx.frictionMixFn = fn;
	b3World_SetFrictionCallback( bx.worldId, fn ? box_friction_mix_cb : NULL );
	Com_Printf( "[physics] Soft Step friction mix callback %s\n", fn ? "set" : "cleared" );
}

void Phys_SetRestitutionCallback_Impl( PhysRestitutionMixFn fn ) {
	if ( !bx.initialized ) {
		return;
	}
	bx.restitutionMixFn = fn;
	b3World_SetRestitutionCallback( bx.worldId, fn ? box_restitution_mix_cb : NULL );
	Com_Printf( "[physics] Soft Step restitution mix callback %s\n", fn ? "set" : "cleared" );
}

#endif /* USE_BOX3D_PHYSICS_IMPL */
