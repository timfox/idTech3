#include "q_shared.h"
#include "qcommon.h"
#include "dvar.h"

#define MAX_DVARS 512
#define HASH_SIZE 256

static dvar_t *dvar_list = NULL;
static dvar_t dvar_pool[MAX_DVARS];
static int dvar_count = 0;
static mutex_t dvar_mutex;
static qboolean dvar_initialized = qfalse;

// Hash table for fast lookups
static dvar_t *dvar_hashTable[HASH_SIZE];

// Forward declarations
static unsigned int Dvar_GenerateHash(const char *name);
static qboolean Dvar_ValidateName(const char *name);
static qboolean Dvar_ValidateValue(dvar_t *dvar, const char *value);
static void Dvar_SetLatched(dvar_t *dvar, const char *value);
static dvar_t *Dvar_Alloc(void);

/*
================
Dvar_GenerateHash
================
*/
static unsigned int Dvar_GenerateHash(const char *name) {
	unsigned int hash = 0;
	int i = 0;
	char c;

	while ((c = name[i++]) != '\0') {
		hash = hash * 33 + tolower(c);
	}

	return hash % HASH_SIZE;
}

/*
================
Dvar_ValidateName
================
*/
static qboolean Dvar_ValidateName(const char *name) {
	const char *s;
	int c;

	if (!name || !name[0]) {
		return qfalse;
	}

	s = name;
	while ((c = *s++) != '\0') {
		if (c == '\\' || c == '\"' || c == ';' || c == '%' || c <= ' ' || c >= '~') {
			return qfalse;
		}
	}

	if ((s - name) >= MAX_STRING_CHARS) {
		return qfalse;
	}

	return qtrue;
}

/*
================
Dvar_ValidateValue
================
*/
static qboolean Dvar_ValidateValue(dvar_t *dvar, const char *value) {
	if (!dvar || !value) {
		return qfalse;
	}

	switch (dvar->type) {
	case DVAR_TYPE_BOOL:
		return (Q_stricmp(value, "0") == 0 || Q_stricmp(value, "1") == 0 ||
		        Q_stricmp(value, "false") == 0 || Q_stricmp(value, "true") == 0);

	case DVAR_TYPE_INT:
		{
			int val = atoi(value);
			if (dvar->domain.min != dvar->domain.max) {
				return (val >= dvar->domain.min && val <= dvar->domain.max);
			}
			return qtrue;
		}

	case DVAR_TYPE_FLOAT:
		{
			float val = atof(value);
			if (dvar->domain.min != dvar->domain.max) {
				return (val >= dvar->domain.min && val <= dvar->domain.max);
			}
			return qtrue;
		}

	case DVAR_TYPE_VEC2:
	case DVAR_TYPE_VEC3:
	case DVAR_TYPE_VEC4:
	case DVAR_TYPE_COLOR_RGB:
	case DVAR_TYPE_COLOR_RGBA:
		// Vector validation would be more complex, for now accept any valid float values
		return qtrue;

	case DVAR_TYPE_ENUM:
		if (dvar->enumList) {
			const dvarEnum_t *enumVal = dvar->enumList;
			while (enumVal->name) {
				if (Q_stricmp(value, enumVal->name) == 0) {
					return qtrue;
				}
				enumVal++;
			}
		}
		return qfalse;

	case DVAR_TYPE_STRING:
	default:
		return (strlen(value) < MAX_STRING_CHARS);
	}
}

/*
================
Dvar_Alloc
================
*/
static dvar_t *Dvar_Alloc(void) {
	if (dvar_count >= MAX_DVARS) {
		Com_Error(ERR_DROP, "Dvar_Alloc: MAX_DVARS exceeded");
		return NULL;
	}

	dvar_t *dvar = &dvar_pool[dvar_count++];
	memset(dvar, 0, sizeof(dvar_t));

	return dvar;
}

