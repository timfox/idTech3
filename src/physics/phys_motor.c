/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_bullet.h"
#include "phys_events.h"
#include "phys_motor.h"

typedef struct physMotorSlot_s {
	qboolean            active;
	procAnimHandle_t    anim;
	physRagdollHandle_t ragdoll;
	float               physicsWeight[PHYS_MOTOR_MAX_BONES];
	joint_motor_cmd_t   boneCmd[PHYS_MOTOR_MAX_BONES];
} physMotorSlot_t;

static physMotorSlot_t motors[PHYS_MOTOR_MAX];
static int motorCount;
static qboolean motorInitialized;

static cvar_t *phys_motor;

static float PhysMotor_BoneWeightDefault( int bone ) {
	switch ( bone ) {
		case PROCANIM_BONE_HEAD:          return 0.3f;
		case PROCANIM_BONE_SPINE:         return 0.35f;
		case PROCANIM_BONE_UPPER_ARM_L:
		case PROCANIM_BONE_UPPER_ARM_R:   return 0.45f;
		case PROCANIM_BONE_LOWER_ARM_L:
		case PROCANIM_BONE_LOWER_ARM_R:   return 0.55f;
		case PROCANIM_BONE_PELVIS:        return 0.25f;
		case PROCANIM_BONE_UPPER_LEG_L:
		case PROCANIM_BONE_UPPER_LEG_R:   return 0.2f;
		case PROCANIM_BONE_LOWER_LEG_L:
		case PROCANIM_BONE_LOWER_LEG_R:   return 0.15f;
		default:                          return 0.3f;
	}
}

void PhysMotor_Init( void ) {
	if ( motorInitialized ) {
		return;
	}
	phys_motor = Cvar_Get( "phys_motor", "1", CVAR_ARCHIVE );
	motorCount = 0;
	Com_Memset( motors, 0, sizeof( motors ) );
	motorInitialized = qtrue;
	Com_Printf( "PhysMotor: active ragdoll motor layer initialized\n" );
}

void PhysMotor_Shutdown( void ) {
	motorCount = 0;
	Com_Memset( motors, 0, sizeof( motors ) );
	motorInitialized = qfalse;
}

physMotorHandle_t PhysMotor_Create( procAnimHandle_t anim, physRagdollHandle_t ragdoll ) {
	int i;
	int idx = -1;

	if ( !motorInitialized ) {
		PhysMotor_Init();
	}

	for ( i = 0; i < PHYS_MOTOR_MAX; i++ ) {
		if ( !motors[i].active ) {
			idx = i;
			break;
		}
	}
	if ( idx < 0 ) {
		Com_Printf( S_COLOR_YELLOW "PhysMotor: no free motor slots\n" );
		return -1;
	}

	motors[idx].active = qtrue;
	motors[idx].anim = anim;
	motors[idx].ragdoll = ragdoll;
	for ( i = 0; i < PHYS_MOTOR_MAX_BONES; i++ ) {
		motors[idx].physicsWeight[i] = PhysMotor_BoneWeightDefault( i );
		Com_Memset( &motors[idx].boneCmd[i], 0, sizeof( joint_motor_cmd_t ) );
		motors[idx].boneCmd[i].stiffness = 0.2f;
		motors[idx].boneCmd[i].damping = 0.4f;
	}
	if ( idx >= motorCount ) {
		motorCount = idx + 1;
	}
	return idx;
}

void PhysMotor_Destroy( physMotorHandle_t handle ) {
	if ( handle < 0 || handle >= PHYS_MOTOR_MAX || !motors[handle].active ) {
	 return;
	}
	motors[handle].active = qfalse;
}

