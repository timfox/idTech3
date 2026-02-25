/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

DMM material library, constraint solver, and prefab definitions.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_dmm_materials.h"
#include <math.h>

static float randf(void) {
	return (float)(rand() & 0x7FFF) / (float)0x7FFF;
}

void DmmMat_GetPreset(dmmMaterialType_t type, dmmMaterialDef_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	out->type = type;
	
	VectorSet(out->grainDirection, 0, 1, 0);

	switch (type) {
		case DMM_WOOD:
			Q_strncpyz(out->name, "Wood", sizeof(out->name));
			out->density = 600; out->youngsModulus = 12000; out->poissonsRatio = 0.35f;
			out->yieldStrength = 40; out->ultimateStrength = 80; out->fractureEnergy = 15000;
			out->hardness = 4; out->elasticity = 0.3f; out->dampingRatio = 0.05f;
			out->fractureMode = DMM_FRAC_SPLINTER; out->isBrittle = qfalse; out->isDuctile = qfalse;
			out->meltingPoint = 0; out->softeningPoint = 0; out->isFlammable = qtrue;
			out->ignitionTemp = 250; out->burnRate = 0.5f; out->splinterAngle = 15.0f;
			break;
		case DMM_GLASS:
			Q_strncpyz(out->name, "Glass", sizeof(out->name));
			out->density = 2500; out->youngsModulus = 70000; out->poissonsRatio = 0.22f;
			out->yieldStrength = 1; out->ultimateStrength = 5; out->fractureEnergy = 8;
			out->hardness = 6; out->elasticity = 0.9f; out->dampingRatio = 0.001f;
			out->fractureMode = DMM_FRAC_SHATTER; out->isBrittle = qtrue; out->isDuctile = qfalse;
			out->meltingPoint = 1500; out->softeningPoint = 700; out->thermalConductivity = 1;
			break;
		case DMM_METAL_THIN:
			Q_strncpyz(out->name, "Thin Metal", sizeof(out->name));
			out->density = 7800; out->youngsModulus = 200000; out->poissonsRatio = 0.3f;
			out->yieldStrength = 250; out->ultimateStrength = 400; out->fractureEnergy = 50000;
			out->hardness = 5; out->elasticity = 0.1f; out->dampingRatio = 0.02f;
			out->fractureMode = DMM_FRAC_TEAR; out->isBrittle = qfalse; out->isDuctile = qtrue;
			out->meltingPoint = 1500; out->softeningPoint = 800; out->thermalConductivity = 50;
			out->thermalExpansion = 12e-6f;
			break;
		case DMM_METAL_THICK:
			Q_strncpyz(out->name, "Thick Metal", sizeof(out->name));
			out->density = 7800; out->youngsModulus = 200000; out->poissonsRatio = 0.3f;
			out->yieldStrength = 400; out->ultimateStrength = 800; out->fractureEnergy = 200000;
			out->hardness = 6; out->elasticity = 0.05f; out->dampingRatio = 0.02f;
			out->fractureMode = DMM_FRAC_TEAR; out->isBrittle = qfalse; out->isDuctile = qtrue;
			out->meltingPoint = 1500; out->softeningPoint = 800; out->thermalConductivity = 50;
			break;
		case DMM_CONCRETE:
			Q_strncpyz(out->name, "Concrete", sizeof(out->name));
			out->density = 2400; out->youngsModulus = 30000; out->poissonsRatio = 0.2f;
			out->yieldStrength = 3; out->ultimateStrength = 10; out->fractureEnergy = 150;
			out->hardness = 7; out->elasticity = 0.8f; out->dampingRatio = 0.01f;
			out->fractureMode = DMM_FRAC_CRUMBLE; out->isBrittle = qtrue;
			break;
		case DMM_STONE:
			Q_strncpyz(out->name, "Stone", sizeof(out->name));
			out->density = 2700; out->youngsModulus = 50000; out->poissonsRatio = 0.25f;
			out->yieldStrength = 5; out->ultimateStrength = 15; out->fractureEnergy = 200;
			out->hardness = 8; out->elasticity = 0.85f; out->dampingRatio = 0.01f;
			out->fractureMode = DMM_FRAC_CRUMBLE; out->isBrittle = qtrue;
			break;
		case DMM_ICE:
			Q_strncpyz(out->name, "Ice", sizeof(out->name));
			out->density = 917; out->youngsModulus = 9000; out->poissonsRatio = 0.33f;
			out->yieldStrength = 1; out->ultimateStrength = 3; out->fractureEnergy = 30;
			out->hardness = 2; out->elasticity = 0.7f; out->dampingRatio = 0.01f;
			out->fractureMode = DMM_FRAC_SHATTER; out->isBrittle = qtrue;
			out->meltingPoint = 0; out->softeningPoint = -5;
			break;
		case DMM_PLASTIC:
			Q_strncpyz(out->name, "Plastic", sizeof(out->name));
			out->density = 1200; out->youngsModulus = 3000; out->poissonsRatio = 0.4f;
			out->yieldStrength = 30; out->ultimateStrength = 60; out->fractureEnergy = 5000;
			out->hardness = 3; out->elasticity = 0.4f; out->dampingRatio = 0.08f;
			out->fractureMode = DMM_FRAC_VORONOI; out->isDuctile = qtrue;
			out->isFlammable = qtrue; out->ignitionTemp = 300; out->burnRate = 0.3f;
			break;
		case DMM_CLOTH:
			Q_strncpyz(out->name, "Cloth", sizeof(out->name));
			out->density = 300; out->youngsModulus = 100; out->poissonsRatio = 0.3f;
			out->yieldStrength = 50; out->ultimateStrength = 200; out->fractureEnergy = 1000;
			out->hardness = 1; out->elasticity = 0.2f; out->dampingRatio = 0.15f;
			out->fractureMode = DMM_FRAC_TEAR; out->isDuctile = qtrue;
			out->isFlammable = qtrue; out->ignitionTemp = 200; out->burnRate = 1.0f;
			break;
		case DMM_RUBBER:
			Q_strncpyz(out->name, "Rubber", sizeof(out->name));
			out->density = 1100; out->youngsModulus = 50; out->poissonsRatio = 0.49f;
			out->yieldStrength = 200; out->ultimateStrength = 500; out->fractureEnergy = 50000;
			out->hardness = 2; out->elasticity = 0.01f; out->dampingRatio = 0.2f;
			out->fractureMode = DMM_FRAC_TEAR; out->isDuctile = qtrue;
			break;
		case DMM_FLESH:
			Q_strncpyz(out->name, "Flesh", sizeof(out->name));
			out->density = 1050; out->youngsModulus = 500; out->poissonsRatio = 0.45f;
			out->yieldStrength = 10; out->ultimateStrength = 30; out->fractureEnergy = 800;
			out->hardness = 1; out->elasticity = 0.1f; out->dampingRatio = 0.3f;
			out->fractureMode = DMM_FRAC_TEAR; out->isDuctile = qtrue;
			break;
		default:
			Q_strncpyz(out->name, "Custom", sizeof(out->name));
			out->density = 1000; out->youngsModulus = 10000; out->yieldStrength = 50;
			out->ultimateStrength = 100; out->fractureEnergy = 5000;
			out->fractureMode = DMM_FRAC_VORONOI;
			break;
	}
}

