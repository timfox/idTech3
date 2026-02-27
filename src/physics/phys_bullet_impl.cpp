/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Bullet Physics C++ backend -- full implementation.
Every function in phys_bullet.h has a corresponding _Impl here.
===========================================================================
*/

#ifdef USE_BULLET_PHYSICS_IMPL

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>

extern "C" {
#include "phys_bullet.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
}

/* ---------- internal types ---------- */

struct PhysBody {
	btRigidBody      *rigidBody;
	btCollisionShape *shape;
	btMotionState    *motionState;
	qboolean          active;
};

struct PhysConstraint {
	btTypedConstraint *constraint;
	physBodyHandle_t   bodyA, bodyB;
	qboolean           active;
};

struct RagdollBone {
	btRigidBody      *body;
	btCollisionShape *shape;
	btMotionState    *motionState;
};

#define RAGDOLL_MAX_BONES  32
#define RAGDOLL_MAX_JOINTS 31

struct PhysRagdoll {
	RagdollBone         bones[RAGDOLL_MAX_BONES];
	btTypedConstraint  *joints[RAGDOLL_MAX_JOINTS];
	int                 numBones;
	int                 numJoints;
	float               muscleStiffness;
	float               muscleDamping;
	float               balanceForce;
	qboolean            balanceEnabled;
	btVector3           balanceTarget;
	float               animBlend;
	qboolean            active;
};

struct DmmElement {
	float strain;
	float stress;
	float plasticity;
};

struct PhysDmmObject {
	btSoftBody         *softBody;
	dmmMaterialType_t   material;
	float               yieldStrength;
	float               fractureStrength;
	float               deformability;
	float               integrity;
	qboolean            fractured;
	int                 numFragments;
	DmmElement         *elements;
	int                 numElements;
	qboolean            active;
};

static struct {
	btDefaultCollisionConfiguration    *collisionConfig;
	btCollisionDispatcher              *dispatcher;
	btBroadphaseInterface              *broadphase;
	btSequentialImpulseConstraintSolver *solver;
	btSoftRigidDynamicsWorld           *world;

	PhysBody        bodies[PHYS_MAX_RIGID_BODIES];
	int             bodyCount;

	PhysConstraint  constraints[PHYS_MAX_CONSTRAINTS];
	int             constraintCount;

	PhysRagdoll     ragdolls[PHYS_MAX_RAGDOLLS];
	int             ragdollCount;

	PhysDmmObject   dmmObjects[PHYS_MAX_DMM_OBJECTS];
	int             dmmCount;

	btSoftBodyWorldInfo softBodyWorldInfo;
	qboolean        initialized;
} bs;

/* ---------- helpers ---------- */

static btCollisionShape *createShape(const physBodyDef_t *def) {
	switch (def->shape) {
		case PHYS_SHAPE_BOX:
			return new btBoxShape(btVector3(def->halfExtents[0], def->halfExtents[1], def->halfExtents[2]));
		case PHYS_SHAPE_SPHERE:
			return new btSphereShape(def->radius);
		case PHYS_SHAPE_CAPSULE:
			return new btCapsuleShape(def->radius, def->height);
		case PHYS_SHAPE_CYLINDER:
			return new btCylinderShape(btVector3(def->halfExtents[0], def->halfExtents[1], def->halfExtents[2]));
		default:
			return new btBoxShape(btVector3(1, 1, 1));
	}
}

static void getDmmPreset(dmmMaterialType_t t, float &stiff, float &yield, float &frac) {
	switch (t) {
		case DMM_WOOD:       stiff=12000; yield=40;  frac=80;  break;
		case DMM_GLASS:      stiff=70000; yield=1;   frac=5;   break;
		case DMM_METAL_THIN: stiff=200000;yield=250; frac=400; break;
		case DMM_METAL_THICK:stiff=200000;yield=400; frac=800; break;
		case DMM_CONCRETE:   stiff=30000; yield=3;   frac=10;  break;
		case DMM_STONE:      stiff=50000; yield=5;   frac=15;  break;
		case DMM_ICE:        stiff=9000;  yield=1;   frac=3;   break;
		case DMM_PLASTIC:    stiff=3000;  yield=30;  frac=60;  break;
		case DMM_CLOTH:      stiff=100;   yield=50;  frac=200; break;
		case DMM_RUBBER:     stiff=50;    yield=200; frac=500; break;
		case DMM_FLESH:      stiff=500;   yield=10;  frac=30;  break;
		default:             stiff=10000; yield=50;  frac=100; break;
	}
}

