/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Bullet Physics C++ backend implementation.
Wraps Bullet3 rigid body dynamics, constraints, and custom
Euphoria-style procedural ragdoll and DMM deformable bodies.

Requires Bullet Physics library (zlib license):
  apt-get install libbullet-dev
  or build from https://github.com/bulletphysics/bullet3

Compile with USE_BULLET_PHYSICS=ON.
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

struct PhysBody {
	btRigidBody        *rigidBody;
	btCollisionShape   *shape;
	btMotionState      *motionState;
	qboolean            active;
};

struct PhysConstraint {
	btTypedConstraint  *constraint;
	qboolean            active;
};

struct RagdollBone {
	btRigidBody        *body;
	btCollisionShape   *shape;
	btMotionState      *motionState;
};

struct PhysRagdoll {
	RagdollBone         bones[32];
	btTypedConstraint  *joints[31];
	int                 numBones;
	int                 numJoints;
	float               muscleStiffness;
	float               muscleDamping;
	float               balanceForce;
	qboolean            balanceEnabled;
	btVector3           balanceTarget;
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
	btDefaultCollisionConfiguration     *collisionConfig;
	btCollisionDispatcher               *dispatcher;
	btBroadphaseInterface               *broadphase;
	btSequentialImpulseConstraintSolver  *solver;
	btSoftRigidDynamicsWorld            *world;

	PhysBody         bodies[PHYS_MAX_RIGID_BODIES];
	int              bodyCount;

	PhysConstraint   constraints[PHYS_MAX_CONSTRAINTS];
	int              constraintCount;

	PhysRagdoll      ragdolls[PHYS_MAX_RAGDOLLS];
	int              ragdollCount;

	PhysDmmObject    dmmObjects[PHYS_MAX_DMM_OBJECTS];
	int              dmmCount;

	btSoftBodyWorldInfo softBodyWorldInfo;

	qboolean         initialized;
} bulletState;

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

static void getDmmMaterialParams(dmmMaterialType_t type, float *stiffness, float *yield, float *fracture) {
	switch (type) {
		case DMM_WOOD:     *stiffness = 12000; *yield = 40;  *fracture = 80; break;
		case DMM_GLASS:    *stiffness = 70000; *yield = 1;   *fracture = 5; break;
		case DMM_METAL_THIN: *stiffness = 200000; *yield = 250; *fracture = 400; break;
		case DMM_METAL_THICK: *stiffness = 200000; *yield = 400; *fracture = 800; break;
		case DMM_CONCRETE: *stiffness = 30000; *yield = 3;   *fracture = 10; break;
		case DMM_STONE:    *stiffness = 50000; *yield = 5;   *fracture = 15; break;
		case DMM_ICE:      *stiffness = 9000;  *yield = 1;   *fracture = 3; break;
		case DMM_PLASTIC:  *stiffness = 3000;  *yield = 30;  *fracture = 60; break;
		case DMM_CLOTH:    *stiffness = 100;   *yield = 50;  *fracture = 200; break;
		case DMM_RUBBER:   *stiffness = 50;    *yield = 200; *fracture = 500; break;
		case DMM_FLESH:    *stiffness = 500;   *yield = 10;  *fracture = 30; break;
		default:           *stiffness = 10000; *yield = 50;  *fracture = 100; break;
	}
}

extern "C" qboolean Phys_Init_Impl(void) {
	if (bulletState.initialized) return qtrue;

	bulletState.collisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
	bulletState.dispatcher = new btCollisionDispatcher(bulletState.collisionConfig);
	bulletState.broadphase = new btDbvtBroadphase();
	bulletState.solver = new btSequentialImpulseConstraintSolver();
	bulletState.world = new btSoftRigidDynamicsWorld(
		bulletState.dispatcher, bulletState.broadphase,
		bulletState.solver, bulletState.collisionConfig);

	bulletState.world->setGravity(btVector3(0, -800, 0));

	bulletState.softBodyWorldInfo.m_broadphase = bulletState.broadphase;
	bulletState.softBodyWorldInfo.m_dispatcher = bulletState.dispatcher;
	bulletState.softBodyWorldInfo.m_gravity = bulletState.world->getGravity();
	bulletState.softBodyWorldInfo.air_density = 1.2f;
	bulletState.softBodyWorldInfo.water_density = 0;
	bulletState.softBodyWorldInfo.water_offset = 0;
	bulletState.softBodyWorldInfo.water_normal = btVector3(0, 0, 0);

	bulletState.bodyCount = 0;
	bulletState.constraintCount = 0;
	bulletState.ragdollCount = 0;
	bulletState.dmmCount = 0;
	bulletState.initialized = qtrue;

	return qtrue;
}