const char *DmmMat_GetName(dmmMaterialType_t type) {
	static const char *names[] = {
		"Wood", "Glass", "Thin Metal", "Thick Metal", "Concrete",
		"Stone", "Ice", "Plastic", "Cloth", "Rubber", "Flesh", "Custom"
	};
	return (type >= 0 && type <= DMM_CUSTOM) ? names[type] : "Unknown";
}

dmmStructure_t *DmmStruct_Create(const dmmMaterialDef_t *material, int numNodes) {
	dmmStructure_t *s = (dmmStructure_t *)Z_Malloc(sizeof(dmmStructure_t));
	Com_Memset(s, 0, sizeof(*s));
	if (material) Com_Memcpy(&s->material, material, sizeof(dmmMaterialDef_t));
	VectorSet(s->gravity, 0, 0, -800);
	s->solverIterations = 8;
	if (s->solverIterations < 1) s->solverIterations = 8;
	s->active = qtrue;
	(void)numNodes;
	return s;
}

void DmmStruct_Destroy(dmmStructure_t *s) {
	if (s) Z_Free(s);
}

void DmmStruct_AddNode(dmmStructure_t *s, const vec3_t pos, float mass, qboolean fixed) {
	if (!s || s->numNodes >= DMM_MAX_NODES) return;
	dmmNode_t *n = &s->nodes[s->numNodes++];
	VectorCopy(pos, n->position);
	VectorClear(n->velocity);
	VectorClear(n->force);
	n->mass = mass;
	n->invMass = fixed ? 0 : (mass > 0 ? 1.0f / mass : 0);
	n->fixed = fixed;
	n->damage = 0;
}