#define VALID_BODY(h) ((h) >= 0 && (h) < bs.bodyCount && bs.bodies[(h)].active)
#define VALID_CON(h)  ((h) >= 0 && (h) < bs.constraintCount && bs.constraints[(h)].active)
#define VALID_RAG(h)  ((h) >= 0 && (h) < bs.ragdollCount && bs.ragdolls[(h)].active)
#define VALID_DMM(h)  ((h) >= 0 && (h) < bs.dmmCount && bs.dmmObjects[(h)].active)

/* ========== core ========== */

extern "C" qboolean Phys_Init_Impl(void) {
	if (bs.initialized) return qtrue;
	bs.collisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
	bs.dispatcher      = new btCollisionDispatcher(bs.collisionConfig);
	bs.broadphase      = new btDbvtBroadphase();
	bs.solver          = new btSequentialImpulseConstraintSolver();
	bs.world           = new btSoftRigidDynamicsWorld(bs.dispatcher, bs.broadphase, bs.solver, bs.collisionConfig);
	bs.world->setGravity(btVector3(0, 0, -800));

	bs.softBodyWorldInfo.m_broadphase  = bs.broadphase;
	bs.softBodyWorldInfo.m_dispatcher  = bs.dispatcher;
	bs.softBodyWorldInfo.m_gravity     = bs.world->getGravity();
	bs.softBodyWorldInfo.air_density   = 1.2f;
	bs.softBodyWorldInfo.water_density = 0;
	bs.softBodyWorldInfo.water_offset  = 0;
	bs.softBodyWorldInfo.water_normal  = btVector3(0,0,0);

	bs.bodyCount = bs.constraintCount = bs.ragdollCount = bs.dmmCount = 0;
	bs.initialized = qtrue;
	return qtrue;
}

extern "C" void Phys_Shutdown_Impl(void) {
	if (!bs.initialized) return;
	int i;
	for (i = bs.world->getNumCollisionObjects()-1; i >= 0; i--) {
		btCollisionObject *obj = bs.world->getCollisionObjectArray()[i];
		btRigidBody *rb = btRigidBody::upcast(obj);
		if (rb && rb->getMotionState()) delete rb->getMotionState();
		bs.world->removeCollisionObject(obj);
		delete obj;
	}
	for (i = 0; i < bs.bodyCount; i++)
		if (bs.bodies[i].shape) { delete bs.bodies[i].shape; bs.bodies[i].shape = nullptr; }
	for (i = 0; i < bs.constraintCount; i++)
		if (bs.constraints[i].constraint) { delete bs.constraints[i].constraint; bs.constraints[i].constraint = nullptr; }
	for (i = 0; i < bs.dmmCount; i++)
		if (bs.dmmObjects[i].elements) { delete[] bs.dmmObjects[i].elements; bs.dmmObjects[i].elements = nullptr; }
	delete bs.world; delete bs.solver; delete bs.broadphase; delete bs.dispatcher; delete bs.collisionConfig;
	memset(&bs, 0, sizeof(bs));
}

extern "C" void Phys_StepSimulation_Impl(float dt) {
	if (!bs.initialized) return;
	bs.world->stepSimulation(dt, 4, 1.0f/60.0f);

	for (int r = 0; r < bs.ragdollCount; r++) {
		PhysRagdoll *rag = &bs.ragdolls[r];
		if (!rag->active || !rag->balanceEnabled) continue;
		for (int b = 0; b < rag->numBones; b++) {
			if (!rag->bones[b].body) continue;
			btTransform t; rag->bones[b].body->getMotionState()->getWorldTransform(t);
			btVector3 toTarget = rag->balanceTarget - t.getOrigin();
			toTarget.setY(0);
			if (toTarget.length() > 0.1f)
				rag->bones[b].body->applyCentralForce(toTarget.normalized() * rag->balanceForce * rag->muscleStiffness);
		}
	}

	for (int d = 0; d < bs.dmmCount; d++) {
		PhysDmmObject *dmm = &bs.dmmObjects[d];
		if (!dmm->active || dmm->fractured) continue;
		float maxStrain = 0;
		for (int e = 0; e < dmm->numElements; e++) {
			if (dmm->elements[e].strain > maxStrain) maxStrain = dmm->elements[e].strain;
			if (dmm->elements[e].strain > dmm->yieldStrength)
				dmm->elements[e].plasticity += (dmm->elements[e].strain - dmm->yieldStrength) * 0.01f;
		}
		dmm->integrity = 1.0f - (maxStrain / dmm->fractureStrength);
		if (dmm->integrity <= 0.0f) { dmm->fractured = qtrue; dmm->integrity = 0.0f; }
	}
}