extern "C" void Phys_Shutdown_Impl(void) {
	if (!bulletState.initialized) return;

	for (int i = bulletState.world->getNumCollisionObjects() - 1; i >= 0; i--) {
		btCollisionObject *obj = bulletState.world->getCollisionObjectArray()[i];
		btRigidBody *body = btRigidBody::upcast(obj);
		if (body && body->getMotionState()) {
			delete body->getMotionState();
		}
		bulletState.world->removeCollisionObject(obj);
		delete obj;
	}

	for (int i = 0; i < bulletState.bodyCount; i++) {
		if (bulletState.bodies[i].shape) {
			delete bulletState.bodies[i].shape;
			bulletState.bodies[i].shape = nullptr;
		}
	}

	delete bulletState.world;
	delete bulletState.solver;
	delete bulletState.broadphase;
	delete bulletState.dispatcher;
	delete bulletState.collisionConfig;

	memset(&bulletState, 0, sizeof(bulletState));
}

extern "C" void Phys_StepSimulation_Impl(float dt) {
	if (!bulletState.initialized || !bulletState.world) return;
	bulletState.world->stepSimulation(dt, 4, 1.0f / 60.0f);

	for (int i = 0; i < bulletState.ragdollCount; i++) {
		PhysRagdoll *rag = &bulletState.ragdolls[i];
		if (!rag->active || !rag->balanceEnabled) continue;

		for (int b = 0; b < rag->numBones; b++) {
			if (!rag->bones[b].body) continue;
			btTransform t;
			rag->bones[b].body->getMotionState()->getWorldTransform(t);
			btVector3 pos = t.getOrigin();
			btVector3 toTarget = rag->balanceTarget - pos;
			toTarget.setY(0);
			float dist = toTarget.length();
			if (dist > 0.1f) {
				btVector3 force = toTarget.normalized() * rag->balanceForce * rag->muscleStiffness;
				rag->bones[b].body->applyCentralForce(force);
			}
		}
	}

	for (int i = 0; i < bulletState.dmmCount; i++) {
		PhysDmmObject *dmm = &bulletState.dmmObjects[i];
		if (!dmm->active || !dmm->softBody || dmm->fractured) continue;

		float maxStrain = 0;
		for (int e = 0; e < dmm->numElements; e++) {
			if (dmm->elements[e].strain > maxStrain) {
				maxStrain = dmm->elements[e].strain;
			}
			if (dmm->elements[e].strain > dmm->yieldStrength) {
				dmm->elements[e].plasticity += (dmm->elements[e].strain - dmm->yieldStrength) * 0.01f;
			}
		}

		dmm->integrity = 1.0f - (maxStrain / dmm->fractureStrength);
		if (dmm->integrity <= 0.0f) {
			dmm->fractured = qtrue;
			dmm->integrity = 0.0f;
		}
	}
}