void DmmStruct_AddConstraint(dmmStructure_t *s, int a, int b, float stiffness, float breakForce) {
	if (!s || s->numConstraints >= DMM_MAX_CONSTRAINTS) return;
	if (a < 0 || a >= s->numNodes || b < 0 || b >= s->numNodes) return;
	dmmConstraint_t *c = &s->constraints[s->numConstraints++];
	c->nodeA = a;
	c->nodeB = b;
	c->restLength = Distance(s->nodes[a].position, s->nodes[b].position);
	c->stiffness = stiffness;
	c->damping = 0.01f;
	c->breakForce = breakForce;
	c->broken = qfalse;
}

void DmmStruct_BuildGrid(dmmStructure_t *s, const vec3_t origin, const vec3_t size,
                         int resX, int resY, int resZ) {
	int x, y, z, idx;
	float nodeMass;

	if (!s) return;
	nodeMass = s->material.density * (size[0] / resX) * (size[1] / resY) * (size[2] / resZ);

	for (z = 0; z <= resZ; z++) {
		for (y = 0; y <= resY; y++) {
			for (x = 0; x <= resX; x++) {
				vec3_t pos;
				pos[0] = origin[0] + ((float)x / resX) * size[0];
				pos[1] = origin[1] + ((float)y / resY) * size[1];
				pos[2] = origin[2] + ((float)z / resZ) * size[2];
				qboolean fixed = (z == 0) ? qtrue : qfalse;
				DmmStruct_AddNode(s, pos, nodeMass, fixed);
			}
		}
	}

	float stiffness = s->material.youngsModulus * 0.01f;
	float breakForce = s->material.ultimateStrength;
	int stride_x = 1;
	int stride_y = (resX + 1);
	int stride_z = (resX + 1) * (resY + 1);

	for (z = 0; z <= resZ; z++) {
		for (y = 0; y <= resY; y++) {
			for (x = 0; x <= resX; x++) {
				idx = x * stride_x + y * stride_y + z * stride_z;
				if (x < resX) DmmStruct_AddConstraint(s, idx, idx + stride_x, stiffness, breakForce);
				if (y < resY) DmmStruct_AddConstraint(s, idx, idx + stride_y, stiffness, breakForce);
				if (z < resZ) DmmStruct_AddConstraint(s, idx, idx + stride_z, stiffness, breakForce);
			}
		}
	}
}

void DmmStruct_ApplyForce(dmmStructure_t *s, int nodeIdx, const vec3_t force) {
	if (!s || nodeIdx < 0 || nodeIdx >= s->numNodes) return;
	VectorAdd(s->nodes[nodeIdx].force, force, s->nodes[nodeIdx].force);
}

void DmmStruct_ApplyImpact(dmmStructure_t *s, const vec3_t point, const vec3_t dir,
                           float energy, float radius) {
	int i;
	if (!s) return;
	for (i = 0; i < s->numNodes; i++) {
		float dist = Distance(s->nodes[i].position, point);
		if (dist < radius) {
			float atten = 1.0f - (dist / radius);
			atten *= atten;
			vec3_t f;
			VectorScale(dir, energy * atten, f);
			VectorAdd(s->nodes[i].force, f, s->nodes[i].force);
			s->nodes[i].damage += energy * atten / s->material.fractureEnergy;
		}
	}
}