static void PhysMotor_RunBalance( physMotorSlot_t *m, procAnimState_t state, float balance, float dt ) {
	procAnimStatus_t status;
	float stiff;
	int i;

	ProcAnim_GetStatus( m->anim, &status );
	stiff = ( state == PROCANIM_STATE_STUMBLE || state == PROCANIM_STATE_FALLING ) ? 0.15f : 0.45f;
	stiff *= balance;

	for ( i = 0; i < PHYS_MOTOR_MAX_BONES; i++ ) {
		m->boneCmd[i].stiffness = stiff;
		m->boneCmd[i].damping = 0.35f + ( 1.0f - balance ) * 0.25f;
		m->boneCmd[i].targetAngularVelocity[0] = 0.0f;
		m->boneCmd[i].targetAngularVelocity[1] = ( 1.0f - balance ) * 2.0f * dt;
		m->boneCmd[i].targetAngularVelocity[2] = 0.0f;
	}

	Phys_RagdollSetBalance( m->ragdoll, qtrue, status.centerOfMass );
}

static void PhysMotor_RunProtectHead( physMotorSlot_t *m, float pain ) {
	if ( pain < 0.2f ) {
		return;
	}
	m->physicsWeight[PROCANIM_BONE_HEAD] = 0.5f + pain * 0.4f;
	m->boneCmd[PROCANIM_BONE_HEAD].stiffness = 0.6f + pain * 0.3f;
	m->boneCmd[PROCANIM_BONE_SPINE].stiffness = 0.35f + pain * 0.2f;
}

static void PhysMotor_RunBrace( physMotorSlot_t *m, procAnimState_t state ) {
	if ( state != PROCANIM_STATE_BRACING && state != PROCANIM_STATE_FALLING ) {
		return;
	}
	m->physicsWeight[PROCANIM_BONE_UPPER_ARM_L] = 0.75f;
	m->physicsWeight[PROCANIM_BONE_UPPER_ARM_R] = 0.75f;
	m->physicsWeight[PROCANIM_BONE_LOWER_ARM_L] = 0.85f;
	m->physicsWeight[PROCANIM_BONE_LOWER_ARM_R] = 0.85f;
	m->boneCmd[PROCANIM_BONE_UPPER_ARM_L].stiffness = 0.55f;
	m->boneCmd[PROCANIM_BONE_UPPER_ARM_R].stiffness = 0.55f;
}

static void PhysMotor_RunStumble( physMotorSlot_t *m, procAnimState_t state, float balance ) {
	if ( state != PROCANIM_STATE_STUMBLE && state != PROCANIM_STATE_FALLING ) {
		return;
	}
	m->physicsWeight[PROCANIM_BONE_PELVIS] = 0.45f;
	m->physicsWeight[PROCANIM_BONE_UPPER_LEG_L] = 0.35f;
	m->physicsWeight[PROCANIM_BONE_UPPER_LEG_R] = 0.35f;
	m->boneCmd[PROCANIM_BONE_PELVIS].targetAngularVelocity[1] = ( 0.5f - balance ) * 3.0f;
}

static void PhysMotor_RunPain( physMotorSlot_t *m, float pain ) {
	int i;
	float scale;

	if ( pain < 0.05f ) {
		return;
	}
	scale = 1.0f - pain * 0.4f;
	for ( i = 0; i < PHYS_MOTOR_MAX_BONES; i++ ) {
		m->boneCmd[i].stiffness *= scale;
	}
}

static void PhysMotor_ApplyMotors( physMotorSlot_t *m ) {
	float blend;
	procAnimStatus_t status;

	ProcAnim_GetStatus( m->anim, &status );
	blend = 1.0f - status.muscleStiffness;
	if ( blend < 0.0f ) {
		blend = 0.0f;
	}
	if ( blend > 1.0f ) {
		blend = 1.0f;
	}

	Phys_RagdollSetMuscleStiffness( m->ragdoll,
		status.muscleStiffness + m->boneCmd[PROCANIM_BONE_PELVIS].stiffness * blend * 0.5f );
	Phys_RagdollBlendToAnimation( m->ragdoll, 1.0f - blend );
}