/*
================
Dvar_SetLatched
================
*/
static void Dvar_SetLatched(dvar_t *dvar, const char *value) {
	if (!dvar || !value) {
		return;
	}

	// Free existing latched string if it exists
	if (dvar->latched.s && dvar->type == DVAR_TYPE_STRING) {
		Z_Free(dvar->latched.s);
	}

	switch (dvar->type) {
	case DVAR_TYPE_BOOL:
		dvar->latched.b = atoi(value) != 0;
		break;
	case DVAR_TYPE_INT:
		dvar->latched.i = atoi(value);
		break;
	case DVAR_TYPE_FLOAT:
		dvar->latched.f = atof(value);
		break;
	case DVAR_TYPE_STRING:
		dvar->latched.s = CopyString(value);
		break;
	case DVAR_TYPE_VEC2:
		sscanf(value, "%f %f", &dvar->latched.v2[0], &dvar->latched.v2[1]);
		break;
	case DVAR_TYPE_VEC3:
		sscanf(value, "%f %f %f", &dvar->latched.v3[0], &dvar->latched.v3[1], &dvar->latched.v3[2]);
		break;
	case DVAR_TYPE_VEC4:
	case DVAR_TYPE_COLOR_RGBA:
		sscanf(value, "%f %f %f %f", &dvar->latched.v4[0], &dvar->latched.v4[1], &dvar->latched.v4[2], &dvar->latched.v4[3]);
		break;
	case DVAR_TYPE_COLOR_RGB:
		sscanf(value, "%f %f %f", &dvar->latched.v3[0], &dvar->latched.v3[1], &dvar->latched.v3[2]);
		break;
	case DVAR_TYPE_ENUM:
		dvar->latched.i = atoi(value);
		break;
	default:
		break;
	}
}

/*
================
Dvar_IsDeveloperMode
================
*/
qboolean Dvar_IsDeveloperMode(void) {
	const char *dev = Cvar_VariableString("developer");
	return (dev && atoi(dev) != 0);
}

/*
================
Dvar_Init
================
*/
void Dvar_Init(void) {
	if (dvar_initialized) {
		return;
	}

	MUTEX_INIT(dvar_mutex);
	dvar_list = NULL;
	dvar_count = 0;
	memset(dvar_pool, 0, sizeof(dvar_pool));
	memset(dvar_hashTable, 0, sizeof(dvar_hashTable));

	dvar_initialized = qtrue;

	// Register console commands
	Cmd_AddCommand("dvar", Dvar_Command);
	Cmd_AddCommand("dvarlist", Dvar_List);
	Cmd_AddCommand("dvardump", Dvar_Dump);

	Com_Printf("Developer variable system initialized\n");
}

/*
================
Dvar_Shutdown
================
*/
void Dvar_Shutdown(void) {
	if (!dvar_initialized) {
		return;
	}

	MUTEX_LOCK(dvar_mutex);

	// Free all allocated strings
	dvar_t *dvar = dvar_list;
	while (dvar) {
		if (dvar->type == DVAR_TYPE_STRING) {
			if (dvar->current.s) Z_Free(dvar->current.s);
			if (dvar->reset.s) Z_Free(dvar->reset.s);
			if (dvar->latched.s) Z_Free(dvar->latched.s);
		}
		dvar = dvar->next;
	}

	MUTEX_UNLOCK(dvar_mutex);
	MUTEX_DESTROY(dvar_mutex);

	// Remove console commands
	Cmd_RemoveCommand("dvar");
	Cmd_RemoveCommand("dvarlist");
	Cmd_RemoveCommand("dvardump");

	dvar_initialized = qfalse;
}

