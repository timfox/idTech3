/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Physics C dispatch layer (Box3D default / Bullet optional).
Routes Phys_* calls to the compiled substrate via phys_impl.h.

===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "phys_bullet.h"
#include "phys_character.h"
#include "phys_middleware.h"
#include "phys_events.h"
#include "phys_materials.h"
#include "phys_props.h"
#include "phys_volumes.h"
#include "phys_debugdraw.h"
#include "phys_solvers.h"

#if defined(USE_BOX3D_PHYSICS_IMPL) || defined(USE_BULLET_PHYSICS_IMPL)
#define PHYS_HAS_IMPL 1
#endif

#include <math.h>

static qboolean physInitialized = qfalse;

static cvar_t *phys_enabled;
static cvar_t *phys_timestep;
static cvar_t *phys_maxSubSteps;
static cvar_t *phys_gravity;
static cvar_t *phys_debugDraw;
static cvar_t *phys_workers;
static cvar_t *phys_sleep;
static cvar_t *phys_ccd;
static cvar_t *phys_ragdoll_stiffness;
static cvar_t *phys_ragdoll_damping;
static cvar_t *phys_ragdoll_muscles;
static cvar_t *phys_ragdoll_balance;
static cvar_t *phys_dmm_enabled;
static cvar_t *phys_dmm_resolution;
static cvar_t *phys_dmm_fracture;

#ifdef PHYS_HAS_IMPL
#include "phys_impl.h"
#endif

void Phys_RegisterCvars(void) {
	phys_enabled          = Cvar_Get("phys_enabled",          "1",     CVAR_ARCHIVE);
	phys_timestep         = Cvar_Get("phys_timestep",         "0.016", CVAR_ARCHIVE);
	phys_maxSubSteps      = Cvar_Get("phys_maxSubSteps",      "4",     CVAR_ARCHIVE);
	phys_gravity          = Cvar_Get("phys_gravity",          "-800",  CVAR_ARCHIVE);
	phys_debugDraw        = Cvar_Get("phys_debugDraw",        "0",     CVAR_ARCHIVE);
	phys_workers          = Cvar_Get("phys_workers",          "0",     CVAR_ARCHIVE);
	phys_sleep            = Cvar_Get("phys_sleep",            "1",     CVAR_ARCHIVE);
	phys_ccd              = Cvar_Get("phys_ccd",              "1",     CVAR_ARCHIVE);
	phys_ragdoll_stiffness= Cvar_Get("phys_ragdoll_stiffness","0.8",   CVAR_ARCHIVE);
	phys_ragdoll_damping  = Cvar_Get("phys_ragdoll_damping",  "0.4",   CVAR_ARCHIVE);
	phys_ragdoll_muscles  = Cvar_Get("phys_ragdoll_muscles",  "1.0",   CVAR_ARCHIVE);
	phys_ragdoll_balance  = Cvar_Get("phys_ragdoll_balance",  "1",     CVAR_ARCHIVE);
	phys_dmm_enabled      = Cvar_Get("phys_dmm_enabled",      "1",     CVAR_ARCHIVE);
	phys_dmm_resolution   = Cvar_Get("phys_dmm_resolution",   "8",     CVAR_ARCHIVE);
	phys_dmm_fracture     = Cvar_Get("phys_dmm_fracture",     "1",     CVAR_ARCHIVE);
	Cvar_SetDescription(phys_workers, "Box3D Soft Step worker threads (0=auto up to 8). Requires vid_restart/phys reinit.");
	Cvar_SetDescription(phys_sleep, "Enable island sleep for idle bodies (Box3D).");
	Cvar_SetDescription(phys_ccd, "Enable continuous collision for fast bodies (Box3D).");
	Phys_CharacterInit();
}

physBackendKind_t Phys_GetBackend(void) {
#ifdef USE_BOX3D_PHYSICS_IMPL
	return PHYS_BACKEND_BOX3D;
#elif defined(USE_BULLET_PHYSICS_IMPL)
	return PHYS_BACKEND_BULLET;
#else
	return PHYS_BACKEND_NONE;
#endif
}