void DmmStruct_Solve(dmmStructure_t *s, float dt) {
	int i, iter;
	if (!s || !s->active) return;

	for (i = 0; i < s->numNodes; i++) {
		if (s->nodes[i].fixed) continue;
		VectorAdd(s->nodes[i].force, s->gravity, s->nodes[i].force);
		VectorScale(s->nodes[i].force, s->nodes[i].invMass, s->nodes[i].force);
		VectorMA(s->nodes[i].velocity, dt, s->nodes[i].force, s->nodes[i].velocity);
		VectorScale(s->nodes[i].velocity, 1.0f - s->material.dampingRatio, s->nodes[i].velocity);
		VectorMA(s->nodes[i].position, dt, s->nodes[i].velocity, s->nodes[i].position);
		VectorClear(s->nodes[i].force);
	}

	for (iter = 0; iter < s->solverIterations; iter++) {
		for (i = 0; i < s->numConstraints; i++) {
			dmmConstraint_t *c = &s->constraints[i];
			if (c->broken) continue;

			dmmNode_t *a = &s->nodes[c->nodeA];
			dmmNode_t *b = &s->nodes[c->nodeB];
			vec3_t delta;
			VectorSubtract(b->position, a->position, delta);
			float dist = VectorLength(delta);
			if (dist < 0.0001f) continue;

			float diff = (dist - c->restLength) / dist;
			float force = diff * c->stiffness;

			if (fabsf(force) > c->breakForce) {
				c->broken = qtrue;
				a->damage += 0.1f;
				b->damage += 0.1f;
				continue;
			}

			float totalInvMass = a->invMass + b->invMass;
			if (totalInvMass < 0.0001f) continue;

			vec3_t correction;
			VectorScale(delta, diff * 0.5f, correction);

			if (!a->fixed) {
				float ratioA = a->invMass / totalInvMass;
				VectorMA(a->position, ratioA, correction, a->position);
			}
			if (!b->fixed) {
				float ratioB = b->invMass / totalInvMass;
				VectorMA(b->position, -ratioB, correction, b->position);
			}
		}
	}
}

int DmmStruct_GetBrokenConstraints(dmmStructure_t *s) {
	int i, count = 0;
	if (!s) return 0;
	for (i = 0; i < s->numConstraints; i++) {
		if (s->constraints[i].broken) count++;
	}
	return count;
}

void DmmStruct_GenerateFragments(dmmStructure_t *s, const vec3_t impactPoint,
                                 dmmFractureMode_t mode, dmmFracturePattern_t *pattern) {
	if (!s || !pattern) return;
	Com_Memset(pattern, 0, sizeof(*pattern));

	switch (mode) {
		case DMM_FRAC_SHATTER: {
			int numCells = 12 + (rand() % 8);
			Dmm_GenerateVoronoiPattern(impactPoint, 50.0f, numCells, pattern);
			break;
		}
		case DMM_FRAC_SPLINTER: {
			int i, numSplinters = 6 + (rand() % 4);
			if (numSplinters > DMM_MAX_FRACTURE_POINTS) numSplinters = DMM_MAX_FRACTURE_POINTS;
			for (i = 0; i < numSplinters; i++) {
				float angle = ((float)i / numSplinters) * 2.0f * 3.14159f + (randf() - 0.5f) * 0.5f;
				float len = 10.0f + randf() * 40.0f;
				pattern->points[i][0] = cosf(angle) * len;
				pattern->points[i][1] = s->material.grainDirection[1] * (randf() - 0.5f) * len * 2.0f;
				pattern->points[i][2] = sinf(angle) * len;
			}
			pattern->numPoints = numSplinters;
			pattern->randomness = 0.3f;
			pattern->minFragmentSize = 3.0f;
			break;
		}
		case DMM_FRAC_CRUMBLE: {
			int numChunks = 8 + (rand() % 12);
			Dmm_GenerateVoronoiPattern(impactPoint, 40.0f, numChunks, pattern);
			int ci;
			for (ci = 0; ci < pattern->numPoints; ci++) {
				pattern->points[ci][0] += (randf() - 0.5f) * 10.0f;
				pattern->points[ci][1] += (randf() - 0.5f) * 10.0f;
				pattern->points[ci][2] += (randf() - 0.5f) * 10.0f;
			}
			pattern->minFragmentSize = 2.0f;
			break;
		}
		case DMM_FRAC_TEAR: {
			int numTears = 3 + (rand() % 3);
			int ti;
			if (numTears > DMM_MAX_FRACTURE_POINTS) numTears = DMM_MAX_FRACTURE_POINTS;
			for (ti = 0; ti < numTears; ti++) {
				pattern->points[ti][0] = (randf() - 0.5f) * 30.0f;
				pattern->points[ti][1] = (randf() - 0.5f) * 30.0f;
				pattern->points[ti][2] = (randf() - 0.5f) * 10.0f;
			}
			pattern->numPoints = numTears;
			pattern->randomness = 0.7f;
			pattern->minFragmentSize = 8.0f;
			break;
		}
		default:
			Dmm_GenerateVoronoiPattern(impactPoint, 30.0f, 8, pattern);
			break;
	}
}

/* ========== prefab definitions ========== */

