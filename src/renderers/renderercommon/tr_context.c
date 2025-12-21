/*
===========================================================================
Context-Aware Renderer API

Renderer functions that accept a context parameter instead of relying on
global state, enabling better testing and decoupling.
===========================================================================
*/

#include "tr_public.h"
#include "../../common/q_shared.h"

// Minimal model_t definition for context functions
typedef enum {
	MOD_BAD = 0,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MD4,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct model_s {
	char name[MAX_QPATH];
	modtype_t type;
	int index;
	int numLods;
} model_t;

// Forward declarations
extern refimport_t ri;
extern refexport_t re;
extern trGlobals_t tr;

// imgFlags_t type for context functions
typedef int imgFlags_t;

// Stub function removed - was unused

// Context-aware model management
// static qhandle_t R_Context_RegisterModel(renderer_context_t *ctx, const char *name) {
// 	// Use context's globals if available, otherwise fall back to global
// 	trGlobals_t *tr_ctx = ctx->globals ? ctx->globals : &tr;
// 
// 	if (!name || !name[0]) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_ALL, "R_Context_RegisterModel: NULL name\n");
// 		}
// 		return 0;
// 	}
// 
// 	// Check if already loaded
// 	for (int i = 1; i < tr_ctx->numModels; i++) {
// 		model_t *mod = tr_ctx->models[i];
// 		if (mod && !Q_stricmp(mod->name, name)) {
// 			return i;
// 		}
// 	}
// 
// 	// Allocate new model
// 	if (tr_ctx->numModels >= MAX_MOD_KNOWN) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_WARNING, "R_Context_RegisterModel: MAX_MOD_KNOWN hit\n");
// 		}
// 		return 0;
// 	}
// 
// 	model_t *mod;
// 	if (ctx->services && ctx->services->Hunk_Alloc) {
// 		mod = (model_t *)ctx->services->Hunk_Alloc(sizeof(*tr_ctx->models[tr_ctx->numModels]), h_low);
// 	} else {
// 		mod = (model_t *)ri.Hunk_Alloc(sizeof(*tr_ctx->models[tr_ctx->numModels]), h_low);
// 	}
// 
// 	if (!mod) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_WARNING, "R_Context_RegisterModel: Hunk_Alloc failed\n");
// 		}
// 		return 0;
// 	}
// 
// 	mod->index = tr_ctx->numModels;
// 	Q_strncpyz(mod->name, name, sizeof(mod->name));
// 	mod->type = MOD_BAD;
// 	mod->numLods = 0;
// 
// 	tr_ctx->models[tr_ctx->numModels] = mod;
// 	tr_ctx->numModels++;
// 
// 	return mod->index;
// }

// static model_t *R_Context_GetModelByHandle(renderer_context_t *ctx, qhandle_t handle) {
// 	trGlobals_t *tr_ctx = ctx->globals ? ctx->globals : &tr;
// 
// 	if (handle < 1 || handle >= tr_ctx->numModels) {
// 		return tr_ctx->models[0]; // Return default model
// 	}
// 
// 	return tr_ctx->models[handle];
// }

// Context-aware rendering functions removed - not currently used

// Context-aware API table - commented out due to API signature mismatches
// static const context_aware_renderer_api_t context_api = { };

// Context-aware versions of core renderer functions
// model_t *R_GetModelByHandle_Context(renderer_context_t *ctx, qhandle_t handle) {
// 	trGlobals_t *tr_ctx = ctx->globals ? ctx->globals : &tr;
// 
// 	// out of range gets the default model
// 	if (handle < 1 || handle >= tr_ctx->numModels) {
// 		return tr_ctx->models[0];
// 	}
// 
// 	return tr_ctx->models[handle];
// }

// model_t *R_AllocModel_Context(renderer_context_t *ctx) {
// 	trGlobals_t *tr_ctx = ctx->globals ? ctx->globals : &tr;
// 
// 	if (tr_ctx->numModels >= MAX_MOD_KNOWN) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_WARNING, "R_AllocModel: MAX_MOD_KNOWN hit\n");
// 		}
// 		return NULL;
// 	}
// 
// 	model_t *mod;
// 	if (ctx->services && ctx->services->Hunk_Alloc) {
// 		mod = (model_t *)ctx->services->Hunk_Alloc(sizeof(*tr_ctx->models[tr_ctx->numModels]), h_low);
// 	} else {
// 		mod = (model_t *)ri.Hunk_Alloc(sizeof(*tr_ctx->models[tr_ctx->numModels]), h_low);
// 	}
// 
// 	if (!mod) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_WARNING, "R_AllocModel: Hunk_Alloc failed\n");
// 		}
// 		return NULL;
// 	}
// 
// 	mod->index = tr_ctx->numModels;
// 	tr_ctx->models[tr_ctx->numModels] = mod;
// 	tr_ctx->numModels++;
// 
// 	return mod;
// }

// qhandle_t R_RegisterModel_Context(renderer_context_t *ctx, const char *name) {
// 	trGlobals_t *tr_ctx = ctx->globals ? ctx->globals : &tr;
// 
// 	if (!name || !name[0]) {
// 		if (ctx->services && ctx->services->Printf) {
// 			ctx->services->Printf(PRINT_ALL, "R_RegisterModel: NULL name\n");
// 		}
// 		return 0;
// 	}
// 
// 	// Check if already loaded
// 	for (int i = 1; i < tr_ctx->numModels; i++) {
// 		model_t *mod = tr_ctx->models[i];
// 		if (mod && !Q_stricmp(mod->name, name)) {
// 			return i;
// 		}
// 	}
// 
// 	// Allocate new model
// 	model_t *mod = R_AllocModel_Context(ctx);
// 	if (!mod) {
// 		return 0;
// 	}
// 
// 	// Initialize the model
// 	Q_strncpyz(mod->name, name, sizeof(mod->name));
// 	mod->type = MOD_BAD;
// 	mod->numLods = 0;
// 
// 	return mod->index;
// }

// Get the context-aware renderer API
// const context_aware_renderer_api_t *R_GetContextAwareAPI(void) {
// 	return &context_api;
// }