const char *Phys_GetBackendName(void) {
	switch ( Phys_GetBackend() ) {
	case PHYS_BACKEND_BOX3D:  return "box3d";
	case PHYS_BACKEND_BULLET: return "bullet";
	default:                  return "none";
	}
}

qboolean Phys_Init(void) {
	if (physInitialized) return qtrue;

	Phys_RegisterCvars();

	if (!phys_enabled || !phys_enabled->integer) {
		Com_Printf("Physics: disabled by cvar (backend=%s)\n", Phys_GetBackendName());
		return qfalse;
	}

#ifdef PHYS_HAS_IMPL
	if (!Phys_Init_Impl()) {
		Com_Printf(S_COLOR_RED "Physics: %s backend init failed\n", Phys_GetBackendName());
		return qfalse;
	}
	{
		vec3_t g;
		VectorSet(g, 0, 0, phys_gravity->value);
		Phys_SetGravity_Impl(g);
	}
	PhysMiddleware_Init();
	PhysSolvers_Init();
	Com_Printf("Physics: initialized (backend=%s)\n", Phys_GetBackendName());
#else
	Com_Printf(S_COLOR_YELLOW "Physics: no substrate compiled (set IDTECH3_PHYSICS_BACKEND)\n");
	return qfalse;
#endif

	physInitialized = qtrue;
	return qtrue;
}

void Phys_Shutdown(void) {
	if (!physInitialized) return;
	PhysSolvers_Shutdown();
	PhysMiddleware_Shutdown();
#ifdef PHYS_HAS_IMPL
	Phys_Shutdown_Impl();
#endif
	physInitialized = qfalse;
	Com_Printf("Physics: shut down (backend=%s)\n", Phys_GetBackendName());
}

void Phys_SetGravity(const vec3_t gravity) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_SetGravity_Impl(gravity);
#else
	(void)gravity;
#endif
}

void Phys_ClearWorld(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		PhysProp_Clear();
		PhysVolume_Clear();
		Phys_ClearWorld_Impl();
	}
#endif
}

physBodyHandle_t Phys_AddStaticTriMesh(const float *verts, int numVerts, const int *indices, int numIndices) {
	if (!physInitialized || !verts || numVerts < 3 || !indices || numIndices < 3) {
		return -1;
	}
#ifdef PHYS_HAS_IMPL
	return Phys_AddStaticTriMesh_Impl(verts, numVerts, indices, numIndices);
#else
	(void)verts; (void)numVerts; (void)indices; (void)numIndices;
	return -1;
#endif
}

physBodyHandle_t Phys_AddStaticCompoundBoxes(const float *centersXYZ, const float *halfExtentsXYZ, int count) {
	if (!physInitialized || !centersXYZ || !halfExtentsXYZ || count < 1) {
		return -1;
	}
#ifdef PHYS_HAS_IMPL
	return Phys_AddStaticCompoundBoxes_Impl(centersXYZ, halfExtentsXYZ, count);
#else
	return -1;
#endif
}

physBodyHandle_t Phys_AddStaticHeightField(const float *heights, int countX, int countY,
	float cellSize, float heightScale, const vec3_t origin) {
	if (!physInitialized || !heights || countX < 2 || countY < 2) {
		return -1;
	}
#ifdef PHYS_HAS_IMPL
	return Phys_AddStaticHeightField_Impl(heights, countX, countY, cellSize, heightScale, origin);
#else
	(void)cellSize; (void)heightScale; (void)origin;
	return -1;
#endif
}

qboolean Phys_MoverStep(vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		return Phys_MoverStep_Impl(origin, velocity, radius, height, wishDir, wishSpeed, dt, jump, groundedOut);
	}
#endif
	(void)origin; (void)velocity; (void)radius; (void)height;
	(void)wishDir; (void)wishSpeed; (void)dt; (void)jump; (void)groundedOut;
	return qfalse;
}