extern "C" void Phys_SetGravity_Impl(const vec3_t g) {
	if (bs.initialized && bs.world) bs.world->setGravity(btVector3(g[0], g[1], g[2]));
}

extern "C" void Phys_ClearWorld_Impl(void) {
	if (!bs.initialized) return;
	Phys_Shutdown_Impl();
	Phys_Init_Impl();
}

/* ========== rigid bodies ========== */

extern "C" physBodyHandle_t Phys_CreateBody_Impl(const physBodyDef_t *def) {
	if (!bs.initialized || bs.bodyCount >= PHYS_MAX_RIGID_BODIES) return -1;
	int idx = bs.bodyCount++;
	PhysBody *pb = &bs.bodies[idx];
	pb->shape = createShape(def);
	btVector3 inertia(0,0,0);
	float mass = (def->type == PHYS_BODY_STATIC) ? 0.0f : def->mass;
	if (mass > 0) pb->shape->calculateLocalInertia(mass, inertia);
	btTransform startXform; startXform.setIdentity();
	startXform.setOrigin(btVector3(def->position[0], def->position[1], def->position[2]));
	pb->motionState = new btDefaultMotionState(startXform);
	btRigidBody::btRigidBodyConstructionInfo ci(mass, pb->motionState, pb->shape, inertia);
	ci.m_friction = def->friction; ci.m_restitution = def->restitution;
	ci.m_linearDamping = def->linearDamping; ci.m_angularDamping = def->angularDamping;
	pb->rigidBody = new btRigidBody(ci);
	if (def->type == PHYS_BODY_KINEMATIC) {
		pb->rigidBody->setCollisionFlags(pb->rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		pb->rigidBody->setActivationState(DISABLE_DEACTIVATION);
	}
	int group = def->collisionGroup ? def->collisionGroup : 1;
	int mask  = def->collisionMask  ? def->collisionMask  : -1;
	bs.world->addRigidBody(pb->rigidBody, group, mask);
	pb->active = qtrue;
	return idx;
}

extern "C" void Phys_DestroyBody_Impl(physBodyHandle_t h) {
	if (!VALID_BODY(h)) return;
	PhysBody *pb = &bs.bodies[h];
	bs.world->removeRigidBody(pb->rigidBody);
	if (pb->rigidBody->getMotionState()) delete pb->rigidBody->getMotionState();
	delete pb->rigidBody; delete pb->shape;
	pb->rigidBody = nullptr; pb->shape = nullptr; pb->motionState = nullptr;
	pb->active = qfalse;
}

extern "C" void Phys_GetBodyTransform_Impl(physBodyHandle_t h, physTransform_t *out) {
	if (!VALID_BODY(h) || !out) return;
	PhysBody *pb = &bs.bodies[h];
	btTransform t; pb->rigidBody->getMotionState()->getWorldTransform(t);
	btVector3 p = t.getOrigin(); btQuaternion q = t.getRotation();
	btVector3 lv = pb->rigidBody->getLinearVelocity(), av = pb->rigidBody->getAngularVelocity();
	out->position[0] = p.x(); out->position[1] = p.y(); out->position[2] = p.z();
	float yaw, pitch, roll; btMatrix3x3(q).getEulerYPR(yaw, pitch, roll);
	out->rotation[0] = pitch*57.2957795f; out->rotation[1] = yaw*57.2957795f; out->rotation[2] = roll*57.2957795f;
	out->linearVelocity[0] = lv.x(); out->linearVelocity[1] = lv.y(); out->linearVelocity[2] = lv.z();
	out->angularVelocity[0] = av.x(); out->angularVelocity[1] = av.y(); out->angularVelocity[2] = av.z();
}

extern "C" void Phys_SetBodyTransform_Impl(physBodyHandle_t h, const vec3_t pos, const vec3_t rot) {
	if (!VALID_BODY(h)) return;
	btTransform t; t.setIdentity();
	t.setOrigin(btVector3(pos[0], pos[1], pos[2]));
	btQuaternion q; q.setEulerZYX(rot[2]*0.0174533f, rot[1]*0.0174533f, rot[0]*0.0174533f);
	t.setRotation(q);
	bs.bodies[h].rigidBody->setWorldTransform(t);
	bs.bodies[h].rigidBody->getMotionState()->setWorldTransform(t);
}

extern "C" void Phys_ApplyForce_Impl(physBodyHandle_t h, const vec3_t f, const vec3_t p) {
	if (!VALID_BODY(h)) return;
	bs.bodies[h].rigidBody->activate(true);
	bs.bodies[h].rigidBody->applyForce(btVector3(f[0],f[1],f[2]), btVector3(p[0],p[1],p[2]));
}

extern "C" void Phys_ApplyImpulse_Impl(physBodyHandle_t h, const vec3_t imp, const vec3_t p) {
	if (!VALID_BODY(h)) return;
	bs.bodies[h].rigidBody->activate(true);
	bs.bodies[h].rigidBody->applyImpulse(btVector3(imp[0],imp[1],imp[2]), btVector3(p[0],p[1],p[2]));
}

extern "C" void Phys_ApplyTorque_Impl(physBodyHandle_t h, const vec3_t torque) {
	if (!VALID_BODY(h)) return;
	bs.bodies[h].rigidBody->activate(true);
	bs.bodies[h].rigidBody->applyTorque(btVector3(torque[0], torque[1], torque[2]));
}

extern "C" void Phys_SetBodyVelocity_Impl(physBodyHandle_t h, const vec3_t lin, const vec3_t ang) {
	if (!VALID_BODY(h)) return;
	bs.bodies[h].rigidBody->activate(true);
	bs.bodies[h].rigidBody->setLinearVelocity(btVector3(lin[0], lin[1], lin[2]));
	bs.bodies[h].rigidBody->setAngularVelocity(btVector3(ang[0], ang[1], ang[2]));
}

extern "C" void Phys_SetBodyActive_Impl(physBodyHandle_t h, qboolean active) {
	if (!VALID_BODY(h)) return;
	if (active) bs.bodies[h].rigidBody->activate(true);
	else bs.bodies[h].rigidBody->setActivationState(DISABLE_SIMULATION);
}

/* ========== constraints ========== */

extern "C" physConstraintHandle_t Phys_CreateConstraint_Impl(const physConstraintDef_t *def) {
	if (!bs.initialized || bs.constraintCount >= PHYS_MAX_CONSTRAINTS) return -1;
	if (!VALID_BODY(def->bodyA) || !VALID_BODY(def->bodyB)) return -1;
	btRigidBody *a = bs.bodies[def->bodyA].rigidBody, *b = bs.bodies[def->bodyB].rigidBody;
	btVector3 pivA(def->pivotA[0],def->pivotA[1],def->pivotA[2]);
	btVector3 pivB(def->pivotB[0],def->pivotB[1],def->pivotB[2]);
	btTypedConstraint *c = nullptr;
	switch (def->type) {
		case PHYS_CONSTRAINT_POINT:
			c = new btPoint2PointConstraint(*a, *b, pivA, pivB); break;
		case PHYS_CONSTRAINT_HINGE: {
			btVector3 axA(def->axisA[0],def->axisA[1],def->axisA[2]);
			btVector3 axB(def->axisB[0],def->axisB[1],def->axisB[2]);
			btHingeConstraint *hc = new btHingeConstraint(*a, *b, pivA, pivB, axA, axB);
			hc->setLimit(def->lowerLimit, def->upperLimit, def->softness, def->biasFactor, def->relaxationFactor);
			c = hc; break;
		}
		case PHYS_CONSTRAINT_CONE_TWIST: {
			btTransform fA, fB; fA.setIdentity(); fB.setIdentity();
			fA.setOrigin(pivA); fB.setOrigin(pivB);
			btConeTwistConstraint *ct = new btConeTwistConstraint(*a, *b, fA, fB);
			ct->setLimit(def->upperLimit, def->upperLimit, def->lowerLimit, def->softness, def->biasFactor, def->relaxationFactor);
			c = ct; break;
		}
		case PHYS_CONSTRAINT_FIXED: {
			btTransform fA, fB; fA.setIdentity(); fB.setIdentity();
			fA.setOrigin(pivA); fB.setOrigin(pivB);
			c = new btFixedConstraint(*a, *b, fA, fB); break;
		}
		default: {
			btTransform fA, fB; fA.setIdentity(); fB.setIdentity();
			fA.setOrigin(pivA); fB.setOrigin(pivB);
			c = new btGeneric6DofConstraint(*a, *b, fA, fB, true); break;
		}
	}
	if (!c) return -1;
	int idx = bs.constraintCount++;
	bs.constraints[idx].constraint = c;
	bs.constraints[idx].bodyA = def->bodyA;
	bs.constraints[idx].bodyB = def->bodyB;
	bs.constraints[idx].active = qtrue;
	bs.world->addConstraint(c, def->disableCollision ? true : false);
	return idx;
}

extern "C" void Phys_DestroyConstraint_Impl(physConstraintHandle_t h) {
	if (!VALID_CON(h)) return;
	bs.world->removeConstraint(bs.constraints[h].constraint);
	delete bs.constraints[h].constraint;
	bs.constraints[h].constraint = nullptr;
	bs.constraints[h].active = qfalse;
}

extern "C" void Phys_SetConstraintLimits_Impl(physConstraintHandle_t h, float lo, float hi) {
	if (!VALID_CON(h)) return;
	btHingeConstraint *hc = dynamic_cast<btHingeConstraint*>(bs.constraints[h].constraint);
	if (hc) hc->setLimit(lo, hi);
}

/* ========== ragdoll ========== */

extern "C" physRagdollHandle_t Phys_CreateRagdoll_Impl(const physRagdollDef_t *def) {
	if (!bs.initialized || bs.ragdollCount >= PHYS_MAX_RAGDOLLS) return -1;
	int idx = bs.ragdollCount++;
	PhysRagdoll *rag = &bs.ragdolls[idx];
	memset(rag, 0, sizeof(*rag));
	rag->muscleStiffness = def->jointStiffness > 0 ? def->jointStiffness : 0.8f;
	rag->muscleDamping = def->jointDamping > 0 ? def->jointDamping : 0.4f;
	rag->balanceForce = def->balanceForce > 0 ? def->balanceForce : 100.0f;
	rag->balanceEnabled = qfalse;
	rag->animBlend = 0.0f;
	rag->active = qtrue;

	float scale = def->scale > 0 ? def->scale : 1.0f;
	float limbMass = def->limbMass > 0 ? def->limbMass : 5.0f;
	struct { float radius; float height; float yOff; } boneSpec[] = {
		{8*scale,  20*scale,  0},
		{6*scale,  16*scale, 28*scale},
		{5*scale,  10*scale, 48*scale},
		{4*scale,  14*scale, 20*scale},
		{3*scale,  12*scale, 10*scale},
		{4*scale,  14*scale, 20*scale},
		{3*scale,  12*scale, 10*scale},
		{4*scale,  16*scale, -10*scale},
		{3*scale,  14*scale, -28*scale},
		{4*scale,  16*scale, -10*scale},
		{3*scale,  14*scale, -28*scale},
	};
	int numBones = 11;
	rag->numBones = numBones;
	for (int b = 0; b < numBones; b++) {
		rag->bones[b].shape = new btCapsuleShape(boneSpec[b].radius, boneSpec[b].height);
		btVector3 inertia(0,0,0);
		rag->bones[b].shape->calculateLocalInertia(limbMass, inertia);
		btTransform t; t.setIdentity();
		t.setOrigin(btVector3(def->rootPosition[0], def->rootPosition[1] + boneSpec[b].yOff, def->rootPosition[2]));
		rag->bones[b].motionState = new btDefaultMotionState(t);
		btRigidBody::btRigidBodyConstructionInfo ci(limbMass, rag->bones[b].motionState, rag->bones[b].shape, inertia);
		ci.m_linearDamping = 0.15f; ci.m_angularDamping = 0.35f;
		rag->bones[b].body = new btRigidBody(ci);
		rag->bones[b].body->setActivationState(DISABLE_DEACTIVATION);
		if (def->selfCollision)
			bs.world->addRigidBody(rag->bones[b].body);
		else
			bs.world->addRigidBody(rag->bones[b].body, 1<<(idx+1), ~(1<<(idx+1)));
	}

	int pairs[][2] = {{0,1},{1,2},{0,3},{3,4},{0,5},{5,6},{0,7},{7,8},{0,9},{9,10}};
	rag->numJoints = 10;
	for (int j = 0; j < 10; j++) {
		btRigidBody *a = rag->bones[pairs[j][0]].body, *b = rag->bones[pairs[j][1]].body;
		btTransform fA, fB; fA.setIdentity(); fB.setIdentity();
		btConeTwistConstraint *ct = new btConeTwistConstraint(*a, *b, fA, fB);
		ct->setLimit(0.5f, 0.5f, 0.3f, rag->muscleStiffness, 0.3f, rag->muscleDamping);
		ct->setDamping(rag->muscleDamping);
		bs.world->addConstraint(ct, true);
		rag->joints[j] = ct;
	}
	return idx;
}

extern "C" void Phys_DestroyRagdoll_Impl(physRagdollHandle_t h) {
	if (!VALID_RAG(h)) return;
	PhysRagdoll *rag = &bs.ragdolls[h];
	for (int j = 0; j < rag->numJoints; j++)
		if (rag->joints[j]) { bs.world->removeConstraint(rag->joints[j]); delete rag->joints[j]; }
	for (int b = 0; b < rag->numBones; b++) {
		if (rag->bones[b].body) {
			bs.world->removeRigidBody(rag->bones[b].body);
			delete rag->bones[b].body->getMotionState(); delete rag->bones[b].body; delete rag->bones[b].shape;
		}
	}
	rag->active = qfalse;
}

extern "C" void Phys_RagdollApplyImpact_Impl(physRagdollHandle_t h, const vec3_t pt, const vec3_t imp, float radius) {
	if (!VALID_RAG(h)) return;
	btVector3 impactPt(pt[0],pt[1],pt[2]), impulse(imp[0],imp[1],imp[2]);
	PhysRagdoll *rag = &bs.ragdolls[h];
	for (int b = 0; b < rag->numBones; b++) {
		if (!rag->bones[b].body) continue;
		btTransform t; rag->bones[b].body->getMotionState()->getWorldTransform(t);
		float dist = (t.getOrigin() - impactPt).length();
		if (dist < radius) {
			float atten = 1.0f - (dist / radius);
			rag->bones[b].body->activate(true);
			rag->bones[b].body->applyImpulse(impulse * atten, btVector3(0,0,0));
		}
	}
}

extern "C" void Phys_RagdollSetBalance_Impl(physRagdollHandle_t h, qboolean en, const vec3_t tgt) {
	if (!VALID_RAG(h)) return;
	bs.ragdolls[h].balanceEnabled = en;
	if (tgt) bs.ragdolls[h].balanceTarget = btVector3(tgt[0], tgt[1], tgt[2]);
}

extern "C" void Phys_RagdollReach_Impl(physRagdollHandle_t h, int limb, const vec3_t tgt, float str) {
	if (!VALID_RAG(h) || limb < 0 || limb >= bs.ragdolls[h].numBones) return;
	btRigidBody *body = bs.ragdolls[h].bones[limb].body;
	if (!body) return;
	btTransform t; body->getMotionState()->getWorldTransform(t);
	btVector3 dir = btVector3(tgt[0],tgt[1],tgt[2]) - t.getOrigin();
	if (dir.length() > 0.1f) { body->activate(true); body->applyCentralForce(dir.normalized() * str); }
}

extern "C" void Phys_RagdollGetBoneTransform_Impl(physRagdollHandle_t h, int bone, physTransform_t *out) {
	if (!VALID_RAG(h) || bone < 0 || bone >= bs.ragdolls[h].numBones || !out) return;
	btRigidBody *body = bs.ragdolls[h].bones[bone].body;
	if (!body) { memset(out, 0, sizeof(*out)); return; }
	btTransform t; body->getMotionState()->getWorldTransform(t);
	btVector3 p = t.getOrigin(); btQuaternion q = t.getRotation();
	out->position[0]=p.x(); out->position[1]=p.y(); out->position[2]=p.z();
	float yaw,pitch,roll; btMatrix3x3(q).getEulerYPR(yaw,pitch,roll);
	out->rotation[0]=pitch*57.2957795f; out->rotation[1]=yaw*57.2957795f; out->rotation[2]=roll*57.2957795f;
	btVector3 lv=body->getLinearVelocity(), av=body->getAngularVelocity();
	out->linearVelocity[0]=lv.x(); out->linearVelocity[1]=lv.y(); out->linearVelocity[2]=lv.z();
	out->angularVelocity[0]=av.x(); out->angularVelocity[1]=av.y(); out->angularVelocity[2]=av.z();
}

extern "C" void Phys_RagdollSetMuscleStiffness_Impl(physRagdollHandle_t h, float s) {
	if (!VALID_RAG(h)) return;
	bs.ragdolls[h].muscleStiffness = s;
	for (int j = 0; j < bs.ragdolls[h].numJoints; j++) {
		btConeTwistConstraint *ct = dynamic_cast<btConeTwistConstraint*>(bs.ragdolls[h].joints[j]);
		if (ct) ct->setLimit(0.5f, 0.5f, 0.3f, s, 0.3f, bs.ragdolls[h].muscleDamping);
	}
}

extern "C" void Phys_RagdollBlendToAnimation_Impl(physRagdollHandle_t h, float blend) {
	if (!VALID_RAG(h)) return;
	bs.ragdolls[h].animBlend = blend < 0 ? 0 : (blend > 1 ? 1 : blend);
}

/* ========== DMM ========== */

extern "C" dmmObjectHandle_t Dmm_CreateObject_Impl(const dmmObjectDef_t *def) {
	if (!bs.initialized || bs.dmmCount >= PHYS_MAX_DMM_OBJECTS) return -1;
	int idx = bs.dmmCount++;
	PhysDmmObject *dmm = &bs.dmmObjects[idx];
	memset(dmm, 0, sizeof(*dmm));
	dmm->material = def->material;
	dmm->deformability = def->deformability > 0 ? def->deformability : 1.0f;
	float stiff, yield, frac;
	if (def->stiffness > 0) { stiff=def->stiffness; yield=def->yieldStrength; frac=def->fractureStrength; }
	else getDmmPreset(def->material, stiff, yield, frac);
	dmm->yieldStrength = yield; dmm->fractureStrength = frac;
	dmm->integrity = 1.0f; dmm->fractured = qfalse;
	int res = def->gridResolution > 0 ? def->gridResolution : 8;
	dmm->numElements = res*res*res;
	dmm->elements = new DmmElement[dmm->numElements];
	memset(dmm->elements, 0, sizeof(DmmElement)*dmm->numElements);
	dmm->softBody = nullptr;
	dmm->active = qtrue;
	return idx;
}

extern "C" void Dmm_DestroyObject_Impl(dmmObjectHandle_t h) {
	if (!VALID_DMM(h)) return;
	PhysDmmObject *dmm = &bs.dmmObjects[h];
	if (dmm->softBody) { bs.world->removeSoftBody(dmm->softBody); delete dmm->softBody; }
	if (dmm->elements) delete[] dmm->elements;
	memset(dmm, 0, sizeof(*dmm));
}

extern "C" void Dmm_ApplyForce_Impl(dmmObjectHandle_t h, const vec3_t f, const vec3_t p) {
	if (!VALID_DMM(h)) return;
	if (bs.dmmObjects[h].softBody)
		bs.dmmObjects[h].softBody->addForce(btVector3(f[0],f[1],f[2]), 0);
	(void)p;
}

extern "C" void Dmm_ApplyImpact_Impl(dmmObjectHandle_t h, const vec3_t pt, const vec3_t dir, float energy) {
	if (!VALID_DMM(h) || bs.dmmObjects[h].fractured) return;
	PhysDmmObject *dmm = &bs.dmmObjects[h];
	float stress = energy / dmm->deformability;
	for (int i = 0; i < dmm->numElements; i++) {
		dmm->elements[i].strain += stress * 0.5f;
		dmm->elements[i].stress = dmm->elements[i].strain * (1.0f - dmm->elements[i].plasticity);
	}
	if (dmm->softBody)
		dmm->softBody->addForce(btVector3(dir[0],dir[1],dir[2]) * energy, 0);
	(void)pt;
}

extern "C" void Dmm_GetState_Impl(dmmObjectHandle_t h, dmmState_t *out) {
	if (!VALID_DMM(h) || !out) return;
	PhysDmmObject *dmm = &bs.dmmObjects[h];
	float ms=0, mx=0, md=0;
	for (int i = 0; i < dmm->numElements; i++) {
		if (dmm->elements[i].strain > ms) ms = dmm->elements[i].strain;
		if (dmm->elements[i].stress > mx) mx = dmm->elements[i].stress;
		if (dmm->elements[i].plasticity > md) md = dmm->elements[i].plasticity;
	}
	out->strain = ms; out->stress = mx; out->deformation = md;
	out->integrity = dmm->integrity; out->numFragments = dmm->numFragments; out->fractured = dmm->fractured;
}

extern "C" qboolean Dmm_IsFractured_Impl(dmmObjectHandle_t h) {
	return VALID_DMM(h) ? bs.dmmObjects[h].fractured : qfalse;
}

extern "C" int Dmm_GetFragments_Impl(dmmObjectHandle_t h, physBodyHandle_t *frags, int max) {
	(void)h; (void)frags; (void)max;
	return 0;
}

extern "C" void Dmm_SetMaterialParams_Impl(dmmObjectHandle_t h, float stiff, float yield, float frac) {
	if (!VALID_DMM(h)) return;
	bs.dmmObjects[h].yieldStrength = yield;
	bs.dmmObjects[h].fractureStrength = frac;
	(void)stiff;
}

/* ========== queries ========== */

extern "C" qboolean Phys_RayCast_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result) {
	if (!bs.initialized || !result) return qfalse;
	btVector3 f(from[0],from[1],from[2]), t(to[0],to[1],to[2]);
	btCollisionWorld::ClosestRayResultCallback cb(f, t);
	bs.world->rayTest(f, t, cb);
	if (cb.hasHit()) {
		result->hit = qtrue;
		result->hitPoint[0]=cb.m_hitPointWorld.x(); result->hitPoint[1]=cb.m_hitPointWorld.y(); result->hitPoint[2]=cb.m_hitPointWorld.z();
		result->hitNormal[0]=cb.m_hitNormalWorld.x(); result->hitNormal[1]=cb.m_hitNormalWorld.y(); result->hitNormal[2]=cb.m_hitNormalWorld.z();
		result->fraction = cb.m_closestHitFraction;
		result->body = -1;
		for (int i = 0; i < bs.bodyCount; i++)
			if (bs.bodies[i].active && bs.bodies[i].rigidBody == cb.m_collisionObject) { result->body = i; break; }
		return qtrue;
	}
	result->hit = qfalse; return qfalse;
}