extern "C" physBodyHandle_t Phys_CreateBody_Impl(const physBodyDef_t *def) {
	if (!bulletState.initialized || bulletState.bodyCount >= PHYS_MAX_RIGID_BODIES) return -1;

	int idx = bulletState.bodyCount++;
	PhysBody *pb = &bulletState.bodies[idx];

	pb->shape = createShape(def);

	btVector3 localInertia(0, 0, 0);
	float mass = (def->type == PHYS_BODY_STATIC) ? 0.0f : def->mass;
	if (mass > 0) {
		pb->shape->calculateLocalInertia(mass, localInertia);
	}

	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(def->position[0], def->position[1], def->position[2]));

	pb->motionState = new btDefaultMotionState(startTransform);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, pb->motionState, pb->shape, localInertia);
	rbInfo.m_friction = def->friction;
	rbInfo.m_restitution = def->restitution;
	rbInfo.m_linearDamping = def->linearDamping;
	rbInfo.m_angularDamping = def->angularDamping;

	pb->rigidBody = new btRigidBody(rbInfo);

	if (def->type == PHYS_BODY_KINEMATIC) {
		pb->rigidBody->setCollisionFlags(pb->rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		pb->rigidBody->setActivationState(DISABLE_DEACTIVATION);
	}

	bulletState.world->addRigidBody(pb->rigidBody, def->collisionGroup, def->collisionMask);
	pb->active = qtrue;

	return idx;
}

extern "C" void Phys_GetBodyTransform_Impl(physBodyHandle_t handle, physTransform_t *out) {
	if (handle < 0 || handle >= bulletState.bodyCount || !out) return;
	PhysBody *pb = &bulletState.bodies[handle];
	if (!pb->active || !pb->rigidBody) return;

	btTransform t;
	pb->rigidBody->getMotionState()->getWorldTransform(t);
	btVector3 pos = t.getOrigin();
	btQuaternion rot = t.getRotation();
	btVector3 linVel = pb->rigidBody->getLinearVelocity();
	btVector3 angVel = pb->rigidBody->getAngularVelocity();

	out->position[0] = pos.x();
	out->position[1] = pos.y();
	out->position[2] = pos.z();

	float yaw, pitch, roll;
	btMatrix3x3(rot).getEulerYPR(yaw, pitch, roll);
	out->rotation[0] = pitch * 57.2957795f;
	out->rotation[1] = yaw * 57.2957795f;
	out->rotation[2] = roll * 57.2957795f;

	out->linearVelocity[0] = linVel.x();
	out->linearVelocity[1] = linVel.y();
	out->linearVelocity[2] = linVel.z();

	out->angularVelocity[0] = angVel.x();
	out->angularVelocity[1] = angVel.y();
	out->angularVelocity[2] = angVel.z();
}

extern "C" void Phys_ApplyImpulse_Impl(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point) {
	if (handle < 0 || handle >= bulletState.bodyCount) return;
	PhysBody *pb = &bulletState.bodies[handle];
	if (!pb->active || !pb->rigidBody) return;

	pb->rigidBody->activate(true);
	pb->rigidBody->applyImpulse(
		btVector3(impulse[0], impulse[1], impulse[2]),
		btVector3(point[0], point[1], point[2]));
}

extern "C" void Phys_SetGravity_Impl(const vec3_t gravity) {
	if (!bulletState.initialized || !bulletState.world) return;
	bulletState.world->setGravity(btVector3(gravity[0], gravity[1], gravity[2]));
}