int Phys_GetWorkerCount(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		return Phys_GetWorkerCount_Impl();
	}
#endif
	return 0;
}

qboolean Phys_LoadBSPCollision(void) {
	vec3_t worldMins = {-4096, -4096, -4096};
	vec3_t worldMaxs = { 4096,  4096,  4096};
	float step = 32.0f;
	float *verts;
	int *indices;
	int numVerts = 0, numIndices = 0;
	int gridW, gridH, gx, gy;
	int maxVerts, maxIndices;
	trace_t tr;
	vec3_t start, end, mins, maxs;
	cvar_t *stepCv;

	if (!physInitialized) return qfalse;

	Phys_ClearWorld();

	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);

	/* Prefer map AABB when available; denser grid via phys_bspGridStep. */
	if ( CM_NumInlineModels() > 0 ) {
		vec3_t cmMins, cmMaxs;
		CM_ModelBounds( CM_InlineModel( 0 ), cmMins, cmMaxs );
		if ( cmMaxs[0] > cmMins[0] && cmMaxs[1] > cmMins[1] ) {
			VectorCopy( cmMins, worldMins );
			VectorCopy( cmMaxs, worldMaxs );
			/* Pad vertically so vertical traces find floors */
			worldMins[2] -= 64.0f;
			worldMaxs[2] += 256.0f;
		}
	}

	stepCv = Cvar_Get( "phys_bspGridStep", "24", CVAR_ARCHIVE );
	Cvar_SetDescription( stepCv,
		"BSP Soft Step collision height-grid cell size (smaller = denser tri-mesh, more cost)." );
	step = stepCv && stepCv->value > 4.0f ? stepCv->value : 24.0f;
	if ( step < 8.0f ) {
		step = 8.0f;
	}
	if ( step > 64.0f ) {
		step = 64.0f;
	}

	gridW = (int)((worldMaxs[0] - worldMins[0]) / step);
	gridH = (int)((worldMaxs[1] - worldMins[1]) / step);
	if (gridW > 384) gridW = 384;
	if (gridH > 384) gridH = 384;
	if (gridW < 2) gridW = 2;
	if (gridH < 2) gridH = 2;

	maxVerts = (gridW + 1) * (gridH + 1);
	maxIndices = gridW * gridH * 6;
	verts = (float *)Z_Malloc(maxVerts * 3 * sizeof(float));
	indices = (int *)Z_Malloc(maxIndices * sizeof(int));

	if (!verts || !indices) {
		if (verts) Z_Free(verts);
		if (indices) Z_Free(indices);
		return qfalse;
	}

	for (gy = 0; gy <= gridH; gy++) {
		for (gx = 0; gx <= gridW; gx++) {
			float x = worldMins[0] + gx * step;
			float y = worldMins[1] + gy * step;

			VectorSet(start, x, y, worldMaxs[2]);
			VectorSet(end, x, y, worldMins[2]);
			CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);

			if (tr.fraction < 1.0f) {
				verts[numVerts * 3 + 0] = tr.endpos[0];
				verts[numVerts * 3 + 1] = tr.endpos[1];
				verts[numVerts * 3 + 2] = tr.endpos[2];
			} else {
				verts[numVerts * 3 + 0] = x;
				verts[numVerts * 3 + 1] = y;
				verts[numVerts * 3 + 2] = worldMins[2];
			}
			numVerts++;
		}
	}

	for (gy = 0; gy < gridH; gy++) {
		for (gx = 0; gx < gridW; gx++) {
			int stride = gridW + 1;
			int v0 = gy * stride + gx;
			int v1 = v0 + 1;
			int v2 = v0 + stride;
			int v3 = v2 + 1;
			indices[numIndices++] = v0;
			indices[numIndices++] = v1;
			indices[numIndices++] = v2;
			indices[numIndices++] = v1;
			indices[numIndices++] = v3;
			indices[numIndices++] = v2;
		}
	}

	/* Prefer a single static triangle mesh (Box3D native); fall back to cell boxes. */
	{
		physBodyHandle_t mesh = Phys_AddStaticTriMesh(verts, numVerts, indices, numIndices);
		if (mesh >= 0) {
			Com_Printf("Physics: created floor collision mesh (%d verts, %d tris)\n",
				numVerts, numIndices / 3);
		} else {
			float *centers = NULL;
			float *halves = NULL;
			int cellCount = 0;
			int maxCells = gridW * gridH;
			int ci = 0;

			centers = (float *)Z_Malloc(maxCells * 3 * sizeof(float));
			halves = (float *)Z_Malloc(maxCells * 3 * sizeof(float));
			if (centers && halves) {
				for (gy = 0; gy < gridH; gy++) {
					for (gx = 0; gx < gridW; gx++) {
						int idx = gy * (gridW + 1) + gx;
						float z = verts[idx * 3 + 2];
						if (z <= worldMins[2] + 1.0f) {
							continue;
						}
						centers[ci * 3 + 0] = verts[idx * 3 + 0];
						centers[ci * 3 + 1] = verts[idx * 3 + 1];
						centers[ci * 3 + 2] = z - 2.0f;
						halves[ci * 3 + 0] = step * 0.5f;
						halves[ci * 3 + 1] = step * 0.5f;
						halves[ci * 3 + 2] = 2.0f;
						ci++;
					}
				}
				if (ci > 0 && Phys_AddStaticCompoundBoxes(centers, halves, ci) >= 0) {
					cellCount = ci;
					Com_Printf("Physics: created floor compound (%d boxes in 1 body)\n", cellCount);
				}
			}
			if (cellCount == 0) {
				for (gy = 0; gy < gridH; gy++) {
					for (gx = 0; gx < gridW; gx++) {
						int idx = gy * (gridW + 1) + gx;
						float z = verts[idx * 3 + 2];
						physBodyDef_t def;

						if (z <= worldMins[2] + 1.0f) {
							continue;
						}

						Com_Memset(&def, 0, sizeof(def));
						def.shape = PHYS_SHAPE_BOX;
						def.type = PHYS_BODY_STATIC;
						def.mass = 0;
						def.halfExtents[0] = step * 0.5f;
						def.halfExtents[1] = step * 0.5f;
						def.halfExtents[2] = 2.0f;
						def.position[0] = verts[idx * 3 + 0];
						def.position[1] = verts[idx * 3 + 1];
						def.position[2] = z - 2.0f;
						def.friction = 0.8f;
						def.restitution = 0.3f;
						Phys_CreateBody(&def);
						cellCount++;
					}
				}
				Com_Printf("Physics: created %d floor collision cells\n", cellCount);
			}
			if (centers) {
				Z_Free(centers);
			}
			if (halves) {
				Z_Free(halves);
			}
		}
	}

	/* Also add 4 wall planes around the map boundary */
	{
		physBodyDef_t wdef;
		Com_Memset(&wdef, 0, sizeof(wdef));
		wdef.shape = PHYS_SHAPE_BOX;
		wdef.type = PHYS_BODY_STATIC;
		wdef.mass = 0;
		wdef.friction = 0.5f;
		wdef.restitution = 0.2f;

		/* Floor (catch-all at very bottom) */
		wdef.halfExtents[0] = 4096; wdef.halfExtents[1] = 4096; wdef.halfExtents[2] = 1;
		wdef.position[0] = 0; wdef.position[1] = 0; wdef.position[2] = worldMins[2] - 1;
		Phys_CreateBody(&wdef);

		/* Ceiling */
		wdef.position[2] = worldMaxs[2] + 1;
		Phys_CreateBody(&wdef);

		/* Walls */
		wdef.halfExtents[0] = 1; wdef.halfExtents[1] = 4096; wdef.halfExtents[2] = 4096;
		wdef.position[0] = worldMins[0] - 1; wdef.position[1] = 0; wdef.position[2] = 0;
		Phys_CreateBody(&wdef);
		wdef.position[0] = worldMaxs[0] + 1;
		Phys_CreateBody(&wdef);

		wdef.halfExtents[0] = 4096; wdef.halfExtents[1] = 1; wdef.halfExtents[2] = 4096;
		wdef.position[0] = 0; wdef.position[1] = worldMins[1] - 1; wdef.position[2] = 0;
		Phys_CreateBody(&wdef);
		wdef.position[1] = worldMaxs[1] + 1;
		Phys_CreateBody(&wdef);
	}

	Z_Free(verts);
	Z_Free(indices);

	Com_Printf("Physics: loaded BSP collision (%d raycasts, %d floor cells + 6 boundary planes)\n",
		numVerts, numVerts);
	return qtrue;
}

