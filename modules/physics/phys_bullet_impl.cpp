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
#include "phys_impl.h"
#include "phys_debugdraw.h"
#include "phys_events.h"
#include "q_shared.h"
#include "qcommon.h"
}

/* ---------- internal types ---------- */

struct PhysBody {
	btRigidBody      *rigidBody;
	btCollisionShape *shape;
	btMotionState    *motionState;
	physBodyType_t    bodyType;
	int               materialId;
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

class EngineBulletDebugDraw : public btIDebugDraw {
public:
	void drawLine( const btVector3 &from, const btVector3 &to, const btVector3 &color ) override {
		vec3_t f, t, c;
		f[0] = from.x(); f[1] = from.y(); f[2] = from.z();
		t[0] = to.x(); t[1] = to.y(); t[2] = to.z();
		c[0] = color.x(); c[1] = color.y(); c[2] = color.z();
		PhysDebug_AddLine( f, t, c );
	}
	void drawContactPoint( const btVector3 &, const btVector3 &, btScalar, int, const btVector3 & ) override {}
	void reportErrorWarning( const char * ) override {}
	void draw3dText( const btVector3 &, const char * ) override {}
	void setDebugMode( int ) override {}
	int getDebugMode() const override { return DBG_DrawWireframe; }
};

static EngineBulletDebugDraw g_engineBulletDebugDraw;

static int findBodyHandle( const btCollisionObject *obj ) {
	for ( int i = 0; i < bs.bodyCount; i++ ) {
		if ( bs.bodies[i].active && bs.bodies[i].rigidBody == obj ) {
			return i;
		}
	}
	return -1;
}

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
	bs.world->setDebugDrawer( &g_engineBulletDebugDraw );
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
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
	memset(&bs, 0, sizeof(bs));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
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
	pb->bodyType = def->type;
	pb->materialId = def->materialId;
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
	if (rot) {
		btQuaternion q; q.setEulerZYX(rot[2]*0.0174533f, rot[1]*0.0174533f, rot[0]*0.0174533f);
		t.setRotation(q);
	} else {
		t.setRotation(bs.bodies[h].rigidBody->getWorldTransform().getRotation());
	}
	bs.bodies[h].rigidBody->setWorldTransform(t);
	bs.bodies[h].rigidBody->getMotionState()->setWorldTransform(t);
}

extern "C" void Phys_SetBodyTargetTransform_Impl(physBodyHandle_t h, const vec3_t pos, const vec3_t rot, float timeStep) {
	(void)timeStep;
	Phys_SetBodyTransform_Impl(h, pos, rot);
}

extern "C" void Phys_SetBodyGravityScale_Impl(physBodyHandle_t h, float scale) {
	if (!VALID_BODY(h)) return;
	bs.bodies[h].rigidBody->setGravity(btVector3(0, 0, -800.0f * scale));
}

extern "C" void Phys_SetBodyMotionLocks_Impl(physBodyHandle_t h, int lockBits) {
	(void)h; (void)lockBits;
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
	if (lin) {
		bs.bodies[h].rigidBody->setLinearVelocity(btVector3(lin[0], lin[1], lin[2]));
	}
	if (ang) {
		bs.bodies[h].rigidBody->setAngularVelocity(btVector3(ang[0], ang[1], ang[2]));
	}
}

extern "C" void Phys_SetBodyActive_Impl(physBodyHandle_t h, qboolean active) {
	if (!VALID_BODY(h)) return;
	if (active) bs.bodies[h].rigidBody->activate(true);
	else bs.bodies[h].rigidBody->setActivationState(DISABLE_SIMULATION);
}

extern "C" physBodyType_t Phys_GetBodyType_Impl(physBodyHandle_t h) {
	if (!VALID_BODY(h)) return PHYS_BODY_STATIC;
	return bs.bodies[h].bodyType;
}

