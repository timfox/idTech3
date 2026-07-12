/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Active ragdoll motor layer (Euphoria-like) — PD joint targets from
balance, protect-head, brace, reach, stumble, getup, limp, and pain.
Runs above Soft Step constraints; ProcAnim drives high-level state.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "phys_events.h"
#include "phys_procedural_anim.h"

#define PHYS_MOTOR_MAX           64
#define PHYS_MOTOR_MAX_BONES     PROCANIM_BONE_COUNT

typedef struct joint_motor_cmd_s {
	float   stiffness;
	float   damping;
	vec3_t  targetAngularVelocity;
	quat_t  targetOrientation;
} joint_motor_cmd_t;

typedef enum {
	PHYS_CTRL_BALANCE,
	PHYS_CTRL_PROTECT_HEAD,
	PHYS_CTRL_BRACE,
	PHYS_CTRL_REACH,
	PHYS_CTRL_STUMBLE,
	PHYS_CTRL_GETUP,
	PHYS_CTRL_LIMP,
	PHYS_CTRL_PAIN,
	PHYS_CTRL_COUNT
} phys_motor_controller_t;

typedef int physMotorHandle_t;

typedef struct phys_motor_status_s {
	qboolean            active;
	procAnimState_t     animState;
	float               balance;
	float               physicsWeight[PHYS_MOTOR_MAX_BONES];
	joint_motor_cmd_t   boneCmd[PHYS_MOTOR_MAX_BONES];
} phys_motor_status_t;

void            PhysMotor_Init( void );
void            PhysMotor_Shutdown( void );
physMotorHandle_t PhysMotor_Create( procAnimHandle_t anim, physRagdollHandle_t ragdoll );
void            PhysMotor_Destroy( physMotorHandle_t handle );
void            PhysMotor_UpdateAll( float dt );
void            PhysMotor_ApplyHit( physMotorHandle_t handle, const phys_hit_event_t *hit );
void            PhysMotor_GetStatus( physMotorHandle_t handle, phys_motor_status_t *out );
void            PhysMotor_SetBonePhysicsWeight( physMotorHandle_t handle, int bone, float weight );
int             PhysMotor_GetActiveCount( void );
physMotorHandle_t PhysMotor_FindByRagdoll( physRagdollHandle_t ragdoll );

#ifdef __cplusplus
}
#endif