void Phys_StepSimulation(float dt) {
	if (!physInitialized) return;
#ifdef PHYS_HAS_IMPL
	PhysSolvers_PreStep(dt);
	Phys_StepSimulation_Impl(dt);
	Phys_ProcessContactEvents_Impl();
	PhysSolvers_PostStep(dt);
#else
	(void)dt;
#endif
}

physBodyHandle_t Phys_CreateBody(const physBodyDef_t *def) {
#ifdef PHYS_HAS_IMPL
	physBodyDef_t local;

	if (physInitialized && def) {
		if (def->materialId > 0) {
			local = *def;
			PhysMat_ApplyToBodyDef(&local, def->materialId);
			return Phys_CreateBody_Impl(&local);
		}
		return Phys_CreateBody_Impl(def);
	}
#endif
	(void)def;
	return -1;
}

void Phys_DestroyBody(physBodyHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_DestroyBody_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_GetBodyTransform(physBodyHandle_t handle, physTransform_t *out) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_GetBodyTransform_Impl(handle, out); return; }
#endif
	(void)handle;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

void Phys_SetBodyTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyTransform_Impl(handle, pos, rot); return; }
#endif
	(void)handle; (void)pos; (void)rot;
}

void Phys_SetBodyTargetTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot, float timeStep) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyTargetTransform_Impl(handle, pos, rot, timeStep); return; }
#endif
	(void)handle; (void)pos; (void)rot; (void)timeStep;
}