/*
================
Dvar_Get
================
*/
dvar_t *Dvar_Get(const char *name, dvarType_t type, int flags) {
	if (!dvar_initialized) {
		Dvar_Init();
	}

	if (!Dvar_ValidateName(name)) {
		Com_Printf("Dvar_Get: invalid name '%s'\n", name);
		return NULL;
	}

	MUTEX_LOCK(dvar_mutex);

	// Check if it already exists
	dvar_t *dvar = Dvar_Find(name);
	if (dvar) {
		// Type must match for existing dvars
		if (dvar->type != type) {
			Com_Printf("Dvar_Get: type mismatch for '%s'\n", name);
			MUTEX_UNLOCK(dvar_mutex);
			return NULL;
		}

		// Update flags
		dvar->flags |= flags;

		MUTEX_UNLOCK(dvar_mutex);
		return dvar;
	}

	// Check developer mode requirement
	if ((flags & DVAR_DEVELOPER) && !Dvar_IsDeveloperMode()) {
		MUTEX_UNLOCK(dvar_mutex);
		return NULL;
	}

	// Allocate new dvar
	dvar = Dvar_Alloc();
	if (!dvar) {
		MUTEX_UNLOCK(dvar_mutex);
		return NULL;
	}

	// Initialize dvar
	dvar->name = CopyString(name);
	dvar->description = CopyString("Developer variable");
	dvar->type = type;
	dvar->flags = flags;
	dvar->domain.min = 0.0f;
	dvar->domain.max = 0.0f;
	dvar->enumList = NULL;

	// Set default values based on type
	switch (type) {
	case DVAR_TYPE_BOOL:
		dvar->current.b = qfalse;
		dvar->reset.b = qfalse;
		break;
	case DVAR_TYPE_INT:
		dvar->current.i = 0;
		dvar->reset.i = 0;
		break;
	case DVAR_TYPE_FLOAT:
		dvar->current.f = 0.0f;
		dvar->reset.f = 0.0f;
		break;
	case DVAR_TYPE_VEC2:
		Vector2Set(dvar->current.v2, 0.0f, 0.0f);
		Vector2Set(dvar->reset.v2, 0.0f, 0.0f);
		break;
	case DVAR_TYPE_VEC3:
	case DVAR_TYPE_COLOR_RGB:
		VectorSet(dvar->current.v3, 0.0f, 0.0f, 0.0f);
		VectorSet(dvar->reset.v3, 0.0f, 0.0f, 0.0f);
		break;
	case DVAR_TYPE_VEC4:
	case DVAR_TYPE_COLOR_RGBA:
		Vector4Set(dvar->current.v4, 0.0f, 0.0f, 0.0f, 1.0f);
		Vector4Set(dvar->reset.v4, 0.0f, 0.0f, 0.0f, 1.0f);
		break;
	case DVAR_TYPE_STRING:
		dvar->current.s = CopyString("");
		dvar->reset.s = CopyString("");
		break;
	case DVAR_TYPE_ENUM:
		dvar->current.i = 0;
		dvar->reset.i = 0;
		break;
	default:
		break;
	}

	// Add to linked list
	dvar->next = dvar_list;
	if (dvar_list) {
		dvar_list->prev = dvar;
	}
	dvar_list = dvar;

	// Add to hash table
	unsigned int hash = Dvar_GenerateHash(name);
	dvar->next = dvar_hashTable[hash];
	dvar_hashTable[hash] = dvar;

	MUTEX_UNLOCK(dvar_mutex);
	return dvar;
}

/*
================
Dvar_Find
================
*/
dvar_t *Dvar_Find(const char *name) {
	if (!name) {
		return NULL;
	}

	unsigned int hash = Dvar_GenerateHash(name);
	dvar_t *dvar = dvar_hashTable[hash];

	while (dvar) {
		if (Q_stricmp(dvar->name, name) == 0) {
			return dvar;
		}
		dvar = dvar->next;
	}

	return NULL;
}