void DmmPrefab_WoodenDoor(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Wooden Door", sizeof(out->name));
	out->material = DMM_WOOD; VectorSet(out->dimensions, 40, 80, 4);
	out->gridResolution = 6; out->health = 150; out->fractureMode = DMM_FRAC_SPLINTER;
	out->minFragments = 4; out->maxFragments = 10; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/wood_break", sizeof(out->breakSound));
	Q_strncpyz(out->stressSound, "sound/world/wood_creak", sizeof(out->stressSound));
}

void DmmPrefab_GlassPane(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Glass Pane", sizeof(out->name));
	out->material = DMM_GLASS; VectorSet(out->dimensions, 48, 48, 1);
	out->gridResolution = 8; out->health = 20; out->fractureMode = DMM_FRAC_SHATTER;
	out->minFragments = 10; out->maxFragments = 25; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/glass_shatter", sizeof(out->breakSound));
}

void DmmPrefab_MetalBarrel(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Metal Barrel", sizeof(out->name));
	out->material = DMM_METAL_THIN; VectorSet(out->dimensions, 16, 24, 16);
	out->gridResolution = 4; out->health = 300; out->fractureMode = DMM_FRAC_TEAR;
	out->minFragments = 3; out->maxFragments = 6; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/metal_crash", sizeof(out->breakSound));
	Q_strncpyz(out->stressSound, "sound/world/metal_stress", sizeof(out->stressSound));
}

void DmmPrefab_ConcreteWall(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Concrete Wall Section", sizeof(out->name));
	out->material = DMM_CONCRETE; VectorSet(out->dimensions, 64, 64, 8);
	out->gridResolution = 6; out->health = 500; out->fractureMode = DMM_FRAC_CRUMBLE;
	out->minFragments = 6; out->maxFragments = 15; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/concrete_break", sizeof(out->breakSound));
}

void DmmPrefab_IceBlock(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Ice Block", sizeof(out->name));
	out->material = DMM_ICE; VectorSet(out->dimensions, 24, 24, 24);
	out->gridResolution = 4; out->health = 40; out->fractureMode = DMM_FRAC_SHATTER;
	out->minFragments = 8; out->maxFragments = 20; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/ice_crack", sizeof(out->breakSound));
}

void DmmPrefab_WoodenCrate(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Wooden Crate", sizeof(out->name));
	out->material = DMM_WOOD; VectorSet(out->dimensions, 24, 24, 24);
	out->gridResolution = 4; out->health = 80; out->fractureMode = DMM_FRAC_SPLINTER;
	out->minFragments = 5; out->maxFragments = 12; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/wood_break", sizeof(out->breakSound));
}

void DmmPrefab_MetalGrate(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Metal Grate", sizeof(out->name));
	out->material = DMM_METAL_THIN; VectorSet(out->dimensions, 32, 32, 2);
	out->gridResolution = 6; out->health = 200; out->fractureMode = DMM_FRAC_TEAR;
	out->minFragments = 2; out->maxFragments = 5; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/metal_clang", sizeof(out->breakSound));
}

void DmmPrefab_BrickWall(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Brick Wall", sizeof(out->name));
	out->material = DMM_STONE; VectorSet(out->dimensions, 64, 48, 8);
	out->gridResolution = 8; out->health = 400; out->fractureMode = DMM_FRAC_CRUMBLE;
	out->minFragments = 10; out->maxFragments = 30; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/brick_collapse", sizeof(out->breakSound));
}

void DmmPrefab_Railing(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Metal Railing", sizeof(out->name));
	out->material = DMM_METAL_THIN; VectorSet(out->dimensions, 64, 36, 4);
	out->gridResolution = 8; out->health = 180; out->fractureMode = DMM_FRAC_TEAR;
	out->minFragments = 2; out->maxFragments = 4; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/metal_bend", sizeof(out->breakSound));
	Q_strncpyz(out->stressSound, "sound/world/metal_stress", sizeof(out->stressSound));
}

void DmmPrefab_TreeTrunk(dmmPrefab_t *out) {
	Com_Memset(out, 0, sizeof(*out));
	Q_strncpyz(out->name, "Tree Trunk", sizeof(out->name));
	out->material = DMM_WOOD; VectorSet(out->dimensions, 12, 80, 12);
	out->gridResolution = 4; out->health = 350; out->fractureMode = DMM_FRAC_SPLINTER;
	out->minFragments = 3; out->maxFragments = 6; out->hasPhysics = qtrue;
	Q_strncpyz(out->breakSound, "sound/world/tree_fall", sizeof(out->breakSound));
	Q_strncpyz(out->stressSound, "sound/world/wood_creak", sizeof(out->stressSound));
}