extern "C" int Phys_OverlapSphere_Impl(const vec3_t c, float r, physBodyHandle_t *res, int max) {
	if (!bs.initialized) return 0;
	int count = 0;
	btVector3 center(c[0],c[1],c[2]);
	for (int i = 0; i < bs.bodyCount && count < max; i++) {
		if (!bs.bodies[i].active) continue;
		btTransform t; bs.bodies[i].rigidBody->getMotionState()->getWorldTransform(t);
		if ((t.getOrigin() - center).length() <= r)
			res[count++] = i;
	}
	return count;
}

extern "C" int Phys_OverlapBox_Impl(const vec3_t c, const vec3_t he, physBodyHandle_t *res, int max) {
	if (!bs.initialized) return 0;
	int count = 0;
	btVector3 mn(c[0]-he[0], c[1]-he[1], c[2]-he[2]), mx(c[0]+he[0], c[1]+he[1], c[2]+he[2]);
	for (int i = 0; i < bs.bodyCount && count < max; i++) {
		if (!bs.bodies[i].active) continue;
		btTransform t; bs.bodies[i].rigidBody->getMotionState()->getWorldTransform(t);
		btVector3 p = t.getOrigin();
		if (p.x()>=mn.x() && p.x()<=mx.x() && p.y()>=mn.y() && p.y()<=mx.y() && p.z()>=mn.z() && p.z()<=mx.z())
			res[count++] = i;
	}
	return count;
}

extern "C" void Phys_DebugDraw_Impl(void) { if (bs.initialized && bs.world) bs.world->debugDrawWorld(); }
extern "C" int Phys_GetBodyCount_Impl(void) { return bs.bodyCount; }
extern "C" int Phys_GetConstraintCount_Impl(void) { return bs.constraintCount; }

#endif /* USE_BULLET_PHYSICS_IMPL */