void Phys_SetBodyGravityScale(physBodyHandle_t handle, float scale) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyGravityScale_Impl(handle, scale); return; }
#endif
	(void)handle; (void)scale;
}

void Phys_SetBodyMotionLocks(physBodyHandle_t handle, int lockBits) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyMotionLocks_Impl(handle, lockBits); return; }
#endif
	(void)handle; (void)lockBits;
}

void Phys_ApplyForce(physBodyHandle_t handle, const vec3_t force, const vec3_t point) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_ApplyForce_Impl(handle, force, point); return; }
#endif
	(void)handle; (void)force; (void)point;
}

void Phys_ApplyImpulse(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_ApplyImpulse_Impl(handle, impulse, point); return; }
#endif
	(void)handle; (void)impulse; (void)point;
}

void Phys_ApplyTorque(physBodyHandle_t handle, const vec3_t torque) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_ApplyTorque_Impl(handle, torque); return; }
#endif
	(void)handle; (void)torque;
}

void Phys_SetBodyVelocity(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyVelocity_Impl(handle, linear, angular); return; }
#endif
	(void)handle; (void)linear; (void)angular;
}

void Phys_SetBodyActive(physBodyHandle_t handle, qboolean active) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyActive_Impl(handle, active); return; }
#endif
	(void)handle; (void)active;
}

physBodyType_t Phys_GetBodyType(physBodyHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		return Phys_GetBodyType_Impl(handle);
	}
#endif
	(void)handle;
	return PHYS_BODY_STATIC;
}

qboolean Phys_IsBodyDynamic(physBodyHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		return Phys_IsBodyDynamic_Impl(handle);
	}
#endif
	(void)handle;
	return qfalse;
}