/*
================
Dvar_SetString
================
*/
void Dvar_SetString(dvar_t *dvar, const char *value) {
	if (!dvar || !value) {
		return;
	}

	if (!Dvar_ValidateValue(dvar, value)) {
		Com_Printf("Dvar_SetString: invalid value '%s' for dvar '%s'\n", value, dvar->name);
		return;
	}

	MUTEX_LOCK(dvar_mutex);

	if (dvar->flags & DVAR_LATCH) {
		Dvar_SetLatched(dvar, value);
	} else {
		// Free existing string
		if (dvar->type == DVAR_TYPE_STRING && dvar->current.s) {
			Z_Free(dvar->current.s);
		}

		switch (dvar->type) {
		case DVAR_TYPE_BOOL:
			dvar->current.b = atoi(value) != 0;
			break;
		case DVAR_TYPE_INT:
			dvar->current.i = atoi(value);
			break;
		case DVAR_TYPE_FLOAT:
			dvar->current.f = atof(value);
			break;
		case DVAR_TYPE_VEC2:
			sscanf(value, "%f %f", &dvar->current.v2[0], &dvar->current.v2[1]);
			break;
		case DVAR_TYPE_VEC3:
		case DVAR_TYPE_COLOR_RGB:
			sscanf(value, "%f %f %f", &dvar->current.v3[0], &dvar->current.v3[1], &dvar->current.v3[2]);
			break;
		case DVAR_TYPE_VEC4:
		case DVAR_TYPE_COLOR_RGBA:
			sscanf(value, "%f %f %f %f", &dvar->current.v4[0], &dvar->current.v4[1], &dvar->current.v4[2], &dvar->current.v4[3]);
			break;
		case DVAR_TYPE_STRING:
			dvar->current.s = CopyString(value);
			break;
		case DVAR_TYPE_ENUM:
			dvar->current.i = atoi(value);
			break;
		default:
			break;
		}

		atomic_store(&dvar->modified, 1);
		atomic_fetch_add(&dvar->modificationCount, 1);
	}

	MUTEX_UNLOCK(dvar_mutex);
}