void PhysMotor_UpdateAll( float dt ) {
	int i;
	procAnimStatus_t status;

	if ( !motorInitialized || !phys_motor || !phys_motor->integer ) {
		return;
	}

	for ( i = 0; i < motorCount; i++ ) {
		physMotorSlot_t *m = &motors[i];

		if ( !m->active ) {
			continue;
		}

		ProcAnim_GetStatus( m->anim, &status );

		PhysMotor_RunBalance( m, status.state, status.balance, dt );
		PhysMotor_RunProtectHead( m, status.painLevel );
		PhysMotor_RunBrace( m, status.state );
		PhysMotor_RunStumble( m, status.state, status.balance );
		PhysMotor_RunPain( m, status.painLevel );

		if ( status.balance < 0.15f && status.state != PROCANIM_STATE_RAGDOLL ) {
			phys_event_t ev;

			Com_Memset( &ev, 0, sizeof( ev ) );
			ev.type = PHYS_EVENT_BALANCE_LOST;
			VectorCopy( status.centerOfMass, ev.point );
			ev.magnitude = 1.0f - status.balance;
			PhysEvent_Post( &ev );
		}

		PhysMotor_ApplyMotors( m );
	}
}

void PhysMotor_ApplyHit( physMotorHandle_t handle, const phys_hit_event_t *hit ) {
	physMotorSlot_t *m;
	float mag;
	int bone;

	if ( handle < 0 || handle >= PHYS_MOTOR_MAX || !motors[handle].active || !hit ) {
		return;
	}
	m = &motors[handle];
	bone = hit->bone;
	if ( bone < 0 || bone >= PHYS_MOTOR_MAX_BONES ) {
		bone = PROCANIM_BONE_SPINE;
	}

	mag = VectorLength( hit->impulse );
	m->physicsWeight[bone] = MIN( 1.0f, m->physicsWeight[bone] + hit->pain * 0.5f );

	if ( bone == PROCANIM_BONE_HEAD ) {
		PhysMotor_RunProtectHead( m, hit->pain + 0.3f );
	} else if ( bone == PROCANIM_BONE_LOWER_LEG_L || bone == PROCANIM_BONE_LOWER_LEG_R ) {
		m->physicsWeight[bone] = 0.65f;
		m->boneCmd[bone].stiffness = 0.1f;
	} else if ( mag > 400.0f ) {
		m->physicsWeight[PROCANIM_BONE_SPINE] = 0.55f;
		m->physicsWeight[PROCANIM_BONE_PELVIS] = 0.5f;
	}

	ProcAnim_ApplyImpact( m->anim, hit->point, hit->impulse, 24.0f );
}

void PhysMotor_GetStatus( physMotorHandle_t handle, phys_motor_status_t *out ) {
	procAnimStatus_t animStatus;
	int i;

	if ( handle < 0 || handle >= PHYS_MOTOR_MAX || !motors[handle].active || !out ) {
		return;
	}

	ProcAnim_GetStatus( motors[handle].anim, &animStatus );
	out->active = qtrue;
	out->animState = animStatus.state;
	out->balance = animStatus.balance;
	for ( i = 0; i < PHYS_MOTOR_MAX_BONES; i++ ) {
		out->physicsWeight[i] = motors[handle].physicsWeight[i];
		out->boneCmd[i] = motors[handle].boneCmd[i];
	}
}

void PhysMotor_SetBonePhysicsWeight( physMotorHandle_t handle, int bone, float weight ) {
	if ( handle < 0 || handle >= PHYS_MOTOR_MAX || !motors[handle].active ) {
		return;
	}
	if ( bone < 0 || bone >= PHYS_MOTOR_MAX_BONES ) {
		return;
	}
	if ( weight < 0.0f ) {
		weight = 0.0f;
	}
	if ( weight > 1.0f ) {
		weight = 1.0f;
	}
	motors[handle].physicsWeight[bone] = weight;
}