/*
===============
Phys_ApplyImpulseRadius
Applies a radial impulse to all dynamic bodies overlapping the sphere.
falloff 0 = flat magnitude; 1 = linear with distance; >1 = sharper falloff.
Returns number of bodies affected.
===============
*/
int Phys_ApplyImpulseRadius(const vec3_t center, float radius, float magnitude, float falloff) {
	physBodyHandle_t hits[128];
	int count;
	int i;
	int affected = 0;

	if (!center || radius <= 0.0f || magnitude == 0.0f) {
		return 0;
	}
	if (falloff < 0.0f) {
		falloff = 0.0f;
	}

#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		int handled = Phys_ApplyImpulseRadius_Impl(center, radius, magnitude, falloff);
		if (handled >= 0) {
			return handled;
		}
	}
#endif

	count = Phys_OverlapSphere(center, radius, hits, 128);
	for (i = 0; i < count; i++) {
		physTransform_t xf;
		vec3_t dir, impulse, point;
		float dist;
		float scale;

		if (!Phys_IsBodyDynamic(hits[i])) {
			continue;
		}
		Phys_GetBodyTransform(hits[i], &xf);
		VectorSubtract(xf.position, center, dir);
		dist = VectorNormalize(dir);
		if (dist < 0.001f) {
			VectorSet(dir, 0.0f, 0.0f, 1.0f);
			dist = 0.0f;
		}
		if (falloff <= 0.0f) {
			scale = 1.0f;
		} else {
			scale = 1.0f - (dist / radius);
			if (scale < 0.0f) {
				scale = 0.0f;
			}
			if (falloff != 1.0f) {
				scale = powf(scale, falloff);
			}
		}
		VectorScale(dir, magnitude * scale, impulse);
		/* slight upward bias helps debris clear the ground */
		impulse[2] += magnitude * scale * 0.15f;
		VectorCopy(xf.position, point);
		Phys_ApplyImpulse(hits[i], impulse, point);
		affected++;
	}
	return affected;
}

physConstraintHandle_t Phys_CreateConstraint(const physConstraintDef_t *def) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_CreateConstraint_Impl(def);
#endif
	(void)def;
	return -1;
}

void Phys_DestroyConstraint(physConstraintHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_DestroyConstraint_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_SetConstraintLimits(physConstraintHandle_t handle, float lower, float upper) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetConstraintLimits_Impl(handle, lower, upper); return; }
#endif
	(void)handle; (void)lower; (void)upper;
}

void Phys_SetConstraintMotor(physConstraintHandle_t handle, qboolean enable, float speed, float maxForce) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetConstraintMotor_Impl(handle, enable, speed, maxForce); return; }
#endif
	(void)handle; (void)enable; (void)speed; (void)maxForce;
}

void Phys_SetConstraintBreakForce(physConstraintHandle_t handle, float force, float torque) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetConstraintBreakForce_Impl(handle, force, torque); return; }
#endif
	(void)handle; (void)force; (void)torque;
}

void Phys_SetWheelSteering(physConstraintHandle_t handle, float angleRadians, float maxTorque) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetWheelSteering_Impl(handle, angleRadians, maxTorque); return; }
#endif
	(void)handle; (void)angleRadians; (void)maxTorque;
}

int Phys_AttachShape(physBodyHandle_t body, const physBodyDef_t *shapeDef) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_AttachShape_Impl(body, shapeDef);
#endif
	(void)body; (void)shapeDef;
	return -1;
}

void Phys_DestroyAttachedShape(physBodyHandle_t body, int shapeIndex) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_DestroyAttachedShape_Impl(body, shapeIndex); return; }
#endif
	(void)body; (void)shapeIndex;
}

void Phys_SetBodyFilter(physBodyHandle_t body, int categoryBits, int maskBits) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyFilter_Impl(body, categoryBits, maskBits); return; }
#endif
	(void)body; (void)categoryBits; (void)maskBits;
}

