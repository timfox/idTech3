#include "q_shared.h"
#include "qcommon.h"
#include "dvar.h"

dvar_t *dvar_vars = NULL;
static dvar_t *dvar_cheats;
static dvar_t *dvar_developer;
int dvar_modifiedFlags;

static mutex_t dvar_mutex;
static qboolean dvar_initialized = qfalse;

#define MAX_DVARS 2048
static dvar_t dvar_indexes[MAX_DVARS];
static int dvar_numIndexes;

static int dvar_group[ CVG_MAX ];

#define FILE_HASH_SIZE 256