extern "C" dmmObjectHandle_t Dmm_CreateObject_Impl(const dmmObjectDef_t *def) {
	if (!bulletState.initialized || bulletState.dmmCount >= PHYS_MAX_DMM_OBJECTS) return -1;

	int idx = bulletState.dmmCount++;
	PhysDmmObject *dmm = &bulletState.dmmObjects[idx];

	dmm->material = def->material;
	dmm->deformability = def->deformability;

	float stiff, yield, fracture;
	if (def->stiffness > 0) {
		stiff = def->stiffness;
		yield = def->yieldStrength;
		fracture = def->fractureStrength;
	} else {
		getDmmMaterialParams(def->material, &stiff, &yield, &fracture);
	}

	dmm->yieldStrength = yield;
	dmm->fractureStrength = fracture;
	dmm->integrity = 1.0f;
	dmm->fractured = qfalse;
	dmm->numFragments = 0;

	int res = def->gridResolution > 0 ? def->gridResolution : 8;
	dmm->numElements = res * res * res;
	dmm->elements = new DmmElement[dmm->numElements];
	memset(dmm->elements, 0, sizeof(DmmElement) * dmm->numElements);

	btVector3 corner(
		def->position[0] - def->dimensions[0] * 0.5f,
		def->position[1] - def->dimensions[1] * 0.5f,
		def->position[2] - def->dimensions[2] * 0.5f);

	dmm->softBody = btSoftBodyHelpers::CreateFromTriMesh(
		bulletState.softBodyWorldInfo, nullptr, nullptr, 0);

	if (dmm->softBody) {
		dmm->softBody->m_cfg.kDF = 0.5f;
		dmm->softBody->m_cfg.kDP = def->damping > 0 ? def->damping : 0.01f;
		dmm->softBody->m_cfg.kMT = stiff * 0.001f;

		btSoftBody::Material *mat = dmm->softBody->m_materials[0];
		mat->m_kLST = stiff * 0.0001f;
		mat->m_kAST = stiff * 0.0001f;
		mat->m_kVST = stiff * 0.0001f;

		bulletState.world->addSoftBody(dmm->softBody);
	}

	dmm->active = qtrue;
	return idx;
}

extern "C" void Dmm_ApplyImpact_Impl(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy) {
	if (handle < 0 || handle >= bulletState.dmmCount) return;
	PhysDmmObject *dmm = &bulletState.dmmObjects[handle];
	if (!dmm->active || dmm->fractured) return;

	float impactStress = energy / (dmm->deformability > 0 ? dmm->deformability : 1.0f);

	for (int i = 0; i < dmm->numElements; i++) {
		float dist = 1.0f;
		float attenuation = 1.0f / (1.0f + dist * dist);
		dmm->elements[i].strain += impactStress * attenuation;
		dmm->elements[i].stress = dmm->elements[i].strain * (1.0f - dmm->elements[i].plasticity);
	}

	if (dmm->softBody) {
		dmm->softBody->addForce(
			btVector3(direction[0], direction[1], direction[2]) * energy,
			0);
	}
}

extern "C" void Dmm_GetState_Impl(dmmObjectHandle_t handle, dmmState_t *out) {
	if (handle < 0 || handle >= bulletState.dmmCount || !out) return;
	PhysDmmObject *dmm = &bulletState.dmmObjects[handle];

	float maxStrain = 0, maxStress = 0, maxDeform = 0;
	for (int i = 0; i < dmm->numElements; i++) {
		if (dmm->elements[i].strain > maxStrain) maxStrain = dmm->elements[i].strain;
		if (dmm->elements[i].stress > maxStress) maxStress = dmm->elements[i].stress;
		if (dmm->elements[i].plasticity > maxDeform) maxDeform = dmm->elements[i].plasticity;
	}

	out->strain = maxStrain;
	out->stress = maxStress;
	out->deformation = maxDeform;
	out->integrity = dmm->integrity;
	out->numFragments = dmm->numFragments;
	out->fractured = dmm->fractured;
}

extern "C" qboolean Phys_RayCast_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result) {
	if (!bulletState.initialized || !bulletState.world || !result) return qfalse;

	btVector3 btFrom(from[0], from[1], from[2]);
	btVector3 btTo(to[0], to[1], to[2]);

	btCollisionWorld::ClosestRayResultCallback callback(btFrom, btTo);
	bulletState.world->rayTest(btFrom, btTo, callback);

	if (callback.hasHit()) {
		result->hit = qtrue;
		result->hitPoint[0] = callback.m_hitPointWorld.x();
		result->hitPoint[1] = callback.m_hitPointWorld.y();
		result->hitPoint[2] = callback.m_hitPointWorld.z();
		result->hitNormal[0] = callback.m_hitNormalWorld.x();
		result->hitNormal[1] = callback.m_hitNormalWorld.y();
		result->hitNormal[2] = callback.m_hitNormalWorld.z();
		result->fraction = callback.m_closestHitFraction;
		return qtrue;
	}

	result->hit = qfalse;
	return qfalse;
}

#endif /* USE_BULLET_PHYSICS_IMPL */