/*
================
Dvar_SetBool
================
*/
void Dvar_SetBool(dvar_t *dvar, qboolean value) {
	char buf[16];
	Com_sprintf(buf, sizeof(buf), "%d", value ? 1 : 0);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_SetInt
================
*/
void Dvar_SetInt(dvar_t *dvar, int value) {
	char buf[32];
	Com_sprintf(buf, sizeof(buf), "%d", value);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_SetFloat
================
*/
void Dvar_SetFloat(dvar_t *dvar, float value) {
	char buf[32];
	Com_sprintf(buf, sizeof(buf), "%.6f", value);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_SetVec2
================
*/
void Dvar_SetVec2(dvar_t *dvar, float x, float y) {
	char buf[64];
	Com_sprintf(buf, sizeof(buf), "%.6f %.6f", x, y);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_SetVec3
================
*/
void Dvar_SetVec3(dvar_t *dvar, float x, float y, float z) {
	char buf[96];
	Com_sprintf(buf, sizeof(buf), "%.6f %.6f %.6f", x, y, z);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_SetVec4
================
*/
void Dvar_SetVec4(dvar_t *dvar, float x, float y, float z, float w) {
	char buf[128];
	Com_sprintf(buf, sizeof(buf), "%.6f %.6f %.6f %.6f", x, y, z, w);
	Dvar_SetString(dvar, buf);
}

/*
================
Dvar_GetString
================
*/
const char *Dvar_GetString(dvar_t *dvar) {
	if (!dvar) {
		return "";
	}

	// Return latched value if it exists
	if ((dvar->flags & DVAR_LATCH) && dvar->latched.s) {
		return dvar->latched.s;
	}

	switch (dvar->type) {
	case DVAR_TYPE_BOOL:
		return dvar->current.b ? "1" : "0";
	case DVAR_TYPE_INT:
		{
			static char buf[32];
			Com_sprintf(buf, sizeof(buf), "%d", dvar->current.i);
			return buf;
		}
	case DVAR_TYPE_FLOAT:
		{
			static char buf[32];
			Com_sprintf(buf, sizeof(buf), "%.6f", dvar->current.f);
			return buf;
		}
	case DVAR_TYPE_VEC2:
		{
			static char buf[64];
			Com_sprintf(buf, sizeof(buf), "%.6f %.6f", dvar->current.v2[0], dvar->current.v2[1]);
			return buf;
		}
	case DVAR_TYPE_VEC3:
	case DVAR_TYPE_COLOR_RGB:
		{
			static char buf[96];
			Com_sprintf(buf, sizeof(buf), "%.6f %.6f %.6f", dvar->current.v3[0], dvar->current.v3[1], dvar->current.v3[2]);
			return buf;
		}
	case DVAR_TYPE_VEC4:
	case DVAR_TYPE_COLOR_RGBA:
		{
			static char buf[128];
			Com_sprintf(buf, sizeof(buf), "%.6f %.6f %.6f %.6f", dvar->current.v4[0], dvar->current.v4[1], dvar->current.v4[2], dvar->current.v4[3]);
			return buf;
		}
	case DVAR_TYPE_STRING:
		return dvar->current.s ? dvar->current.s : "";
	case DVAR_TYPE_ENUM:
		{
			static char buf[32];
			Com_sprintf(buf, sizeof(buf), "%d", dvar->current.i);
			return buf;
		}
	default:
		return "";
	}
}

/*
================
Dvar_GetBool
================
*/
qboolean Dvar_GetBool(dvar_t *dvar) {
	if (!dvar) {
		return qfalse;
	}

	if ((dvar->flags & DVAR_LATCH) && dvar->type == DVAR_TYPE_BOOL) {
		return dvar->latched.b;
	}

	return dvar->current.b;
}

/*
================
Dvar_GetInt
================
*/
int Dvar_GetInt(dvar_t *dvar) {
	if (!dvar) {
		return 0;
	}

	if (dvar->flags & DVAR_LATCH) {
		switch (dvar->type) {
		case DVAR_TYPE_INT:
		case DVAR_TYPE_ENUM:
			return dvar->latched.i;
		default:
			return 0;
		}
	}

	return dvar->current.i;
}

/*
================
Dvar_GetFloat
================
*/
float Dvar_GetFloat(dvar_t *dvar) {
	if (!dvar) {
		return 0.0f;
	}

	if ((dvar->flags & DVAR_LATCH) && dvar->type == DVAR_TYPE_FLOAT) {
		return dvar->latched.f;
	}

	return dvar->current.f;
}

/*
================
Dvar_GetVec2
================
*/
void Dvar_GetVec2(dvar_t *dvar, vec2_t result) {
	if (!dvar || !result) {
		return;
	}

	if ((dvar->flags & DVAR_LATCH) && dvar->type == DVAR_TYPE_VEC2) {
		Vector2Copy(dvar->latched.v2, result);
	} else {
		Vector2Copy(dvar->current.v2, result);
	}
}

/*
================
Dvar_GetVec3
================
*/
void Dvar_GetVec3(dvar_t *dvar, vec3_t result) {
	if (!dvar || !result) {
		return;
	}

	if ((dvar->flags & DVAR_LATCH) && (dvar->type == DVAR_TYPE_VEC3 || dvar->type == DVAR_TYPE_COLOR_RGB)) {
		VectorCopy(dvar->latched.v3, result);
	} else {
		VectorCopy(dvar->current.v3, result);
	}
}

/*
================
Dvar_GetVec4
================
*/
void Dvar_GetVec4(dvar_t *dvar, vec4_t result) {
	if (!dvar || !result) {
		return;
	}

	if ((dvar->flags & DVAR_LATCH) && (dvar->type == DVAR_TYPE_VEC4 || dvar->type == DVAR_TYPE_COLOR_RGBA)) {
		Vector4Copy(dvar->latched.v4, result);
	} else {
		Vector4Copy(dvar->current.v4, result);
	}
}

/*
================
Dvar_Reset
================
*/
void Dvar_Reset(dvar_t *dvar) {
	if (!dvar) {
		return;
	}

	MUTEX_LOCK(dvar_mutex);

	// Copy reset values to current
	switch (dvar->type) {
	case DVAR_TYPE_BOOL:
		dvar->current.b = dvar->reset.b;
		break;
	case DVAR_TYPE_INT:
	case DVAR_TYPE_ENUM:
		dvar->current.i = dvar->reset.i;
		break;
	case DVAR_TYPE_FLOAT:
		dvar->current.f = dvar->reset.f;
		break;
	case DVAR_TYPE_VEC2:
		Vector2Copy(dvar->reset.v2, dvar->current.v2);
		break;
	case DVAR_TYPE_VEC3:
	case DVAR_TYPE_COLOR_RGB:
		VectorCopy(dvar->reset.v3, dvar->current.v3);
		break;
	case DVAR_TYPE_VEC4:
	case DVAR_TYPE_COLOR_RGBA:
		Vector4Copy(dvar->reset.v4, dvar->current.v4);
		break;
	case DVAR_TYPE_STRING:
		if (dvar->current.s) {
			Z_Free(dvar->current.s);
		}
		dvar->current.s = CopyString(dvar->reset.s);
		break;
	default:
		break;
	}

	atomic_store(&dvar->modified, 1);
	atomic_fetch_add(&dvar->modificationCount, 1);

	MUTEX_UNLOCK(dvar_mutex);
}

/*
================
Dvar_SetModified
================
*/
void Dvar_SetModified(dvar_t *dvar) {
	if (dvar) {
		atomic_store(&dvar->modified, 1);
		atomic_fetch_add(&dvar->modificationCount, 1);
	}
}

/*
================
Dvar_Command
================
*/
void Dvar_Command(void) {
	const char *cmd = Cmd_Argv(0);

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <dvar_name> [value]\n", cmd);
		return;
	}

	const char *name = Cmd_Argv(1);
	dvar_t *dvar = Dvar_Find(name);

	if (!dvar) {
		Com_Printf("Unknown dvar '%s'\n", name);
		return;
	}

	if (Cmd_Argc() == 2) {
		// Display current value
		Com_Printf("%s = \"%s\"\n", dvar->name, Dvar_GetString(dvar));
	} else {
		// Set new value
		const char *value = Cmd_ArgsFrom(2);
		Dvar_SetString(dvar, value);
		Com_Printf("%s = \"%s\"\n", dvar->name, Dvar_GetString(dvar));
	}
}

/*
================
Dvar_List
================
*/
void Dvar_List(void) {
	MUTEX_LOCK(dvar_mutex);

	int count = 0;
	dvar_t *dvar = dvar_list;

	Com_Printf("\nDeveloper Variables:\n");
	Com_Printf("------------------\n");

	while (dvar) {
		Com_Printf("%-32s = %s\n", dvar->name, Dvar_GetString(dvar));
		count++;
		dvar = dvar->next;
	}

	Com_Printf("\nTotal: %d dvars\n", count);
	MUTEX_UNLOCK(dvar_mutex);
}

/*
================
Dvar_Dump
================
*/
void Dvar_Dump(void) {
	MUTEX_LOCK(dvar_mutex);

	dvar_t *dvar = dvar_list;

	Com_Printf("\nDeveloper Variable Dump:\n");
	Com_Printf("========================\n");

	while (dvar) {
		Com_Printf("Name: %s\n", dvar->name);
		Com_Printf("Type: %d\n", dvar->type);
		Com_Printf("Flags: 0x%08x\n", dvar->flags);
		Com_Printf("Value: %s\n", Dvar_GetString(dvar));
		Com_Printf("Modified: %d\n", atomic_load(&dvar->modified));
		Com_Printf("Modification Count: %d\n", atomic_load(&dvar->modificationCount));
		Com_Printf("---\n");

		dvar = dvar->next;
	}

	MUTEX_UNLOCK(dvar_mutex);
}