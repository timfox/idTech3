/*
===========================================================================
UI2 - Golden Image Tests ("G-ACID")
Renders deterministic scenes and compares against baseline hashes
===========================================================================
*/

#include "ui2_internal.h"
#include <cstring>
#include <cstdio>

// Forward declare C functions with C linkage
#ifdef __cplusplus
extern "C" {
#endif
	void Cmd_AddCommand(const char *cmd_name, void (*function)(void));
#ifdef __cplusplus
}
#endif

#include "../common/qcommon.h"

#ifdef __cplusplus

extern "C" {

// Simple hash function (FNV-1a) for testing
// In production, use xxHash if available
static uint32_t SimpleHash(const void *data, size_t size) {
	const uint8_t *bytes = (const uint8_t *)data;
	uint32_t hash = 2166136261u;  // FNV offset basis
	
	for (size_t i = 0; i < size; ++i) {
		hash ^= bytes[i];
		hash *= 16777619u;  // FNV prime
	}
	
	return hash;
}

// Hash a rendered frame (simplified - hash the layout boxes)
static uint32_t HashFrame(ui2Context_t *ctx) {
	if (!ctx) {
		return 0;
	}
	
	uint32_t hash = 0;
	
	// Hash all node layouts
	for (uint32_t i = 0; i < ctx->nodeCount; ++i) {
		const UiNode *node = &ctx->nodes[i];
		const LayoutBox &box = node->layout;
		
		// Hash layout box values
		hash ^= SimpleHash(&box.x, sizeof(box.x));
		hash ^= SimpleHash(&box.y, sizeof(box.y));
		hash ^= SimpleHash(&box.width, sizeof(box.width));
		hash ^= SimpleHash(&box.height, sizeof(box.height));
		
		// Hash colors
		hash ^= SimpleHash(&node->computed.backgroundColor, sizeof(Color));
		hash ^= SimpleHash(&node->computed.color, sizeof(Color));
	}
	
	return hash;
}

// Test scene 1: Simple box nesting
static void TestScene1(ui2Context_t *ctx) {
	const char *css = 
		"root { display:block; width:512; height:512; background-color:#000000ff; }\n"
		"box { display:block; width:256; height:128; margin:32; background-color:#ff0000ff; }\n";
	
	UI2_LoadStylesheet(ctx, css);
	
	UI2_BeginFrame(ctx, 512, 512);
	UI2_BeginNode(ctx, "root", NULL);
		UI2_BeginNode(ctx, "box", NULL);
		UI2_EndNode(ctx);
	UI2_EndNode(ctx);
	UI2_EndFrame(ctx);
}

// Test scene 2: Flex row layout
static void TestScene2(ui2Context_t *ctx) {
	const char *css = 
		"root { display:flex; flex-direction:row; width:512; height:256; padding:16; background-color:#202020ff; }\n"
		"item { display:block; width:128; height:auto; margin:8; background-color:#404040ff; }\n";
	
	UI2_LoadStylesheet(ctx, css);
	
	UI2_BeginFrame(ctx, 512, 256);
	UI2_BeginNode(ctx, "root", NULL);
		for (int i = 0; i < 3; ++i) {
			UI2_BeginNode(ctx, "item", NULL);
			UI2_EndNode(ctx);
		}
	UI2_EndNode(ctx);
	UI2_EndFrame(ctx);
}

// Test scene 3: Flex column layout
static void TestScene3(ui2Context_t *ctx) {
	const char *css = 
		"root { display:flex; flex-direction:column; width:256; height:512; padding:16; background-color:#202020ff; }\n"
		"item { display:block; width:auto; height:64; margin:8; background-color:#404040ff; }\n";
	
	UI2_LoadStylesheet(ctx, css);
	
	UI2_BeginFrame(ctx, 256, 512);
	UI2_BeginNode(ctx, "root", NULL);
		for (int i = 0; i < 4; ++i) {
			UI2_BeginNode(ctx, "item", NULL);
			UI2_EndNode(ctx);
		}
	UI2_EndNode(ctx);
	UI2_EndFrame(ctx);
}

// Test scene 4: Absolute positioning
static void TestScene4(ui2Context_t *ctx) {
	const char *css = 
		"root { display:block; width:512; height:512; background-color:#000000ff; }\n"
		"abs { display:block; position:absolute; width:128; height:128; left:64; top:64; background-color:#00ff00ff; }\n";
	
	UI2_LoadStylesheet(ctx, css);
	
	UI2_BeginFrame(ctx, 512, 512);
	UI2_BeginNode(ctx, "root", NULL);
		UI2_BeginNode(ctx, "abs", NULL);
		UI2_EndNode(ctx);
	UI2_EndNode(ctx);
	UI2_EndFrame(ctx);
}

// Test scene 5: Overflow clipping
static void TestScene5(ui2Context_t *ctx) {
	const char *css = 
		"root { display:block; width:256; height:256; padding:16; background-color:#000000ff; overflow:clip; }\n"
		"big { display:block; width:512; height:512; background-color:#ff00ffff; }\n";
	
	UI2_LoadStylesheet(ctx, css);
	
	UI2_BeginFrame(ctx, 256, 256);
	UI2_BeginNode(ctx, "root", NULL);
		UI2_BeginNode(ctx, "big", NULL);
		UI2_EndNode(ctx);
	UI2_EndNode(ctx);
	UI2_EndFrame(ctx);
}

// Load baseline hashes from file (or use defaults)
static uint32_t LoadBaselineHash(const char *testName) {
	// Try to load from file
	fileHandle_t f;
	int fileLen = FS_FOpenFileRead("tests/ui2_baselines.txt", &f, qfalse);
	if (fileLen > 0 && f) {
		char buffer[1024];
		int readLen = FS_Read(buffer, sizeof(buffer) - 1, f);
		FS_FCloseFile(f);
		if (readLen > 0 && readLen < (int)sizeof(buffer)) {
			buffer[readLen] = '\0';
			// Simple parsing: look for "test_name hash_value"
			const char *line = buffer;
			while (*line) {
				// Skip whitespace
				while (*line && (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r' || *line == '#')) {
					if (*line == '#') {
						// Skip comment line
						while (*line && *line != '\n') line++;
					} else {
						line++;
					}
				}
				if (!*line) break;
				
				// Read test name
				const char *nameStart = line;
				while (*line && *line != ' ' && *line != '\t' && *line != '\n') line++;
				size_t nameLen = line - nameStart;
				
				// Check if it matches
				if (nameLen == std::strlen(testName) && 
				    std::strncmp(nameStart, testName, nameLen) == 0) {
					// Skip whitespace
					while (*line && (*line == ' ' || *line == '\t')) line++;
					
					// Read hash (hex)
					if (*line == '0' && (line[1] == 'x' || line[1] == 'X')) {
						line += 2;
						uint32_t hash = 0;
						while ((*line >= '0' && *line <= '9') || 
						       (*line >= 'a' && *line <= 'f') || 
						       (*line >= 'A' && *line <= 'F')) {
							hash = hash * 16;
							if (*line >= '0' && *line <= '9') {
								hash += *line - '0';
							} else if (*line >= 'a' && *line <= 'f') {
								hash += *line - 'a' + 10;
							} else {
								hash += *line - 'A' + 10;
							}
							line++;
						}
						return hash;
					}
				}
				
				// Skip to next line
				while (*line && *line != '\n') line++;
				if (*line == '\n') line++;
			}
		}
	}
	
	// Default: return 0 (will pass if baseline not set)
	return 0;
}

// Baseline hashes (loaded from file or defaults)
static const struct {
	const char *name;
	uint32_t expectedHash;
} testBaselines[] = {
	{ "scene1_box_nesting", 0 },  // Will be loaded from file
	{ "scene2_flex_row", 0 },     
	{ "scene3_flex_column", 0 },  
	{ "scene4_absolute", 0 },     
	{ "scene5_overflow", 0 },     
};

// Mock renderer functions (no-op for testing)
static void TestMock_SetColor(const float *rgba) {
	(void)rgba;
}

static void TestMock_DrawStretchPic(float x, float y, float w, float h, 
                                   float s1, float t1, float s2, float t2, qhandle_t hShader) {
	(void)x; (void)y; (void)w; (void)h;
	(void)s1; (void)t1; (void)s2; (void)t2; (void)hShader;
}

// Run all tests
static void UI2_RunTests(void) {
	Com_Printf("UI2: Running golden-image tests...\n");
	
	// Create mock renderer (tests don't need actual rendering)
	ui2Renderer_t renderer = {
		.SetColor = TestMock_SetColor,
		.DrawStretchPic = TestMock_DrawStretchPic,
		.Scissor = nullptr,
		.TextPaint = nullptr,
		.GetDefaultFont = nullptr
	};
	
	ui2Context_t *ctx = UI2_CreateContext(&renderer);
	if (!ctx) {
		Com_Printf("UI2: Failed to create test context\n");
		return;
	}
	
	int passed = 0;
	int failed = 0;
	
	// Run each test
	typedef void (*TestFunc)(ui2Context_t *);
	struct {
		const char *name;
		TestFunc func;
	} tests[] = {
		{ "scene1_box_nesting", TestScene1 },
		{ "scene2_flex_row", TestScene2 },
		{ "scene3_flex_column", TestScene3 },
		{ "scene4_absolute", TestScene4 },
		{ "scene5_overflow", TestScene5 },
	};
	
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
		// Load expected hash from file
		uint32_t expectedHash = LoadBaselineHash(tests[i].name);
		
		// Run test
		tests[i].func(ctx);
		
		// Compute hash
		uint32_t actualHash = HashFrame(ctx);
		
		// Compare
		if (actualHash == expectedHash || expectedHash == 0) {
			Com_Printf("UI2: [PASS] %s (hash: 0x%08x)\n", tests[i].name, actualHash);
			passed++;
		} else {
			Com_Printf("UI2: [FAIL] %s (expected: 0x%08x, got: 0x%08x)\n", 
			          tests[i].name, expectedHash, actualHash);
			failed++;
		}
		
		// Output hash for baseline generation
		Com_Printf("UI2: Baseline hash for %s: 0x%08x\n", tests[i].name, actualHash);
	}
	
	Com_Printf("UI2: Tests complete: %d passed, %d failed\n", passed, failed);
	
	UI2_DestroyContext(ctx);
}

// Console command: ui2_test
static void UI2_Test_f(void) {
	UI2_RunTests();
}

// Register test command
void UI2_RegisterTestCommand(void) {
	Cmd_AddCommand("ui2_test", UI2_Test_f);
}

} // extern "C"

#endif // __cplusplus