extern "C" qboolean Phys_IsBodyDynamic_Impl(physBodyHandle_t h) {
	if (!VALID_BODY(h)) return qfalse;
	return (bs.bodies[h].bodyType == PHYS_BODY_DYNAMIC) ? qtrue : qfalse;
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
		case PHYS_CONSTRAINT_DISTANCE:
		case PHYS_CONSTRAINT_SLIDER:
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

extern "C" void Phys_SetConstraintMotor_Impl(physConstraintHandle_t h, qboolean enable, float speed, float maxForce) {
	(void)h; (void)enable; (void)speed; (void)maxForce;
}

extern "C" void Phys_SetConstraintBreakForce_Impl(physConstraintHandle_t h, float force, float torque) {
	(void)h; (void)force; (void)torque;
}

extern "C" void Phys_SetWheelSteering_Impl(physConstraintHandle_t h, float angleRadians, float maxTorque) {
	(void)h; (void)angleRadians; (void)maxTorque;
}

extern "C" void Phys_SetConstraintSpring_Impl(physConstraintHandle_t h, qboolean enable,
	float hertz, float dampingRatio) {
	(void)h; (void)enable; (void)hertz; (void)dampingRatio;
}

extern "C" void Phys_SetSphericalLimits_Impl(physConstraintHandle_t h, float coneAngleRadians,
	float twistLowerRadians, float twistUpperRadians) {
	(void)h; (void)coneAngleRadians; (void)twistLowerRadians; (void)twistUpperRadians;
}

extern "C" void Phys_GetConstraintReaction_Impl(physConstraintHandle_t h, vec3_t forceOut, vec3_t torqueOut) {
	if (forceOut) { forceOut[0] = forceOut[1] = forceOut[2] = 0.0f; }
	if (torqueOut) { torqueOut[0] = torqueOut[1] = torqueOut[2] = 0.0f; }
	(void)h;
}

extern "C" int Phys_AttachShape_Impl(physBodyHandle_t body, const physBodyDef_t *shapeDef) {
	(void)body; (void)shapeDef;
	return -1;
}

extern "C" void Phys_DestroyAttachedShape_Impl(physBodyHandle_t body, int shapeIndex) {
	(void)body; (void)shapeIndex;
}

extern "C" void Phys_SetBodyFilter_Impl(physBodyHandle_t body, int categoryBits, int maskBits) {
	Phys_SetBodyFilterEx_Impl(body, categoryBits, maskBits, 0);
}

extern "C" void Phys_SetBodyFilterEx_Impl(physBodyHandle_t body, int categoryBits, int maskBits, int groupIndex) {
	(void)body; (void)categoryBits; (void)maskBits; (void)groupIndex;
}

/* ========== ragdoll ========== */

extern "C" physRagdollHandle_t Phys_CreateRagdoll_Impl(const physRagdollDef_t *def) {
	if (!bs.initialized || bs.ragdollCount >= PHYS_MAX_RAGDOLLS) return -1;
	int idx = bs.ragdollCount++;
	PhysRagdoll *rag = &bs.ragdolls[idx];
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
	memset(rag, 0, sizeof(*rag));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
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

extern "C" void Phys_RagdollSetBoneAnimTarget_Impl(physRagdollHandle_t h, int bone,
	const vec3_t position, const vec3_t rotationDeg) {
	(void)h; (void)bone; (void)position; (void)rotationDeg;
}

extern "C" void Phys_RagdollClearAnimTargets_Impl(physRagdollHandle_t h) {
	(void)h;
}

extern "C" void Phys_RagdollApplyBoneTorque_Impl(physRagdollHandle_t h, int bone, const vec3_t torque) {
	if (!VALID_RAG(h) || bone < 0 || bone >= bs.ragdolls[h].numBones || !torque) return;
	btRigidBody *body = bs.ragdolls[h].bones[bone].body;
	if (!body) return;
	body->activate(true);
	body->applyTorque(btVector3(torque[0], torque[1], torque[2]));
}

extern "C" int Phys_GetRagdollCount_Impl(void) {
	int n = 0;
	for (int i = 0; i < bs.ragdollCount; i++) {
		if (bs.ragdolls[i].active) n++;
	}
	return n;
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

extern "C" int Dmm_SpawnFragments_Impl(dmmObjectHandle_t h, const vec3_t impactPoint, float energy) {
	(void)h; (void)impactPoint; (void)energy;
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

extern "C" int Phys_OverlapShape_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults) {
	return Phys_OverlapSphere_Impl(center, radius, results, maxResults);
}

extern "C" qboolean Phys_RayCastFiltered_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result,
	const physQueryFilter_t *filter) {
	(void)filter;
	return Phys_RayCast_Impl(from, to, result);
}

extern "C" int Phys_OverlapShapeFiltered_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults,
	const physQueryFilter_t *filter) {
	(void)filter;
	return Phys_OverlapShape_Impl(center, radius, results, maxResults);
}

extern "C" int Phys_GetBodyContacts_Impl(physBodyHandle_t body, physContact_t *out, int maxOut) {
	(void)body; (void)out; (void)maxOut;
	return 0;
}

extern "C" void Phys_SetHitEventThreshold_Impl(float approachSpeed) {
	(void)approachSpeed;
}

extern "C" void Phys_DebugDraw_Impl(void) {
	if ( bs.initialized && bs.world ) {
		bs.world->debugDrawWorld();
	}
}

extern "C" void Phys_SetBodyMaterial_Impl( physBodyHandle_t h, int materialId ) {
	if ( !VALID_BODY( h ) ) {
		return;
	}
	bs.bodies[h].materialId = materialId;
}

extern "C" void Phys_SetBodyFriction_Impl( physBodyHandle_t h, float friction ) {
	(void)h; (void)friction;
}

extern "C" void Phys_SetBodyRestitution_Impl( physBodyHandle_t h, float restitution ) {
	(void)h; (void)restitution;
}

extern "C" int Phys_GetBodyMaterial_Impl( physBodyHandle_t h ) {
	if ( !VALID_BODY( h ) ) {
		return 0;
	}
	return bs.bodies[h].materialId;
}

extern "C" qboolean Phys_ConvexSweep_Impl( const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result ) {
	if ( !bs.initialized || !shapeDef || !result ) {
		return qfalse;
	}

	btCollisionShape *shape = createShape( shapeDef );
	btConvexShape *convex = dynamic_cast<btConvexShape *>( shape );
	btTransform fromXform;
	btTransform toXform;
	fromXform.setIdentity();
	toXform.setIdentity();
	fromXform.setOrigin( btVector3( from[0], from[1], from[2] ) );
	toXform.setOrigin( btVector3( to[0], to[1], to[2] ) );
	(void)rotation;

	if ( !convex ) {
		delete shape;
		result->hit = qfalse;
		return qfalse;
	}

	btCollisionWorld::ClosestConvexResultCallback cb( fromXform.getOrigin(), toXform.getOrigin() );
	bs.world->convexSweepTest( convex, fromXform, toXform, cb );
	delete shape;

	if ( cb.hasHit() ) {
		result->hit = qtrue;
		result->hitPoint[0] = cb.m_hitPointWorld.x();
		result->hitPoint[1] = cb.m_hitPointWorld.y();
		result->hitPoint[2] = cb.m_hitPointWorld.z();
		result->hitNormal[0] = cb.m_hitNormalWorld.x();
		result->hitNormal[1] = cb.m_hitNormalWorld.y();
		result->hitNormal[2] = cb.m_hitNormalWorld.z();
		result->fraction = cb.m_closestHitFraction;
		result->body = findBodyHandle( cb.m_hitCollisionObject );
		return qtrue;
	}

	result->hit = qfalse;
	return qfalse;
}

extern "C" qboolean Phys_ConvexSweepFiltered_Impl( const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result, const physQueryFilter_t *filter ) {
	(void)filter;
	return Phys_ConvexSweep_Impl( shapeDef, from, to, rotation, result );
}

extern "C" void Phys_ProcessContactEvents_Impl( void ) {
	if ( !bs.initialized || !bs.dispatcher ) {
		return;
	}

	const int numManifolds = bs.dispatcher->getNumManifolds();
	for ( int i = 0; i < numManifolds; i++ ) {
		btPersistentManifold *manifold = bs.dispatcher->getManifoldByIndexInternal( i );
		const btCollisionObject *obA = static_cast<const btCollisionObject *>( manifold->getBody0() );
		const btCollisionObject *obB = static_cast<const btCollisionObject *>( manifold->getBody1() );
		const int bodyA = findBodyHandle( obA );
		const int bodyB = findBodyHandle( obB );
		float totalImpulse = 0.0f;
		vec3_t point = { 0.0f, 0.0f, 0.0f };
		vec3_t normal = { 0.0f, 0.0f, 1.0f };
		vec3_t impulse = { 0.0f, 0.0f, 0.0f };
		const int numContacts = manifold->getNumContacts();

		if ( bodyA < 0 && bodyB < 0 ) {
			continue;
		}

		for ( int p = 0; p < numContacts; p++ ) {
			const btManifoldPoint &pt = manifold->getContactPoint( p );
			totalImpulse += pt.getAppliedImpulse();
			point[0] = pt.getPositionWorldOnA().x();
			point[1] = pt.getPositionWorldOnA().y();
			point[2] = pt.getPositionWorldOnA().z();
			normal[0] = pt.m_normalWorldOnB.x();
			normal[1] = pt.m_normalWorldOnB.y();
			normal[2] = pt.m_normalWorldOnB.z();
		}

		if ( totalImpulse < 25.0f ) {
			continue;
		}

		impulse[0] = normal[0] * totalImpulse;
		impulse[1] = normal[1] * totalImpulse;
		impulse[2] = normal[2] * totalImpulse;

		PhysEvent_PostImpact( -1, bodyA, bodyB,
			bodyA >= 0 ? bs.bodies[bodyA].materialId : 0,
			bodyB >= 0 ? bs.bodies[bodyB].materialId : 0,
			point, normal, impulse, totalImpulse );
	}
}

extern "C" int Phys_GetBodyCount_Impl(void) { return bs.bodyCount; }
extern "C" int Phys_GetConstraintCount_Impl(void) { return bs.constraintCount; }

extern "C" physBodyHandle_t Phys_AddStaticTriMesh_Impl(const float *verts, int numVerts, const int *indices, int numIndices) {
	physBodyDef_t def;
	int i;
	float mn[3], mx[3];

	if (!bs.initialized || !verts || numVerts < 3 || !indices || numIndices < 3) {
		return -1;
	}

	mn[0] = mx[0] = verts[0];
	mn[1] = mx[1] = verts[1];
	mn[2] = mx[2] = verts[2];
	for (i = 1; i < numVerts; i++) {
		const float *v = verts + i * 3;
		if (v[0] < mn[0]) mn[0] = v[0];
		if (v[1] < mn[1]) mn[1] = v[1];
		if (v[2] < mn[2]) mn[2] = v[2];
		if (v[0] > mx[0]) mx[0] = v[0];
		if (v[1] > mx[1]) mx[1] = v[1];
		if (v[2] > mx[2]) mx[2] = v[2];
	}

	memset(&def, 0, sizeof(def));
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_STATIC;
	def.halfExtents[0] = (mx[0] - mn[0]) * 0.5f;
	def.halfExtents[1] = (mx[1] - mn[1]) * 0.5f;
	def.halfExtents[2] = (mx[2] - mn[2]) * 0.5f;
	if (def.halfExtents[0] < 1.0f) def.halfExtents[0] = 1.0f;
	if (def.halfExtents[1] < 1.0f) def.halfExtents[1] = 1.0f;
	if (def.halfExtents[2] < 1.0f) def.halfExtents[2] = 1.0f;
	def.position[0] = (mn[0] + mx[0]) * 0.5f;
	def.position[1] = (mn[1] + mx[1]) * 0.5f;
	def.position[2] = (mn[2] + mx[2]) * 0.5f;
	def.friction = 0.8f;
	(void)indices;
	(void)numIndices;
	return Phys_CreateBody_Impl(&def);
}

extern "C" physBodyHandle_t Phys_AddStaticHeightField_Impl(const float *heights, int countX, int countY,
	float cellSize, float heightScale, const vec3_t origin) {
	/* Bullet path: approximate with a static AABB from height extents. */
	physBodyDef_t def;
	float minH, maxH;
	int i, n;
	(void)cellSize;
	(void)heightScale;
	if (!heights || countX < 2 || countY < 2) {
		return -1;
	}
	n = countX * countY;
	minH = maxH = heights[0];
	for (i = 1; i < n; i++) {
		if (heights[i] < minH) minH = heights[i];
		if (heights[i] > maxH) maxH = heights[i];
	}
	memset(&def, 0, sizeof(def));
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_STATIC;
	def.halfExtents[0] = countX * (cellSize > 0 ? cellSize : 32.0f) * 0.5f;
	def.halfExtents[1] = countY * (cellSize > 0 ? cellSize : 32.0f) * 0.5f;
	def.halfExtents[2] = (maxH - minH) * 0.5f + 1.0f;
	if (origin) {
		def.position[0] = origin[0];
		def.position[1] = origin[1];
		def.position[2] = origin[2] + (minH + maxH) * 0.5f;
	}
	def.friction = 0.8f;
	return Phys_CreateBody_Impl(&def);
}

extern "C" physBodyHandle_t Phys_AddStaticCompoundBoxes_Impl(const float *centersXYZ, const float *halfExtentsXYZ, int count) {
	physBodyHandle_t first = -1;
	int i;

	if (!bs.initialized || !centersXYZ || !halfExtentsXYZ || count < 1) {
		return -1;
	}
	for (i = 0; i < count; i++) {
		physBodyDef_t def;
		physBodyHandle_t h;
		memset(&def, 0, sizeof(def));
		def.shape = PHYS_SHAPE_BOX;
		def.type = PHYS_BODY_STATIC;
		def.position[0] = centersXYZ[i * 3 + 0];
		def.position[1] = centersXYZ[i * 3 + 1];
		def.position[2] = centersXYZ[i * 3 + 2];
		def.halfExtents[0] = halfExtentsXYZ[i * 3 + 0];
		def.halfExtents[1] = halfExtentsXYZ[i * 3 + 1];
		def.halfExtents[2] = halfExtentsXYZ[i * 3 + 2];
		def.friction = 0.8f;
		h = Phys_CreateBody_Impl(&def);
		if (first < 0) {
			first = h;
		}
	}
	return first;
}

extern "C" qboolean Phys_MoverStep_Impl(vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut) {
	(void)origin;
	(void)velocity;
	(void)radius;
	(void)height;
	(void)wishDir;
	(void)wishSpeed;
	(void)dt;
	(void)jump;
	(void)groundedOut;
	return qfalse;
}

extern "C" int Phys_GetWorkerCount_Impl(void) {
	return 1;
}

extern "C" int Phys_ApplyImpulseRadius_Impl(const vec3_t center, float radius, float magnitude, float falloff) {
	(void)center;
	(void)radius;
	(void)magnitude;
	(void)falloff;
	return -1;
}

extern "C" void Phys_GetSoftStepProfile_Impl(physSoftStepProfile_t *out) {
	if (out) memset(out, 0, sizeof(*out));
}

extern "C" void Phys_StartRecording_Impl(void) {}

extern "C" void Phys_StopRecording_Impl(const char *path) {
	(void)path;
}

extern "C" qboolean Phys_ValidateReplay_Impl(const char *path) {
	(void)path;
	return qfalse;
}

extern "C" void Phys_DumpWorld_Impl(void) {
	Com_Printf( "phys_dump: Soft Step only\n" );
}

extern "C" qboolean Phys_GetClosestPoint_Impl(physBodyHandle_t body, const vec3_t target, vec3_t closestOut, float *distanceOut) {
	(void)body; (void)target;
	if (closestOut) { closestOut[0]=closestOut[1]=closestOut[2]=0; }
	if (distanceOut) *distanceOut = 0.0f;
	return qfalse;
}
extern "C" qboolean Phys_SphereTimeOfImpact_Impl(const vec3_t from, const vec3_t to, float radius,
	physBodyHandle_t againstBody, physRayResult_t *result) {
	(void)from; (void)to; (void)radius; (void)againstBody;
	if (result) memset(result, 0, sizeof(*result));
	return qfalse;
}
extern "C" void Phys_SetCustomFilterCallback_Impl(PhysCustomFilterFn fn, void *userData) { (void)fn; (void)userData; }
extern "C" void Phys_SetPreSolveCallback_Impl(PhysPreSolveFn fn, void *userData) { (void)fn; (void)userData; }
extern "C" void Phys_SetBodyContinuous_Impl(physBodyHandle_t body, qboolean enable) { (void)body; (void)enable; }
extern "C" void Phys_SetBodySleepEnabled_Impl(physBodyHandle_t body, qboolean enable) { (void)body; (void)enable; }
extern "C" void Phys_SetBodySleepThreshold_Impl(physBodyHandle_t body, float linearThreshold) { (void)body; (void)linearThreshold; }
extern "C" void Phys_SetContactTuning_Impl(float hertz, float dampingRatio, float contactSpeed) {
	(void)hertz; (void)dampingRatio; (void)contactSpeed;
}
extern "C" void Phys_SetMaxLinearSpeed_Impl(float maxSpeed) { (void)maxSpeed; }
extern "C" void Phys_EnableSpeculative_Impl(qboolean enable) { (void)enable; }
extern "C" void Phys_SetDebugDrawFlags_Impl(unsigned flags) { (void)flags; }
extern "C" qboolean Phys_UpdateStaticTriMesh_Impl(physBodyHandle_t body, const float *verts, int numVerts,
	const int *indices, int numIndices) {
	(void)body; (void)verts; (void)numVerts; (void)indices; (void)numIndices;
	return qfalse;
}
extern "C" void Phys_RebuildStaticTree_Impl(void) {}
extern "C" qboolean Phys_ReplayOpen_Impl(const char *path) { (void)path; return qfalse; }
extern "C" void Phys_ReplayClose_Impl(void) {}
extern "C" qboolean Phys_ReplayStep_Impl(void) { return qfalse; }
extern "C" void Phys_ReplaySeek_Impl(int frame) { (void)frame; }
extern "C" int Phys_ReplayGetFrame_Impl(void) { return -1; }
extern "C" int Phys_ReplayGetFrameCount_Impl(void) { return 0; }
extern "C" qboolean Phys_ReplayHasDiverged_Impl(void) { return qfalse; }
extern "C" qboolean Phys_ReplayIsOpen_Impl(void) { return qfalse; }
extern "C" void Phys_SetHingeTargetAngle_Impl(physConstraintHandle_t handle, float targetRadians) {
	(void)handle; (void)targetRadians;
}
extern "C" void Phys_SetSliderTarget_Impl(physConstraintHandle_t handle, float targetTranslation) {
	(void)handle; (void)targetTranslation;
}
extern "C" void Phys_SetDistanceLength_Impl(physConstraintHandle_t handle, float length) {
	(void)handle; (void)length;
}
extern "C" void Phys_SetWheelSuspension_Impl(physConstraintHandle_t handle, float hertz, float dampingRatio,
	float lower, float upper) {
	(void)handle; (void)hertz; (void)dampingRatio; (void)lower; (void)upper;
}
extern "C" void Phys_SetWheelSpin_Impl(physConstraintHandle_t handle, float speed, float maxTorque) {
	(void)handle; (void)speed; (void)maxTorque;
}
extern "C" void Phys_SetMotorVelocities_Impl(physConstraintHandle_t handle, const vec3_t linearVelocity,
	const vec3_t angularVelocity, float maxForce, float maxTorque) {
	(void)handle; (void)linearVelocity; (void)angularVelocity; (void)maxForce; (void)maxTorque;
}
extern "C" void Phys_SetSphericalTarget_Impl(physConstraintHandle_t handle, const vec3_t rotationDeg) {
	(void)handle; (void)rotationDeg;
}
extern "C" void Phys_SetBodyDamping_Impl(physBodyHandle_t body, float linearDamping, float angularDamping) {
	(void)body; (void)linearDamping; (void)angularDamping;
}
extern "C" void Phys_SetBodyType_Impl(physBodyHandle_t body, physBodyType_t type) {
	(void)body; (void)type;
}
extern "C" void Phys_ApplyWind_Impl(physBodyHandle_t body, const vec3_t wind, float drag, float lift, float maxSpeed) {
	(void)body; (void)wind; (void)drag; (void)lift; (void)maxSpeed;
}
extern "C" int Phys_Explode_Impl(const vec3_t center, float radius, float impulsePerArea, float falloff,
	unsigned maskBits) {
	(void)center; (void)radius; (void)impulsePerArea; (void)falloff; (void)maskBits;
	return 0;
}
extern "C" int Phys_RayCastAll_Impl(const vec3_t from, const vec3_t to, physRayResult_t *results, int maxResults,
	const physQueryFilter_t *filter) {
	(void)from; (void)to; (void)filter;
	if (results && maxResults > 0) memset(results, 0, (size_t)maxResults * sizeof(*results));
	return 0;
}
extern "C" void Phys_SetFrictionCallback_Impl(PhysFrictionMixFn fn) { (void)fn; }
extern "C" void Phys_SetRestitutionCallback_Impl(PhysRestitutionMixFn fn) { (void)fn; }

#endif /* USE_BULLET_PHYSICS_IMPL */