physRagdollHandle_t Phys_CreateRagdoll(const physRagdollDef_t *def) {
	physRagdollDef_t local;

#ifdef PHYS_HAS_IMPL
	if ( !physInitialized || !def ) {
		return -1;
	}
	local = *def;
	if ( phys_ragdoll_stiffness && local.jointStiffness <= 0.0f ) {
		local.jointStiffness = phys_ragdoll_stiffness->value;
	}
	if ( phys_ragdoll_damping && local.jointDamping <= 0.0f ) {
		local.jointDamping = phys_ragdoll_damping->value;
	}
	if ( phys_ragdoll_muscles && local.muscleStrength <= 0.0f ) {
		local.muscleStrength = phys_ragdoll_muscles->value;
	}
	{
		physRagdollHandle_t h = Phys_CreateRagdoll_Impl( &local );
		if ( h >= 0 && phys_ragdoll_balance && phys_ragdoll_balance->integer ) {
			vec3_t up;
			VectorSet( up, local.rootPosition[0], local.rootPosition[1] + 48.0f, local.rootPosition[2] );
			Phys_RagdollSetBalance( h, qtrue, up );
		}
		return h;
	}
#else
	(void)def;
	return -1;
#endif
}

void Phys_DestroyRagdoll(physRagdollHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_DestroyRagdoll_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_RagdollApplyImpact(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollApplyImpact_Impl(handle, point, impulse, radius); return; }
#endif
	(void)handle; (void)point; (void)impulse; (void)radius;
}

void Phys_RagdollSetBalance(physRagdollHandle_t handle, qboolean enabled, const vec3_t target) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollSetBalance_Impl(handle, enabled, target); return; }
#endif
	(void)handle; (void)enabled; (void)target;
}

void Phys_RagdollReach(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollReach_Impl(handle, limbIndex, target, strength); return; }
#endif
	(void)handle; (void)limbIndex; (void)target; (void)strength;
}

void Phys_RagdollGetBoneTransform(physRagdollHandle_t handle, int boneIndex, physTransform_t *out) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollGetBoneTransform_Impl(handle, boneIndex, out); return; }
#endif
	(void)handle; (void)boneIndex;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

void Phys_RagdollSetMuscleStiffness(physRagdollHandle_t handle, float stiffness) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollSetMuscleStiffness_Impl(handle, stiffness); return; }
#endif
	(void)handle; (void)stiffness;
}

void Phys_RagdollBlendToAnimation(physRagdollHandle_t handle, float blend) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollBlendToAnimation_Impl(handle, blend); return; }
#endif
	(void)handle; (void)blend;
}

void Phys_RagdollSetBoneAnimTarget(physRagdollHandle_t handle, int boneIndex,
	const vec3_t position, const vec3_t rotationDeg) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		Phys_RagdollSetBoneAnimTarget_Impl(handle, boneIndex, position, rotationDeg);
		return;
	}
#endif
	(void)handle; (void)boneIndex; (void)position; (void)rotationDeg;
}

void Phys_RagdollClearAnimTargets(physRagdollHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollClearAnimTargets_Impl(handle); return; }
#endif
	(void)handle;
}

void Phys_RagdollApplyBoneTorque(physRagdollHandle_t handle, int boneIndex, const vec3_t torque) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_RagdollApplyBoneTorque_Impl(handle, boneIndex, torque); return; }
#endif
	(void)handle; (void)boneIndex; (void)torque;
}

int Phys_GetRagdollCount(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) {
		return Phys_GetRagdollCount_Impl();
	}
#endif
	return 0;
}

dmmObjectHandle_t Dmm_CreateObject(const dmmObjectDef_t *def) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Dmm_CreateObject_Impl(def);
#endif
	(void)def;
	return -1;
}

void Dmm_DestroyObject(dmmObjectHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Dmm_DestroyObject_Impl(handle);
#else
	(void)handle;
#endif
}

void Dmm_ApplyForce(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Dmm_ApplyForce_Impl(handle, force, point); return; }
#endif
	(void)handle; (void)force; (void)point;
}

void Dmm_ApplyImpact(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Dmm_ApplyImpact_Impl(handle, point, direction, energy); return; }
#endif
	(void)handle; (void)point; (void)direction; (void)energy;
}

