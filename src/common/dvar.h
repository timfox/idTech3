#ifndef __DVAR_H__
#define __DVAR_H__

#include "q_shared.h"

// Developer variable types
typedef enum {
	DVAR_TYPE_BOOL,
	DVAR_TYPE_FLOAT,
	DVAR_TYPE_VEC2,
	DVAR_TYPE_VEC3,
	DVAR_TYPE_VEC4,
	DVAR_TYPE_INT,
	DVAR_TYPE_ENUM,
	DVAR_TYPE_STRING,
	DVAR_TYPE_COLOR_RGB,
	DVAR_TYPE_COLOR_RGBA,
	DVAR_TYPE_MAX
} dvarType_t;

// Developer variable flags
#define DVAR_ARCHIVE			0x0001	// Save to config
#define DVAR_USERINFO			0x0002	// Send to server on connect/change
#define DVAR_SERVERINFO			0x0004	// Server broadcasts
#define DVAR_INIT				0x0010	// Can only be set at startup
#define DVAR_LATCH				0x0020	// Takes effect on restart
#define DVAR_READONLY			0x0040	// Cannot be modified
#define DVAR_CHEAT				0x0200	// Only works with cheats enabled
#define DVAR_TEMP				0x0400	// Temporary, not saved
#define DVAR_DEVELOPER			0x10000	// Only available in developer mode

// Domain limits for variables
typedef struct {
	float min;
	float max;
} dvarLimits_t;

// String enumeration for DVAR_TYPE_ENUM
typedef struct {
	const char *name;
	int value;
} dvarEnum_t;

// Developer variable structure
typedef struct dvar_s {
	char *name;
	char *description;
	dvarType_t type;
	int flags;

	// Value storage (union for different types)
	union {
		qboolean b;
		float f;
		int i;
		char *s;
		vec2_t v2;
		vec3_t v3;
		vec4_t v4;
	} current;

	// Default/reset values
	union {
		qboolean b;
		float f;
		int i;
		char *s;
		vec2_t v2;
		vec3_t v3;
		vec4_t v4;
	} reset;

	// Latched values (for restart-required changes)
	union {
		qboolean b;
		float f;
		int i;
		char *s;
		vec2_t v2;
		vec3_t v3;
		vec4_t v4;
	} latched;

	// Domain limits and validation
	dvarLimits_t domain;
	const dvarEnum_t *enumList;

	// Modification tracking
	atomic_int_t modified;
	atomic_int_t modificationCount;

	// Linked list
	struct dvar_s *next;
	struct dvar_s *prev;
} dvar_t;

// Public API functions
dvar_t *Dvar_Get(const char *name, dvarType_t type, int flags);
void Dvar_SetString(dvar_t *dvar, const char *value);
void Dvar_SetBool(dvar_t *dvar, qboolean value);
void Dvar_SetInt(dvar_t *dvar, int value);
void Dvar_SetFloat(dvar_t *dvar, float value);
void Dvar_SetVec2(dvar_t *dvar, float x, float y);
void Dvar_SetVec3(dvar_t *dvar, float x, float y, float z);
void Dvar_SetVec4(dvar_t *dvar, float x, float y, float z, float w);

// Get current values
const char *Dvar_GetString(dvar_t *dvar);
qboolean Dvar_GetBool(dvar_t *dvar);
int Dvar_GetInt(dvar_t *dvar);
float Dvar_GetFloat(dvar_t *dvar);
void Dvar_GetVec2(dvar_t *dvar, vec2_t result);
void Dvar_GetVec3(dvar_t *dvar, vec3_t result);
void Dvar_GetVec4(dvar_t *dvar, vec4_t result);

// Utility functions
dvar_t *Dvar_Find(const char *name);
void Dvar_Reset(dvar_t *dvar);
void Dvar_SetModified(dvar_t *dvar);
qboolean Dvar_IsDeveloperMode(void);

// Initialization and shutdown
void Dvar_Init(void);
void Dvar_Shutdown(void);

// Console command integration
void Dvar_Command(void);
void Dvar_List(void);
void Dvar_Dump(void);

#endif // __DVAR_H__