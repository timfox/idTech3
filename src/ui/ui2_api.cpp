/*
===========================================================================
UI2 - C API Implementation
Wrapper functions for C++ implementation
===========================================================================
*/

#include "ui2_internal.h"
#include <climits>
#include <new>

// Forward declare C functions with C linkage to avoid name mangling
#ifdef __cplusplus
extern "C" {
#endif
	void *Hunk_AllocateTempMemory(int size);
	void Hunk_FreeTempMemory(void *buf);
	cvar_t *Cvar_Get(const char *var_name, const char *value, int flags);
	void Cvar_SetDescription(cvar_s *cv, const char *description);
	void Cmd_AddCommand(const char *cmd_name, void (*function)(void));
#ifdef __cplusplus
}
#endif

#include "../qcommon/qcommon.h"

#ifdef __cplusplus

extern "C" {

// Arena functions (inline implementation)
static inline void *Arena_Alloc(ui2Context_t *ctx, size_t size) {
	if (!ctx || size == 0) {
		return nullptr;
	}
	
	// Align to 8 bytes
	size = (size + 7) & ~7;
	
	if (ctx->arenaUsed + size > ctx->arenaSize) {
		Com_Printf("UI2: Arena overflow (%zu + %zu > %zu)\n", 
		          ctx->arenaUsed, size, ctx->arenaSize);
		return nullptr;
	}
	
	void *ptr = (char *)ctx->arenaBase + ctx->arenaUsed;
	ctx->arenaUsed += size;
	
	// Zero-initialize
	std::memset(ptr, 0, size);
	
	return ptr;
}

static inline void Arena_Reset(ui2Context_t *ctx) {
	if (ctx) {
		ctx->arenaUsed = 0;
	}
}

static inline qboolean Arena_Init(ui2Context_t *ctx, size_t size) {
	if (!ctx || size == 0) {
		return qfalse;
	}
	
	// Allocate from Hunk temp memory (Hunk uses int, so clamp size)
	if (size > INT_MAX) {
		size = INT_MAX;
	}
	
	ctx->arenaBase = Hunk_AllocateTempMemory((int)size);
	if (!ctx->arenaBase) {
		return qfalse;
	}
	
	ctx->arenaSize = size;
	ctx->arenaUsed = 0;
	
	return qtrue;
}

static inline void Arena_Shutdown(ui2Context_t *ctx) {
	if (ctx && ctx->arenaBase) {
		// Hunk temp memory is automatically freed at end of frame
		ctx->arenaBase = nullptr;
		ctx->arenaSize = 0;
		ctx->arenaUsed = 0;
	}
}

// CVar for enabling UI2
static cvar_t *ui2_enable = nullptr;

// Global context (singleton for now)
static ui2Context_t *g_ui2Context = nullptr;

// Forward declarations
static void UI2_ResetContext(ui2Context_t *ctx);
static qboolean UI2_ValidateRenderer(const ui2Renderer_t *renderer);

// Create UI2 context
ui2Context_t *UI2_CreateContext(const ui2Renderer_t *renderer) {
	if (!renderer) {
		Com_Printf("UI2_CreateContext: NULL renderer\n");
		return nullptr;
	}
	
	if (!UI2_ValidateRenderer(renderer)) {
		Com_Printf("UI2_CreateContext: Invalid renderer callbacks\n");
		return nullptr;
	}
	
	// Allocate context from Hunk
	ui2Context_t *ctx = (ui2Context_t *)Hunk_AllocateTempMemory(sizeof(ui2Context_t));
	if (!ctx) {
		Com_Printf("UI2_CreateContext: Failed to allocate context\n");
		return nullptr;
	}
	
	// Zero-initialize (struct has default constructors for members)
	// Use placement new with default constructor
	new(ctx) ui2Context_t();
	
	// Copy renderer callbacks
	ctx->renderer = *renderer;
	
	// Initialize arena (256KB for now)
	const size_t arenaSize = 256 * 1024;
	if (!Arena_Init(ctx, arenaSize)) {
		Com_Printf("UI2_CreateContext: Failed to initialize arena\n");
		Hunk_FreeTempMemory(ctx);
		return nullptr;
	}
	
	// Initialize string table
	ctx->stringTable.reset();
	
	// Initialize style sheet
	ctx->styleSheet.reset();
	
	// Cache default font if available
	if (renderer->GetDefaultFont) {
		ctx->defaultFont = renderer->GetDefaultFont();
	}
	
	// Reset node pool
	UI2_ResetContext(ctx);
	
	return ctx;
}

// Destroy UI2 context
void UI2_DestroyContext(ui2Context_t *ctx) {
	if (!ctx) {
		return;
	}
	
	Arena_Shutdown(ctx);
	
	// Context was allocated from Hunk temp memory, so it will be
	// automatically freed at end of frame. Just clear pointer.
	if (ctx == g_ui2Context) {
		g_ui2Context = nullptr;
	}
}

// Reset context state (for new frame)
static void UI2_ResetContext(ui2Context_t *ctx) {
	if (!ctx) {
		return;
	}
	
	// Reset node pool
	ctx->nodeCount = 0;
	ctx->rootNode = -1;
	ctx->currentNode = -1;
	
	// Reset string table (keep interned strings across frames for now)
	// ctx->stringTable.reset();
	
	// Reset arena
	Arena_Reset(ctx);
}

// Begin frame
void UI2_BeginFrame(ui2Context_t *ctx, int screenWidth, int screenHeight) {
	if (!ctx) {
		return;
	}
	
	if (ctx->inFrame) {
		Com_Printf("UI2_BeginFrame: Already in frame\n");
		return;
	}
	
	ctx->screenWidth = screenWidth;
	ctx->screenHeight = screenHeight;
	ctx->inFrame = qtrue;
	
	UI2_ResetContext(ctx);
	
	// Create root node
	if (ctx->nodeCount >= ui2Context_t::MAX_NODES) {
		Com_Printf("UI2_BeginFrame: Node pool exhausted\n");
		return;
	}
	
	UiNode *root = &ctx->nodes[ctx->nodeCount++];
	root->parent = -1;
	root->firstChild = -1;
	root->nextSibling = -1;
	root->tagId = ctx->stringTable.intern("root");
	root->style.width.value = screenWidth;
	root->style.width.unit = SizeUnit::Px;
	root->style.height.value = screenHeight;
	root->style.height.unit = SizeUnit::Px;
	root->style.display = DisplayType::Block;
	
	ctx->rootNode = 0;
	ctx->currentNode = 0;
}

// Forward declarations
extern void ComputeLayout(ui2Context_t *ctx);
extern void RenderNodes(ui2Context_t *ctx);

// End frame (compute layout and render)
void UI2_EndFrame(ui2Context_t *ctx) {
	if (!ctx || !ctx->inFrame) {
		return;
	}
	
	// Compute layout
	ComputeLayout(ctx);
	
	// Render nodes
	RenderNodes(ctx);
	
	ctx->inFrame = qfalse;
}

// Load stylesheet (implemented in ui2_style_parser.cpp)
extern qboolean UI2_LoadStylesheet(ui2Context_t *ctx, const char *cssText);

// Begin node
void UI2_BeginNode(ui2Context_t *ctx, const char *tag, const char *className) {
	if (!ctx || !ctx->inFrame || !tag) {
		return;
	}
	
	if (ctx->nodeCount >= ui2Context_t::MAX_NODES) {
		Com_Printf("UI2_BeginNode: Node pool exhausted\n");
		return;
	}
	
	// Create new node
	UiNode *node = &ctx->nodes[ctx->nodeCount++];
	node->parent = ctx->currentNode;
	node->firstChild = -1;
	node->nextSibling = -1;
	node->tagId = ctx->stringTable.intern(tag);
	node->classNameId = className ? ctx->stringTable.intern(className) : 0;
	
	// Add to parent's children
	if (ctx->currentNode >= 0) {
		UiNode *parent = &ctx->nodes[ctx->currentNode];
		if (parent->firstChild < 0) {
			parent->firstChild = ctx->nodeCount - 1;
		} else {
			// Find last sibling
			int32_t sibling = parent->firstChild;
			while (ctx->nodes[sibling].nextSibling >= 0) {
				sibling = ctx->nodes[sibling].nextSibling;
			}
			ctx->nodes[sibling].nextSibling = ctx->nodeCount - 1;
		}
		parent->childCount++;
	}
	
	// Apply styles from stylesheet (will be applied during layout)
	
	// Set as current node
	ctx->currentNode = ctx->nodeCount - 1;
}

// End node
void UI2_EndNode(ui2Context_t *ctx) {
	if (!ctx || !ctx->inFrame || ctx->currentNode < 0) {
		return;
	}
	
	// Move back to parent
	UiNode *node = &ctx->nodes[ctx->currentNode];
	ctx->currentNode = node->parent;
}

// Text content
void UI2_Text(ui2Context_t *ctx, const char *tag, const char *text) {
	if (!ctx || !ctx->inFrame || !tag || !text) {
		return;
	}
	
	// Create text node
	UI2_BeginNode(ctx, tag, nullptr);
	
	if (ctx->currentNode >= 0) {
		UiNode *node = &ctx->nodes[ctx->currentNode];
		node->isText = true;
		node->text = text;  // TODO: Copy to arena
	}
	
	UI2_EndNode(ctx);
}

// Validate renderer callbacks
static qboolean UI2_ValidateRenderer(const ui2Renderer_t *renderer) {
	if (!renderer) {
		return qfalse;
	}
	
	if (!renderer->SetColor || !renderer->DrawStretchPic) {
		return qfalse;
	}
	
	// Scissor is optional
	return qtrue;
}

// Forward declaration
extern void UI2_RegisterTestCommand(void);

// Initialize UI2 system
void UI2_Init(void) {
	ui2_enable = Cvar_Get("ui2_enable", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(ui2_enable, "Enable UI2 deterministic layout system (0 = disabled, 1 = enabled)");
	
	// Register test command
	UI2_RegisterTestCommand();
	
	Com_Printf("UI2 system initialized\n");
}

// Shutdown UI2 system
void UI2_Shutdown(void) {
	if (g_ui2Context) {
		UI2_DestroyContext(g_ui2Context);
		g_ui2Context = nullptr;
	}
	
	Com_Printf("UI2 system shut down\n");
}

// Check if UI2 is enabled
qboolean UI2_IsEnabled(void) {
	return ui2_enable && ui2_enable->integer != 0 ? qtrue : qfalse;
}

} // extern "C"

#endif // __cplusplus