void Dmm_GetState(dmmObjectHandle_t handle, dmmState_t *out) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Dmm_GetState_Impl(handle, out); return; }
#endif
	(void)handle;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

qboolean Dmm_IsFractured(dmmObjectHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Dmm_IsFractured_Impl(handle);
#endif
	(void)handle;
	return qfalse;
}

int Dmm_GetFragments(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Dmm_GetFragments_Impl(handle, fragments, maxFragments);
#endif
	(void)handle; (void)fragments; (void)maxFragments;
	return 0;
}

void Dmm_SetMaterialParams(dmmObjectHandle_t handle, float stiffness, float yield, float fracture) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Dmm_SetMaterialParams_Impl(handle, stiffness, yield, fracture); return; }
#endif
	(void)handle; (void)stiffness; (void)yield; (void)fracture;
}

qboolean Phys_RayCast(const vec3_t from, const vec3_t to, physRayResult_t *result) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_RayCast_Impl(from, to, result);
#endif
	(void)from; (void)to;
	if (result) Com_Memset(result, 0, sizeof(*result));
	return qfalse;
}

int Phys_OverlapSphere(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_OverlapSphere_Impl(center, radius, results, maxResults);
#endif
	(void)center; (void)radius; (void)results; (void)maxResults;
	return 0;
}

int Phys_OverlapBox(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_OverlapBox_Impl(center, halfExtents, results, maxResults);
#endif
	(void)center; (void)halfExtents; (void)results; (void)maxResults;
	return 0;
}

int Phys_OverlapShape(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_OverlapShape_Impl(center, radius, results, maxResults);
#endif
	return Phys_OverlapSphere(center, radius, results, maxResults);
}

void Phys_GetSoftStepProfile(physSoftStepProfile_t *out) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_GetSoftStepProfile_Impl(out); return; }
#endif
	if (out) Com_Memset(out, 0, sizeof(*out));
}

void Phys_StartRecording(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_StartRecording_Impl();
#endif
}

void Phys_StopRecording(const char *path) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_StopRecording_Impl(path); return; }
#endif
	(void)path;
}

qboolean Phys_ValidateReplay(const char *path) {
#ifdef PHYS_HAS_IMPL
	return Phys_ValidateReplay_Impl(path);
#else
	(void)path;
	return qfalse;
#endif
}

void Phys_DebugDraw(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized && phys_debugDraw && phys_debugDraw->integer) {
		PhysDebug_Clear();
		Phys_DebugDraw_Impl();
		PhysSolvers_DebugDraw();
	}
#endif
}

qboolean Phys_ConvexSweep(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_ConvexSweep_Impl(shapeDef, from, to, rotation, result);
#endif
	(void)shapeDef; (void)from; (void)to; (void)rotation;
	if (result) Com_Memset(result, 0, sizeof(*result));
	return qfalse;
}

void Phys_SetBodyMaterial(physBodyHandle_t handle, int materialId) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) Phys_SetBodyMaterial_Impl(handle, materialId);
#else
	(void)handle; (void)materialId;
#endif
}

void Phys_SetBodyFriction(physBodyHandle_t handle, float friction) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyFriction_Impl(handle, friction); return; }
#endif
	(void)handle; (void)friction;
}

void Phys_SetBodyRestitution(physBodyHandle_t handle, float restitution) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) { Phys_SetBodyRestitution_Impl(handle, restitution); return; }
#endif
	(void)handle; (void)restitution;
}

int Phys_GetBodyMaterial(physBodyHandle_t handle) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_GetBodyMaterial_Impl(handle);
#endif
	(void)handle;
	return 0;
}

int Phys_GetBodyCount(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_GetBodyCount_Impl();
#endif
	return 0;
}

int Phys_GetConstraintCount(void) {
#ifdef PHYS_HAS_IMPL
	if (physInitialized) return Phys_GetConstraintCount_Impl();
#endif
	return 0;
}
